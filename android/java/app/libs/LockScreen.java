package app.libs;

import android.app.Activity;
import android.app.KeyguardManager;
import android.content.Context;
import android.content.Intent;

public final class LockScreen
{
    public static final String EXTRA_LOCK_SCREEN = "lock_screen";

    private static volatile boolean shownOverLock;
    private static volatile boolean listening;

    private LockScreen()
    {
    }

    public static boolean isLocked(Context context)
    {
        return keyguard(context).isKeyguardLocked();
    }

    public static boolean isShownOverLock()
    {
        return shownOverLock;
    }

    public static void setListening(boolean value)
    {
        listening = value;
    }

    public static void requestUnlock(Context context)
    {
        if (!(context instanceof Activity))
        {
            nativeUnlockFinished(false);
            return;
        }
        final Activity activity = (Activity) context;
        activity.runOnUiThread(() -> dismissKeyguard(activity));
    }

    static void show(Activity activity, Intent intent)
    {
        boolean overLock = false;
        if (intent != null)
        {
            overLock = intent.getBooleanExtra(EXTRA_LOCK_SCREEN, false) && isLocked(activity);
            intent.removeExtra(EXTRA_LOCK_SCREEN);
        }
        activity.setShowWhenLocked(overLock);
        activity.setTurnScreenOn(overLock);
        update(overLock);
    }

    static void release(Activity activity)
    {
        activity.setShowWhenLocked(false);
        activity.setTurnScreenOn(false);
        update(false);
    }

    static void notifyChanged()
    {
        if (listening)
        {
            nativeStateChanged();
        }
    }

    private static void update(boolean overLock)
    {
        shownOverLock = overLock;
        notifyChanged();
    }

    private static void dismissKeyguard(Activity activity)
    {
        KeyguardManager manager = keyguard(activity);
        if (!manager.isKeyguardLocked())
        {
            nativeUnlockFinished(true);
            return;
        }
        manager.requestDismissKeyguard(activity, new KeyguardManager.KeyguardDismissCallback()
        {
            @Override
            public void onDismissSucceeded()
            {
                nativeUnlockFinished(true);
            }

            @Override
            public void onDismissCancelled()
            {
                nativeUnlockFinished(false);
            }

            @Override
            public void onDismissError()
            {
                nativeUnlockFinished(false);
            }
        });
    }

    private static KeyguardManager keyguard(Context context)
    {
        return (KeyguardManager) context.getSystemService(Context.KEYGUARD_SERVICE);
    }

    private static native void nativeStateChanged();

    private static native void nativeUnlockFinished(boolean succeeded);
}
