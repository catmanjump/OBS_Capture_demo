#ifndef CPSWIDGET_H
#define CPSWIDGET_H

#include <QWidget>
#include "obssdk.h"

#include "display-helpers.hpp"
#include "obsqtdisplay.h"
#include <QScreen>
#include <QPointer>
#include "window-projector.hpp"

//截图相关
#define STAGE_SCREENSHOT 0
#define STAGE_DOWNLOAD 1
#define STAGE_COPY_AND_SAVE 2
#define STAGE_FINISH 3

QT_BEGIN_NAMESPACE
namespace Ui { class cpswidget; }
QT_END_NAMESPACE

class cpswidget : public QWidget
{
    Q_OBJECT

public:
    cpswidget(QWidget *parent = nullptr);
    ~cpswidget();
    OBSSource source;

private slots:
    static void OBSRender(void *data, uint32_t cx, uint32_t cy);

    void on_capture01_clicked();

    void on_capture02_clicked();

    void on_craete_capture02_clicked();

    void on_screenshot_clicked();

    void on_project_clicked();

private:
    Ui::cpswidget *ui;
    std::unique_ptr<obssdk> obs;

    QPointer<OBSQTDisplay> program; //预览类
    QScreen *screen = nullptr;
    QPointer<QObject> screenshotData; //截图类
    OBSProjector *projector;//投屏类
};
#endif // CPSWIDGET_H
