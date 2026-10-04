package pl.onkyo.remote.touchprobe;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

public final class ProbeReceiver extends BroadcastReceiver {
    @Override public void onReceive(Context context, Intent intent) {
        String event = switch (intent.getIntExtra("event", -1)) {
            case 1001 -> "DOWN"; case 1002 -> "UP"; case 1003 -> "CANCEL"; default -> "UNKNOWN";
        };
        String history = context.getSharedPreferences("probe", 0).getString("events", "");
        context.getSharedPreferences("probe", 0).edit().putString("events", history + event + "@"
                + android.os.SystemClock.elapsedRealtime() + ";").apply();
    }
}
