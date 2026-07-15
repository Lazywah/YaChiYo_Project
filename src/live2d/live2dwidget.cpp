// ZH: glew 必須在任何 GL 標頭之前 | EN: glew must come before any GL header
#include <GL/glew.h>

#include "live2dwidget.h"

#include <QFile>
#include <QFileInfo>
#include <QByteArray>
#include <QImage>
#include <QTimer>
#include <QDebug>
#include <QCoreApplication>

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>

#include <QDateTime>
#include <QRandomGenerator>

#include <CubismFramework.hpp>
#include <ICubismAllocator.hpp>
#include <CubismModelSettingJson.hpp>
#include <CubismDefaultParameterId.hpp>
#include <Id/CubismIdManager.hpp>
#include <Model/CubismUserModel.hpp>
#include <Model/CubismModel.hpp>
#include <Math/CubismModelMatrix.hpp>
#include <Math/CubismMatrix44.hpp>
#include <Type/csmMap.hpp>
#include <Type/csmString.hpp>
#include <Type/csmVector.hpp>
#include <Effect/CubismEyeBlink.hpp>
#include <Effect/CubismBreath.hpp>
#include <Effect/CubismPose.hpp>
#include <Motion/CubismMotion.hpp>
#include <Motion/CubismMotionManager.hpp>
#include <Rendering/OpenGL/CubismRenderer_OpenGLES2.hpp>
#include <Rendering/OpenGL/CubismOffscreenManager_OpenGLES2.hpp>

using namespace Csm;
using namespace Csm::DefaultParameterId;
using Renderer = Csm::Rendering::CubismRenderer_OpenGLES2;
using OffscreenMgr = Csm::Rendering::CubismOffscreenManager_OpenGLES2;

namespace {

// ZH: Cubism 記憶體配置器 (對齊邏輯依官方範例) | EN: Cubism allocator (aligned logic from the official sample)
class YaAllocator : public ICubismAllocator
{
public:
    void* Allocate(const csmSizeType size) override { return malloc(size); }
    void  Deallocate(void* memory) override { free(memory); }

    void* AllocateAligned(const csmSizeType size, const csmUint32 alignment) override
    {
        size_t offset = alignment - 1 + sizeof(void*);
        void*  allocation = malloc(size + offset);
        size_t alignedAddress = reinterpret_cast<size_t>(allocation) + sizeof(void*);
        size_t shift = alignedAddress % alignment;
        if (shift)
            alignedAddress += (alignment - shift);
        void** preamble = reinterpret_cast<void**>(alignedAddress);
        preamble[-1] = allocation;
        return reinterpret_cast<void*>(alignedAddress);
    }

    void DeallocateAligned(void* alignedMemory) override
    {
        void** preamble = static_cast<void**>(alignedMemory);
        free(preamble[-1]);
    }
};

YaAllocator             s_allocator;
CubismFramework::Option s_option;
bool                    s_frameworkStarted = false;

void onCubismLog(const csmChar* message)
{
    qInfo("[Cubism] %s", message);
}

// ZH: 供 Cubism 載入檔案 (主要是 r.5 渲染器的 shader)；相對路徑解析至執行檔目錄
// EN: file loader for Cubism (mainly the r.5 renderer's shaders); relative paths resolve to the exe dir
csmByte* cubismLoadFile(const std::string filePath, csmSizeInt* outSize)
{
    QString p = QString::fromStdString(filePath);
    if (QFileInfo(p).isRelative())
        p = QCoreApplication::applicationDirPath() + "/" + p;

    QFile f(p);
    if (!f.open(QIODevice::ReadOnly))
    {
        qWarning("[Live2D] cubismLoadFile failed: %s", qPrintable(p));
        *outSize = 0;
        return nullptr;
    }
    QByteArray data = f.readAll();
    *outSize = static_cast<csmSizeInt>(data.size());
    csmByte* buf = static_cast<csmByte*>(malloc(static_cast<size_t>(data.size())));
    memcpy(buf, data.constData(), static_cast<size_t>(data.size()));
    return buf;
}

void cubismReleaseBytes(csmByte* bytes)
{
    free(bytes);
}

QByteArray readFileBytes(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

} // namespace

//===============================================================================================

Live2DWidget::Live2DWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    // ZH: 要求帶 alpha 的 framebuffer，並讓背景透明 (角色懸浮桌面)
    // EN: request an alpha framebuffer and make the background transparent (float on desktop)
    QSurfaceFormat fmt = format();
    fmt.setAlphaBufferSize(8);
    setFormat(fmt);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);

    // ZH: ~60FPS 重繪 | EN: ~60FPS repaint
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, QOverload<>::of(&Live2DWidget::update));
    m_timer->start(16);
}

Live2DWidget::~Live2DWidget()
{
    makeCurrent();
    if (m_model) { delete m_model; m_model = nullptr; }
    if (m_setting) { delete m_setting; m_setting = nullptr; }
    if (!m_textures.isEmpty())
    {
        std::vector<GLuint> ids;
        for (unsigned int t : m_textures) ids.push_back(t);
        glDeleteTextures(static_cast<GLsizei>(ids.size()), ids.data());
        m_textures.clear();
    }
    doneCurrent();
}

void Live2DWidget::setModel(const QString &modelDir, const QString &modelName)
{
    m_modelDir  = modelDir;
    m_modelName = modelName;
    if (isValid())
    {
        makeCurrent();
        loadModelNow();
        doneCurrent();
        update();
    }
}

void Live2DWidget::initializeGL()
{
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        qWarning("[Live2D] glewInit failed: %s", reinterpret_cast<const char*>(glewGetErrorString(err)));
        return;
    }

    if (!s_frameworkStarted)
    {
        s_option.LogFunction         = onCubismLog;
        s_option.LoggingLevel        = CubismFramework::Option::LogLevel_Verbose;
        s_option.LoadFileFunction    = cubismLoadFile;     // ZH: r.5 渲染器載入 shader 需要 | EN: needed by r.5 renderer to load shaders
        s_option.ReleaseBytesFunction = cubismReleaseBytes;
        CubismFramework::StartUp(&s_allocator, &s_option);
        CubismFramework::Initialize();
        s_frameworkStarted = true;
        qInfo("[Live2D] Cubism framework started (core %u)", Live2D::Cubism::Core::csmGetVersion());
    }

    if (!m_modelName.isEmpty())
        loadModelNow();
}

unsigned int Live2DWidget::loadTexture(const QString &path)
{
    QImage img(path);
    if (img.isNull())
    {
        qWarning("[Live2D] cannot load texture: %s", qPrintable(path));
        return 0;
    }
    QImage rgba = img.convertToFormat(QImage::Format_RGBA8888);

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba.width(), rgba.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.constBits());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

bool Live2DWidget::loadModelNow()
{
    // ZH: 1. 讀 model3.json | EN: read model3.json
    QByteArray jsonBytes = readFileBytes(m_modelDir + "/" + m_modelName + ".model3.json");
    if (jsonBytes.isEmpty())
    {
        qWarning("[Live2D] cannot read model3.json in %s", qPrintable(m_modelDir));
        return false;
    }
    if (m_setting) { delete m_setting; m_setting = nullptr; }
    m_setting = new CubismModelSettingJson(
        reinterpret_cast<const csmByte*>(jsonBytes.constData()),
        static_cast<csmSizeInt>(jsonBytes.size()));

    if (m_model) { delete m_model; m_model = nullptr; }
    m_model = new CubismUserModel();

    // ZH: 2. moc3 | EN: moc3
    QByteArray moc = readFileBytes(m_modelDir + "/" + m_setting->GetModelFileName());
    if (moc.isEmpty()) { qWarning("[Live2D] cannot read moc3"); return false; }
    m_model->LoadModel(reinterpret_cast<const csmByte*>(moc.constData()),
                       static_cast<csmSizeInt>(moc.size()));

    // ZH: 3. physics / pose (可選) | EN: physics / pose (optional)
    if (std::strlen(m_setting->GetPhysicsFileName()) > 0)
    {
        QByteArray b = readFileBytes(m_modelDir + "/" + m_setting->GetPhysicsFileName());
        if (!b.isEmpty())
            m_model->LoadPhysics(reinterpret_cast<const csmByte*>(b.constData()), static_cast<csmSizeInt>(b.size()));
    }
    if (std::strlen(m_setting->GetPoseFileName()) > 0)
    {
        QByteArray b = readFileBytes(m_modelDir + "/" + m_setting->GetPoseFileName());
        if (!b.isEmpty())
            // ZH: 自己持有 Pose 以便每幀更新 (管理互斥圖層透明度) | EN: hold pose ourselves to update it each frame
            m_pose = CubismPose::Create(reinterpret_cast<const csmByte*>(b.constData()), static_cast<csmSizeInt>(b.size()));
    }

    // ZH: 4. layout | EN: layout
    csmMap<csmString, csmFloat32> layout;
    m_setting->GetLayoutMap(layout);
    m_model->GetModelMatrix()->SetupFromLayout(layout);
    m_model->GetModel()->SaveParameters();

    // ZH: 5. 建立渲染器 | EN: create renderer
    m_model->CreateRenderer(width() > 0 ? width() : 512, height() > 0 ? height() : 512);
    Renderer *r = m_model->GetRenderer<Renderer>();

    // ZH: 6. 載入並綁定紋理 | EN: load + bind textures
    for (csmInt32 i = 0; i < m_setting->GetTextureCount(); ++i)
    {
        const csmChar *texFile = m_setting->GetTextureFileName(i);
        if (!texFile || std::strlen(texFile) == 0) continue;
        unsigned int tex = loadTexture(m_modelDir + "/" + texFile);
        m_textures.push_back(tex);
        r->BindTexture(i, tex);
    }
    r->IsPremultipliedAlpha(false);

    // ZH: 7. 自動眨眼 (參數 id 由 model3.json 提供) | EN: auto eye-blink (param ids from model3.json)
    m_eyeBlink = CubismEyeBlink::Create(m_setting);

    // ZH: 8. 呼吸 (使用預設參數，讓身體/頭部自然起伏) | EN: breathing (default params for natural sway)
    {
        CubismIdManager *ids = CubismFramework::GetIdManager();
        m_breath = CubismBreath::Create();
        csmVector<CubismBreath::BreathParameterData> breathParams;
        breathParams.PushBack(CubismBreath::BreathParameterData(ids->GetId(ParamAngleX),     0.0f, 15.0f, 6.5345f, 0.5f));
        breathParams.PushBack(CubismBreath::BreathParameterData(ids->GetId(ParamAngleY),     0.0f,  8.0f, 3.5345f, 0.5f));
        breathParams.PushBack(CubismBreath::BreathParameterData(ids->GetId(ParamAngleZ),     0.0f, 10.0f, 5.5345f, 0.5f));
        breathParams.PushBack(CubismBreath::BreathParameterData(ids->GetId(ParamBodyAngleX), 0.0f,  4.0f, 15.5345f, 0.5f));
        breathParams.PushBack(CubismBreath::BreathParameterData(ids->GetId(ParamBreath),     0.5f,  0.5f, 3.2345f, 1.0f));
        m_breath->SetParameters(breathParams);
    }

    // ZH: 9. 載入動作群組 (Idle + TapBody) | EN: load motion groups (Idle + TapBody)
    m_motionManager = new CubismMotionManager();
    auto loadGroup = [this](const char *group, QList<ACubismMotion*> &out) {
        const csmInt32 count = m_setting->GetMotionCount(group);
        for (csmInt32 i = 0; i < count; ++i)
        {
            QByteArray b = readFileBytes(m_modelDir + "/" + m_setting->GetMotionFileName(group, i));
            if (b.isEmpty()) continue;
            ACubismMotion *m = CubismMotion::Create(reinterpret_cast<const csmByte*>(b.constData()),
                                                    static_cast<csmSizeInt>(b.size()));
            if (m) out.push_back(m);
        }
    };
    loadGroup("Idle", m_idleMotions);
    loadGroup("TapBody", m_tapMotions);
    qInfo("[Live2D] motions loaded: idle=%d tap=%d",
          static_cast<int>(m_idleMotions.size()), static_cast<int>(m_tapMotions.size()));

    m_lastUpdateMs = QDateTime::currentMSecsSinceEpoch();
    m_model->GetModel()->Update();
    qInfo("[Live2D] model ready: %s (params=%d, drawables=%d)",
          qPrintable(m_modelName),
          m_model->GetModel()->GetParameterCount(),
          m_model->GetModel()->GetDrawableCount());
    return true;
}

void Live2DWidget::paintGL()
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!m_model || !m_model->GetModel())
        return;

    Renderer *r = m_model->GetRenderer<Renderer>();
    if (!r)
        return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ZH: 投影矩陣 — 「貼合高度」：角色大小由視窗高度決定，左右多餘畫布留白裁掉
    // EN: projection — fit to height: character size follows window height, extra side margins are cropped
    CubismMatrix44 projection;
    projection.LoadIdentity();
    const float w = static_cast<float>(width());
    const float h = static_cast<float>(height());
    projection.Scale(h / w, 1.0f);
    projection.MultiplyByMatrix(m_model->GetModelMatrix());

    // ZH: 計算與上一幀的時間差 (秒) | EN: delta time since last frame (seconds)
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    float dt = (m_lastUpdateMs > 0) ? (now - m_lastUpdateMs) / 1000.0f : 0.0f;
    m_lastUpdateMs = now;
    if (dt < 0.0f || dt > 0.5f) dt = 0.016f;   // ZH: 防止暫停後跳動 | EN: clamp after pauses

    CubismModel *model = m_model->GetModel();
    model->LoadParameters();                        // ZH: 還原基準狀態 | EN: restore base state

    // ZH: idle 動作 — 播完就隨機換一個 | EN: idle motion — pick a random one when finished
    if (m_motionManager)
    {
        if (m_motionManager->IsFinished() && !m_idleMotions.isEmpty())
        {
            int idx = QRandomGenerator::global()->bounded(m_idleMotions.size());
            m_motionManager->StartMotionPriority(m_idleMotions[idx], false, 1);
        }
        else
        {
            m_motionManager->UpdateMotion(model, dt);
        }
    }

    model->SaveParameters();
    if (m_eyeBlink) m_eyeBlink->UpdateParameters(model, dt);   // ZH: 自動眨眼 | EN: auto blink
    if (m_breath)   m_breath->UpdateParameters(model, dt);     // ZH: 呼吸起伏 | EN: breathing
    if (m_pose)     m_pose->UpdateParameters(model, dt);       // ZH: 圖層姿勢 (互斥部件透明度) | EN: pose (part opacity)

    // ZH: 看向方向 — 平滑轉頭/轉身/眼神 (X 水平、Y 上下，疊加在動作之上) | EN: look — smooth head/body/eye turn (X horiz, Y vert)
    const float ease = qMin(1.0f, dt * 6.0f);
    m_faceCurrentX += (m_faceTargetX - m_faceCurrentX) * ease;
    m_faceCurrentY += (m_faceTargetY - m_faceCurrentY) * ease;
    CubismIdManager *ids = CubismFramework::GetIdManager();
    model->AddParameterValue(ids->GetId(ParamAngleX),     m_faceCurrentX * 30.0f);
    model->AddParameterValue(ids->GetId(ParamAngleY),     m_faceCurrentY * 30.0f);
    model->AddParameterValue(ids->GetId(ParamBodyAngleX), m_faceCurrentX * 10.0f);
    model->AddParameterValue(ids->GetId(ParamEyeBallX),   m_faceCurrentX * 1.0f);
    model->AddParameterValue(ids->GetId(ParamEyeBallY),   m_faceCurrentY * 1.0f);

    // ZH: 說話嘴型 — 到期自動閉嘴 (保險，防外部語音來源崩潰卡開嘴)
    // EN: talking mouth — auto-close on deadline (safety against external voice source crash)
    if (m_talking && m_talkEndMs > 0 && now >= m_talkEndMs)
        m_talking = false;

    // ZH: 多頻正弦疊加取絕對值 → 不規則的說話開合律動 (V1.5 合成，非真音訊振幅)
    // EN: |sum of sines| → irregular talking flutter (V1.5 synthetic, not real audio amplitude)
    float mouthTarget = 0.0f;
    if (m_talking)
    {
        m_mouthPhase += dt;
        const float o = std::sin(m_mouthPhase * 11.0f) * 0.5f
                      + std::sin(m_mouthPhase * 19.0f) * 0.3f
                      + std::sin(m_mouthPhase *  7.0f) * 0.2f;
        mouthTarget = std::fabs(o);   // ZH: 0~1 | EN: 0~1
    }
    const float mouthEase = qMin(1.0f, dt * 15.0f);   // ZH: 嘴比看向反應更快 | EN: mouth reacts faster than gaze
    m_mouthCurrent += (mouthTarget - m_mouthCurrent) * mouthEase;

    // ZH: 說話中或仍在閉合時才覆蓋，否則交還給動作自行控制嘴部 | EN: only override while talking/closing; else leave to motions
    if (m_talking || m_mouthCurrent > 0.01f)
        model->SetParameterValue(ids->GetId(ParamMouthOpenY), m_mouthCurrent);

    model->Update();

    OffscreenMgr *osm = OffscreenMgr::GetInstance();
    osm->BeginFrameProcess();
    r->SetMvpMatrix(&projection);
    r->DrawModel();
    osm->EndFrameProcess();
}

void Live2DWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void Live2DWidget::playTapBody()
{
    if (!m_motionManager || m_tapMotions.isEmpty())
        return;
    // ZH: 優先度 2 > idle 的 1，會中斷 idle 播放點擊動作 | EN: priority 2 > idle's 1, interrupts idle
    int idx = QRandomGenerator::global()->bounded(m_tapMotions.size());
    m_motionManager->StartMotionPriority(m_tapMotions[idx], false, 2);
}

void Live2DWidget::startTalking(double durationSec)
{
    m_talking = true;
    // ZH: 有時長就設自動閉嘴期限 (再加緩衝)，否則等 stopTalking | EN: set auto-close deadline if a duration is given, else wait for stopTalking
    m_talkEndMs = (durationSec > 0.0)
        ? QDateTime::currentMSecsSinceEpoch() + static_cast<qint64>(durationSec * 1000.0) + 300
        : 0;
}

void Live2DWidget::stopTalking()
{
    // ZH: 只停振盪，m_mouthCurrent 由 paintGL 平滑歸零 (自然閉嘴) | EN: stop flutter; paintGL eases mouth to 0 (natural close)
    m_talking = false;
    m_talkEndMs = 0;
}
