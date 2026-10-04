package pl.onkyo.remote;

import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProviderInfo;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import org.json.JSONObject;
import java.text.DateFormat;
import java.util.Date;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

public final class CommandReceiver extends BroadcastReceiver {
    static final String ACTION_COMMAND = "pl.onkyo.remote.COMMAND";
    static final String EXTRA_COMMAND = "command";
    static final String EXTRA_GENERATION = "generation";
    // Discard concurrent taps instead of queuing IR commands during a slow connection.
    private static final Set<String> BUSY = ConcurrentHashMap.newKeySet();

    @Override public void onReceive(Context context, Intent intent) {
        if (!ACTION_COMMAND.equals(intent.getAction())) return;
        int id = intent.getIntExtra(AppWidgetManager.EXTRA_APPWIDGET_ID, AppWidgetManager.INVALID_APPWIDGET_ID);
        String command = intent.getStringExtra(EXTRA_COMMAND);
        if (command == null || !RemoteClient.COMMANDS.contains(command)) return;
        AppWidgetProviderInfo info = AppWidgetManager.getInstance(context).getAppWidgetInfo(id);
        if (info == null || !info.provider.getClassName().equals(OnkyoWidgetProvider.class.getName())
                || !info.provider.getPackageName().equals(context.getPackageName())) return;
        // Android can defer another broadcast until the preceding goAsync() finishes.
        // Each rendered view carries a revision: discard taps sent from an older view.
        if (intent.getLongExtra(EXTRA_GENERATION, -1) != WidgetSettings.generation(context, id)) return;
        String endpoint = WidgetSettings.endpoint(context, id);
        if (endpoint.isEmpty() || !BUSY.add(endpoint)) return;
        PendingResult pending = goAsync();
        new Thread(() -> {
            try {
                OnkyoWidgetProvider.updateDevice(context, endpoint, context.getString(R.string.sending, command), true);
                String status;
                try {
                    JSONObject response = new JSONObject(RemoteClient.command(endpoint, command));
                    if (!Boolean.TRUE.equals(response.opt("ok"))) throw new IllegalStateException();
                    status = context.getString(R.string.sent, command,
                            DateFormat.getTimeInstance(DateFormat.SHORT).format(new Date()));
                } catch (Exception e) {
                    // A lost response can follow a successful IR send: never automatically retry.
                    status = context.getString(R.string.failed, command);
                }
                OnkyoWidgetProvider.updateDevice(context, endpoint, status, false);
            } finally {
                BUSY.remove(endpoint);
                pending.finish();
            }
        }, "onkyo-command").start();
    }
}
