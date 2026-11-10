package com.smartarranger.standalone;

import android.content.Intent;
import android.net.Uri;
import com.rmsl.juce.JuceActivity;

/** JUCE activity bridge for opening Yamaha style documents from Android file browsers. */
public final class SmartArrangerActivity extends JuceActivity
{
    private native void nativeOpenStyle (String uri);

    @Override
    protected void onCreate (android.os.Bundle state)
    {
        super.onCreate (state);
        dispatchStyleIntent (getIntent());
    }

    @Override
    protected void onNewIntent (Intent intent)
    {
        super.onNewIntent (intent);
        setIntent (intent);
        dispatchStyleIntent (intent);
    }

    private void dispatchStyleIntent (Intent intent)
    {
        if (intent == null || ! Intent.ACTION_VIEW.equals (intent.getAction()))
            return;

        Uri uri = intent.getData();
        if (uri != null)
            nativeOpenStyle (uri.toString());
    }
}
