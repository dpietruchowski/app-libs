package app.libs;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Typeface;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.text.style.StyleSpan;
import android.view.View;
import android.widget.RemoteViews;

final class QuickEntryView
{
    private static final String MODE_SEPARATOR = " ⇄ ";
    private static final String COLLAPSED_LAYOUT = "libs_quick_entry";
    private static final String EXPANDED_LAYOUT = "libs_quick_entry_expanded";

    private QuickEntryView()
    {
    }

    static Notification build(Context context, QuickEntryConfig config, QuickEntryModel model, String header)
    {
        createChannel(context, config);

        PendingIntent open = openApp(context);
        QuickEntryViewState state
            = new QuickEntryViewState(model, open != null && context.getApplicationInfo().icon != 0);
        Notification.Builder builder = new Notification.Builder(context, config.channelId)
            .setSmallIcon(smallIcon(context, config.smallIcon))
            .setStyle(new Notification.DecoratedCustomViewStyle())
            .setCustomContentView(bind(context, COLLAPSED_LAYOUT, state, open))
            .setCustomBigContentView(bind(context, EXPANDED_LAYOUT, state, open))
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

    private static RemoteViews bind(Context context, String layout, QuickEntryViewState state,
                                    PendingIntent open)
    {
        String packageName = context.getPackageName();
        RemoteViews views
            = new RemoteViews(packageName, context.getResources().getIdentifier(layout, "layout", packageName));

        int minus = viewId(context, "libs_quick_entry_minus");
        int value = viewId(context, "libs_quick_entry_value");
        int plus = viewId(context, "libs_quick_entry_plus");
        int variant = viewId(context, "libs_quick_entry_variant");
        int mode = viewId(context, "libs_quick_entry_mode");

        views.setTextViewText(value, state.valueText);
        views.setOnClickPendingIntent(minus, tap(context, QuickEntry.ACTION_STEP_DOWN));
        views.setOnClickPendingIntent(value, tap(context, QuickEntry.ACTION_COMMIT));
        views.setOnClickPendingIntent(plus, tap(context, QuickEntry.ACTION_STEP_UP));
        paint(context, views, value, QuickEntryTone.solidButton(state.tone), QuickEntryTone.solidText(state.tone));
        paint(context, views, minus, QuickEntryTone.softButton(state.tone), QuickEntryTone.text(state.tone));
        paint(context, views, plus, QuickEntryTone.softButton(state.tone), QuickEntryTone.text(state.tone));

        views.setViewVisibility(variant, visibility(state.variantVisible));
        if (state.variantVisible)
        {
            views.setTextViewText(variant, state.variantLabel);
            views.setOnClickPendingIntent(variant, tap(context, QuickEntry.ACTION_CYCLE_VARIANT));
            paint(context, views, variant, QuickEntryTone.softButton(state.tone),
                QuickEntryTone.text(state.tone));
        }

        views.setViewVisibility(mode, visibility(state.modeVisible));
        views.setTextViewText(mode, modeLabels(context, state));
        views.setOnClickPendingIntent(mode, tap(context, QuickEntry.ACTION_SWITCH_MODE));

        views.setViewVisibility(viewId(context, "libs_quick_entry_mode_gap"), visibility(state.modeGapVisible));
        views.setViewVisibility(viewId(context, "libs_quick_entry_open_gap"), visibility(state.openGapVisible));
        bindOpen(context, views, state, open);
        return views;
    }

    private static void bindOpen(Context context, RemoteViews views, QuickEntryViewState state,
                                 PendingIntent open)
    {
        int button = viewId(context, "libs_quick_entry_open");
        views.setViewVisibility(button, visibility(state.openVisible));
        if (state.openVisible)
        {
            views.setImageViewResource(button, context.getApplicationInfo().icon);
            views.setContentDescription(button,
                context.getApplicationInfo().loadLabel(context.getPackageManager()));
            views.setOnClickPendingIntent(button, open);
        }
    }

    private static void paint(Context context, RemoteViews views, int viewId, String backgroundName,
                              String textColorName)
    {
        int background = resource(context, "drawable", backgroundName);
        if (background != 0)
        {
            views.setInt(viewId, "setBackgroundResource", background);
        }
        int textColor = resource(context, "color", textColorName);
        if (textColor != 0)
        {
            views.setTextColor(viewId, color(context, textColor));
        }
    }

    private static CharSequence modeLabels(Context context, QuickEntryViewState state)
    {
        SpannableStringBuilder labels = new SpannableStringBuilder();
        for (int i = 0; i < state.modeLabels.size(); i++)
        {
            if (i > 0)
            {
                labels.append(MODE_SEPARATOR);
            }
            int start = labels.length();
            labels.append(state.modeLabels.get(i));
            if (i == state.activeModeIndex)
            {
                labels.setSpan(new StyleSpan(Typeface.BOLD), start, labels.length(),
                    Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
                int textColor = resource(context, "color", QuickEntryTone.text(state.modeTones.get(i)));
                if (textColor != 0)
                {
                    labels.setSpan(new ForegroundColorSpan(color(context, textColor)), start, labels.length(),
                        Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
                }
            }
        }
        return labels;
    }

    private static int visibility(boolean visible)
    {
        return visible ? View.VISIBLE : View.GONE;
    }

    private static int color(Context context, int resource)
    {
        return context.getResources().getColor(resource, context.getTheme());
    }

    private static int resource(Context context, String type, String name)
    {
        return name.isEmpty() ? 0 : context.getResources().getIdentifier(name, type, context.getPackageName());
    }

    private static int viewId(Context context, String name)
    {
        return resource(context, "id", name);
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
