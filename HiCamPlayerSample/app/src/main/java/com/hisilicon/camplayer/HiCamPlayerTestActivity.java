package com.hisilicon.camplayer;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Timer;
import java.util.TimerTask;
import com.hisilicon.camplayer.HiCamPlayer.HiCamPlayerState;
import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.LinearGradient;
import android.graphics.PixelFormat;
import android.graphics.Point;
import android.os.Bundle;
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
import android.widget.SeekBar;
import android.widget.TextView;

public class HiCamPlayerTestActivity extends Activity implements
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
        if (mfilePath.contains("action=play")) {
            mbLiveStreamFlag = true;
            mViewControll.setVisibility(View.GONE);
        }
    }

    protected void onPause() {
        Log.d(TAG, "onPause()");
        super.onPause();
    }

    protected void onResume() {
        Log.d(TAG, "onResume()");
        super.onResume();
    }

    protected void onDestroy() {
        Log.d(TAG, "onDestroy()");
        super.onDestroy();
    }

    @Override
    protected void onStop() {
        // TODO Auto-generated method stub
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
        }

        @Override
        public void onStopTrackingTouch(SeekBar seekBar) {
            // TODO Auto-generated method stub
            int seekpos = seekBar.getProgress();
            if (player != null)
                player.seekTo(seekpos);
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
        String model = String.valueOf(android.os.Build.MODEL);
        Log.d(TAG, "phone_type:" + model);

        System.loadLibrary("ffmpeg");

        if (version < 14) {
            Log.e(TAG, "load libhi_camplayer_ffmpeg.so");
            System.loadLibrary("hi_camplayer_ffmpeg");
        } else if (version < 19) {
            Log.e(TAG, "load libhi_camplayer_ics.so");
            System.loadLibrary("hi_camplayer_ics");
        } else if (version < 21) {
            Log.e(TAG, "load libhi_camplayer_kitkat.so");
            System.loadLibrary("hi_camplayer_kitkat");
        } else {
            Log.e(TAG, "load libhi_camplayer_lollipop.so");
            System.loadLibrary("hi_camplayer_lollipop");
        }

        if (!err) {
            sLoaded = true;
        }
        return sLoaded;
    }

    public void updateSurfaceView() {
        Log.e(TAG, "update_surface_view");
        if (player != null) {
            return;
        }
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
            if (state == HiCamPlayerState.HICAMPLAYER_STATE_PAUSE) {
                mPlayerHandler.sendEmptyMessage(PLAYER_ONPAUSED);
                mbPaused = true;
            } else if (state == HiCamPlayerState.HICAMPLAYER_STATE_PLAY) {
                mPlayerHandler.sendEmptyMessage(PLAYER_ONPLAYED);
                mbPaused = false;
                mbStartNotified = true;
            }
        }

        @Override
        public void onFinish(HiCamPlayer player) {
            // TODO Auto-generated method stub
            Log.i(TAG, "onFinish");
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
                mivPlay.setImageResource(R.drawable.selector_button_pause);
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
        mPlayerHandler.sendEmptyMessageDelayed(SEEK_BAR_REFRESH, 1000);
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

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        // TODO Auto-generated method stub
        Log.i(TAG, "surfaceCreated");
        mStartPlay.start();
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        // TODO Auto-generated method stub
        Log.i(TAG, "surfaceDestroyed");
        mPlayerHandler.removeMessages(SEEK_BAR_REFRESH);
        try {
            player.reset();
            player.release();
            player = null;
        } catch (Exception e) {
            // TODO: handle exception
            e.printStackTrace();
        }

    }

    Thread mStartPlay = new Thread(new Runnable() {

        @Override
        public void run() {
            // TODO Auto-generated method stub
            testCase10();
            testCase11();
            testCase12();
            testCase13();
            testCase14();
            testCase15();
            testCase16();
            testCase1();
            testCase2();
            testCase3();

            testCase5();
            testCase6();
        testCase7();
        }
    });
    boolean mbStartNotified = false;

    void testCase10() {
        // setlistener interface test
        boolean bExceptionFlag = false;
        mbStartNotified = false;
        player = new HiCamPlayer();
        try {
            player.setHiCamPlayerListener(mPlayerListener);
            player.setOnSeekBufferingStateListener(mSeekListener);
            player.setDisplay(mMovieView.getHolder());
            player.setDataSource(mfilePath);
            player.prepare();
            player.start();

        } catch (Exception e) {
            // TODO: handle exception
            e.printStackTrace();
            bExceptionFlag = true;
        }
        ;
        try {
            Thread.sleep(1000);
        } catch (InterruptedException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
        if (bExceptionFlag || !mbStartNotified)
            Log.e(TAG, "testCase 10 failed");
        player.reset();
        player.release();
        Log.i(TAG, "testCase 10");
    }

    void testCase11() {
        // loop times 1000 for steablity
        boolean bExceptionFlag = false;
        for (int i = 0; i < 300; i++) {
            player = new HiCamPlayer();
            try {
                player.setHiCamPlayerListener(mPlayerListener);
                player.setDisplay(mMovieView.getHolder());
                player.setDataSource(mfilePath);
                player.prepare();
                player.start();
                player.stop();
                player.reset();
                player.release();
            } catch (Exception e) {
                // TODO: handle exception
                e.printStackTrace();
                bExceptionFlag = true;
                break;
            }
            ;
        }

        if (bExceptionFlag)
            Log.e(TAG, "testCase 11 failed");
        try {
            player.reset();
            player.release();
        } catch (Exception e) {
            // TODO: handle exception
            e.printStackTrace();
        }

        Log.i(TAG, "testCase 11");
    }

    void testCase12() {
        // pause test
        boolean bExceptionFlag = false;
        player = new HiCamPlayer();
        try {
            player.setHiCamPlayerListener(mPlayerListener);
            player.setDisplay(mMovieView.getHolder());
            player.setDataSource(mfilePath);
            player.prepare();
            player.start();

        } catch (Exception e) {
            // TODO: handle exception
            e.printStackTrace();
            bExceptionFlag = true;
        }
        ;

        try {
            player.pause();
            Thread.sleep(10000);
            player.start();
            Thread.sleep(10000);
        } catch (InterruptedException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
        if (bExceptionFlag)
            Log.e(TAG, "testCase 12 failed");
        player.reset();
        player.release();
        Log.i(TAG, "testCase 12");
    }

    void testCase13() {
        // pause test
        boolean bExceptionFlag = false;
        boolean bisplaying = false;
        player = new HiCamPlayer();
        try {
            player.setHiCamPlayerListener(mPlayerListener);
            player.setDisplay(mMovieView.getHolder());
            player.setDataSource(mfilePath);
            player.prepare();
            bisplaying = player.isPlaying();
            if (bisplaying)
                Log.e(TAG, "player should not be in playing after prepare");
            player.start();
            bisplaying = player.isPlaying();
            if (!bisplaying)
                Log.e(TAG, "player should be in playing after start");
            player.pause();
            bisplaying = player.isPlaying();
            if (bisplaying)
                Log.e(TAG, "player should not be in playing after pause");
        } catch (Exception e) {
            // TODO: handle exception
            e.printStackTrace();
            bExceptionFlag = true;
        }
        ;

        if (bExceptionFlag)
            Log.e(TAG, "testCase 13 failed");
        player.reset();
        player.release();
        Log.i(TAG, "testCase 13");
    }

    void testCase14() {
        // getVideoWidth test
        boolean bExceptionFlag = false;
        int width = 0;
        player = new HiCamPlayer();
        try {
            player.setHiCamPlayerListener(mPlayerListener);
            player.setDisplay(mMovieView.getHolder());
            width = player.getVideoWidth();
            if (width > 0)
                Log.e(TAG, "player video width should be 0 before prepare");
            player.setDataSource(mfilePath);
            player.prepare();
            width = player.getVideoWidth();
            if (width == 0)
                Log.e(TAG, "player video width should not be 0 after prepare");
            player.start();
        } catch (Exception e) {
            // TODO: handle exception
            e.printStackTrace();
            bExceptionFlag = true;
        }
        ;

        if (bExceptionFlag)
            Log.e(TAG, "testCase 14 failed");
        player.reset();
        player.release();
        Log.i(TAG, "testCase 14");
    }

    void testCase15() {
        // getVideoHeight test
        boolean bExceptionFlag = false;
        int height = 0;
        player = new HiCamPlayer();
        try {
            player.setHiCamPlayerListener(mPlayerListener);
            player.setDisplay(mMovieView.getHolder());
            height = player.getVideoHeight();
            if (height > 0)
                Log.e(TAG, "player video height should be 0 before prepare");
            player.setDataSource(mfilePath);
            player.prepare();
            height = player.getVideoHeight();
            if (height == 0)
                Log.e(TAG, "player video height should not be 0 after prepare");
            player.start();
        } catch (Exception e) {
            // TODO: handle exception
            e.printStackTrace();
            bExceptionFlag = true;
        }
        ;

        if (bExceptionFlag)
            Log.e(TAG, "testCase 15 failed");
        player.reset();
        player.release();
        Log.i(TAG, "testCase 15");
    }

    void testCase16() {
        // seekTo test
        boolean bExceptionFlag = false;
        player = new HiCamPlayer();
        try {
            player.setHiCamPlayerListener(mPlayerListener);
            player.setDisplay(mMovieView.getHolder());
            player.setDataSource(mfilePath);
            player.prepare();
            int duration = player.getDuration();
            player.seekTo(duration / 2);
            player.start();

        } catch (Exception e) {
            // TODO: handle exception
            e.printStackTrace();
            bExceptionFlag = true;
        }
        ;
        try {
            Thread.sleep(10000);
        } catch (InterruptedException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
        if (bExceptionFlag)
            Log.e(TAG, "testCase 16 failed");
        player.reset();
        player.release();
        Log.i(TAG, "testCase 16");
    }

    void testCase1() {
        // do not set surface before prepare, should throw excepetion
        boolean bExceptionFlag = false;
        player = new HiCamPlayer();
        try {

            player.setDataSource(mfilePath);
            player.prepare();
            player.setDisplay(mMovieView.getHolder());
        } catch (Exception e) {
            // TODO: handle exception
            e.printStackTrace();
            bExceptionFlag = true;
        }
        ;
        if (!bExceptionFlag)
            Log.e(TAG, "testCase 1 failed");
        player.reset();
        player.release();
        Log.i(TAG, "testCase 1");
    }

    void testCase2() {
        // loop times 1000 for steablity
        boolean bExceptionFlag = false;
        player = new HiCamPlayer();

        try {
            player.setHiCamPlayerListener(mPlayerListener);
            player.setDisplay(mMovieView.getHolder());


             for (int i = 0; i < 200; i++) {
                 player.setDataSource(mfilePath);
                 player.prepare();
                 player.start();
                 player.stop();
                 player.reset();
             }


            Log.i(TAG, "testCase2 111");

            player.setDataSource(mfilePath);
            player.prepare();
            for (int i = 0; i < 500; i++) {
                player.start();
                player.stop();
                Log.i(TAG, "testCase2 loop2 times " + i);
            }

        } catch (Exception e) {
            // TODO: handle exception
            e.printStackTrace();
            bExceptionFlag = true;
        }
        ;
        if (bExceptionFlag)
            Log.e(TAG, "testCase 2 failed");
        player.reset();
        player.release();
        Log.i(TAG, "testCase 2");
    }

    void testCase3() {
        // do not setdatasource
        boolean bExceptionFlag = false;
        player = new HiCamPlayer();
        try {
            player.setDisplay(mMovieView.getHolder());
            player.prepare();
            player.start();

        } catch (Exception e) {
            // TODO: handle exception
            e.printStackTrace();
            bExceptionFlag = true;
        }
        ;
        if (!bExceptionFlag)
            Log.e(TAG, "testCase 3 failed");
        player.reset();
        player.release();
        Log.i(TAG, "testCase 3");
    }

    int cont = 0;

    void testCase4() {
        // multi thread
        boolean bExceptionFlag = false;
        player = new HiCamPlayer();
        player.setHiCamPlayerListener(mPlayerListener);
        cont = 0;
        player.setDisplay(mMovieView.getHolder());

        for (int i = 0; i < 300; i++) {
            new Thread(new Runnable() {

                @Override
                public void run() {
                    // TODO Auto-generated method stub
                    try {

                        if (cont % 3 == 0) {
                            player.setDataSource(mfilePath);
                            player.prepare();
                        } else if (cont % 3 == 1) {
                            player.setDataSource(mfilePath);
                            player.prepare();
                            player.start();
                        }
                    } catch (Exception e) {
                        // TODO: handle exception
                        e.printStackTrace();
                    }
                    ;

                }
            }).start();

            new Thread(new Runnable() {

                @Override
                public void run() {
                    // TODO Auto-generated method stub
                    try {
                        Thread.sleep(cont * 5);
                    } catch (InterruptedException e) {
                        // TODO Auto-generated catch block
                        e.printStackTrace();
                    }
                    player.stop();
                    player.reset();
                }
            }).start();

            cont++;
            try {
                Thread.sleep(1000);
            } catch (InterruptedException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            }
        }

        if (bExceptionFlag)
            Log.e(TAG, "testCase 4 failed");

        player.release();
        Log.i(TAG, "testCase 4");
    }

    void testCase5() {
        // do not setdatasource
        boolean bExceptionFlag = false;
        player = new HiCamPlayer();
        player.setHiCamPlayerListener(mPlayerListener);
        try {
            player.setDisplay(mMovieView.getHolder());
            player.prepare();
            player.start();

        } catch (Exception e) {
            // TODO: handle exception
            e.printStackTrace();
            bExceptionFlag = true;
        }
        ;
        if (!bExceptionFlag)
            Log.e(TAG, "testCase 5 failed");

        player.stop();
        player.reset();
        player.release();
        Log.i(TAG, "testCase 5");
    }

    void testCase6() {
        // do not setdatasource
        boolean bExceptionFlag = false;
        player = new HiCamPlayer();
        player.setHiCamPlayerListener(mPlayerListener);
        try {
            player.setDisplay(mMovieView.getHolder());
            player.setDataSource(mfilePath);
            player.prepare();
            player.start();

        } catch (Exception e) {
            // TODO: handle exception
            e.printStackTrace();
            bExceptionFlag = true;
        }
        ;
        if (bExceptionFlag)
            Log.e(TAG, "testCase 6 failed");

        try {
            Thread.sleep(20000);
        } catch (InterruptedException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
        player.stop();
        player.reset();
        player.release();
        Log.i(TAG, "testCase 6");
    }

    long cacluAverage(long [] longArray){
        long sum = 0;
        for(int i=0; i<longArray.length; i++)
        {
            sum += longArray[i];
        }
        long average = sum/longArray.length;
        return average;
    }

    void printTimeConsume(long [] longArray, String interfaceName)
    {
        long Ava = cacluAverage(longArray);
        Log.d(TAG, interfaceName+"  lastTime average: "+Ava+" Us "+Ava/1000+" ms");
    }

    void testCase7() {
     // loop times 1000 for steablity
        boolean bExceptionFlag = false;
        int testTimes = 20;
        long beginTime = 0;
        long endTime = 0;
        long divDeltaUs = 1000;
        long[] dataSourceTimeArray = new long [testTimes];
        long[] prepareTimeArray = new long [testTimes];
        long[] startTimeArray = new long [testTimes];
        long[] stopTimeArray = new long [testTimes];
        long[] resetTimeArray = new long [testTimes];
        for (int i = 0; i < testTimes; i++) {
            player = new HiCamPlayer();
            try {
                player.setHiCamPlayerListener(mPlayerListener);
                player.setDisplay(mMovieView.getHolder());
                beginTime = System.nanoTime();
                player.setDataSource(mfilePath);
                endTime = System.nanoTime();
                dataSourceTimeArray[i] = (endTime - beginTime)/divDeltaUs;

                beginTime = System.nanoTime();
                player.prepare();
                endTime = System.nanoTime();
                prepareTimeArray[i] = (endTime - beginTime)/divDeltaUs;

                beginTime = System.nanoTime();
                player.start();
                endTime = System.nanoTime();
                startTimeArray[i] = (endTime - beginTime)/divDeltaUs;

                beginTime = System.nanoTime();
                player.stop();
                endTime = System.nanoTime();
                stopTimeArray[i] = (endTime - beginTime)/divDeltaUs;

                beginTime = System.nanoTime();
                player.reset();
                endTime = System.nanoTime();
                resetTimeArray[i] = (endTime - beginTime)/divDeltaUs;

                player.release();
            } catch (Exception e) {
                // TODO: handle exception
                e.printStackTrace();
                bExceptionFlag = true;
                break;
            };
            try {
                Thread.sleep(3000);
            } catch (InterruptedException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            }
        }
        printTimeConsume(dataSourceTimeArray, "setDataSource");
        printTimeConsume(prepareTimeArray, "prepare");
        printTimeConsume(startTimeArray, "start");
        printTimeConsume(stopTimeArray, "stop");
        printTimeConsume(resetTimeArray, "reset");

        if (bExceptionFlag)
            Log.e(TAG, "testCase 7 failed");
        try {
            player.reset();
            player.release();
        } catch (Exception e) {
            // TODO: handle exception
            e.printStackTrace();
        }

        Log.i(TAG, "testCase 7");
    }
}
