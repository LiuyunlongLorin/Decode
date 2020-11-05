#ifndef _MEDIA_PLAYER_NATIVE_LISTENER_
#define _MEDIA_PLAYER_NATIVE_LISTENER_

#include "MediaPlayerListener.h"
#include "MediaPlayer.h"

class MediaPlayerNativeListener: public MediaPlayerListener
{
public:
    MediaPlayerNativeListener(MediaPlayer* player);
    ~MediaPlayerNativeListener();
    void notify(int msg, int ext1, int ext2);

private:
    MediaPlayer* mClient;
};
#endif
