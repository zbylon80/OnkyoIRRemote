package pl.onkyo.remote;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;
import android.content.Context;
import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.widget.RemoteViews;

public final class OnkyoWidgetProvider extends AppWidgetProvider {
    static final int[] BUTTONS = {R.id.power, R.id.volume_down, R.id.volume_up, R.id.mute,
            R.id.tape, R.id.cd, R.id.phono, R.id.tuner, R.id.video, R.id.previous, R.id.next};
    static final String[] COMMANDS = {"POWER", "VOL-", "VOL+", "MUTE", "TAPE-1", "CD",
            "PHONO", "TUNER", "VIDEO-1", "PRESET-", "PRESET+"};

    @Override public void onUpdate(Context context, AppWidgetManager manager, int[] ids) {
        for (int id : ids) update(context, id);
    }

    @Override public void onAppWidgetOptionsChanged(Context context, AppWidgetManager manager,
                                                   int id, Bundle options) {
        update(context, id);
    }

    @Override public void onDeleted(Context context, int[] ids) {
        for (int id : ids) WidgetSettings.delete(context, id);
    }

    static void update(Context context, int id) {
        update(context, id, WidgetSettings.status(context, id), false);
    }

    static synchronized void update(Context context, int id, String status, boolean busy) {
        AppWidgetManager.getInstance(context).updateAppWidget(id, views(context, id, status, busy));
    }

    static void updateDevice(Context context, String endpoint, String status, boolean busy) {
        int[] ids = AppWidgetManager.getInstance(context).getAppWidgetIds(
                new ComponentName(context, OnkyoWidgetProvider.class));
        for (int id : ids) {
            if (endpoint.equals(WidgetSettings.endpoint(context, id))) {
                if (!busy) WidgetSettings.saveStatus(context, id, status);
                update(context, id, status, busy);
            }
        }
    }

    static RemoteViews views(Context context, int id, String status, boolean busy) {
        RemoteViews views = new RemoteViews(context.getPackageName(), R.layout.widget_basic);
        boolean configured = !WidgetSettings.endpoint(context, id).isEmpty();
        long generation = WidgetSettings.nextGeneration(context, id);
        for (int i = 0; i < BUTTONS.length; i++) {
            Intent command = new Intent(context, CommandReceiver.class)
                    .setAction(CommandReceiver.ACTION_COMMAND)
                    .addFlags(Intent.FLAG_RECEIVER_FOREGROUND)
                    .setData(Uri.parse("onkyo://widget/" + id + "/" + Uri.encode(COMMANDS[i])))
                    .putExtra(AppWidgetManager.EXTRA_APPWIDGET_ID, id)
                    .putExtra(CommandReceiver.EXTRA_GENERATION, generation)
                    .putExtra(CommandReceiver.EXTRA_COMMAND, COMMANDS[i]);
            views.setOnClickPendingIntent(BUTTONS[i], PendingIntent.getBroadcast(context, 0, command,
                    PendingIntent.FLAG_CANCEL_CURRENT | PendingIntent.FLAG_IMMUTABLE));
            views.setBoolean(BUTTONS[i], "setEnabled", configured && !busy);
        }
        Intent configure = new Intent(context, ConfigureActivity.class)
                .setData(Uri.parse("onkyo://configure/" + id))
                .putExtra(AppWidgetManager.EXTRA_APPWIDGET_ID, id);
        views.setOnClickPendingIntent(R.id.settings, PendingIntent.getActivity(context, 0, configure,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE));
        views.setTextViewText(R.id.status, configured ? status : context.getString(R.string.not_configured));
        return views;
    }
}
