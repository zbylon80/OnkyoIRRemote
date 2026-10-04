package pl.onkyo.remote.touchprobe;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;
import android.content.Context;
import android.content.Intent;
import android.widget.RemoteViews;
import java.util.Collections;

public final class ProbeProvider extends AppWidgetProvider {
    @Override public void onUpdate(Context context, AppWidgetManager manager, int[] ids) {
        for (int id : ids) manager.updateAppWidget(id, views(context));
    }

    static RemoteViews views(Context context) {
        RemoteViews.DrawInstructions instructions = new RemoteViews.DrawInstructions.Builder(
                Collections.singletonList(ProbeDocument.create())).build();
        RemoteViews views = new RemoteViews(instructions);
        for (int id : new int[]{1001, 1002, 1003}) {
            Intent event = new Intent(context, ProbeReceiver.class).setAction("touch." + id)
                    .addFlags(Intent.FLAG_RECEIVER_FOREGROUND).putExtra("event", id);
            views.setOnClickPendingIntent(id, PendingIntent.getBroadcast(context, id, event,
                    PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE));
        }
        return views;
    }
}
