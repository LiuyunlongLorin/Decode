package com.hisilicon.camplayer;

import java.io.IOException;
import java.lang.ref.WeakReference;
import java.nio.ByteBuffer;



import android.media.AudioFormat;
import android.media.AudioTrack;
import android.os.PowerManager;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.widget.SeekBar;

/**
 * hisilicon CAM Player interface, used for playing CAM<br>
 * media at android client<br>
 * CN: 播放器接口，用于android手机客户端播放器<br>
 *
 * @author hisilicon
 *
 */
public class HiCamPlayer {
    private static final int MEDIA_PREPARED = 1;

    private static final int MEDIA_PLAYBACK_INFO = 2;

    private static final int MEDIA_BUFFERING_UPDATE = 3;

    private static final int MEDIA_ERROR = 100;

    private static final int MEDIA_INFO = 200;

    private static final int MEDIA_ASR_CHANGE = 300;

    private static final int MEDIA_FILE_EOF = 400;

    private static final int MEDIA_ERROR_UNKNOWN = 1;

    private static final int MEDIA_ERROR_SERVER_DIED = 100;

    private static final int MEDIA_PLAYBACK_PREPARED = 1;

    private static final int MEDIA_PLAYBACK_PAUSED = 7;

    private static final int MEDIA_PLAYBACK_STARTED = 6;

    private static final int MEDIA_PLAYBACK_STOPPED = 8;

    private static final int MEDIA_PLAYBACK_BUFFERING_START = 701;

    private static final int MEDIA_PLAYBACK_BUFFERING_END = 702;

    private static final int MEDIA_PLAYBACK_LOADING_PERCNT = 711;

    private final static String TAG = "HICAMPlayer";

    private int mNativeContext = 0; // accessed by native methods
    private int mNativeField = 0; // accessed by native methods
    private int mNativeSurface = 0; // accessed by native methods
    private Surface mSurface = null;
    private SurfaceHolder mSurfaceHolder;
    private static AudioTrack mAudioTrack = null;
    private PowerManager.WakeLock mWakeLock = null;
    private HiCamPlayerStateListener mCamPlayerListener = null;
    private onSeekBufferingStateListener mSeekBufferingStateListener;
    private boolean mScreenOnWhilePlaying = false;
    private boolean mStayAwake = false;

    /*payload H264: 1 for pframe 5 for iframe*/
    /*payload H265: 1 for pframe 19 for iframe*/

    public  class RecFrameInfo
    {
        int frameSize;
        int payload;
        long pts;
    }
    /*yuv info*/
    public  class YuvFrameInfo
    {
        int width;
        int height;
        int ypitch;
        int upitch;
        int vpitch;
        int uoffset;
        int voffset;
        long pts;
    }

    public static final int MEDIA_STREAM_TYPE_UNKOWN = 4;
    public static final int MEDIA_STREAM_TYPE_METADATA = 3;
    public static final int MEDIA_STREAM_TYPE_AUDIO = 2;
    public static final int MEDIA_STREAM_TYPE_VIDEO = 1;

    public  class StreamInfo
    {
        int trackType;

        //only valid when trackType is video
        int width;
        int height;
        //only valid when trackType is audio
        int audiochn;
        int sampleRate;

        String mime;
    }

    /**
     * HiCamPlayer state enumerate, used for state callback<br>
     * CN: HiCamPlayer 各状态描述，用于播放器状态回调通知<br>
     *
     */
    public enum HiCamPlayerState {
        HICAMPLAYER_STATE_IDLE, HICAMPLAYER_STATE_PREPARED, HICAMPLAYER_STATE_PLAY, HICAMPLAYER_STATE_PAUSE, HICAMPLAYER_STATE_STOP, HICAMPLAYER_STATE_ERROR
    }

    /**
     * HiCamPlayerStateListener: event callback and state notification of player<br>
     * CN: player事件通知和状态回调<br>
     *
     */
    public interface HiCamPlayerStateListener {
        /**
         * player callback when native error happened<br>
         * CN: 播放器异常发生的通知<br>
         *
         * @param player
         *            object of state change<br>
         *            CN: 发生异常的播放器对象<br>
         * @param msg
         *            string of exception information<br>
         *            CN: 异常的具体信息<br>
         * @param extra
         *            extending use<br>
         *            CN: 扩展使用<br>
         */
        public void onError(HiCamPlayer player, String msg, int extra);

        /**
         * player callback when surface width&height changed<br>
         * CN: 视频宽高发生变化<br>
         *
         * @param player
         *            player object<br>
         *            CN: 播放器对象<br>
         */
        public void onASRChange(HiCamPlayer player);

        /**
         * player callback when media file or stream playing end<br>
         * CN: 媒体播放结束状态通知<br>
         *
         * @param player
         *            player object<br>
         *            CN: 播放器对象<br>
         */
        public void onFinish(HiCamPlayer player);

        /**
         * player state change notification<br>
         * CN: 播放器状态改变事件通知
         * @param player
         *            player object<br>
         *            CN: 播放器对象<br>
         * @param state
         *            player state<br>
         *            CN: 播放器当前状态<br>
         */
        public void onStateChange(HiCamPlayer player, HiCamPlayerState state);

        /**
         * notification of buffering data take how many part of whole media stream<br>
         * CN: 播放器播放当前网络文件所缓存的数据量占总文件大小的百分比，每1s回调一次
         * @param player
         * @param percent
         */
        public void onBufferingUpdate(HiCamPlayer player, int percent);
    }

    /**
     * onSeekBufferingStateListener: buffering event callback of player<br>
     * CN: player媒体文件缓冲状态回调<br>
     *
     */
    public interface onSeekBufferingStateListener {
        /**
         * player buffering started notification
         * CN: 播放媒体数据开始缓冲事件通知，播放器此时会暂停播放
         * @param player
         *            player object<br>
         *            CN: 播放器对象<br>
         */
        public void onSeekBufferingStart(HiCamPlayer player);

        /**
         * player buffering ended notification<br>
         * CN: 播放媒体数据结束缓冲事件通知，播放器此时会自动开始播放<br>
         * @param player
         *            player object<br>
         *            CN: 播放器对象<br>
         */
        public void onSeekBufferingEnd(HiCamPlayer player);

        /**
         *
         * @param player
         *            player object<br>
         *            CN: 播放器对象<br>
         * @param percent
         *            buffering percent, range from 0--100,<br>
         *            stop buffering when 100<br>
         *            CN: 缓冲百分比，范围0--100,100%时停止缓冲<br>
         */
        public void onSeekBufferingLoadingPercent(HiCamPlayer player,
                int percent);
    }

    static {
        native_init();
    }

    /**
     * constructor of player , some resource allocate<br>
     * CN: 播放器构造函数，做一些初始资源分配工作<br>
     *
     */
    public HiCamPlayer() throws IllegalStateException{
        // TODO Auto-generated constructor stub
        native_setup(new WeakReference<HiCamPlayer>(this));
    }

    /**
     * set player buffering listener<br>
     * CN: 设置播放器缓冲回调监听接口<br>
     * @param listener
     *          input callback interface<br>
     *          CN: 回调接口<br>
     * @see onSeekBufferingStateListener
     */
    public void setOnSeekBufferingStateListener(
            onSeekBufferingStateListener listener) {
        mSeekBufferingStateListener = listener;
    }

    /**
     * set player state change and exception listener<br>
     * CN: 设置播放器状态以及异常监听接口<br>
     * @param listener
     *          input callback interface<br>
     *          CN: 回调接口<br>
     * @see HiCamPlayerStateListener
     */
    public void setHiCamPlayerListener(HiCamPlayerStateListener listener) {
        mCamPlayerListener = listener;
    }

    /**
     * Sets the SurfaceHolder to use for displaying the video portion of the<br>
     * media. first calling it must before prepare, otherwise,<br>
     * will result prepare failed; not support dynamic change surface.<br>
     * CN: 设置surfaceholder，用以显示视频，需要在prepare之前调用，否则<br>
     * 会导致prepare失败，且不支持动态修改设置surfaceholder<br>
     *
     * @param sh
     *           the SurfaceHolder used for video display<br>
     *           CN: 视频显示容器<br>
     * @throws IllegalStateException
     *           called in an invalid state<br>
     *           CN: 调用时播放器状态异常<br>
     */
    public void setDisplay(SurfaceHolder sh) throws IllegalStateException {
        mSurfaceHolder = sh;
        if (sh != null) {
            mSurface = sh.getSurface();
        } else {
            mSurface = null;
        }
        int osVersion = android.os.Build.VERSION.SDK_INT;
        Log.e(TAG, "android sdk version: " + osVersion);
        _setVideoSurface(mSurface, osVersion);
        setScreenOnWhilePlaying(true);
    }
    /**
     * set the state of record<br>
     * call it before start record thread or after stop record thread<br>
     * CN: 设置录像状态，在启动录像线程前调用，及在录像停止后调用<br>
     *
     * @param flag
     *           the state flag of record<br>
     *           CN: 录像状态<br>
     * @throws IllegalStateException
     *           called in an invalid state<br>
     *           CN: 调用时播放器状态异常<br>
     */
    public void setRecordFlag(int flag) throws IllegalStateException {
        _setRecordFlag(flag);
    }
    /**
     * Starts or resumes playback. If playback had previously been paused,<br>
     * playback will continue from where it was paused. If playback had been<br>
     * stopped, or never started before, playback will start at the beginning.<br>
     * CN： 启动媒体播放，如果当前播放器处于暂停，将会在暂停点继续播放；如果当前还未调用<br>
     *     过该接口，则会从媒体流开始处进行播放<br>
     *
     * @throws IllegalStateException
     *             called in an invalid state<br>
     *             CN: 调用时播放器状态异常<br>
     */
    public void start() throws IllegalStateException {
        stayAwake(true);
        if (mAudioTrack != null) {
            mAudioTrack.play();
        }
        _start();
    }

    /**
     * Stops current playback if player in running，<br>
     * Call start() to resume.<br>
     * CN: 停止当前媒体播放, 可通过调用start()恢复播放<br>
     *
     * @throws IllegalStateException
     *             called in an invalid state<br>
     *             CN: 调用时播放器状态异常<br>
     */
    public void stop() throws IllegalStateException {
        stayAwake(false);
        _stop();
    }

    /**
     * Starts get video frame from the native<br>
     * call once get one frame <br>
     * CN： 启动录像线程时，循环调用此接口获取一帧码流数据<br>
     *
     * @throws IllegalStateException
     *             called in an invalid state<br>
     *             CN: 调用时播放器状态异常<br>
     */
    public RecFrameInfo getRecordVideo(ByteBuffer buffer) throws IllegalStateException {
        return _getRecordVideo( buffer);
    }

     /**
     * Starts get audio frame from the native<br>
     * call once get one audio frame <br>
     * CN： 启动录像线程时，循环调用此接口获取一帧音频数据<br>
     *
     * @throws IllegalStateException
     *             called in an invalid state<br>
     *             CN: 调用时播放器状态异常<br>
     */
    public RecFrameInfo getRecordAudio(ByteBuffer buffer) throws IllegalStateException {
        return _getRecordAudio( buffer);
    }

    /**
     * Starts get yuv data from the native<br>
     * call once get one frame <br>
     * CN： 点击抓拍按钮，调用本接口，实现获取一帧YUV数据<br>
     *
     * @throws IllegalStateException
     *             called in an invalid state<br>
     *             CN: 调用时播放器状态异常<br>
     */
    public YuvFrameInfo getSnapData(ByteBuffer buffer) throws IllegalStateException {
        return _getSnapData( buffer);
    }

    public StreamInfo[] getStreamInfo() throws IllegalStateException {
        return _getStreamInfo();
    }

    public int selectIndex(int index) throws IllegalStateException {
        return _selectIndex(index);
    }

    /**
     * Pauses current playback  if player in running，<br>
     * Call start() to resume.<br>
     * CN: 暂停当前媒体播放, 可通过调用start()恢复播放<br>
     *
     * @throws IllegalStateException
     *             called in an invalid state<br>
     *             CN: 调用时播放器状态异常<br>
     */
    public void pause() throws IllegalStateException {
        stayAwake(false);
        _pause();
    }



    /**
     * Control whether we should use the attached SurfaceHolder to keep the<br>
     * screen on while video playback is occurring. it doesn't require that<br>
     * the application have permission for low-level wake lock access.<br>
     * CN: 设置surface是否保持屏幕常亮，该接口不需要wake lock访问权限<br>
     *
     * @param screenOn
     *            Supply true to keep the screen on, false to allow it to turn<br>
     *            off.<br>
     *            CN: true时常亮，false时允许屏幕自动黑屏<br>
     */
    public void setScreenOnWhilePlaying(boolean screenOn) {
        if (mScreenOnWhilePlaying != screenOn) {
            mScreenOnWhilePlaying = screenOn;
            updateSurfaceScreenOn();
        }
    }

    /**
     * Sets the media data source as a Uri.<br>
     * CN：设置媒体数据源<br>
     *
     * @param URL
     *            the Content URI of the data to play<br>
     *            CN: 待播放的本地/远端媒体流<br>
     * @throws IllegalStateException
     *             called in an invalid state<br>
     *             CN: 调用时播放器状态异常<br>
     * @throws IOException
     *             called when url access error<br>
     *             CN: 调用时URL访问异常<br>
     * @throws IllegalArgumentException
     *             called when input para error<br>
     *             CN: 调用时传入参数异常<br>
     */
    public void setDataSource(String uri) throws IOException,
            IllegalArgumentException, IllegalStateException {
        _setDataSource(uri);
    }

    /**
     * Releases resources associated with this MediaPlayer object.<br>
     * It is considered good practice to call this method<br>
     * when you're done using the MediaPlayer.<br>
     * CN：释放播放器所占用的资源，调用该接口后，播放器对象不再可用<br>
     *
     */
    public void release() {
        stayAwake(false);
        mCamPlayerListener = null;
        _release();
        if (mAudioTrack != null) {
            mAudioTrack.release();
            mAudioTrack = null;
        }
    }

    /**
     * Resets the Player to its uninitialized state. After calling this<br>
     * method, you will have to initialize it again by setting the data source<br>
     * and calling prepare().<br>
     * CN：重置播放器到未初始化状态，该接口调用后，需要重新setDataSource和prepare<br>
     *
     */
    public void reset() {
        stayAwake(false);
        _reset();
        if (mAudioTrack != null) {
            mAudioTrack.release();
            mAudioTrack = null;
        }
    }

    /**
     * for extend and customization use<br>
     * CN：为扩展和用户定制功能预留<br>
     * @param msgId
     *          message ID to identify function<br>
     *          CN：功能标示<br>
     * @param what
     *          input parameter 1<br>
     *          CN：输入参数1<br>
     * @param extra
     *          input parameter 2<br>
     *          CN：输入参数2<br>
     * @return
     *          execute result<br>
     *          CN：执行结果<br>
     */
    public int invoke(int msgId, int what, int extra) {
        return _invoke(msgId, what, extra);
    }

    /**
     * Prepares the player for playback, synchronously. need to<br>
     * be called after setting the datasource and the display surface<br>
     * CN：为播放器播放媒体准备资源,同步接口，调用前需要播放器已设置URL<br>
     * 和surfaceHolder<br>
     *
     * @throws IllegalStateException
     *             called in an invalid state<br>
     *             CN: 调用时播放器状态异常<br>
     */
    public native void prepare() throws IOException, IllegalStateException;

    /**
     * Returns the width of the video. return 0 if video resource not ready<br>
     * , or the width has not been determined yet.<br>
     * CN：返回视频宽度，如视频资源还未设置，或者无视频，或者宽度还未被解析<br>
     *     会返回0值.<br>
     *
     * @return
     *          the width of the video<br>
     *          CN：视频宽度<br>
     */
    public native int getVideoWidth();

    /**
     * Returns the height of the video. return 0 if video resource not ready<br>
     * , or the height has not been determined yet.<br>
     * CN：返回视频高度，如视频资源还未设置，或者无视频，或者高度还未被解析<br>
     *     会返回0值.<br>
     *
     * @return
     *          the height of the video<br>
     *          CN：视频高度<br>
     */
    public native int getVideoHeight();

    /**
     * Checks whether Player is in running state.<br>
     * CN：返回当前播放器是否在播放状态<br>
     *
     * @return
     *          true if currently playing, false otherwise<br>
     *          CN：播放中返回true，其他返回false<br>
     */
    public native boolean isPlaying();

    /**
     * Seeks to specified time position. need to call <br>
     * after prepare or start, other will not take action.<br>
     * seekTime must in range of 0 and maxDuration, duration value<br>
     * could be get through{@link HiDVPlayer#getDuration}<br>
     * CN :设置从某个时间点播放，在prepare或start执行后调用才会起作用，<br>
     *     设置的时间点单位是毫秒，范围是0到最大时长，时长可通过<br>
     *     {@link HiDVPlayer#getDuration}接口获得<br>
     *
     * @param msec
     *            the offset in milliseconds from the start to seek to<br>
     *            CN：seek的时间点，以毫秒为单位<br>
     * @throws IllegalStateException
     *            called in an invalid state<br>
     *            CN: 调用时播放器状态异常<br>
     */
    public native void seekTo(int msec) throws IllegalStateException;

    /**
     * Gets the current playBack position. always return 0<br>
     * if player not be running， or playing live stream<br>
     * CN：获取媒体流的播放当前时间，如播放器不是正在播放，或者正在<br>
     * 播放直播流，返回0.<br>
     *
     * @return
     *          the current position in milliseconds<br>
     *          CN: 当前播放时间，以毫秒为单位<br>
     */
    public native int getCurrentPosition();

    /**
     * Gets the duration of media stream. need called after setDataSoure<br>
     * CN：获取当前播放媒体时长，需要在setDataSource之后调用，否则返回0.<br>
     *
     * @return
     *          the duration in milliseconds<br>
     *          CN：毫秒级的媒体总时长<br>
     */
    public native int getDuration();

    /**
     * call after player setDataSoure, before prepare, range from 0----100,<br>
     * only used for live stream mediaBuf water level<br>
     *
     * @param dropLimit
     *            when mediaBuf content size large than this limit, will drop <br>
     *            several frames in mediaBuf and wait until next I frame;default:70<br>
     * @param clearLimit
     *            when mediaBuf content size large than this limit, will<br>
     *            clear mediaBuf and wait until next I frame; default: 90<br>
     */
    private native void setVideoMbufLimit(int dropLimit, int clearLimit);

    /**
     * just use for 265 decoder, need call after player setDataSoure and before<br>
     * prepare; when DV video stream max than default, this interface called<br>
     * to adapt<br>
     *
     * @param maxWidth
     *            max decode width capability for 265 decoder;default 1920;<br>
     *            must be multiple of 16, max to 2592<br>
     * @param maxHeight
     *            max decode height capability for 265 decoder;default:1080;<br>
     *            must be multiple of 16, max to 1944<br>
     */
    private native void setMaxResolution(int maxWidth, int maxHeight);


    /**
     * just used for liveClient private protocol, call after player created,<br>
     * before player setDataSource;used to save video stream as file:<br>
     * /sdcard/org_vid<br>
     *
     * @param flag
     *            whether save video stream received from remote server, value:<br>
     *            0, indicate no; 1, indicate yes, default:0<br>
     */
    private native void setSaveDataFlag(int flag);

    /**
     * just used for liveClient private protocol, call after player<br>
     * setDataSource and before preapare<br>
     *
     * @param mode
     *            0/low delay mode; 1/fluent mode ,default 0<br>
     */
    public native void setLivePlayMode(int mode);

    /**
     * just used for record set state in liveClient private protocol, call after player created,<br>
     * before record ;used to set the state of record :<br>
     * /sdcard/org_vid<br>
     *
     * @param flag
     *            whether save video stream received from remote server, value:<br>
     *            0, stop record no; 1, start record yes, default:0<br>
     */
    private native void _setRecordFlag(int flag);

    /**
     * set surface display , not interupt by sleep of phone/pad
     */
    private void updateSurfaceScreenOn() {
        if (mSurfaceHolder != null) {
            mSurfaceHolder.setKeepScreenOn(mScreenOnWhilePlaying && mStayAwake);
        }
    }

    private static void onMediaPlayBackInfo(Object mediaplayer_ref, int arg1,
            int arg2) {
        HiCamPlayer player = (HiCamPlayer) ((WeakReference) mediaplayer_ref)
                .get();
        HiCamPlayerStateListener listener = player.mCamPlayerListener;
        onSeekBufferingStateListener seekListener = player.mSeekBufferingStateListener;

        switch (arg1) {
        case MEDIA_PLAYBACK_PREPARED:
            Log.d(TAG, "Playback Prepared~");
            if (listener != null) {
                listener.onStateChange(player,
                        HiCamPlayerState.HICAMPLAYER_STATE_PREPARED);
            }
            break;

        case MEDIA_PLAYBACK_STARTED:
            Log.d(TAG, "Playback started~");
            if (listener != null) {
                listener.onStateChange(player,
                        HiCamPlayerState.HICAMPLAYER_STATE_PLAY);
            }
            break;

        case MEDIA_PLAYBACK_STOPPED:
            Log.d(TAG, "Playback stopped~");
            if (listener != null) {
                listener.onStateChange(player,
                        HiCamPlayerState.HICAMPLAYER_STATE_STOP);
            }
            break;

        case MEDIA_PLAYBACK_PAUSED:
            Log.d(TAG, "Playback paused~");
            if (listener != null) {
                listener.onStateChange(player,
                        HiCamPlayerState.HICAMPLAYER_STATE_PAUSE);
            }
            break;

        case MEDIA_PLAYBACK_BUFFERING_START:
            Log.d(TAG, "cache buffering started~");
            if (seekListener != null) {
                seekListener.onSeekBufferingStart(player);
            }
            break;

        case MEDIA_PLAYBACK_BUFFERING_END:
            Log.d(TAG, "cache buffering ended~ ");
            if (seekListener != null) {
                Log.d(TAG, "MEDIA_PLAYBACK_BUFFERING_END");
                seekListener.onSeekBufferingEnd(player);
            }
            break;

        case MEDIA_PLAYBACK_LOADING_PERCNT:
            Log.d(TAG, "loading percent updated: " + arg2);
            if (seekListener != null) {
                seekListener.onSeekBufferingLoadingPercent(player, arg2);
            }
            break;
        }
    }

    /**
     * Called from native code when an interesting event happens. This method
     * just uses the EventHandler system to post the event back to the main app
     * thread. We use a weak reference to the original MediaPlayer object so
     * that the native code is safe from the object disappearing from underneath
     * it.
     */
    private static void postEventFromNative(Object mediaplayer_ref, int what,
            int arg1, int arg2, Object obj) {
        HiCamPlayer player = (HiCamPlayer) ((WeakReference) mediaplayer_ref)
                .get();
        switch (what) {
        case MEDIA_PLAYBACK_INFO:
            if (player.mCamPlayerListener == null)
                Log.d(TAG, "HiCamPlayer Error listener null~");
            onMediaPlayBackInfo(mediaplayer_ref, arg1, arg2);
            break;

        case MEDIA_ERROR:
            if (arg1 == MEDIA_ERROR_SERVER_DIED) {
                Log.d(TAG, "HiCamPlayer Error MEDIA_ERROR_SERVER_DIED~");
                if (player.mCamPlayerListener != null) {
                    player.mCamPlayerListener.onError(player,
                            "HiCamPlayer Error~", 0);
                }
            }
            break;

        case MEDIA_ASR_CHANGE:
            if (player.mCamPlayerListener != null) {
                player.mCamPlayerListener.onASRChange(player);
            }
            break;

        case MEDIA_BUFFERING_UPDATE:
        if (player.mCamPlayerListener != null) {
        player.mCamPlayerListener.onBufferingUpdate(player, arg1);
        Log.d(TAG, "buffering updated: " + arg1);
        }
        break;

        case MEDIA_FILE_EOF:
            if (player.mCamPlayerListener != null) {
                Log.i(TAG, "HiCamPlayer file endof ~");
                //onMediaPlayBackInfo(mediaplayer_ref, MEDIA_PLAYBACK_PAUSED, arg2);
                player.mCamPlayerListener.onFinish(player);
            }
            break;
        }
    }

    private void stayAwake(boolean awake) {
        if (mWakeLock != null) {
            if (awake && !mWakeLock.isHeld()) {
                mWakeLock.acquire();
            } else if (!awake && mWakeLock.isHeld()) {
                mWakeLock.release();
            }
        }
        mStayAwake = awake;
        updateSurfaceScreenOn();
    }

    /**
     * interface of Audio Track to flush audioTrack data
     */
    private static void audioFlush() {
        Log.d(TAG, "audioFlush");
        if (mAudioTrack != null
                && AudioTrack.PLAYSTATE_PLAYING == mAudioTrack.getPlayState())
            mAudioTrack.flush();
    }

    /**
     * interface of Audio Track to writing in pcm audio data
     *
     * @param byteArray
     *            PCM data
     */
    private static void writePCM(byte[] byteArray) {
        if (mAudioTrack != null
                && AudioTrack.PLAYSTATE_PLAYING == mAudioTrack.getPlayState())
            mAudioTrack.write(byteArray, 0, byteArray.length);
    }

    /**
     * config attributes of AudioTrack and run it.
     *
     * @param streamType
     *            eg:STREAM_VOICE_CALL, STREAM_SYSTEM, STREAM_RING, STREAM_MUSIC
     *            and STREAM_ALARM
     * @param sampleRate
     *            eg:48000,44100etc.
     * @param channelConfig
     *            channel num eg:1,2,5 etc
     * @param bytesPerSample
     *            bandwidth eg: 2,3 etc
     * @param trackMode
     *            file or stream
     * @return broadcast delay
     */
    private static int configATrack(int streamType, int sampleRate,
            int channelConfig, int bytesPerSample, int trackMode) {
        int latency = 0;
        int chanConfig = (channelConfig == 2) ? (AudioFormat.CHANNEL_OUT_MONO)
                : (AudioFormat.CHANNEL_OUT_STEREO);
        // Get the AudioTrack minimum buffer size
        int iMinBufSize = AudioTrack.getMinBufferSize(sampleRate, chanConfig,
                bytesPerSample);
        Log.i(TAG, "iMinBufSize" + iMinBufSize);
        if (iMinBufSize == AudioTrack.ERROR_BAD_VALUE
                || iMinBufSize == AudioTrack.ERROR) {
            return 0;
        }
        // Constructor a AudioTrack object
        try {
            mAudioTrack = new AudioTrack(streamType, sampleRate, chanConfig,
                    bytesPerSample, iMinBufSize * 4, trackMode);
            Log.i(TAG, "mAudioTrack OK " + "streamType:"+ streamType +
                    " sampleRate:"+sampleRate + " chanConfig:"+ chanConfig
                    + " audioFormat:"+bytesPerSample + " trackMode:"+ trackMode);
            mAudioTrack.play();
            latency = (int) (iMinBufSize * 4.0 * 1000 / bytesPerSample
                    / sampleRate / channelConfig);
            Log.i(TAG, "mAudioTrack play OK");
        } catch (IllegalArgumentException iae) {
            Log.i(TAG, "new AudioTrack Exceeption:" + iae.toString());
        }
        return latency;
    }

    private native void _setVideoSurface(Surface surface, int apiVersion)
            throws IllegalStateException;

    private native void _setDataSource(String URL) throws IOException,
            IllegalArgumentException, IllegalStateException;

    private native void _start() throws IllegalStateException;

    private native void _stop() throws IllegalStateException;

    private native void _pause() throws IllegalStateException;

    private native void _release();

    private native void _reset();

    private native int _invoke(int msgId, int what, int extra);

    private native RecFrameInfo _getRecordVideo(ByteBuffer byteBuf) throws IllegalStateException, RuntimeException;

    private native RecFrameInfo _getRecordAudio(ByteBuffer byteBuf) throws IllegalStateException, RuntimeException;

    private native YuvFrameInfo _getSnapData(ByteBuffer byteBuf) throws IllegalStateException, RuntimeException;

    private native StreamInfo[] _getStreamInfo() throws IllegalStateException, RuntimeException;

    private native int _selectIndex(int index)  throws IllegalStateException, RuntimeException;;

    private static native final void native_init() throws RuntimeException;

    private native final void native_setup(Object mediaplayer_this) throws IllegalStateException;

}
