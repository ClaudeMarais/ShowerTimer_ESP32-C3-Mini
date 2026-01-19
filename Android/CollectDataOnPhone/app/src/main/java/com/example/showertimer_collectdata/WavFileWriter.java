package com.example.showertimer_collectdata;

import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class WavFileWriter {
    private final FileOutputStream outputStream;
    private int sampleRate = 8000;  // 8Khz
    private int dataSize = 0; // Track data size for header updates
    private final File file;

    public WavFileWriter(String filePath, int sampleRate) throws IOException {
        this.sampleRate = sampleRate;
        file = new File(filePath);
        outputStream = new FileOutputStream(file);
        writeWavHeader();
    }

    private void writeWavHeader() throws IOException {
        byte[] header = new byte[44];
        ByteBuffer buffer = ByteBuffer.wrap(header).order(ByteOrder.LITTLE_ENDIAN);

        // RIFF header
        buffer.put("RIFF".getBytes());
        buffer.putInt(36 + dataSize);
        buffer.put("WAVE".getBytes());

        // fmt chunk
        buffer.put("fmt ".getBytes());
        buffer.putInt(16); // PCM format
        buffer.putShort((short) 1); // Audio format = PCM

        // Mono
        int numChannels = 1;
        buffer.putShort((short) numChannels);

        buffer.putInt(sampleRate);

        // 16-bit audio (2 bytes per sample)
        int byteRate = sampleRate * numChannels * 2;
        buffer.putInt(byteRate);
        buffer.putShort((short) (numChannels * 2)); // Block align
        buffer.putShort((short) 16); // Bits per sample

        // data chunk
        buffer.put("data".getBytes());
        buffer.putInt(dataSize);

        outputStream.write(header);
    }

    public void writeAudioData(byte[] rawData) throws IOException {
        outputStream.write(rawData); // Write raw PCM data
        dataSize += rawData.length; // Track data size
    }

    public void writeEndianSwappedAudioData(byte[] rawData) throws IOException {
        short[] swappedData = swapByteOrder(rawData);
        ByteBuffer buffer = ByteBuffer.allocate(swappedData.length * 2);
        buffer.order(ByteOrder.LITTLE_ENDIAN); // Ensure correct WAV format

        for (short sample : swappedData) {
            buffer.putShort(sample);
        }

        outputStream.write(buffer.array()); // Write swapped PCM data to WAV
        dataSize += rawData.length;
    }

    public static short[] swapByteOrder(byte[] data) {
        short[] result = new short[data.length / 2]; // Each int16 is 2 bytes
        for (int i = 0; i < result.length; i++) {
            result[i] = (short) ((data[i * 2] & 0xFF) | (data[i * 2 + 1] << 8)); // Swap bytes
        }
        return result;
    }

    public void close() throws IOException {
        updateWavHeader();
        outputStream.close();
    }

    private void updateWavHeader() throws IOException {
        RandomAccessFile raf = new RandomAccessFile(file, "rw");
        raf.seek(4);
        raf.writeInt(36 + dataSize); // Update RIFF chunk size
        raf.seek(40);
        raf.writeInt(dataSize); // Update data chunk size
        raf.close();
        Log.println(1,"WavFileWrite", "Num bytes in audio " + dataSize);
    }
}
