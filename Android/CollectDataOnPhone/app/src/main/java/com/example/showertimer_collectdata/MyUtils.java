package com.example.showertimer_collectdata;


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
    private String dataFolder;

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

    String getDataFolder() { return dataFolder; }

    // NOTE: Requires
    //  <uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" />
    //  <uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" />
    //  android:requestLegacyExternalStorage="true"

    private static final int REQUEST_EXTERNAL_STORAGE = 1;

    private static String[] PERMISSIONS_STORAGE = {
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE
    };

    public Boolean isStorageAvailable() {

        // Check storage permission
        int permission = this.activity.checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE);
        if (permission != PackageManager.PERMISSION_GRANTED) {
            this.activity.requestPermissions(PERMISSIONS_STORAGE, REQUEST_EXTERNAL_STORAGE);
        }

        return createDataFolder();
    }

    private static String getInternalStorageDirectoryPath(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            StorageManager storageManager = (StorageManager) context.getSystemService(Context.STORAGE_SERVICE);
            return storageManager.getPrimaryStorageVolume().getDirectory().getAbsolutePath();
        } else {
            return Environment.getExternalStorageDirectory().getAbsolutePath();
        }
    }

    private Boolean createDataFolder() {
        String filePath = String.format("%1s/%2s/%3s", getInternalStorageDirectoryPath(this.activity), Environment.DIRECTORY_DOCUMENTS, "ShowerTimer");

        File newDir = new File(filePath);
        if (!newDir.exists()) {
            if (!newDir.mkdir()) {
                return false;
            };
        }

        dataFolder = filePath;
        return true;
    }
}
