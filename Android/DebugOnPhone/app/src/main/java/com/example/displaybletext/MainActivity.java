package com.example.displaybletext;

import static android.bluetooth.BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT;

import androidx.appcompat.app.AppCompatActivity;

import android.annotation.SuppressLint;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity {

    MyUtils myUtils;

    DynamicTextView textFromBLEDevice;

    boolean bUpdateText;
    String newText;

    Thread UpdateTextThread = new Thread() {
        public void run() {
            // Check every half second if we found the device
            while (true) {
                if (bUpdateText)
                {
                    updateText(newText);
                    bUpdateText = false;
                }
            }
        }
    };


    // The Bluetooth device name and UUIDs are required to be exactly the same between the Arduino and Android source code. But, since Java does not allow
    // #include's of files, these defines are duplicated rather than shared in a single header file that could have been included in both projects. Therefore,
    // if any of these change, be sure to update in both projects.

    // Arduino Bluetooth device name
    static String ArduinoBluetoothDeviceName = "ShowerTimerDevice";

    // Custom generated UUIDs for this project (using https://www.uuidgenerator.net/), also defined in the Arduino project
    static String SHOWER_TIMER_SERVICE_UUID = "0372c18f-6bb1-4c2f-a8c0-8125d51c41a4";
    static String DATA_FROM_PHONE_TO_ARDUINO_CHARACTERISTIC_UUID = "157bd625-aa1f-474a-8e26-251cf2ca626e";
    static String DATA_FROM_ARDUINO_TO_PHONE_CHARACTERISTIC_UUID = "56de086a-9c2a-4765-8761-3b42f6398673";

    // UUID for Bluetooth Client Configuration Characteristic Descriptor (CCCD)
    static String BLUETOOTH_LE_CCCD = "00002902-0000-1000-8000-00805F9B34FB";

    // Our simple Bluetooth scanner for finding the Arduino device
    SimpleBlueToothScanner simpleBlueToothScanner;

    // Bluetooth device and its services
    BluetoothGatt btGatt;
    BluetoothDevice btDevice;
    Boolean isBlueToothDeviceConnected = false;

    // Thread for finding the Bluetooth device
    Thread ConnectBluetoothDeviceThread;
	
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        textFromBLEDevice = new DynamicTextView("textFromBLEDevice", (TextView)findViewById(R.id.textView_textFromBLEDevice), "");
        UpdateTextThread.start();
        updateText("Disconnected");

        myUtils = new MyUtils(this);

        // Find the Arduino Bluetooth device and connect to it
        connectBluetoothDevice();
    }

    private void updateText(String text) {
        runOnUiThread(new Runnable() {
            public void run() {
                textFromBLEDevice.setText(text);
                Log.d("ShowBLEText", "textFromBLEDevice.updateText(" + text + ")");
            }
        });
    }

    private void showBlueToothScanMessage(Boolean show) {
        runOnUiThread(new Runnable() {
            public void run() {
                TextView textViewScanningBLE = (TextView)findViewById(R.id.textView_ScanningBLE);
                textViewScanningBLE.setVisibility(show ? View.VISIBLE : View.INVISIBLE);
            }
        });
    }

    private void connectBluetoothDevice()
    {
        // Create a simple Bluetooth scanner
        simpleBlueToothScanner = new SimpleBlueToothScanner(ArduinoBluetoothDeviceName, getPackageManager(), this);

        // Create a thread that will scan for and connect to our Bluetooth device
        ConnectBluetoothDeviceThread = new Thread() {
            @SuppressLint("MissingPermission")
            public void run() {

                // Start with an invalid device
                btDevice = null;

                // Start scanning
                showBlueToothScanMessage(true);
                simpleBlueToothScanner.startScanning();

                // Check every half second if we found the device
                while (btDevice == null) {
                    btDevice = simpleBlueToothScanner.GetDevice();

                    try {
                        Thread.sleep(500);
                    }
                    catch (InterruptedException e)
                    {
                        e.printStackTrace();
                    }
                }

                // We now have a valid device, so we can stop scanning
                showBlueToothScanMessage(false);
                simpleBlueToothScanner.stopScanning();

                // Connect to the device and specify "auto connect" so that the device will
                // automatically reconnect if for some reason it disconnects while using it
                btGatt = btDevice.connectGatt(getApplicationContext(), true, btGattCallback);
            }
        };

        // Start running the thread
        ConnectBluetoothDeviceThread.start();
    }

    // Device connect callback
    private BluetoothGattCallback btGattCallback = new BluetoothGattCallback() {

        @Override
        // This will get called when the device sends a notification of a data change
        public void onCharacteristicChanged(BluetoothGatt gatt, final BluetoothGattCharacteristic characteristic) {
            final byte[] textData = characteristic.getValue();
            String text = new String(textData);
            Log.d("ShowBLEText", text);
            newText = text;
            bUpdateText = true;
        }

        @SuppressLint("MissingPermission")
        @Override
        // Called when a device connects or disconnects
        public void onConnectionStateChange(final BluetoothGatt gatt, final int status, final int newState) {
            switch (newState) {
                case BluetoothGatt.STATE_DISCONNECTED:
                    // We specify "auto connect" when calling connectGatt(), therefore there is no extra work to be done here
                    // to disconnect gatt or rescan for the device, it will automatically reconnect once available again
                    isBlueToothDeviceConnected = false;
                    String message = "Bluetooth device '" + ArduinoBluetoothDeviceName + "' disconnected!";
                    System.out.println(message);
                    myUtils.showToastMessage(message);
                    showBlueToothScanMessage(true);
                    textFromBLEDevice.setText("Disconnected");
                    break;

                case BluetoothGatt.STATE_CONNECTED:
                    isBlueToothDeviceConnected = true;
                    message = "Bluetooth device '" + ArduinoBluetoothDeviceName + "' connected";
                    System.out.println(message);
                    myUtils.showToastMessage(message);
                    btGatt.discoverServices();  // Discover services and characteristics for this device
                    textFromBLEDevice.setText("...");
                    showBlueToothScanMessage(false);
                    break;

                default:
                    break;
            }
        }

        @Override
        public void onServicesDiscovered(final BluetoothGatt gatt, final int status) {
            // The Arduino device will send notifications when a target zone was hit, therefore make sure notifications are enabled
            BluetoothGattService gattService = btGatt.getService(java.util.UUID.fromString(SHOWER_TIMER_SERVICE_UUID));
            BluetoothGattCharacteristic arduinoToPhoneChar = gattService.getCharacteristic(java.util.UUID.fromString(DATA_FROM_ARDUINO_TO_PHONE_CHARACTERISTIC_UUID));
            BluetoothGattCharacteristic phoneToArduinoChar = gattService.getCharacteristic(java.util.UUID.fromString(DATA_FROM_PHONE_TO_ARDUINO_CHARACTERISTIC_UUID));

            btGatt.setCharacteristicNotification(arduinoToPhoneChar, true);
            btGatt.setCharacteristicNotification(phoneToArduinoChar, true);

            BluetoothGattDescriptor descriptor = arduinoToPhoneChar.getDescriptor(java.util.UUID.fromString(BLUETOOTH_LE_CCCD));
            descriptor.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
            btGatt.writeDescriptor(descriptor);
        }
    };
}