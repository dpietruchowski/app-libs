package app.libs;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.Bundle;

import org.qtproject.qt.android.bindings.QtActivity;

public class LockScreenActivity extends QtActivity
{
    private final BroadcastReceiver unlockReceiver = new BroadcastReceiver()
    {
        @Override
        public void onReceive(Context context, Intent intent)
        {
            LockScreen.release(LockScreenActivity.this);
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        LockScreen.show(this, getIntent());
        registerUnlockReceiver();
        super.onCreate(savedInstanceState);
    }

    @Override
    protected void onNewIntent(Intent intent)
    {
        LockScreen.show(this, intent);
        super.onNewIntent(intent);
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        LockScreen.notifyChanged();
    }

    @Override
    protected void onStop()
    {
        LockScreen.release(this);
        super.onStop();
    }

    @Override
    protected void onDestroy()
    {
        unregisterReceiver(unlockReceiver);
        super.onDestroy();
    }

    private void registerUnlockReceiver()
    {
        IntentFilter filter = new IntentFilter(Intent.ACTION_USER_PRESENT);
        if (Build.VERSION.SDK_INT >= 33)
        {
            registerReceiver(unlockReceiver, filter, Context.RECEIVER_NOT_EXPORTED);
        }
        else
        {
            registerReceiver(unlockReceiver, filter);
        }
    }
}
