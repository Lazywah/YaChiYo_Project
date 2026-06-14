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
#include <string>

#include <CubismFramework.hpp>
#include <ICubismAllocator.hpp>
#include <CubismModelSettingJson.hpp>
#include <Model/CubismUserModel.hpp>
#include <Model/CubismModel.hpp>
#include <Math/CubismModelMatrix.hpp>
#include <Math/CubismMatrix44.hpp>
#include <Type/csmMap.hpp>
#include <Type/csmString.hpp>
#include <Rendering/OpenGL/CubismRenderer_OpenGLES2.hpp>
#include <Rendering/OpenGL/CubismOffscreenManager_OpenGLES2.hpp>

using namespace Csm;
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
    // ZH: ~60FPS 重繪 (L1.4 靜態也需至少一次繪製，並為 L1.5 動畫鋪路)
    // EN: ~60FPS repaint (static needs at least one paint; preps L1.5 animation)
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
            m_model->LoadPose(reinterpret_cast<const csmByte*>(b.constData()), static_cast<csmSizeInt>(b.size()));
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

    // ZH: 投影矩陣，依視窗長寬比縮放讓模型不變形 | EN: projection scaled by aspect to avoid distortion
    CubismMatrix44 projection;
    projection.LoadIdentity();
    const float w = static_cast<float>(width());
    const float h = static_cast<float>(height());
    if (m_model->GetModel()->GetCanvasWidth() > 1.0f && w < h)
    {
        m_model->GetModelMatrix()->SetWidth(2.0f);
        projection.Scale(1.0f, w / h);
    }
    else
    {
        projection.Scale(h / w, 1.0f);
    }
    projection.MultiplyByMatrix(m_model->GetModelMatrix());

    m_model->GetModel()->Update();

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
