package pl.onkyo.remote;

import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProviderInfo;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/** The pin API does not launch configuration: apply the address captured before pinning. */
public final class PinWidgetReceiver extends BroadcastReceiver {
    static final String ACTION = "pl.onkyo.remote.PINNED", EXTRA_ENDPOINT = "endpoint";
    @Override public void onReceive(Context context, Intent intent) {
        if (!ACTION.equals(intent.getAction())) return;
        int id = intent.getIntExtra(AppWidgetManager.EXTRA_APPWIDGET_ID, -1);
        AppWidgetProviderInfo info = AppWidgetManager.getInstance(context).getAppWidgetInfo(id);
        if (info == null || !info.provider.getClassName().equals(OnkyoWidgetProvider.class.getName())
                || !info.provider.getPackageName().equals(context.getPackageName())) return;
        try {
            String endpoint = RemoteEndpoint.normalize(intent.getStringExtra(EXTRA_ENDPOINT));
            WidgetSettings.save(context, id, endpoint);
            OnkyoWidgetProvider.update(context, id);
        } catch (IllegalArgumentException ignored) { }
    }
}
