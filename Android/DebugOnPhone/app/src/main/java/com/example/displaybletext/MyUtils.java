package com.example.displaybletext;


import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Environment;
import android.os.storage.StorageManager;
import android.util.Log;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;

import java.io.File;

public class MyUtils {

    private Activity activity;

    MyUtils(Activity activity) {
        this.activity = activity;
    }

    public void showToastMessage(String msg) {
        activity.runOnUiThread(new Runnable() {
            public void run() {
                Toast.makeText(activity.getApplicationContext(), msg, Toast.LENGTH_SHORT).show();
            }
        });
    }
}
