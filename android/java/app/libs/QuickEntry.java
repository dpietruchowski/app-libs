package app.libs;

import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.util.Log;

import org.json.JSONException;

import java.util.Date;

public final class QuickEntry
{
    static final String ACTION_STEP_DOWN = "app.libs.quickentry.STEP_DOWN";
    static final String ACTION_STEP_UP = "app.libs.quickentry.STEP_UP";
    static final String ACTION_CYCLE_VARIANT = "app.libs.quickentry.CYCLE_VARIANT";
    static final String ACTION_SWITCH_MODE = "app.libs.quickentry.SWITCH_MODE";
    static final String ACTION_COMMIT = "app.libs.quickentry.COMMIT";

    private static final String LOG_TAG = "QuickEntry";
    private static final String NOTIFICATION_TAG = "app.libs.quickentry";
    private static final int NOTIFICATION_ID = 1;

    private static volatile boolean listening;

    private QuickEntry()
    {
    }

    public static synchronized void configure(Context context, String configJson)
    {
        try
        {
            QuickEntryConfig.parse(configJson);
        }
        catch (JSONException invalid)
        {
            Log.e(LOG_TAG, "ignoring an invalid configuration", invalid);
            return;
        }
        QuickEntryStore store = new QuickEntryStore(context);
        store.saveConfig(configJson);
        if (store.visible())
        {
            post(context, store);
        }
    }

    public static synchronized void show(Context context)
    {
        QuickEntryStore store = new QuickEntryStore(context);
        store.saveVisible(true);
        post(context, store);
    }

    public static synchronized void hide(Context context)
    {
        new QuickEntryStore(context).saveVisible(false);
        context.getSystemService(NotificationManager.class).cancel(NOTIFICATION_TAG, NOTIFICATION_ID);
    }

    public static synchronized void setHeader(Context context, String header)
    {
        QuickEntryStore store = new QuickEntryStore(context);
        store.saveHeader(header);
        if (store.visible())
        {
            post(context, store);
        }
    }

    public static boolean canShow(Context context)
    {
        NotificationManager manager = context.getSystemService(NotificationManager.class);
        if (!manager.areNotificationsEnabled())
        {
            return false;
        }
        String channelId = channelId(new QuickEntryStore(context));
        NotificationChannel channel = channelId == null ? null : manager.getNotificationChannel(channelId);
        return channel == null || channel.getImportance() != NotificationManager.IMPORTANCE_NONE;
    }

    public static void setListening(boolean value)
    {
        listening = value;
    }

    public static synchronized String drain(Context context)
    {
        return new QuickEntryStore(context).drain();
    }

    static synchronized void handleTap(Context context, String action)
    {
        QuickEntryStore store = new QuickEntryStore(context);
        QuickEntryConfig config = config(store);
        if (config == null || !store.visible())
        {
            return;
        }

        QuickEntryModel model = new QuickEntryModel(config.modes, store.state());
        boolean committed = false;
        switch (action)
        {
            case ACTION_STEP_DOWN:
                model.stepDown();
                break;
            case ACTION_STEP_UP:
                model.stepUp();
                break;
            case ACTION_CYCLE_VARIANT:
                model.cycleVariant();
                break;
            case ACTION_SWITCH_MODE:
                model.switchMode();
                break;
            case ACTION_COMMIT:
                committed = true;
                break;
            default:
                return;
        }

        store.saveState(model.state());
        if (committed)
        {
            QuickEntryModel.Action entry = model.commit(System.currentTimeMillis());
            store.enqueue(entry);
            store.saveHeader(model.headerAfter(entry, timeText(context, entry.timeMillis)));
        }
        post(context, store);
        if (committed && listening)
        {
            nativeActionsAvailable();
        }
    }

    private static void post(Context context, QuickEntryStore store)
    {
        QuickEntryConfig config = config(store);
        if (config == null)
        {
            return;
        }
        QuickEntryModel model = new QuickEntryModel(config.modes, store.state());
        context.getSystemService(NotificationManager.class).notify(NOTIFICATION_TAG, NOTIFICATION_ID,
            QuickEntryView.build(context, config, model, store.header()));
    }

    private static QuickEntryConfig config(QuickEntryStore store)
    {
        try
        {
            return QuickEntryConfig.parse(store.config());
        }
        catch (JSONException missingOrInvalid)
        {
            return null;
        }
    }

    private static String channelId(QuickEntryStore store)
    {
        QuickEntryConfig config = config(store);
        return config == null ? null : config.channelId;
    }

    private static String timeText(Context context, long timeMillis)
    {
        return android.text.format.DateFormat.getTimeFormat(context).format(new Date(timeMillis));
    }

    private static native void nativeActionsAvailable();
}
