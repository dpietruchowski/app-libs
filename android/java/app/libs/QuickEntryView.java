package app.libs;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Typeface;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.StyleSpan;
import android.view.View;
import android.widget.RemoteViews;

final class QuickEntryView
{
    private static final String MODE_SEPARATOR = " ⇄ ";

    private QuickEntryView()
    {
    }

    static Notification build(Context context, QuickEntryConfig config, QuickEntryModel model, String header)
    {
        createChannel(context, config);

        Notification.Builder builder = new Notification.Builder(context, config.channelId)
            .setSmallIcon(smallIcon(context, config.smallIcon))
            .setStyle(new Notification.DecoratedCustomViewStyle())
            .setCustomContentView(buttons(context, model))
            .setCategory(Notification.CATEGORY_STATUS)
            .setVisibility(Notification.VISIBILITY_PUBLIC)
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            .setShowWhen(false)
            .setLocalOnly(true);
        if (!header.isEmpty())
        {
            builder.setSubText(header);
        }
        PendingIntent open = openApp(context);
        if (open != null)
        {
            builder.setContentIntent(open);
        }
        return builder.build();
    }

    private static void createChannel(Context context, QuickEntryConfig config)
    {
        NotificationChannel channel = new NotificationChannel(config.channelId, config.channelName,
            NotificationManager.IMPORTANCE_LOW);
        channel.setShowBadge(false);
        channel.setSound(null, null);
        channel.enableVibration(false);
        channel.setLockscreenVisibility(Notification.VISIBILITY_PUBLIC);
        context.getSystemService(NotificationManager.class).createNotificationChannel(channel);
    }

    private static RemoteViews buttons(Context context, QuickEntryModel model)
    {
        Resources resources = context.getResources();
        String packageName = context.getPackageName();
        RemoteViews views
            = new RemoteViews(packageName, resources.getIdentifier("libs_quick_entry", "layout", packageName));

        int minus = viewId(context, "libs_quick_entry_minus");
        int value = viewId(context, "libs_quick_entry_value");
        int plus = viewId(context, "libs_quick_entry_plus");
        int variant = viewId(context, "libs_quick_entry_variant");
        int mode = viewId(context, "libs_quick_entry_mode");

        views.setTextViewText(value, model.valueText());
        views.setOnClickPendingIntent(minus, tap(context, QuickEntry.ACTION_STEP_DOWN));
        views.setOnClickPendingIntent(value, tap(context, QuickEntry.ACTION_COMMIT));
        views.setOnClickPendingIntent(plus, tap(context, QuickEntry.ACTION_STEP_UP));

        views.setViewVisibility(variant, model.hasVariants() ? View.VISIBLE : View.GONE);
        if (model.hasVariants())
        {
            views.setTextViewText(variant, model.variant().label);
            views.setOnClickPendingIntent(variant, tap(context, QuickEntry.ACTION_CYCLE_VARIANT));
        }

        views.setViewVisibility(mode, model.modes().size() > 1 ? View.VISIBLE : View.GONE);
        views.setTextViewText(mode, modeLabels(model));
        views.setOnClickPendingIntent(mode, tap(context, QuickEntry.ACTION_SWITCH_MODE));
        return views;
    }

    private static CharSequence modeLabels(QuickEntryModel model)
    {
        SpannableStringBuilder labels = new SpannableStringBuilder();
        for (int i = 0; i < model.modes().size(); i++)
        {
            if (i > 0)
            {
                labels.append(MODE_SEPARATOR);
            }
            int start = labels.length();
            labels.append(model.modes().get(i).label);
            if (i == model.modeIndex())
            {
                labels.setSpan(new StyleSpan(Typeface.BOLD), start, labels.length(),
                    Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            }
        }
        return labels;
    }

    private static int viewId(Context context, String name)
    {
        return context.getResources().getIdentifier(name, "id", context.getPackageName());
    }

    private static int smallIcon(Context context, String name)
    {
        int icon = context.getResources().getIdentifier(name, "drawable", context.getPackageName());
        return icon != 0 ? icon : context.getApplicationInfo().icon;
    }

    private static PendingIntent tap(Context context, String action)
    {
        Intent intent = new Intent(context, QuickEntryReceiver.class).setAction(action);
        return PendingIntent.getBroadcast(context, 0, intent,
            PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
    }

    private static PendingIntent openApp(Context context)
    {
        Intent launch = context.getPackageManager().getLaunchIntentForPackage(context.getPackageName());
        if (launch == null)
        {
            return null;
        }
        return PendingIntent.getActivity(context, 0, launch,
            PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
    }
}
