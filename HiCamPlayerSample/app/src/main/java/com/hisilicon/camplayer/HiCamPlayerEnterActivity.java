package com.hisilicon.camplayer;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.ArrayAdapter;
import android.widget.AdapterView;

public class HiCamPlayerEnterActivity extends Activity {

    private EditText mMovieURL;
    private Spinner mPrefixURL;
    private Button mStartPlay;
    private Context mCtx;
    private final String PREFERNAME = "UrlInput";
    private final String URLNAME = "MovUrl";
    private final String INPUTURL = "inputURL";

    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mCtx = this;
        setContentView(R.layout.hicam_enter_layout);
        mMovieURL = (EditText) findViewById(R.id.input_url);

        mPrefixURL = (Spinner) findViewById(R.id.spinner_url);
        String[] mItems = {
                "udp://192.168.1.30:8090",
                "rtsp://192.168.1.19:554/livestream/12",

        };

        ArrayAdapter<String> adapter = new ArrayAdapter<String>(this,android.R.layout.simple_spinner_item, mItems);

        mPrefixURL.setAdapter(adapter);

        mPrefixURL.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view,
                                       int position, long id) {
                String str = parent.getItemAtPosition(position).toString();
                mMovieURL.setText(str);
            }
            @Override
            public void onNothingSelected(AdapterView<?> parent) {
                // TODO Auto-generated method stub
            }
        });

        mMovieURL.setText(getLastTimeUrl());
        String str=mMovieURL.getText().toString();
 
        mMovieURL.setOnEditorActionListener(new TextView.OnEditorActionListener() {

                    @Override
                    public boolean onEditorAction(TextView v, int actionId,
                            KeyEvent event) {
                        // TODO Auto-generated method stub
                        if (actionId == EditorInfo.IME_ACTION_DONE) {
                            storeUrl(v.getText().toString());
                        }
                        return false;
                    }
                });

        mStartPlay = (Button) findViewById(R.id.start_play);
        mStartPlay.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                // TODO Auto-generated method stub
                String inputUrl = mMovieURL.getText().toString();
                Log.e("ahplayer", "input url:" + inputUrl);
                if (inputUrl.length() == 0) {
                    /* do nothing */
                    String msgStr = getString(R.string.input_indicate);
                    HiCamPlayerMessageBox.show(mCtx, "Tips", msgStr);
                } else {
                    if (!isLaunched) {
                        Log.e("ahplayer", "start player !!!!!");
                        isLaunched = true;
                        storeUrl(inputUrl);
                        startPlayer(inputUrl);
                    }
                }
            }
        });
    }

    private boolean isLaunched = false;

    @Override
    protected void onResume() {
        // TODO Auto-generated method stub
        isLaunched = false;
        super.onResume();
    }

    private void storeUrl(String url) {
        SharedPreferences iprefdata = getSharedPreferences(PREFERNAME,
                MODE_PRIVATE);
        Editor iEdit = iprefdata.edit();
        iEdit.putString(URLNAME, url);
        iEdit.commit();
    }

    private String getLastTimeUrl() {
        String iUrl;
        Editor iEdit;
        SharedPreferences iprefdata;

        iprefdata = getSharedPreferences(PREFERNAME, MODE_PRIVATE);
        iEdit = iprefdata.edit();
        iUrl = iprefdata.getString(URLNAME, "noexist");
        if (iUrl.equalsIgnoreCase("noexist")) {
            iUrl = getString(R.string.input_url);
            iEdit.putString(URLNAME, iUrl);
            iEdit.commit();
        }
        return iUrl;
    }

    private void startPlayer(String filePath) {
        Intent i = new Intent(this, HiCamPlayerActivity.class);
        i.putExtra(getResources().getString(R.string.input_file), filePath);
        startActivity(i);
    }
}
