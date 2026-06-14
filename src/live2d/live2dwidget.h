#ifndef LIVE2DWIDGET_H
#define LIVE2DWIDGET_H

#include <QOpenGLWidget>
#include <QString>

// ZH: 前置宣告，避免在標頭引入 Cubism 標頭 | EN: forward declare to keep Cubism headers out of this header
namespace Live2D { namespace Cubism { namespace Framework {
    class CubismUserModel;
}}}

// ZH: Live2D 角色渲染面 (L1.3：載入模型；L1.4 起渲染)
// EN: Live2D character render surface (L1.3: load model; rendering from L1.4)
class Live2DWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit Live2DWidget(QWidget *parent = nullptr);
    ~Live2DWidget() override;

    // ZH: 指定要載入的模型 (目錄 + 模型名，會在 GL 初始化後載入) | EN: set the model to load (loaded after GL init)
    void setModel(const QString &modelDir, const QString &modelName);

    bool isModelLoaded() const { return m_model != nullptr; }

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private:
    bool loadModelNow();

    Live2D::Cubism::Framework::CubismUserModel *m_model = nullptr;
    QString m_modelDir;
    QString m_modelName;
};

#endif // LIVE2DWIDGET_H
