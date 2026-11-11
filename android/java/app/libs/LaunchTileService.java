package app.libs;

import android.app.PendingIntent;
import android.content.Intent;
import android.os.Build;
import android.service.quicksettings.Tile;
import android.service.quicksettings.TileService;

public abstract class LaunchTileService extends TileService
{
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
        Intent launch = new Intent(this, LockScreenLaunchActivity.class);
        launch.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        launch.putExtra(LockScreenLaunchActivity.EXTRA_LAUNCH_ACTION, launchAction());
        startActivityAndCollapseCompat(launch);
    }

    @SuppressWarnings("deprecation")
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
