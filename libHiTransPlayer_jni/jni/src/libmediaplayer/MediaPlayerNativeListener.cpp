#include "MediaPlayerNativeListener.h"

MediaPlayerNativeListener::MediaPlayerNativeListener(MediaPlayer* player)
{
    mClient = player;
}

MediaPlayerNativeListener::~MediaPlayerNativeListener()
{

}

void notify(int msg, int ext1, int ext2);

