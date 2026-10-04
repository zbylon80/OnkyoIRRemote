package pl.onkyo.remote;

import android.app.Instrumentation;
import android.appwidget.AppWidgetHost;
import android.appwidget.AppWidgetHostView;
import android.appwidget.AppWidgetManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/** No external testing libraries. Exercises widget clicks against a local fake ESP32. */
public final class WidgetSmokeTestRunner extends Instrumentation {
    // Expected behavior from the firmware Basic panel, independent of provider wiring.
    private static final int[] BASIC_BUTTONS = {R.id.power, R.id.volume_down, R.id.volume_up,
            R.id.mute, R.id.tape, R.id.cd, R.id.phono, R.id.tuner, R.id.video, R.id.previous, R.id.next};
    private static final String[] BASIC_COMMANDS = {"POWER", "VOL-", "VOL+", "MUTE", "TAPE-1",
            "CD", "PHONO", "TUNER", "VIDEO-1", "PRESET-", "PRESET+"};
    private final List<String> requests = new ArrayList<>();
    private volatile int delayMs;
    private volatile int responseCode = 200;
    private volatile String responseBody = "{\"ok\":true}";
    private ServerSocket server;
    private Context target;
    private AppWidgetHost host;
    private AppWidgetHostView hostView;
    private WidgetTestActivity activity;
    private int widgetId;
    private String endpoint;
    private String screenshotPath;
    private String smallScreenshotPath;

    @Override public void onCreate(Bundle args) {
        super.onCreate(args);
        screenshotPath = args.getString("screenshot");
        smallScreenshotPath = args.getString("smallScreenshot");
        start();
    }

    @Override public void onStart() {
        Bundle result = new Bundle();
        try {
            target = getTargetContext();
            target.getExternalFilesDir(null); // Create the app-owned screenshot directory.
            validateAddresses();
            startServer();
            endpoint = "http://127.0.0.1:" + server.getLocalPort();
            require(RemoteClient.version(endpoint).equals("{\"version\":\"test\"}"), "Version check");
            require(requestCount() == 1 && requestAt(0).equals("GET /version "), "Check sent an IR command");
            setupHost();
            checkLayout(250, 500);
            if (smallScreenshotPath != null) {
                try (java.io.FileOutputStream output = new java.io.FileOutputStream(smallScreenshotPath)) {
                    getUiAutomation().takeScreenshot().compress(android.graphics.Bitmap.CompressFormat.PNG, 100, output);
                }
            }
            checkLayout(320, 550);

            for (int i = 0; i < BASIC_BUTTONS.length; i++) {
                final int button = BASIC_BUTTONS[i];
                int expected = requestCount() + 1;
                ui(() -> require(hostView.findViewById(button).performClick(), "Button not clickable"));
                awaitRequests(expected);
                String encoded = java.net.URLEncoder.encode(BASIC_COMMANDS[i], "UTF-8");
                require(requestAt(expected - 1).equals("POST /command name=" + encoded), "Incorrect command mapping");
                awaitStatus("ESP32 przyjęło " + BASIC_COMMANDS[i]);
                require(requestCount() == expected, "Duplicate command");
            }

            // Slow connection: a second tap must be discarded, never queued.
            delayMs = 700;
            int expected = requestCount() + 1;
            long tapGeneration = WidgetSettings.generation(target, widgetId);
            ui(() -> hostView.findViewById(R.id.power).performClick());
            awaitRequests(expected);
            target.sendBroadcast(new Intent(target, CommandReceiver.class)
                    .setAction(CommandReceiver.ACTION_COMMAND)
                    .addFlags(Intent.FLAG_RECEIVER_FOREGROUND)
                    .putExtra(AppWidgetManager.EXTRA_APPWIDGET_ID, widgetId)
                    .putExtra(CommandReceiver.EXTRA_GENERATION, tapGeneration)
                    .putExtra(CommandReceiver.EXTRA_COMMAND, "VOL+"));
            awaitStatus("ESP32 przyjęło POWER");
            Thread.sleep(250);
            require(requestCount() == expected, "Slow connection queued commands");
            delayMs = 0;

            // HTTP errors and invalid acknowledgements must be shown as unconfirmed, without retries.
            responseCode = 500;
            clickAndExpectFailure(R.id.mute, "MUTE");
            responseCode = 200;
            responseBody = "{\"ok\":false}";
            clickAndExpectFailure(R.id.cd, "CD");
            responseBody = "{\"ok\":true}";

            // A read timeout must release the gate so the following tap can succeed.
            delayMs = 2600;
            clickAndExpectFailure(R.id.volume_up, "VOL+");
            Thread.sleep(800);
            delayMs = 0;
            int afterTimeout = requestCount() + 1;
            ui(() -> hostView.findViewById(R.id.tuner).performClick());
            awaitRequests(afterTimeout);
            awaitStatus("ESP32 przyjęło TUNER");
            require(requestCount() == afterTimeout, "Timeout retried the command");

            if (screenshotPath != null) {
                waitForIdleSync();
                Thread.sleep(150); // Let the completed widget update reach the display frame.
                try (java.io.FileOutputStream output = new java.io.FileOutputStream(screenshotPath)) {
                    getUiAutomation().takeScreenshot().compress(android.graphics.Bitmap.CompressFormat.PNG, 100, output);
                }
            }
            result.putString("stream", "PASS: 11 Basic button mappings; minimum/resized layout; read-only check; "
                    + "concurrent tap dropped; HTTP/JSON errors; timeout and recovery; no retry.\n");
            finish(ActivityResult.OK, result);
        } catch (Throwable failure) {
            result.putString("stream", "FAIL: " + android.util.Log.getStackTraceString(failure));
            finish(ActivityResult.FAIL, result);
        } finally {
            if (host != null) { host.stopListening(); host.deleteHost(); }
            if (target != null && widgetId != 0) WidgetSettings.delete(target, widgetId);
            if (server != null) try { server.close(); } catch (Exception ignored) { }
            if (activity != null) ui(() -> activity.finish());
        }
    }

    private void ui(Runnable action) {
        Throwable[] failure = {null};
        super.runOnMainSync(() -> {
            try { action.run(); } catch (Throwable error) { failure[0] = error; }
        });
        if (failure[0] != null) throw new AssertionError(failure[0]);
    }

    private void validateAddresses() {
        require(RemoteEndpoint.normalize("192.168.1.46/").equals("http://192.168.1.46"), "IP normalization");
        require(RemoteEndpoint.normalize("HTTP://Onkyo-Remote.local:80").equals("http://onkyo-remote.local"), "Hostname normalization");
        for (String value : new String[]{"", "https://192.168.1.46", "http://a:b@device", "http://device/command",
                "http://device?name=POWER", "http://device#secret", "http://device:0", "http://device:99999"}) {
            try { RemoteEndpoint.normalize(value); throw new AssertionError("Accepted invalid address: " + value); }
            catch (IllegalArgumentException expected) { }
        }
    }

    private void startServer() throws Exception {
        server = new ServerSocket(0, 10, java.net.InetAddress.getByName("127.0.0.1"));
        new Thread(() -> {
            while (!server.isClosed()) {
                try (Socket socket = server.accept()) {
                    BufferedReader in = new BufferedReader(new InputStreamReader(socket.getInputStream(), StandardCharsets.UTF_8));
                    String first = in.readLine();
                    int length = 0;
                    String header;
                    while ((header = in.readLine()) != null && !header.isEmpty()) {
                        if (header.toLowerCase(java.util.Locale.ROOT).startsWith("content-length:")) {
                            length = Integer.parseInt(header.substring(15).trim());
                        }
                    }
                    char[] body = new char[length];
                    int read = 0;
                    while (read < length) {
                        int count = in.read(body, read, length - read);
                        if (count == -1) break;
                        read += count;
                    }
                    synchronized (requests) { requests.add(first.substring(0, first.lastIndexOf(' ')) + " " + new String(body)); }
                    if (delayMs > 0) Thread.sleep(delayMs);
                    String reply = first.startsWith("GET /version ") ? "{\"version\":\"test\"}" : responseBody;
                    byte[] bytes = reply.getBytes(StandardCharsets.UTF_8);
                    socket.getOutputStream().write(("HTTP/1.1 " + responseCode + " Test\r\nContent-Type: application/json\r\nContent-Length: "
                            + bytes.length + "\r\nConnection: close\r\n\r\n").getBytes(StandardCharsets.US_ASCII));
                    socket.getOutputStream().write(bytes);
                } catch (Exception ignored) { /* A client timing out is an expected scenario. */ }
            }
        }, "fake-esp32").start();
    }

    private void setupHost() {
        host = new AppWidgetHost(target, 209);
        widgetId = host.allocateAppWidgetId();
        AppWidgetManager manager = AppWidgetManager.getInstance(target);
        require(manager.bindAppWidgetIdIfAllowed(widgetId, new ComponentName(target, OnkyoWidgetProvider.class)),
                "Run: adb shell appwidget grantbind --package pl.onkyo.remote");
        WidgetSettings.save(target, widgetId, endpoint);
        Intent launch = new Intent(target, WidgetTestActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        activity = (WidgetTestActivity) startActivitySync(launch);
        ui(() -> {
            host.startListening();
            hostView = host.createView(activity, widgetId, manager.getAppWidgetInfo(widgetId));
            OnkyoWidgetProvider.update(target, widgetId);
        });
        waitForIdleSync();
    }

    private void checkLayout(int width, int height) throws Exception {
        ui(() -> activity.show(hostView, width, height));
        Thread.sleep(300);
        waitForIdleSync();
        ui(() -> {
            float density = activity.getResources().getDisplayMetrics().density;
            for (int id : BASIC_BUTTONS) {
                View button = hostView.findViewById(id);
                require(button != null && button.isShown(), "Missing/hidden Basic button");
                require(button.getWidth() / density >= 48 && button.getHeight() / density >= 48,
                        "Button smaller than 48dp: " + target.getResources().getResourceEntryName(id)
                        + " " + button.getWidth() / density + "x" + button.getHeight() / density);
            }
        });
    }

    private void clickAndExpectFailure(int button, String command) throws Exception {
        int expected = requestCount() + 1;
        ui(() -> hostView.findViewById(button).performClick());
        awaitRequests(expected);
        awaitStatus("Brak potwierdzenia · " + command);
        require(requestCount() == expected, "Failed request retried");
    }

    private void awaitStatus(String prefix) throws Exception {
        long limit = android.os.SystemClock.elapsedRealtime() + 5000;
        while (android.os.SystemClock.elapsedRealtime() < limit) {
            final boolean[] done = {false};
            ui(() -> done[0] = ((TextView) hostView.findViewById(R.id.status)).getText().toString().startsWith(prefix)
                    && hostView.findViewById(R.id.power).isEnabled());
            if (done[0]) return;
            Thread.sleep(40);
        }
        String[] actual = {""};
        ui(() -> actual[0] = ((TextView) hostView.findViewById(R.id.status)).getText().toString());
        throw new AssertionError("Widget status did not become: " + prefix + "; actual: " + actual[0]
                + "; HTTP requests: " + requests);
    }

    private void awaitRequests(int count) throws Exception {
        long limit = android.os.SystemClock.elapsedRealtime() + 5000;
        while (requestCount() < count && android.os.SystemClock.elapsedRealtime() < limit) Thread.sleep(20);
        require(requestCount() == count, "Missing or extra HTTP request");
    }

    private int requestCount() { synchronized (requests) { return requests.size(); } }
    private String requestAt(int index) { synchronized (requests) { return requests.get(index); } }
    private static void require(boolean condition, String message) { if (!condition) throw new AssertionError(message); }
    private static final class ActivityResult { static final int OK = -1; static final int FAIL = 0; }
}
