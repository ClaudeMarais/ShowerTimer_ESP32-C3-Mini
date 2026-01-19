// NOTE: This works with target API 31. Target API 33 does not allow the app to show
// Bluetooth permission notifications to the user, and then eventually crashes

package com.example.showertimer_collectdata;
import android.app.Activity;
import android.Manifest;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.location.LocationManager;
import android.provider.Settings;
import android.util.Log;

// Simple Bluetooth scanner to find one specific Bluetooth device
public class SimpleBlueToothScanner {

    static final int REQUEST_ENABLE_BLUETOOTH = 1;
    static final int REQUEST_BLUETOOTH_SCAN = 2;
    static final int REQUEST_BLUETOOTH_CONNECT = 3;
    static final int REQUEST_COARSE_LOCATION = 4;
    static final int REQUEST_FINE_LOCATION = 5;

    String deviceNameToFind;    // The name of the device we're looking for
    BluetoothDevice device;     // When we find the device, it will be saved in this variable

    BluetoothManager manager;
    BluetoothAdapter adapter;
    BluetoothLeScanner scanner;

    Context context;

    MyUtils myUtils;

    Boolean isScanning = false;

    public SimpleBlueToothScanner(String deviceNameToFind, PackageManager packageManager, Activity activity) {
        this.context = activity.getApplicationContext();
        this.deviceNameToFind = deviceNameToFind;

        myUtils = new MyUtils(activity);

        // Check if the phone has all the required permissions turned on
        checkRequiredPermissions(packageManager, activity);

        // Create the scanner
        manager = (BluetoothManager) activity.getSystemService(Context.BLUETOOTH_SERVICE);
        adapter = manager.getAdapter();
        scanner = adapter.getBluetoothLeScanner();
    }

    public Boolean isScanning() { return this.isScanning; }

    public BluetoothDevice GetDevice() { return device; }

    public void startScanning() {
        if (isScanning) {
            return;
        }

        Log.println(1,"ShowerTimer", "startScanning");

        isScanning = true;
        device = null;
        scanner.startScan(scanCallback);
    }

    public void stopScanning() {
        if (isScanning) {
            isScanning = false;
            scanner.stopScan(scanCallback);
        }
    }

    private ScanCallback scanCallback = new ScanCallback() {
        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            if (!isScanning) {
                return;
            }

            String deviceName = result.getDevice().getName();

            if (deviceName != null && deviceName.equals(deviceNameToFind)) {
                myUtils.showToastMessage("Bluetooth device '" + deviceNameToFind + "' found");
                device = result.getDevice();
                stopScanning();
            }
        }

    };

    private void checkRequiredPermissions(PackageManager packageManager, Activity activity) {

        // Ask permission to show notifications, otherwise the notifications to ask permission
        // for Bluetooth won't show
        //activity.requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, REQUEST_CODE_NOTIFICATIONS);

        // Check that BLE is supported
        if (!packageManager.hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE)) {
            myUtils.showToastMessage("Bluetooth Low Energy is NOT supported on this device");
            activity.finish();
        }

        // Check if Bluetooth is enabled
        if (!BluetoothAdapter.getDefaultAdapter().isEnabled()) {
            Intent enableIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
            activity.startActivityForResult(enableIntent, REQUEST_ENABLE_BLUETOOTH);
        }

        // Check for coarse location permission
        if (context.checkSelfPermission(Manifest.permission.ACCESS_COARSE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            activity.requestPermissions(new String[]{Manifest.permission.ACCESS_COARSE_LOCATION}, REQUEST_COARSE_LOCATION);
        }

        // Check for fine location permission
        if (context.checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            activity.requestPermissions(new String[]{Manifest.permission.ACCESS_FINE_LOCATION}, REQUEST_FINE_LOCATION);
        }

        LocationManager locationManager = (LocationManager) activity.getSystemService(Context.LOCATION_SERVICE);
        if (!locationManager.isProviderEnabled(LocationManager.GPS_PROVIDER)) {
            activity.startActivity(new Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS));
        }

        // Check for Bluetooth scan permission
        if (context.checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {
            activity.requestPermissions(new String[]{Manifest.permission.BLUETOOTH_SCAN}, REQUEST_BLUETOOTH_SCAN);
        }

        // Check for Bluetooth connect permission
        if (context.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
            activity.requestPermissions(new String[]{Manifest.permission.BLUETOOTH_CONNECT}, REQUEST_BLUETOOTH_CONNECT);
        }
    }
}
