package app.libs;

import android.app.PendingIntent;
import android.content.Intent;
import android.os.Build;
import android.service.quicksettings.Tile;
import android.service.quicksettings.TileService;

public abstract class LaunchTileService extends TileService
{
    private static final String LAUNCH_ACTION_EXTRA = "launch_action";
    private static final int FIRST_SDK_WITH_PENDING_INTENT_LAUNCH = 34;

    protected abstract String launchAction();

    @Override
    public void onStartListening()
    {
        Tile tile = getQsTile();
        tile.setState(Tile.STATE_INACTIVE);
        tile.updateTile();
    }

    @Override
    public void onClick()
    {
        Intent launch = getPackageManager().getLaunchIntentForPackage(getPackageName());
        if (launch == null)
        {
            return;
        }
        launch.addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
        launch.putExtra(LAUNCH_ACTION_EXTRA, launchAction());
        launch.putExtra(LockScreen.EXTRA_LOCK_SCREEN, isLocked());
        startActivityAndCollapseCompat(launch);
    }

    private void startActivityAndCollapseCompat(Intent launch)
    {
        if (Build.VERSION.SDK_INT >= FIRST_SDK_WITH_PENDING_INTENT_LAUNCH)
        {
            PendingIntent pending = PendingIntent.getActivity(
                this, 0, launch, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
            startActivityAndCollapse(pending);
        }
        else
        {
            startActivityAndCollapse(launch);
        }
    }
}
