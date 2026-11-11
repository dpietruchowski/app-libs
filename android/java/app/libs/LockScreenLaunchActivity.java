package app.libs;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

public final class LockScreenLaunchActivity extends Activity
{
    public static final String EXTRA_LAUNCH_ACTION = "launch_action";

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        Intent launch = getPackageManager().getLaunchIntentForPackage(getPackageName());
        if (launch != null)
        {
            launch.putExtra(EXTRA_LAUNCH_ACTION, getIntent().getStringExtra(EXTRA_LAUNCH_ACTION));
            launch.putExtra(LockScreen.EXTRA_LOCK_SCREEN, LockScreen.isLocked(this));
            startActivity(launch);
        }
        finish();
    }
}
