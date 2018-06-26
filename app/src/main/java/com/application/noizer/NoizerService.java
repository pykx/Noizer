package com.application.noizer;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.util.Log;

public class NoizerService extends Service {
    private static final String TAG = NoizerService.class.getSimpleName();

    private final IBinder mBinder = new LocalBinder();

    public NoizerService() {
        initialize();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    // Return this instance of LocalService so clients can call public methods
    public class LocalBinder extends Binder {
        NoizerService getService() {
            return NoizerService.this;
        }
    }

    protected native void initialize();

    protected native boolean play();

    protected native boolean stop();

    static {
        System.loadLibrary("libnoizer");
    }
}
