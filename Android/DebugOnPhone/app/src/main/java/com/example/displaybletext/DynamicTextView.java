package com.example.displaybletext;

import android.util.Log;
import android.view.View;
import android.widget.TextView;

public class DynamicTextView
{
    private TextView textView;
    private String name;
    private String text;
    private long timeWhenSet;

    public DynamicTextView(String name, TextView textView, String text) {
        this.name = name;
        this.textView = textView;
        setText(text);
    }
    public String getText() { return text; }

    public String getName() { return name; }

    public long getTimeWhenSet() { return timeWhenSet; }

    public void reset() {
        //text = "";
        //textView.setText(text);
        textView.setVisibility(View.VISIBLE);
        textView.requestLayout();
        textView.invalidate();
        Log.d("ShowBLEText", "textFromBLEDevice.reset() " + timeWhenSet);
    }

    public void setText(String text) {
        this.text = text;
        textView.setText(this.text);
        //Log.d("DEBUG", "height=" + textView.getHeight());
        //textView.setVisibility(View.INVISIBLE);
        textView.setVisibility(View.VISIBLE);
        //textView.requestLayout();
        textView.invalidate();
        timeWhenSet = System.currentTimeMillis();
        Log.d("ShowBLEText", "textFromBLEDevice.setText() " + text + " " + timeWhenSet);
    }

/*    public void setText(String text, int delayTimeInMs) {
        setText(text);
        //pause(delayTimeInMs);
        //reset();
        setText("Hello3");
    }

    public static void pause(int delayTimeInMs) {
        try {
            Thread.sleep(delayTimeInMs);
        } catch (InterruptedException e) {
            System.err.format("InterruptedException : %s%n", e);
        }
    }*/
}