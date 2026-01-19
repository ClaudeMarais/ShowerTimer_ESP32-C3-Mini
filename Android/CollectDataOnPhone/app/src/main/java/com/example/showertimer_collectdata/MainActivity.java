package com.example.showertimer_collectdata;

import static android.bluetooth.BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

import android.annotation.SuppressLint;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.File;
import java.io.IOException;
import java.io.PrintStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.UUID;
import java.util.concurrent.ConcurrentLinkedQueue;

public class MainActivity extends AppCompatActivity implements View.OnClickListener {
    // We're sending 8Khz sample rate from Arduino
    static int AudioSampleRate = 8000;

    // Header commands for communication from Arduino device
    static String CommandStartRecording = "RECORD";     // Arduino device started recording audio
    static String CommandStartTransfer = "TRANSFER";    // Arduino device started transferring audio

    // Place raw audio data for a full recording, received from the Arduino device
    ConcurrentLinkedQueue<byte[]> rawAudioDataQueue = new ConcurrentLinkedQueue<>();

    // Defines for ML data tags
    byte DataTag_ShowerOff = 0;
    byte DataTag_ShowerOn = 1;

    // Keep track of what we're currently recording
    byte RecordingDataTag = DataTag_ShowerOff;
    Boolean CurrentlyTransferring = false;
    Boolean WaitingForRecordCommand = true;
    Boolean WaitingForTransferCommand = false;

    int numSecondsOfAudioRecording = 0;
	long timeRecordingStarted = 0;
    int totalBytesExpected = 0;
    int totalBytesReceived = 0;
    int numBLEPacketsReceived = 0;

    Handler handlerUpdateRecordingProgress = new Handler();

    MyUtils myUtils;

    Boolean isStorageAvailable;

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

        showScanneBLE(true);
        showDeviceConnected(false);
        showRecordingText(false);
        showTransferringText(false);
        showProgressBar(false);

        // Only enable the buttons once BLE is connected
        activateRecordShowerOffButton(false, true);
        activateRecordShowerOnButton(false, true);

        myUtils = new MyUtils(this);
        isStorageAvailable = myUtils.isStorageAvailable();

        Log.i("ShowerTimer", isStorageAvailable ? "Storage Available" : "Storage NOT available");

        // Find the Arduino Bluetooth device and connect to it
        connectBluetoothDevice();

        Button button = (Button)findViewById(R.id.buttonRecordShowerOff);
        button.setOnClickListener(this);
        button = (Button)findViewById(R.id.buttonRecordShowerOn);
        button.setOnClickListener(this);
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.buttonRecordShowerOff:
                showScanneBLE(false);
                showDeviceConnected(false);
                showRecordingText(true);
                showTransferringText(false);
                updateProgressBar(0);
                showProgressBar(true);
                activateRecordShowerOffButton(false, true);
                activateRecordShowerOnButton(false, true);
                WaitingForRecordCommand = true;
                WaitingForTransferCommand = false;
                CurrentlyTransferring = false;
                RecordingDataTag = DataTag_ShowerOff;
                rawAudioDataQueue.clear();
                sendDataToArduino(RecordingDataTag);
                break;

            case R.id.buttonRecordShowerOn:
                showScanneBLE(false);
                showDeviceConnected(false);
                showRecordingText(true);
                showTransferringText(false);
                updateProgressBar(0);
                showProgressBar(true);
                activateRecordShowerOffButton(false, true);
                activateRecordShowerOnButton(false, true);
                WaitingForRecordCommand = true;
                WaitingForTransferCommand = false;
                CurrentlyTransferring = false;
                RecordingDataTag = DataTag_ShowerOn;
                rawAudioDataQueue.clear();
                sendDataToArduino(RecordingDataTag);
                break;
        }
    }

    private void showScanneBLE(Boolean show) {
        runOnUiThread(new Runnable() {
            public void run() {
                TextView textView = (TextView)findViewById(R.id.textView_ScanningBLE);
                textView.setVisibility(show ? View.VISIBLE : View.INVISIBLE);
            }
        });
    }

    private void showDeviceConnected(Boolean show) {
        runOnUiThread(new Runnable() {
            public void run() {
                TextView textView = (TextView)findViewById(R.id.textView_DeviceConnected);
                textView.setVisibility(show ? View.VISIBLE : View.INVISIBLE);
            }
        });
    }

    private void showRecordingText(Boolean show) {
        runOnUiThread(new Runnable() {
            public void run() {
                TextView textView = (TextView)findViewById(R.id.textView_RecordingAudio);
                textView.setVisibility(show ? View.VISIBLE : View.INVISIBLE);
            }
        });
    }

    private void showTransferringText(Boolean show) {
        runOnUiThread(new Runnable() {
            public void run() {
                TextView textView = (TextView)findViewById(R.id.textView_TransferringAudio);
                textView.setVisibility(show ? View.VISIBLE : View.INVISIBLE);
            }
        });
    }

    private void showProgressBar(Boolean show) {
        runOnUiThread(new Runnable() {
            public void run() {
                ProgressBar progressBar = (ProgressBar)findViewById(R.id.progressBar);
                progressBar.setVisibility(show ? View.VISIBLE : View.INVISIBLE);
                if (!show) progressBar.setProgress(0);
            }
        });
    }

    private void updateProgressBar(int value) {
        runOnUiThread(new Runnable() {
            public void run() {
                ProgressBar progressBar = (ProgressBar)findViewById(R.id.progressBar);
                progressBar.setProgress(value);
            }
        });
    }

    private void updateProgressBar(int value, int maxValue) {
        runOnUiThread(new Runnable() {
            public void run() {
                ProgressBar progressBar = (ProgressBar)findViewById(R.id.progressBar);
                progressBar.setMax(maxValue);
                progressBar.setProgress(value);
            }
        });
    }

    private void activateRecordShowerOffButton(Boolean activate, Boolean show) {
        runOnUiThread(new Runnable() {
            public void run() {
                Button button = (Button)findViewById(R.id.buttonRecordShowerOff);
                button.setVisibility(show ? View.VISIBLE : View.INVISIBLE);
                button.setEnabled(activate);
            }
        });
    }

    private void activateRecordShowerOnButton(Boolean activate, Boolean show) {
        runOnUiThread(new Runnable() {
            public void run() {
                Button button = (Button)findViewById(R.id.buttonRecordShowerOn);
                button.setVisibility(show ? View.VISIBLE : View.INVISIBLE);
                button.setEnabled(activate);
            }
        });
    }

    @SuppressLint("MissingPermission")
    private void sendDataToArduino(byte value) {
        BluetoothGattService gattService = btGatt.getService(java.util.UUID.fromString(SHOWER_TIMER_SERVICE_UUID));
        BluetoothGattCharacteristic phoneToArduinoChar = gattService.getCharacteristic(java.util.UUID.fromString(DATA_FROM_PHONE_TO_ARDUINO_CHARACTERISTIC_UUID));
        byte[] data = { value };
        btGatt.writeCharacteristic(phoneToArduinoChar, data, WRITE_TYPE_DEFAULT);
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
                showScanneBLE(true);
                showDeviceConnected(false);
                showRecordingText(false);
                showTransferringText(false);
                showProgressBar(false);
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
                simpleBlueToothScanner.stopScanning();

                // Connect to the device and specify "auto connect" so that the device will
                // automatically reconnect if for some reason it disconnects while using it
                btGatt = btDevice.connectGatt(getApplicationContext(), true, btGattCallback);
            }
        };

        // Start running the thread
        ConnectBluetoothDeviceThread.start();
    }

    byte[] getAllReceivedData(ConcurrentLinkedQueue<byte[]> rawAudioDataQueue) {
        // Calculate total byte size
        int totalSize = rawAudioDataQueue.stream().mapToInt(arr -> arr.length).sum();

        // Use ByteBuffer for efficient merging
        ByteBuffer buffer = ByteBuffer.allocate(totalSize);

        while (!rawAudioDataQueue.isEmpty()) {
            buffer.put(rawAudioDataQueue.poll()); // Retrieve and append packet data
        }

        return buffer.array(); // Return merged byte array
    }

    // Callback to connect BLE device
    private BluetoothGattCallback btGattCallback = new BluetoothGattCallback() {

        @Override
        // This will get called when the device sends a notification of a data change
        public void onCharacteristicChanged(BluetoothGatt gatt, final BluetoothGattCharacteristic characteristic) {
            byte[] receivedData = characteristic.getValue();

            if (WaitingForRecordCommand) {
                // Convert first part of receivedData to string
                int stringLength = CommandStartRecording.length();
                String receivedString = new String(receivedData, 0, Math.min(stringLength, receivedData.length), StandardCharsets.UTF_8);

                // If the packet is CommandStartRecording, extract the recording duration (uint32)
                if (receivedString.startsWith(CommandStartRecording)) {
                    WaitingForRecordCommand = false;
                    WaitingForTransferCommand = true;
                    CurrentlyTransferring = false;
                    totalBytesReceived = 0;
                    numBLEPacketsReceived = 0;

                    ByteBuffer buffer = ByteBuffer.wrap(receivedData, stringLength, 4).order(ByteOrder.LITTLE_ENDIAN);
                    numSecondsOfAudioRecording = buffer.getInt();
                    updateProgressBar(0, numSecondsOfAudioRecording * 1000);    // Update progress bar in milliseconds

                    Log.i("ShowerTimer", "" + CommandStartRecording + " " + numSecondsOfAudioRecording);

                    showScanneBLE(false);
                    showDeviceConnected(false);
                    showRecordingText(true);
                    showTransferringText(false);
                    showProgressBar(true);

                    startRecordingTimer(); // Begin timer-based updates

                    return; // No need to process further
                }
            }
            else if (WaitingForTransferCommand) {
                // Convert first part of receivedData to string
                int stringLength = CommandStartTransfer.length();
                String receivedString = new String(receivedData, 0, Math.min(stringLength, receivedData.length), StandardCharsets.UTF_8);

                // If the packet is CommandStartTransfer, extract the total byte count (uint32)
                if (receivedString.startsWith(CommandStartTransfer)) {
                    stopRecordingTimer();   // Stop the timer that updates the progressbar during recording
                    WaitingForRecordCommand = false;
                    WaitingForTransferCommand = false;
                    CurrentlyTransferring = true;
                    totalBytesReceived = 0;
                    numBLEPacketsReceived = 0;

                    ByteBuffer buffer = ByteBuffer.wrap(receivedData, stringLength, 4).order(ByteOrder.LITTLE_ENDIAN);
                    totalBytesExpected = buffer.getInt();
                    updateProgressBar(0, totalBytesExpected);

                    Log.i("ShowerTimer", "" + CommandStartTransfer + " " + totalBytesExpected);

                    showScanneBLE(false);
                    showDeviceConnected(false);
                    showRecordingText(false);
                    showTransferringText(true);
                    showProgressBar(true);

                    return; // No need to process further
                }
            }
            else if (CurrentlyTransferring) {
                rawAudioDataQueue.add(receivedData);

                int byteCount = receivedData.length;
                totalBytesReceived += byteCount;

                // Update progress only every 100th BLE packet received
                numBLEPacketsReceived++;
                if ((numBLEPacketsReceived % 100) == 0) {
                    updateProgressBar(totalBytesReceived);
                }

                if (totalBytesReceived >= totalBytesExpected) {
                    Log.i("ShowerTimer", "All audio data received");
                    showScanneBLE(false);
                    showDeviceConnected(true);
                    showRecordingText(false);
                    showTransferringText(false);
                    showProgressBar(false);
                    activateRecordShowerOffButton(true, true);
                    activateRecordShowerOnButton(true, true);
                    CurrentlyTransferring = false;

                    // Write data to WAV file (with endian swap)
                    try {
                        // Generate unique file name from date and time
                        String tagName = (RecordingDataTag == DataTag_ShowerOff) ? "ShowerOff" : "ShowerOn";
                        String date = new SimpleDateFormat("yyyy-mm-dd-hh-mm-ss").format(new Date());
                        String fileName = myUtils.getDataFolder() + "/" + tagName + "." + date + ".wav";
                        Log.i("ShowerTimer", "Save filename: " + fileName);

                        // Write audio data to WAV file. Arduino uses big endian and Android uses little endian,
                        // therefore when int16 audio data is saved, it needs to be endian swapped
                        WavFileWriter wavFileWriter = new WavFileWriter(fileName, AudioSampleRate);
                        wavFileWriter.writeEndianSwappedAudioData(getAllReceivedData(rawAudioDataQueue));
                        wavFileWriter.close();
                        Log.i("ShowerTimer", "WAV file saved");
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                }
            }
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
                    showScanneBLE(true);
                    showDeviceConnected(false);
                    showTransferringText(false);

                    if (WaitingForTransferCommand) {
                        showRecordingText(true);
                        showProgressBar(true);
                    }
                    else {
                        showRecordingText(false);
                        showProgressBar(false);
                    }
                    activateRecordShowerOffButton(false, true);
                    activateRecordShowerOnButton(false, true);
                    break;

                case BluetoothGatt.STATE_CONNECTED:
                    isBlueToothDeviceConnected = true;
                    message = "Bluetooth device '" + ArduinoBluetoothDeviceName + "' connected";
                    System.out.println(message);
                    myUtils.showToastMessage(message);
                    btGatt.discoverServices();  // Discover services and characteristics for this device
                    showScanneBLE(false);
                    showDeviceConnected(true);
                    showTransferringText(false);

                    if (WaitingForTransferCommand) {
                        showRecordingText(true);
                        showProgressBar(true);
                        activateRecordShowerOffButton(false, true);
                        activateRecordShowerOnButton(false, true);
                    }
                    else {
                        showRecordingText(false);
                        showProgressBar(false);
                        activateRecordShowerOffButton(true, true);
                        activateRecordShowerOnButton(true, true);
                    }
                    break;

                default:
                    break;
            }
        }

        @SuppressLint("MissingPermission")
        @Override
        public void onServicesDiscovered(final BluetoothGatt gatt, final int status) {
            BluetoothGattService gattService = btGatt.getService(java.util.UUID.fromString(SHOWER_TIMER_SERVICE_UUID));
            BluetoothGattCharacteristic arduinoToPhoneChar = gattService.getCharacteristic(java.util.UUID.fromString(DATA_FROM_ARDUINO_TO_PHONE_CHARACTERISTIC_UUID));
            BluetoothGattCharacteristic phoneToArduinoChar = gattService.getCharacteristic(java.util.UUID.fromString(DATA_FROM_PHONE_TO_ARDUINO_CHARACTERISTIC_UUID));

            btGatt.setCharacteristicNotification(arduinoToPhoneChar, true);
            btGatt.setCharacteristicNotification(phoneToArduinoChar, true);

            BluetoothGattDescriptor descriptor = arduinoToPhoneChar.getDescriptor(UUID.fromString(BLUETOOTH_LE_CCCD));
            descriptor.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
            btGatt.writeDescriptor(descriptor);
        }
    };

    // Start recording timer
    private void startRecordingTimer() {
        timeRecordingStarted = System.currentTimeMillis(); // Get start time
        handlerUpdateRecordingProgress.postDelayed(updateProgressRunnable, 100); // Begin periodic updates
    }

    // Stop recording timer
    private void stopRecordingTimer() {
        handlerUpdateRecordingProgress.removeCallbacks(updateProgressRunnable); // Stop updates
    }

    // Runnable to update progress bar every 50ms
    private Runnable updateProgressRunnable = new Runnable() {
        @Override
        public void run() {
            long elapsedTimeInMs = System.currentTimeMillis() - timeRecordingStarted;
            long numMsOfAudioRecording = numSecondsOfAudioRecording * 1000;

            if (elapsedTimeInMs < numMsOfAudioRecording) {
                updateProgressBar((int) elapsedTimeInMs);
                handlerUpdateRecordingProgress.postDelayed(this, 50); // Schedule next update
            } else {
                updateProgressBar((int) numMsOfAudioRecording);
                handlerUpdateRecordingProgress.removeCallbacks(this); // Stop updating
            }
        }
    };
}