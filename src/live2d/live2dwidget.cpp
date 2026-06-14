// ZH: glew 必須在任何 GL 標頭之前 | EN: glew must come before any GL header
#include <GL/glew.h>

#include "live2dwidget.h"

#include <QFile>
#include <QByteArray>
#include <QDebug>

#include <cstdlib>

#include <CubismFramework.hpp>
#include <ICubismAllocator.hpp>
#include <Model/CubismUserModel.hpp>
#include <Model/CubismModel.hpp>

using namespace Csm;   // ZH: Live2D::Cubism::Framework 的別名 | EN: alias for Live2D::Cubism::Framework

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

// ZH: 框架層級狀態 (整個程式僅啟動一次) | EN: framework-level state (started once per process)
YaAllocator                s_allocator;
CubismFramework::Option    s_option;
bool                       s_frameworkStarted = false;

void onCubismLog(const csmChar* message)
{
    qInfo("[Cubism] %s", message);
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
}

Live2DWidget::~Live2DWidget()
{
    makeCurrent();
    if (m_model)
    {
        delete m_model;
        m_model = nullptr;
    }
    doneCurrent();
    // ZH: 框架釋放交由程式結束時處理 (此處為單一 widget) | EN: framework dispose left to app exit (single widget)
}

void Live2DWidget::setModel(const QString &modelDir, const QString &modelName)
{
    m_modelDir  = modelDir;
    m_modelName = modelName;
    if (isValid())          // ZH: GL 已初始化則立即載入 | EN: load now if GL is already initialised
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

    // ZH: 啟動 Cubism 框架 (整個程式一次) | EN: start Cubism framework (once per process)
    if (!s_frameworkStarted)
    {
        s_option.LogFunction   = onCubismLog;
        s_option.LoggingLevel  = CubismFramework::Option::LogLevel_Verbose;
        CubismFramework::StartUp(&s_allocator, &s_option);
        CubismFramework::Initialize();
        s_frameworkStarted = true;
        qInfo("[Live2D] Cubism framework started (core %u)", Live2D::Cubism::Core::csmGetVersion());
    }

    // ZH: 透明背景混合 | EN: transparent background blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (!m_modelName.isEmpty())
        loadModelNow();
}

bool Live2DWidget::loadModelNow()
{
    const QString mocPath = m_modelDir + "/" + m_modelName + ".moc3";
    QByteArray moc = readFileBytes(mocPath);
    if (moc.isEmpty())
    {
        qWarning("[Live2D] cannot read moc3: %s", qPrintable(mocPath));
        return false;
    }

    if (m_model)
    {
        delete m_model;
        m_model = nullptr;
    }

    m_model = new CubismUserModel();
    m_model->LoadModel(reinterpret_cast<const csmByte*>(moc.constData()),
                       static_cast<csmSizeInt>(moc.size()));

    CubismModel *core = m_model->GetModel();
    if (!core)
    {
        qWarning("[Live2D] LoadModel failed for %s", qPrintable(m_modelName));
        delete m_model;
        m_model = nullptr;
        return false;
    }

    qInfo("[Live2D] model loaded: %s  (parameters=%d, parts=%d, drawables=%d)",
          qPrintable(m_modelName),
          core->GetParameterCount(),
          core->GetPartCount(),
          core->GetDrawableCount());
    return true;
}

void Live2DWidget::paintGL()
{
    // ZH: L1.3 僅清空 (透明)；L1.4 起繪製模型 | EN: L1.3 just clears (transparent); model drawing from L1.4
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Live2DWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}
