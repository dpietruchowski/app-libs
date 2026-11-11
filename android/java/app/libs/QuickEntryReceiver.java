package app.libs;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

public final class QuickEntryReceiver extends BroadcastReceiver
{
    @Override
    public void onReceive(Context context, Intent intent)
    {
        String action = intent.getAction();
        if (action != null)
        {
            QuickEntry.handleTap(context.getApplicationContext(), action);
        }
    }
}
