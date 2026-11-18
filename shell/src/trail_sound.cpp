#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#endif

void trailSound(const char* name) {
#ifdef _WIN32
  const auto path=std::string("shell/assets/")+name+".wav";
  PlaySoundA(path.c_str(),nullptr,SND_FILENAME|SND_ASYNC|SND_NODEFAULT);
#else
  (void)name;
#endif
}

void trailMusic(bool enabled) {
#ifdef _WIN32
  static bool opened=false;
  if(!opened && enabled) {
    opened=mciSendStringA("open \"shell/assets/willow-theme.wav\" type waveaudio alias ktTheme",nullptr,0,nullptr)==0;
  }
  if(opened) {
    // Waveaudio devices vary in their support for the repeat verb. The shell
    // checks the playback state between loops.
    if(enabled) {
      char state[32]{};
      mciSendStringA("status ktTheme mode",state,sizeof(state),nullptr);
      if(std::string(state)!="playing")mciSendStringA("play ktTheme from 0",nullptr,0,nullptr);
    } else mciSendStringA("stop ktTheme",nullptr,0,nullptr);
  }
#else
  (void)enabled;
#endif
}
