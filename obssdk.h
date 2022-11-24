#ifndef OBSSDK_H
#define OBSSDK_H

#include <obs.h>
#include <obs.hpp>
#include <obs-source.h>

#include <string>
#include <vector>

using namespace std;
class obssdk
{
public:
    obssdk();
    ~obssdk();

    int init_obs();

    int set_scene_source();
    int add_scene_source();
    void Change_scence(int cpr);

    void activate_s();
    void deactivate_s();

    int  start_rec();
    int  stop_rec();

    OBSSource source;//obs的源
    obs_source_t* source_t = nullptr;//obs的源指针


private:
    bool ResetAudio();
    int  ResetVideo();
    void SetupFFmpeg();
    void Push_camera_id(obs_source_t* src);

private:
    OBSOutput fileOutput;
    obs_source_t* fadeTransition = nullptr;
    obs_scene_t* scene = nullptr;

    obs_source_t* captureSource;
    obs_properties_t* properties;

    OBSEncoder aacTrack[MAX_AUDIO_MIXES];
    std::string aacEncoderID[MAX_AUDIO_MIXES];

    obs_property_t* property = nullptr;
    obs_data_t* setting_source = nullptr;

    vector<string> CameraID;

};

#endif // OBSSDK_H
