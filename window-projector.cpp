#include <QAction>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QMenu>
#include <QScreen>
#include "window-projector.hpp"

OBSProjector::OBSProjector(QWidget *widget, obs_source_t *source_, int monitor,ProjectorType type_)
    : OBSQTDisplay(widget, Qt::Window),
      source(source_),
      removedSignal(obs_source_get_signal_handler(source), "remove",
            OBSSourceRemoved, this)
{
    source = source_;
    type_ = ProjectorType::Source;

    SetMonitor(monitor);

    auto addDrawCallback = [this]() {
            obs_display_add_draw_callback(GetDisplay(), &OBSRender,this);
            obs_display_set_background_color(GetDisplay(), 0x000000);
        };

    connect(this, &OBSQTDisplay::DisplayCreated, addDrawCallback);

    if (source)
        obs_source_inc_showing(source);

    ready = true;
    show();
    activateWindow();

}

OBSProjector::~OBSProjector()
{

    auto addDrawCallback = [this]() {
            obs_display_remove_draw_callback(GetDisplay(), &OBSRender,this);
            obs_display_set_background_color(GetDisplay(), 0x000000);
        };
    connect(this, &OBSQTDisplay::destroyed, addDrawCallback);

    if (source)
        obs_source_dec_showing(source);

}
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


void OBSProjector::OBSSourceRemoved(void *data, calldata_t *params)
{
    OBSProjector *window = reinterpret_cast<OBSProjector *>(data);
}

void OBSProjector::SetMonitor(int monitor)
{
    savedMonitor = monitor;
    screen = QGuiApplication::screens()[monitor];
    setGeometry(screen->geometry());
    showFullScreen();
    SetHideCursor();
}
void OBSProjector::SetHideCursor()
{
    setCursor(Qt::BlankCursor);
}

void OBSProjector::OBSRender(void *data, uint32_t cx, uint32_t cy)
{
    OBSProjector *window = reinterpret_cast<OBSProjector *>(data);

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
