#include "obssdk.h"
#include <string>
#include <libavcodec/avcodec.h>
#include <QApplication>
enum SOURCE_CHANNELS {
    SOURCE_CHANNEL_VIDEO_OUTPUT,
    SOURCE_CHANNEL_AUDIO_OUTPUT,
    SOURCE_CHANNEL_AUDIO_INPUT,
};
using namespace std;
obssdk::obssdk()
{
}
obssdk::~obssdk()
{
}
int obssdk::init_obs()
{

    /*
     *把你的obs-plugins，data等等放入对应的文件夹中，位置取决于你的编译obs.dll
     *时定义的位置
     *定义的位置在obs-windwos.c的static const char *module_bin[]
     *和static const char *module_data[]中
     */
    QString path = qApp->applicationDirPath();
    string path_str = path.toStdString();
    string cfg_path = path_str + "/obs_cfg";
    string plugin_path = path_str + "/obs-plugins/64bit";
    string data_path = path_str + "/data/obs-plugins/%module%";

    if (!obs_initialized())
    {
        if (!obs_startup("zh-CN", cfg_path.c_str(), NULL))
        {
            return -1;
        }

        obs_add_module_path(plugin_path.c_str(), data_path.c_str());
        obs_load_all_modules();
    }

    if (!ResetAudio())
        return -2;

    if (ResetVideo() != OBS_VIDEO_SUCCESS)
        return -3;

    return set_scene_source();

}


/*
 *下列函数从obs的同名函数中提取出来，obs原本的同名函数ResetVideo()中还有很多设置，这里只
 *提取了关键部分。
 *在原本的函数中，使用了读取coinfig的方式，而这边为了方便学习，采取了直接赋值的方式。
 *具体都有哪些值，我们可以从在编译obs源码的时候，加入断点，查看变量值的方式得到。
 */
int obssdk::ResetVideo()
{
    struct obs_video_info ovi;
    int ret;

    ovi.fps_num = 30;
    ovi.fps_den = 1;
    ovi.graphics_module = "libobs-d3d11.dll";
    ovi.base_width = 1920;
    ovi.base_height = 1080;
    ovi.output_width = 1920;
    ovi.output_height = 1080;
    ovi.output_format = VIDEO_FORMAT_I420;
    ovi.colorspace = VIDEO_CS_601;
    ovi.range = VIDEO_RANGE_FULL;
    ovi.adapter = 0;
    ovi.gpu_conversion = true;
    ovi.scale_type = OBS_SCALE_BICUBIC;
    ret = obs_reset_video(&ovi);
    if (ret != OBS_VIDEO_SUCCESS) {
        /* Try OpenGL if DirectX fails on windows */
            ovi.graphics_module = "libobs-opengl.dll";
            ret = obs_reset_video(&ovi);
    }
    return ret;
}


bool obssdk::ResetAudio()
{
    struct obs_audio_info ai;

    ai.samples_per_sec = 48000;
    ai.speakers = SPEAKERS_STEREO;

    return obs_reset_audio(&ai);
}



void ResetAudioDevice(const char* sourceId, const char* deviceId,
    const char* deviceDesc, int channel)
{
    bool disable = deviceId && strcmp(deviceId, "disabled") == 0;
    obs_source_t* source;
    obs_data_t* settings;

    source = obs_get_output_source(channel);
    if (source) {
        if (disable) {
            obs_set_output_source(channel, nullptr);
        }
        else {
            settings = obs_source_get_settings(source);
            const char* oldId =
                obs_data_get_string(settings, "device_id");
            if (strcmp(oldId, deviceId) != 0) {
                obs_data_set_string(settings, "device_id",
                    deviceId);
                obs_source_update(source, settings);
            }
            obs_data_release(settings);
        }

        obs_source_release(source);

    }
    else if (!disable) {
        settings = obs_data_create();
        obs_data_set_string(settings, "device_id", deviceId);
        source = obs_source_create(sourceId, deviceDesc, settings,
            nullptr);
        obs_data_release(settings);

        obs_set_output_source(channel, source);
        obs_source_release(source);
    }
}

static inline bool HasAudioDevices(const char* source_id)
{
    const char* output_id = source_id;
    obs_properties_t* props = obs_get_source_properties(output_id);
    size_t count = 0;

    if (!props)
        return false;

    obs_property_t* devices = obs_properties_get(props, "device_id");
    if (devices)
        count = obs_property_list_item_count(devices);

    obs_properties_destroy(props);

    return count != 0;
}

/*
 * 原obs函数中是AddSourceData结构体，这里我们直接结构体中的核心obs_source_t指针
*/
static void AddSource(void *_data, obs_scene_t *scene)
{
    obs_source_t* source = (obs_source_t*)_data;
    obs_scene_add(scene, source);
    obs_source_release(source);
}

static bool CreateAACEncoder(OBSEncoder& res, string& id,const char* name, size_t idx)
{
    const char* id_ = "ffmpeg_aac";

    res = obs_audio_encoder_create(id_, name, nullptr, idx, nullptr);
    if (res) {
        obs_encoder_release(res);
        return true;
    }

    return false;
}


void obssdk::Push_camera_id(obs_source_t* src)
{

    obs_properties_t* ppts = obs_source_properties(src);
    obs_property_t* property = obs_properties_first(ppts);

    while (property)
    {
        const char* name = obs_property_name(property);
        if (strcmp(name, "video_device_id") == 0)
        {
            size_t  count = obs_property_list_item_count(property);
            for (size_t i = 0; i < count; i++)
            {
                const char* str = obs_property_list_item_string(property, i);
                CameraID.push_back(str);//把所有的视频设备id推入CameraID中
            }
        }

        obs_property_next(&property);
    }

}

int obssdk::set_scene_source()
{
    //设置源的通道，未来可根据此函数的第二个参数，指向不同的源，从而变更源
    obs_set_output_source(SOURCE_CHANNEL_VIDEO_OUTPUT, nullptr);
    obs_set_output_source(SOURCE_CHANNEL_AUDIO_OUTPUT, nullptr);
    obs_set_output_source(SOURCE_CHANNEL_AUDIO_INPUT, nullptr);

    //与obs程序的常用方式一样，先创建场景
    scene = obs_scene_create("MyScene");
    if (!scene)
    {
        return -4;
    }

    /*
     *HasAudioDevices的参数为插件关键字，在obs-app.c有声明，根据不同平台可选择
     *coreaudio_input_capture或者pulse_input_capture,windows下为wasapi_input_capture
     */
    bool hasDesktopAudio = HasAudioDevices("wasapi_input_capture");
    bool hasInputAudio = HasAudioDevices("wasapi_output_capture");

    if (hasDesktopAudio)
        ResetAudioDevice("wasapi_output_capture", "default",
            "Default Desktop Audio", SOURCE_CHANNEL_AUDIO_OUTPUT);

    if (hasInputAudio)
        ResetAudioDevice("wasapi_input_capture", "default",
            "Default Mic/Aux", SOURCE_CHANNEL_AUDIO_INPUT);

    /*
     *obs_source_create用来创建源，第一个参数是插件关键字，第二个是源名称
     *在windows下是dshow,linux下是v4l2
     */
    captureSource = obs_source_create("dshow_input", "DshowWindowsCapture",NULL, nullptr);

    if (captureSource)
    {
        obs_scene_atomic_update(scene, AddSource, captureSource);
        Push_camera_id(captureSource);
    }
    else
    {
        return -5;
    }

    if (captureSource != nullptr)
    {
        const char* deviceID = "";
        if(!CameraID.empty())
        {
            deviceID = CameraID[0].c_str();//作为示例，我们先选择第一个设备ID
        }
        obs_scene_atomic_update(scene, AddSource, captureSource);
        obs_data_t* setting = obs_data_create();
        obs_data_t* curSetting = obs_source_get_settings(captureSource);

        obs_data_apply(setting, curSetting);
        obs_data_set_string( setting, "id", "dshow_input" );
        obs_data_set_string( setting, "video_device_id",  deviceID );
        obs_source_update( captureSource, setting );

        properties = obs_source_properties(captureSource);
        obs_property_t *property = obs_properties_first(properties);


        while (property) {
            obs_property_next(&property);
        }
        obs_data_release(setting);

        obs_set_output_source(SOURCE_CHANNEL_VIDEO_OUTPUT, captureSource);//将源放到输出通道中
        return 0;
    }
    else
    {
        return -6;
    }



}


/*
 * 我们添加第二个源，原理同上
 * 这里我们只创建源，不设置通道
 */
int obssdk::add_scene_source()
{
       captureSource = obs_source_create("dshow_input", "DshowWindowsCapture2", NULL, nullptr);

       if (captureSource != nullptr)
       {

           const char* deviceID = "";
           if (!CameraID.empty() && CameraID.size() > 1)
           {
               deviceID = CameraID[1].c_str();
           }
           obs_scene_atomic_update(scene, AddSource, captureSource);
           obs_data_t* setting = obs_data_create();
           obs_data_t* curSetting = obs_source_get_settings(captureSource);

           obs_data_apply(setting, curSetting);
           obs_data_set_string(setting, "id", "dshow_input");
           obs_data_set_string(setting, "video_device_id", deviceID);
           obs_source_update(captureSource, setting);

           properties = obs_source_properties(captureSource);
           obs_property_t* property = obs_properties_first(properties);


           while (property) {
               obs_property_next(&property);
           }

           obs_data_release(setting);
       }

       setting_source = obs_data_create();
       obs_data_t* curSetting = obs_source_get_settings(captureSource);
       obs_data_apply(setting_source, curSetting);
       obs_source_update(captureSource, setting_source);
       obs_data_release(curSetting);

       properties = obs_source_properties(captureSource);
       property = obs_properties_first(properties);

       return 0;
}
/*
 * 通过同一通道的改变源进行源的切换
 */

void obssdk::Change_scence(int cpr)
{
    //通过obs_get_source_by_name函数获得source，并使用指针指向它
    obs_source_t* source_01 = obs_get_source_by_name("DshowWindowsCapture");
    obs_source_t* source_02 = obs_get_source_by_name("DshowWindowsCapture2");

    //给obs_set_output_source赋不同的值，切换显示的源
    if(1==cpr){
        obs_set_output_source(SOURCE_CHANNEL_VIDEO_OUTPUT, source_01);
    }
    else if(2==cpr){
        obs_set_output_source(SOURCE_CHANNEL_VIDEO_OUTPUT, source_02);
    }

}

/*
 *同样取自obs源码
 *是obs的设置->输出->输出模式高级->自定义输出中的内容，我们可以通过obs的配置文件查看内容
 *或者在编译obs源码后采用断点调试的方式来获取
 */
void obssdk::SetupFFmpeg()
{
    obs_data_t* settings = obs_data_create();
    string out_file_name = "D:/CPS.mp4";

    obs_data_set_string(settings, "url", out_file_name.c_str());
    obs_data_set_string(settings, "format_name", "avi");
    obs_data_set_string(settings, "format_mime_type", "video/x-msvideo");
    obs_data_set_string(settings, "muxer_settings", NULL);
    obs_data_set_int(settings, "gop_size", 250);
    obs_data_set_int(settings, "video_bitrate", 5000);
    obs_data_set_string(settings, "video_encoder", "libx264");
    obs_data_set_int(settings, "video_encoder_id", 27);
    obs_data_set_string(settings, "video_settings", NULL);


    obs_data_set_int(settings, "audio_bitrate", 128);
    obs_data_set_string(settings, "audio_encoder", "aac");
    obs_data_set_int(settings, "audio_encoder_id", AV_CODEC_ID_AAC);
    obs_data_set_string(settings, "audio_settings", NULL);

    obs_data_set_int(settings, "scale_width", 1920);
    obs_data_set_int(settings, "scale_height", 1080);

    obs_output_set_mixer(fileOutput, 1);
    obs_output_set_media(fileOutput, obs_get_video(), obs_get_audio());

    obs_output_update(fileOutput, settings);

    obs_data_release(settings);
}


int obssdk::start_rec()
{
    SetupFFmpeg();

    if(!obs_output_start(fileOutput))
        return -1;

    return 0;
}

int obssdk::stop_rec()
{
    bool force = true;

    if (force)
        obs_output_force_stop(fileOutput);
    else
        obs_output_stop(fileOutput);

    return 0;
}
