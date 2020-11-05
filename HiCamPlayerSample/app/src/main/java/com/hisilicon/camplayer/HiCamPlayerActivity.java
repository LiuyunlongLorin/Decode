package com.hisilicon.camplayer;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.ref.WeakReference;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.util.Timer;
import java.util.TimerTask;
import com.hisilicon.camplayer.HiCamPlayer.HiCamPlayerState;
import com.hisilicon.camplayer.HiCamPlayer.HiCamPlayerStateListener;
import com.hisilicon.camplayer.HiCamPlayer.RecFrameInfo;

import com.hisilicon.camplayer.HiCamPlayer.YuvFrameInfo;





import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.LinearGradient;
import android.graphics.PixelFormat;
import android.graphics.Point;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.SeekBar;
import android.widget.TextView;



public class HiCamPlayerActivity extends Activity implements
        SurfaceHolder.Callback {

    private static final String TAG = "HiCamPlayerActivity";
    private static boolean sLoaded = false;
    String mfilePath;
    private int mWindowWidth = 0;
    private int mWindowHeight = 0;
    private static final int PLAYER_ASR_CHANGE = 0x01;
    private final static int PLAYER_START_ERR = 29;
    private final static int PLAYER_PLAY_ERR = 30;
    private final static int SEEK_BAR_REFRESH = 31;
    private final static int SEEK_BAR_DISPPEAR = 32;
    private final static int SEEK_BAR_SETVISIBLE = 33;
    private final static int SHOW_LOADINGVIEW = 34;
    private final static int HIDE_LOADINGVIEW = 35;
    private final static int PLAYER_ONPAUSED = 36;
    private final static int PLAYER_ONPLAYED = 37;
    private final static int UPDATE_LOADING_PERCENT = 38;
    private final static int SEEK_BAR_DISPPEAR_TIME = 8000;// ms
    private static final int  MAX_VIDEO_FRAME_SIZE  = 1024*1024;
    private static final int  MAX_YUV_FRAME_SIZE  = 1920*1080*2;
    private static final int  MAX_AUDIO_FRAME_SIZE  = 192000;
    SurfaceView mMovieView;
    HiCamPlayer player = null;
    Context mContext;
    SeekBar mSeekBar;
    TextView curPosText;
    TextView durationText;
    LinearLayout mViewControll;
    ImageView mivPlay;
    View mdialogView = null;
    TextView mdialogText = null;
    boolean mbPaused = false;
    boolean mbLiveStreamFlag = false;
    ImageView mivRecord;
    ImageView mivSnap;
    RelativeLayout mRecViewControll;
    ByteBuffer vidReadBuf;
    ByteBuffer audReadBuf;
    int recflag = 0;
    int mAudflag = 0;
    private PlayerRecordThread mRecordThread;


    private void  startVideoRecordThread()
    {
        mRecordThread = new PlayerRecordThread();
        if (mRecordThread != null)
        {
            Log.d(TAG, "video record thread start");
            mRecordThread.start();
        }
    }
    private class PlayerRecordThread extends Thread
    {
        private volatile boolean mbRecThreadExited;

        @Override
        public void run()
        {
            FileChannel fout = null;
            FileChannel audfout = null;
            File saveRecPath = null;
            File path=Environment.getExternalStorageDirectory();
            saveRecPath = new File(path, "RECORD_DIR" );
            if( ! saveRecPath.exists() )
            {
                if( !saveRecPath.mkdir() )
                {
                    Log.d(TAG, saveRecPath.getAbsolutePath() + " mkdir() failed");
                }
            }

            File file= new File( saveRecPath,  "video.h264");
            File audfile= new File( saveRecPath,  "audio.aac");
            Log.d(TAG, saveRecPath.getAbsolutePath() + " save dir==================");
            try
            {
                fout = new FileOutputStream(file).getChannel();
                audfout = new FileOutputStream(audfile).getChannel();
                while (recflag==1 && !Thread.interrupted())
                {

                     vidReadBuf.clear();

                     try{
                         RecFrameInfo frameInfo = player.getRecordVideo(vidReadBuf);

                         if(null == frameInfo)
                         {
                             Log.d(TAG, "can't get frame  ");
                             try {
                                 Thread.sleep(30);
                             } catch (InterruptedException e) {
                                 // TODO Auto-generated catch block
                                 e.printStackTrace();
                             }
                             continue;
                         }
                         if( frameInfo.frameSize < 0 || frameInfo.pts < 0)
                         {
                             Log.d(TAG, "error  frameInfo.frameSize < 0 || frameInfo.pts < 0  ");
                             break;
                         }
                         vidReadBuf.flip();
                         vidReadBuf.limit(frameInfo.frameSize);
                         Log.d(TAG, "getRecordVideo size:"+frameInfo.frameSize+" pts: "+frameInfo.pts+" pt: "+frameInfo.payload);
                         fout.write(vidReadBuf);
                         vidReadBuf.clear();

                         if(mAudflag==1)
                         {
                             RecFrameInfo audframeInfo = player.getRecordAudio(audReadBuf);

                             if(null==audframeInfo)
                             {
                                 Log.d(TAG, "can't get frame  ");
                                 try {
                                     Thread.sleep(30);
                                 } catch (InterruptedException e) {
                                     // TODO Auto-generated catch block
                                     e.printStackTrace();
                                 }
                                 continue;
                             }
                             if(audframeInfo.frameSize < 0 || audframeInfo.pts < 0)
                             {
                                 Log.d(TAG, "error audframeInfo.frameSize < 0  ||  audframeInfo.pts < 0   ");
                                 break;
                             }
                             audReadBuf.flip();
                             audReadBuf.limit(audframeInfo.frameSize);
                             Log.d(TAG, "getRecordAudio size:"+audframeInfo.frameSize+" pts: "+audframeInfo.pts+" pt: "+audframeInfo.payload);
                             audfout.write(audReadBuf);
                             audReadBuf.clear();
                         }

                     }catch(Exception e)
                     {
                        e.printStackTrace();
                        Log.e(TAG, "fail to write video data");
                        break;
                    }



                }
            }
            catch (FileNotFoundException e)
            {
                Log.e(TAG, "fail to open record video file!");
            } finally {
              try {

                if(fout != null) {
                    fout.close();
                }

              } catch(IOException e) {
                  Log.e(TAG, "fail to close record video file!");
              }
            }

        }

    };
    public void stopRecord()
    {
        if(recflag==1)
        {

            recflag = 0;
            if (null != mRecordThread)
            {
                joinThread(mRecordThread);
                mRecordThread =null;
            }

        }
        Log.d(TAG, "stoprecord() end");
    }

    private void joinThread(Thread thread) {
        // TODO Auto-generated method stub
        if (null != thread && thread.isAlive())
        {
            try
            {
                thread.join();
            }
            catch (InterruptedException e)
            {
                Log.e(TAG, "fail to join");
            }
        }
    }

    protected void onCreate(Bundle savedInstanceState) {
        Log.d(TAG, "onCreate()");
        super.onCreate(savedInstanceState);
        mContext = this;
        Intent i = getIntent();
        mfilePath = i.getStringExtra(getResources().getString(
                R.string.input_file));
        getWindow().setSoftInputMode(
                WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().setFormat(PixelFormat.RGB_565);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON,
                WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        Point winSize = new Point();
        getWindowManager().getDefaultDisplay().getSize(winSize);
        mWindowWidth = winSize.x;
        mWindowHeight = winSize.y;
        loadLibs();
        setContentView(R.layout.hicamplayer_layout);
        mSeekBar = (SeekBar) findViewById(R.id.sbProgress);
        curPosText = (TextView) findViewById(R.id.curPostion);
        durationText = (TextView) findViewById(R.id.duration);
        mMovieView = (SurfaceView) findViewById(R.id.playView);
        mViewControll = (LinearLayout) findViewById(R.id.viewControll);
        mivPlay = (ImageView) findViewById(R.id.ivPlay);
        mMovieView.getHolder().addCallback(this);
        mSeekBar.setOnSeekBarChangeListener(seekListener);
        hideSeekBar();
        mdialogView = ((LayoutInflater) getSystemService(LAYOUT_INFLATER_SERVICE))
                .inflate(R.layout.loading_dialog, null);
        mdialogText = (TextView) mdialogView.findViewById(R.id.txtDialog);
        mivPlay.setOnClickListener(playClickListener);

        mRecViewControll = (RelativeLayout) findViewById(R.id.layoutRecControll);
        mivRecord = (ImageView) findViewById(R.id.ivRecord);
        mivSnap = (ImageView) findViewById(R.id.ivSnap);
        mivRecord.setOnClickListener(recordClickListener);
        mivSnap.setOnClickListener(snapClickListener);

        if (mfilePath.contains("action=play")||mfilePath.contains("rtsp")||mfilePath.contains("udp")) {
            mbLiveStreamFlag = true;
            mViewControll.setVisibility(View.GONE);
        }
        else
        {
            mRecViewControll.setVisibility(View.GONE);
        }

        if(mfilePath.contains("action=play")&&mfilePath.contains("audio"))
        {
            mAudflag=1;
        }
        vidReadBuf = ByteBuffer.allocateDirect(MAX_VIDEO_FRAME_SIZE);
        vidReadBuf.clear();
        audReadBuf = ByteBuffer.allocateDirect(MAX_AUDIO_FRAME_SIZE);
        audReadBuf.clear();
        Log.e(TAG, "------------------Lorin Add  Create  Success------------------------");
    }

    protected void onPause() {
        Log.d(TAG, "onPause()");
        finish();
        super.onPause();
    }

    protected void onResume() {
        Log.d(TAG, "onResume()");
        super.onResume();
    }

    protected void onDestroy() {
        Log.d(TAG, "onDestroy()");
        mPlayerHandler.removeMessages(SEEK_BAR_DISPPEAR);
        super.onDestroy();
        Log.d(TAG, "onDestroy()  finish");
    }

    @Override
    protected void onStop() {
        // TODO Auto-generated method stub
        if (null != mRecordThread )
        {
            recflag=0;
            stopRecord();
        }

        Log.d(TAG, "onStop");
        super.onStop();
    }

    View.OnClickListener playClickListener = new View.OnClickListener() {

        @Override
        public void onClick(View v) {
            // TODO Auto-generated method stub
            if (player != null) {
                if (!mbPaused)
                    player.pause();
                else
                    player.start();
            }
        }
    };

    View.OnClickListener snapClickListener = new View.OnClickListener() {

        @Override
        public void onClick(View v) {
            // TODO Auto-generated method stub
            if (player != null) {
                FileChannel fout = null;
                File path=Environment.getExternalStorageDirectory();
                File file= new File( path,  "video.yuv");
                try {
                    fout = new FileOutputStream(file).getChannel();
                } catch (FileNotFoundException e) {
                    // TODO Auto-generated catch block
                    e.printStackTrace();
                }
                ByteBuffer yuvbuffer = ByteBuffer.allocateDirect(MAX_YUV_FRAME_SIZE);
                yuvbuffer.clear();
                YuvFrameInfo yuvInfo = player.getSnapData(yuvbuffer);

                if(null==yuvInfo )
                {
                    Log.e(TAG, " player.getSnapData(yuvbuffer) get no data==================");
                }
                else
                {
                    Log.d(TAG, "getSnapData width:"+yuvInfo.width+" height: "+yuvInfo.height+" pts: "+yuvInfo.pts+" uoffset: "+yuvInfo.uoffset
                        +" voffset: "+yuvInfo.voffset+" ypitch: "+yuvInfo.ypitch+" upitch: "+yuvInfo.upitch+" vpitch: "+yuvInfo.vpitch);
                    yuvbuffer.flip();
                    int limit=yuvInfo.height*yuvInfo.width*3/2;
                    yuvbuffer.limit(limit);
                    try {
                        fout.write(yuvbuffer);
                    } catch (IOException e) {
                        // TODO Auto-generated catch block
                        e.printStackTrace();
                    }
                    yuvbuffer.clear();
                }

            }
        }
    };

    View.OnClickListener recordClickListener = new View.OnClickListener() {

        @Override
        public void onClick(View v) {
            // TODO Auto-generated method stub
            if (player != null) {
                if(0 == recflag)
                {
                    mivRecord.setImageResource(R.drawable.selector_button_stop);
                    recflag=1;
                    player.setRecordFlag(recflag);
                    if (null == mRecordThread )
                    {
                        startVideoRecordThread();
                    }
                }
                else
                {
                    mivRecord.setImageResource(R.drawable.selector_button_record);
                    if (null != mRecordThread )
                    {
                        stopRecord();
                    }
                    recflag=0;
                    player.setRecordFlag(recflag);
                }
            }
        }
    };



    void hideSeekBar() {
        mPlayerHandler.sendEmptyMessageDelayed(SEEK_BAR_DISPPEAR,
                SEEK_BAR_DISPPEAR_TIME);
    }

    SeekBar.OnSeekBarChangeListener seekListener = new SeekBar.OnSeekBarChangeListener() {

        @Override
        public void onProgressChanged(SeekBar seekBar, int progress,
                boolean fromUser) {
            // TODO Auto-generated method stub
        }

        @Override
        public void onStartTrackingTouch(SeekBar seekBar) {
            // TODO Auto-generated method stub
            mPlayerHandler.removeMessages(SEEK_BAR_DISPPEAR);
            mPlayerHandler.removeMessages(SEEK_BAR_REFRESH);

        }

        @Override
        public void onStopTrackingTouch(SeekBar seekBar) {
            // TODO Auto-generated method stub
            int seekpos = seekBar.getProgress();
            if (player != null)
            {
                Log.i(TAG, "onStopTrackingTouch: seekTo");
                player.seekTo(seekpos);
                refreshSeekBar();
            }
            hideSeekBar();
        }
    };

    public boolean onTouchEvent(android.view.MotionEvent event) {
        if (event.getAction() == MotionEvent.ACTION_UP && !mbLiveStreamFlag) {
            if (mViewControll.getVisibility() == View.GONE
                    || mViewControll.getVisibility() == View.INVISIBLE) {
                mPlayerHandler.sendEmptyMessage(SEEK_BAR_SETVISIBLE);
            }
            mPlayerHandler.removeMessages(SEEK_BAR_DISPPEAR);
            if (mViewControll.getVisibility() == View.VISIBLE)
                hideSeekBar();
        }

        return false;
    };

    /**
     * loads all native libraries
     *
     * @return true if all libraries was loaded, otherwise return false
     */
    private static boolean loadLibs() {
        if (sLoaded) {
            return true;
        }
        boolean err = false;
        int version = android.os.Build.VERSION.SDK_INT;
        //version = 13;
        String model = String.valueOf(android.os.Build.MODEL);
        Log.d(TAG, "phone_type:" + model);

        System.loadLibrary("ffmpeg");

        if (version < 21) {
            Log.e(TAG, "load libhi_camplayer_ffmpeg.so");
            System.loadLibrary("hi_camplayer_ffmpeg");
        } else {
            Log.e(TAG, "load libhi_camplayer_mediacodec.so");
            System.loadLibrary("hi_camplayer_mediacodec");
        }


        if (!err) {
            sLoaded = true;
        }
        return sLoaded;
    }

    public void updateSurfaceView() {
        Log.e(TAG, "update_surface_view");
        if(player != null)
        {
            int width = player.getVideoWidth();
            int height = player.getVideoHeight();
            if (width != 0 && height != 0) {
                Log.e(TAG, "player video width : " + width + "height:" + height);
                mMovieView.getHolder().setFixedSize(width / 16 * 16, height);

                LayoutParams lp = mMovieView.getLayoutParams();

                lp.width = mWindowWidth;
                lp.height = mWindowHeight;

                mMovieView.setLayoutParams(lp);
                mMovieView.invalidate();
            }
        }
    }




    HiCamPlayer.onSeekBufferingStateListener mSeekListener = new HiCamPlayer.onSeekBufferingStateListener() {

        @Override
        public void onSeekBufferingStart(HiCamPlayer mplayer) {
            // TODO Auto-generated method stub
            mPlayerHandler.sendEmptyMessage(SHOW_LOADINGVIEW);
        }

        @Override
        public void onSeekBufferingLoadingPercent(HiCamPlayer mplayer, int percent) {
            // TODO Auto-generated method stub
            Message msg = new Message();
            msg.arg1 = percent;
            msg.what = UPDATE_LOADING_PERCENT;
            mPlayerHandler.sendMessage(msg);
        }

        @Override
        public void onSeekBufferingEnd(HiCamPlayer mplayer) {
            // TODO Auto-generated method stub
            Log.i(TAG, "onBufferingEnded");
            mPlayerHandler.sendEmptyMessage(HIDE_LOADINGVIEW);
        }
    };


    HiCamPlayer.HiCamPlayerStateListener mPlayerListener = new HiCamPlayer.HiCamPlayerStateListener() {

        @Override
        public void onStateChange(HiCamPlayer player, HiCamPlayerState state) {
            // TODO Auto-generated method stub
            Log.i(TAG, "onStateChange curstate: " + state);
            if (state == HiCamPlayerState.HICAMPLAYER_STATE_PAUSE  ) {
                mPlayerHandler.sendEmptyMessage(PLAYER_ONPAUSED);
                mbPaused = true;
            } else if (state == HiCamPlayerState.HICAMPLAYER_STATE_PLAY) {
                mPlayerHandler.sendEmptyMessage(PLAYER_ONPLAYED);
                mbPaused = false;
            }
        }

        @Override
        public void onFinish(HiCamPlayer player) {
            // TODO Auto-generated method stub
            Log.i(TAG, "onFinish");
            mPlayerHandler.sendEmptyMessage(SEEK_BAR_SETVISIBLE);

        //onStateChange(player,HiCamPlayerState.HICAMPLAYER_STATE_PAUSE);
        }

        @Override
        public void onError(HiCamPlayer player, String msg, int extra) {
            // TODO Auto-generated method stub
            Message m = new Message();
            m.what = PLAYER_PLAY_ERR;
            Bundle data = new Bundle();
            data.putCharSequence("MSG", msg);
            m.setData(data);
            mPlayerHandler.sendMessage(m);
        }

        @Override
        public void onBufferingUpdate(HiCamPlayer player, int percent) {
            // TODO Auto-generated method stub

        }

        @Override
        public void onASRChange(HiCamPlayer player) {
            // TODO Auto-generated method stub
            mPlayerHandler.sendEmptyMessage(PLAYER_ASR_CHANGE);
        }
    };

    Handler mPlayerHandler = new Handler() {
        public void handleMessage(Message msg) {
            switch (msg.what) {
            case PLAYER_ASR_CHANGE:
                Log.d(TAG, "changeASR ");
                updateSurfaceView();
                break;

            case PLAYER_START_ERR:
                String tips = "create player unsuccessfully, please try again!";
                HiCamPlayerMessageBox.show(mContext, "", tips,
                        new DialogInterface.OnClickListener() {

                            @Override
                            public void onClick(DialogInterface dialog,
                                    int which) {
                                // TODO Auto-generated method stub
                                finish();
                            }
                        });
                break;

            case PLAYER_PLAY_ERR:
                if (null != mRecordThread )
                {
                    stopRecord();
                }
                Bundle data = msg.getData();
                String Errstr = data.getCharSequence("MSG").toString();
                HiCamPlayerMessageBox.show(mContext, "", Errstr,
                        new DialogInterface.OnClickListener() {

                            @Override
                            public void onClick(DialogInterface dialog,
                                    int which) {
                                // TODO Auto-generated method stub
                                finish();
                            }
                        });
                break;

            case SEEK_BAR_REFRESH:
                Log.d(TAG, "SEEK_BAR_REFRESH ");
                refreshSeekBar();
                break;

            case SEEK_BAR_DISPPEAR:
                Log.i(TAG, "set seekbar invisible");
                mViewControll.setVisibility(View.INVISIBLE);
                break;

            case SEEK_BAR_SETVISIBLE:
                Log.i(TAG, "set seekbar visible");
                mViewControll.setVisibility(View.VISIBLE);
                break;

            case SHOW_LOADINGVIEW:
                showLoadingDialog(true);
                break;

            case HIDE_LOADINGVIEW:
                showLoadingDialog(false);
                break;

            case PLAYER_ONPAUSED:
                mivPlay.setImageResource(R.drawable.selector_button_play);
                break;

            case PLAYER_ONPLAYED:
                Log.i(TAG, "handleMessage: PLAYER_ONPLAYED");
                mivPlay.setImageResource(R.drawable.selector_button_pause);
//                refreshSeekBar();
                break;

            case UPDATE_LOADING_PERCENT:
                mdialogText.setText("load " + msg.arg1 + "%");
                break;

            default:
                break;
            }
            super.handleMessage(msg);
        }
    };

    private String seconds2String(int time) {
        int hour, min, second;

        hour = time / 3600;
        time = time % 3600;
        min = time / 60;
        time = time % 60;
        second = time;

        return String.format("%02d:%02d:%02d", hour, min, second);
    }

    private void refreshSeekBar() {
        Log.i(TAG, "refresh seekbar");
        int curPostion = 0;
        if (player != null)
            curPostion = player.getCurrentPosition();
        mSeekBar.setProgress(curPostion);
        curPosText.setText(seconds2String(curPostion / 1000));
        mPlayerHandler.sendEmptyMessageDelayed(SEEK_BAR_REFRESH, 500);
    }

    private void handlePlayerStartErr() {
        mPlayerHandler.sendEmptyMessage(PLAYER_START_ERR);
    }

    Dialog mLoadingDialog = null;

    private void showLoadingDialog(boolean show) {
        if (mLoadingDialog == null) {
            mLoadingDialog = new Dialog(this, R.style.ott_process_dialog);
            mLoadingDialog.setContentView(mdialogView);

            mLoadingDialog
                    .setOnCancelListener(new DialogInterface.OnCancelListener() {
                        public void onCancel(DialogInterface dialog) {
                            finish();
                        }
                    });
        }

        if (show) {
            if (!mLoadingDialog.isShowing()) {
                mLoadingDialog.setCanceledOnTouchOutside(false);
                mdialogText.setText(R.string.waiting);
                mLoadingDialog.show();
                Log.i(TAG, "loading dialog show");
            }
        } else {
            if (mLoadingDialog.isShowing()) {
                mLoadingDialog.dismiss();
                Log.i(TAG, "loading dialog dismiss");
            }
        }
    }

    @Override
    public void onBackPressed() {
        // TODO Auto-generated method stub
        if (mLoadingDialog != null && mLoadingDialog.isShowing()) {
            mLoadingDialog.dismiss();
        }
        super.onBackPressed();
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width,
            int height) {
        // TODO Auto-generated method stub
        Log.i(TAG, "surfaceChanged formate: " + format + " w: " + width
                + " h: " + height);
    }

    public void printTrackInfo(){
        HiCamPlayer.StreamInfo[] info = player.getStreamInfo();
        if(info!=null)
        {
            for(int i=0; i<info.length; i++)
            {
                if(info[i].trackType == HiCamPlayer.MEDIA_STREAM_TYPE_VIDEO)
                {
                    Log.i(TAG, "track: "+i+" video widht:"+ info[i].width+" height: "+info[i].height + " mime:"+info[i].mime);
                }
                else if(info[i].trackType == HiCamPlayer.MEDIA_STREAM_TYPE_AUDIO)
                {
                    Log.i(TAG, "track: "+i+" audio chn:"+ info[i].audiochn+" sampleRate: "+info[i].sampleRate + " mime:"+info[i].mime);
                }
                else if(info[i].trackType == HiCamPlayer.MEDIA_STREAM_TYPE_METADATA)
                {
                    Log.i(TAG, "track: "+i+" type:data");
                }
                else
                {
                    Log.i(TAG, "track: "+i+" type:unknown");
                }
            }
        }
    }

    @Override
    public Object getLastNonConfigurationInstance() {
        return super.getLastNonConfigurationInstance();
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        // TODO Auto-generated method stub
        Log.i(TAG, "surfaceCreated");
        mMovieView.getHolder().setFormat(PixelFormat.RGB_565);

        try {
            player = new HiCamPlayer();
        } catch (Exception e) {
            // TODO: handle exception
            e.printStackTrace();
        }
        Log.i(TAG, " New HiCamPlayer success");
        mViewControll.setEnabled(false);
        try {
            player.setDisplay(holder);
            player.setHiCamPlayerListener(mPlayerListener);
            Log.e(TAG, "----------------------setHiCamPlayerListener-------------------------------");
            player.setOnSeekBufferingStateListener(mSeekListener);
            player.setDataSource(mfilePath);
            Log.e(TAG, "----------------------setDataSource-------------------------------");
//            printTrackInfo();
            Log.e(TAG, "----------------------printTrackInfo-------------------------------");
//            player.selectIndex(2);
            Log.e(TAG, "----------------------selectIndex-------------------------------");
            player.prepare();
            player.start();
            Log.e(TAG, "--------------------Lorin Add  Palyer Start Success---------------------------------");
        } catch (IOException e) {
            // TODO: handle exception
            e.printStackTrace();
            handlePlayerStartErr();
        } catch (IllegalStateException e) {
            // TODO: handle exception
            e.printStackTrace();
            handlePlayerStartErr();
        } catch (IllegalArgumentException e) {
            // TODO: handle exception
            e.printStackTrace();
            handlePlayerStartErr();
        }
        ;
        if (!mbLiveStreamFlag) {
            Log.i(TAG, "debug    mbLiveStreamFlag ");
            mViewControll.setEnabled(true);
            int duration = player.getDuration();
            mSeekBar.setMax(duration);
            durationText.setText(seconds2String(duration / 1000));
            refreshSeekBar();
        }
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        // TODO Auto-generated method stub
        Log.i(TAG, "surfaceDestroyed");
        mPlayerHandler.removeMessages(SEEK_BAR_REFRESH);
        Log.i(TAG, "before reset");
        player.reset();
        Log.i(TAG, "before release");
        player.release();
        player = null;
    }

}
