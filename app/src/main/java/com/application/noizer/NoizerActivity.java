package com.application.noizer;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.view.View;
import android.widget.ToggleButton;

public class NoizerActivity extends Activity {
    private NoizerService mService;
    private boolean mBound = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_noizer);
    }

    @Override
    protected void onStart() {
        super.onStart();

        Intent intent = new Intent(this, NoizerService.class);
        bindService(intent, mConnection, Context.BIND_AUTO_CREATE);
    }

    @Override
    protected void onStop() {
        super.onStop();

        unbindService(mConnection);
        mBound = false;
    }

    public void onButtonClick(View v) {
        ToggleButton toggle = (ToggleButton) v;
        if (mBound) {
            if (toggle.isChecked()) {
                mService.play();
            } else {
                mService.stop();
            }
        }
    }


    private ServiceConnection mConnection = new ServiceConnection() {

        @Override
        public void onServiceConnected(ComponentName className,
                                       IBinder service) {
            NoizerService.LocalBinder binder = (NoizerService.LocalBinder) service;
            mService = binder.getService();
            mBound = true;
        }

        @Override
        public void onServiceDisconnected(ComponentName arg0) {
            mBound = false;
        }
    };
}
