#ifndef LIVE2DWIDGET_H
#define LIVE2DWIDGET_H

#include <QOpenGLWidget>
#include <QString>
#include <QList>

class QTimer;

// ZH: 前置宣告，避免在標頭引入 Cubism 標頭 | EN: forward declare to keep Cubism headers out of this header
namespace Live2D { namespace Cubism { namespace Framework {
    class CubismUserModel;
    class CubismModelSettingJson;
}}}

// ZH: Live2D 角色渲染面 (L1.4：完整載入並渲染靜態模型)
// EN: Live2D character render surface (L1.4: full load + render static model)
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
    unsigned int loadTexture(const QString &path);   // ZH: PNG → GL 紋理 (用 QImage) | EN: PNG → GL texture via QImage

    Live2D::Cubism::Framework::CubismUserModel        *m_model   = nullptr;
    Live2D::Cubism::Framework::CubismModelSettingJson *m_setting = nullptr;
    QList<unsigned int> m_textures;     // ZH: 已建立的 GL 紋理 id | EN: created GL texture ids
    QTimer *m_timer = nullptr;          // ZH: 重繪計時器 | EN: repaint timer

    QString m_modelDir;
    QString m_modelName;
};

#endif // LIVE2DWIDGET_H
