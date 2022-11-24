#include "cpswidget.h"
#include "ui_cpswidget.h"
#include "screenshot-obj.hpp"
#include <QMessageBox>
#include <QDateTime>

cpswidget::cpswidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::cpswidget)
    , obs(new (obssdk))
    //, ui(new Ui::cpswidget)
{
    ui->setupUi(this);

    switch(obs->init_obs())
    {
        case -1:
            QMessageBox::information(this, u8"提示",u8"init_obs()失败");
        case -2:
            QMessageBox::information(this, u8"提示",u8"ResetAudio()失败");
        case -3:
            QMessageBox::information(this, u8"提示",u8"ResetVideo()失败");
        case -4:
            QMessageBox::information(this, u8"提示",u8"obs_scene_create()失败");
        case -5:
            QMessageBox::information(this, u8"提示",u8"obs_source_create()失败");
        case -6:
            QMessageBox::information(this, u8"提示",u8"source为空");
        default:
            break;
    }
    source = obs->source;
    program = new OBSQTDisplay();
    program = new OBSQTDisplay();
    //关于预览的回调函数
    auto addDrawCallback = [this]() {
            obs_display_add_draw_callback(program->GetDisplay(), &OBSRender,this);
            obs_display_set_background_color(program->GetDisplay(), 0x000000);
        };

    connect(program, &OBSQTDisplay::DisplayCreated, addDrawCallback);
    ui->verticalLayout->addWidget(program);
}

cpswidget::~cpswidget()
{
    delete ui;
}

/*
 * 预览相关
 */

static inline void startRegion(int vX, int vY, int vCX, int vCY, float oL,
                   float oR, float oT, float oB)
{
    gs_projection_push();
    gs_viewport_push();
    gs_set_viewport(vX, vY, vCX, vCY);
    gs_ortho(oL, oR, oT, oB, -100.0f, 100.0f);
}

static inline void endRegion()
{
    gs_viewport_pop();
    gs_projection_pop();
}

void cpswidget::OBSRender(void *data, uint32_t cx, uint32_t cy)
{
    cpswidget *window = reinterpret_cast<cpswidget *>(data);
    OBSSource source = window->source;

    uint32_t targetCX;
    uint32_t targetCY;
    int x, y;
    int newCX, newCY;
    float scale;

    if (source) {
        targetCX = std::max(obs_source_get_width(source), 1u);
        targetCY = std::max(obs_source_get_height(source), 1u);

    } else {
        struct obs_video_info ovi;

        ovi.fps_num = 30;
        ovi.fps_den = 1;

        obs_get_video_info(&ovi);
        targetCX = ovi.base_width;
        targetCY = ovi.base_height;
    }

    GetScaleAndCenterPos(targetCX, targetCY, cx, cy, x, y, scale);

    newCX = int(scale * float(targetCX));
    newCY = int(scale * float(targetCY));

    startRegion(x, y, newCX, newCY, 0.0f, float(targetCX), 0.0f,
            float(targetCY));

    if (source)
        //将所要预览的源添加在obs_source_video_render函数
        obs_source_video_render(source);
    else
        obs_render_main_texture();

    endRegion();
}

/*
 * 截图相关
 */

static void ScreenshotTick(void *param, float);

ScreenshotObj::ScreenshotObj(obs_source_t *source)
    : weakSource(OBSGetWeakRef(source))
{
    obs_add_tick_callback(ScreenshotTick, this);
}

ScreenshotObj::~ScreenshotObj()
{
    obs_enter_graphics();
    gs_stagesurface_destroy(stagesurf);
    gs_texrender_destroy(texrender);
    obs_leave_graphics();

    obs_remove_tick_callback(ScreenshotTick, this);

    if (th.joinable()) {
        th.join();

        if (cx && cy) {
            /*MainWindow *main = MainWindow::Get();
            main->ShowStatusBarMessage(
                QTStr("Basic.StatusBar.ScreenshotSavedTo")
                    .arg(QT_UTF8(path.c_str())));*/
        }
    }
}

void ScreenshotObj::Screenshot()
{
    OBSSource source = OBSGetStrongRef(weakSource);

    if (source) {
        cx = obs_source_get_base_width(source);
        cy = obs_source_get_base_height(source);
    } else {
        obs_video_info ovi;
        obs_get_video_info(&ovi);
        cx = ovi.base_width;
        cy = ovi.base_height;
    }

    if (!cx || !cy) {
        blog(LOG_WARNING, "Cannot screenshot, invalid target size");
        //qDebug()<<"Cannot screenshot, invalid target size";
        obs_remove_tick_callback(ScreenshotTick, this);
        deleteLater();
        return;
    }

    texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
    stagesurf = gs_stagesurface_create(cx, cy, GS_RGBA);

    gs_texrender_reset(texrender);
    if (gs_texrender_begin(texrender, cx, cy)) {
        vec4 zero;
        vec4_zero(&zero);

        gs_clear(GS_CLEAR_COLOR, &zero, 0.0f, 0);
        gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f);

        gs_blend_state_push();
        gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

        if (source) {
            obs_source_inc_showing(source);
            obs_source_video_render(source);
            obs_source_dec_showing(source);
        } else {
            obs_render_main_texture();
        }

        gs_blend_state_pop();
        gs_texrender_end(texrender);
    }
}

void ScreenshotObj::Download()
{
    gs_stage_texture(stagesurf, gs_texrender_get_texture(texrender));
}

void ScreenshotObj::Copy()
{
    uint8_t *videoData = nullptr;
    uint32_t videoLinesize = 0;

    image = QImage(cx, cy, QImage::Format::Format_RGBX8888);

    if (gs_stagesurface_map(stagesurf, &videoData, &videoLinesize)) {
        int linesize = image.bytesPerLine();
        for (int y = 0; y < (int)cy; y++)
            memcpy(image.scanLine(y),
                   videoData + (y * videoLinesize), linesize);

        gs_stagesurface_unmap(stagesurf);
    }
}

void ScreenshotObj::Save()
{

    th = std::thread([this] { MuxAndFinish(); });
}

void ScreenshotObj::MuxAndFinish()
{
    QDateTime curtime = QDateTime::currentDateTime();

    QString qpath = "D:/"+curtime.toString("yyyyMMddhhmmss")+".png";
    string path = qpath.toStdString();

    image.save((path.c_str()));

    blog(LOG_INFO, "Saved screenshot to '%s'", path.c_str());
    deleteLater();
}

static void ScreenshotTick(void *param, float)
{
    ScreenshotObj *data = reinterpret_cast<ScreenshotObj *>(param);

    if (data->stage == STAGE_FINISH) {
        return;
    }

    obs_enter_graphics();

    switch (data->stage) {
    case STAGE_SCREENSHOT:
        data->Screenshot();
        break;
    case STAGE_DOWNLOAD:
        data->Download();
        break;
    case STAGE_COPY_AND_SAVE:
        data->Copy();
        QMetaObject::invokeMethod(data, "Save");
        obs_remove_tick_callback(ScreenshotTick, data);
        break;
    }

    obs_leave_graphics();

    data->stage++;
}


void cpswidget::on_capture01_clicked()
{
    obs->Change_scence(1);
}


void cpswidget::on_capture02_clicked()
{
    obs->Change_scence(2);
}


void cpswidget::on_craete_capture02_clicked()
{
    obs->add_scene_source();
}


void cpswidget::on_screenshot_clicked()
{
    screenshotData = new ScreenshotObj(obs->source);
}


void cpswidget::on_project_clicked()
{
    /*
     * 这份代码是预览画面和投屏画面一致
     * 第二个是源，第三个是屏幕，0号是主屏，1号是外接屏1，以此类推
     * 若想不一致，可以将预览框的源于OBSProjector的源设置为不同的源
     * 具体思路：通过源的名字获取到不同的源
     */
    projector = new OBSProjector(this, source, 1, ProjectorType::Source);
}

