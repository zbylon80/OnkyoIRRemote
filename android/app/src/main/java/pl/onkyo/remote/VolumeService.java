package pl.onkyo.remote;

import android.app.Service;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProviderInfo;
import android.content.Intent;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.SystemClock;
import org.json.JSONObject;
import java.math.BigInteger;
import java.text.DateFormat;
import java.util.Date;

/** A short, user-triggered session. Never restarted automatically after process death. */
public final class VolumeService extends Service {
    static final String ACTION = "pl.onkyo.remote.VOLUME_TOUCH";
    static final String EXTRA_EVENT = "touch_event", EXTRA_DIRECTION = "direction";
    static final long MAX_HOLD_MS = 3000;
    private final Handler main = new Handler(Looper.getMainLooper());
    private Gesture current;

    private static final class Gesture {
        final int widget;
        final long generation, began = SystemClock.elapsedRealtime();
        final String endpoint, direction;
        volatile boolean held = true;
        Gesture(int widget, long generation, String endpoint, String direction) {
            this.widget = widget; this.generation = generation;
            this.endpoint = endpoint; this.direction = direction;
        }
    }

    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        NotificationManager notifications = getSystemService(NotificationManager.class);
        notifications.createNotificationChannel(new NotificationChannel("volume", getString(R.string.volume_service), NotificationManager.IMPORTANCE_LOW));
        // Required for reliable widget interactions when the app has been idle or killed.
        startForeground(7, new Notification.Builder(this, "volume").setSmallIcon(R.drawable.ic_remote)
                .setContentTitle(getString(R.string.volume_service)).setContentText(getString(R.string.volume_service_note))
                .setOngoing(true).setOnlyAlertOnce(true).build());
        if (intent == null || !ACTION.equals(intent.getAction())) { idle(); return START_NOT_STICKY; }
        int widget = intent.getIntExtra(AppWidgetManager.EXTRA_APPWIDGET_ID, -1);
        long generation = intent.getLongExtra(CommandReceiver.EXTRA_GENERATION, -1);
        String direction = intent.getStringExtra(EXTRA_DIRECTION);
        int event = intent.getIntExtra(EXTRA_EVENT, -1);
        if (event == VolumeTouchDocument.UP || event == VolumeTouchDocument.CANCEL) {
            if (current != null && current.widget == widget && current.generation == generation
                    && current.direction.equals(direction)) current.held = false;
            idle(); return START_NOT_STICKY;
        }
        if (event != VolumeTouchDocument.DOWN || !("up".equals(direction) || "down".equals(direction))
                || current != null || generation != WidgetSettings.generation(this, widget)) {
            idle(); return START_NOT_STICKY;
        }
        AppWidgetProviderInfo info = AppWidgetManager.getInstance(this).getAppWidgetInfo(widget);
        if (info == null || !info.provider.getClassName().equals(OnkyoWidgetProvider.class.getName())
                || !info.provider.getPackageName().equals(getPackageName())) { idle(); return START_NOT_STICKY; }
        String endpoint = WidgetSettings.endpoint(this, widget);
        if (endpoint.isEmpty() || !DeviceGate.acquire(endpoint)) { idle(); return START_NOT_STICKY; }
        Gesture gesture = new Gesture(widget, generation, endpoint, direction);
        current = gesture;
        new Thread(() -> runGesture(gesture), "onkyo-volume").start();
        return START_NOT_STICKY;
    }

    private void runGesture(Gesture gesture) {
        String session = null;
        boolean confirmed = false;
        String command = gesture.direction.equals("up") ? "VOL+" : "VOL-";
        try {
            JSONObject response = acknowledged(RemoteClient.volume(gesture.endpoint, "press", gesture.direction));
            Object token = response.opt("session");
            if (!(token instanceof String) || !((String) token).matches("[1-9][0-9]{0,19}")
                    || new BigInteger((String) token).bitLength() > 64) throw new IllegalStateException("Invalid session");
            session = (String) token;
            confirmed = true;
            while (held(gesture) && SystemClock.elapsedRealtime() - gesture.began < 350) Thread.sleep(10);
            if (held(gesture)) {
                acknowledged(RemoteClient.volume(gesture.endpoint, "start", session));
                long lastSignal = SystemClock.elapsedRealtime();
                while (held(gesture)) {
                    Thread.sleep(10);
                    if (held(gesture) && SystemClock.elapsedRealtime() - lastSignal >= 500) {
                        acknowledged(RemoteClient.volume(gesture.endpoint, "keepalive", session));
                        lastSignal = SystemClock.elapsedRealtime();
                    }
                }
            }
        } catch (Exception failure) {
            confirmed = false; // Ambiguous responses must not be retried.
        } finally {
            gesture.held = false;
            if (session != null) {
                try { acknowledged(RemoteClient.volume(gesture.endpoint, "stop", session)); }
                catch (Exception failure) { confirmed = false; }
            }
            boolean success = confirmed;
            main.post(() -> {
                if (current != gesture) return;
                current = null;
                DeviceGate.release(gesture.endpoint);
                String status = success ? getString(R.string.sent, command,
                        DateFormat.getTimeInstance(DateFormat.SHORT).format(new Date())) : getString(R.string.failed, command);
                OnkyoWidgetProvider.updateDevice(this, gesture.endpoint, status, false);
                stopSelf();
            });
        }
    }

    private static JSONObject acknowledged(String body) throws Exception {
        JSONObject response = new JSONObject(body);
        if (!Boolean.TRUE.equals(response.opt("ok"))) throw new IllegalStateException("Unconfirmed request");
        return response;
    }
    private static boolean held(Gesture gesture) {
        return gesture.held && SystemClock.elapsedRealtime() - gesture.began < MAX_HOLD_MS;
    }
    private void idle() { if (current == null) stopSelf(); }
    @Override public IBinder onBind(Intent intent) { return null; }
    @Override public void onDestroy() {
        if (current != null) current.held = false;
        stopForeground(STOP_FOREGROUND_REMOVE);
        super.onDestroy();
    }
}
