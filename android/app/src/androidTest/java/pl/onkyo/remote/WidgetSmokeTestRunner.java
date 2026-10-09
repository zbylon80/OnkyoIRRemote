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
    private String alarmScreenshotPath;
    private long touchDown;
    private long actionGeneration;

    @Override public void onCreate(Bundle args) {
        super.onCreate(args);
        screenshotPath = args.getString("screenshot");
        smallScreenshotPath = args.getString("smallScreenshot");
        alarmScreenshotPath = args.getString("alarmScreenshot");
        start();
    }

    @Override public void onStart() {
        Bundle result = new Bundle();
        try {
            cancelTestTouch();
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
                boolean volume = android.os.Build.VERSION.SDK_INT >= 36 && (button == R.id.volume_up || button == R.id.volume_down);
                int before = requestCount();
                int expected = before + (volume ? 2 : 1);
                click(button);
                awaitRequests(expected);
                String encoded = java.net.URLEncoder.encode(BASIC_COMMANDS[i], "UTF-8");
                if (volume) {
                    require(requestAt(before).equals("POST /volume/press direction=" + (button == R.id.volume_up ? "up" : "down")), "Incorrect volume direction");
                    require(requestAt(before + 1).equals("POST /volume/stop session=123"), "Tap started a repeat");
                } else require(requestAt(before).equals("POST /command name=" + encoded), "Incorrect command mapping");
                awaitStatus("ESP32 przyjęło " + BASIC_COMMANDS[i]);
                require(requestCount() == expected, "Duplicate command");
            }

            checkAlarmKeyFeedback();
            if (android.os.Build.VERSION.SDK_INT >= 36) checkVolumeFeedback();

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

            if (android.os.Build.VERSION.SDK_INT >= 36) checkHolds();

            if (screenshotPath != null) {
                waitForIdleSync();
                Thread.sleep(150); // Let the completed widget update reach the display frame.
                try (java.io.FileOutputStream output = new java.io.FileOutputStream(screenshotPath)) {
                    getUiAutomation().takeScreenshot().compress(android.graphics.Bitmap.CompressFormat.PNG, 100, output);
                }
            }
            checkAlarms();
            result.putString("stream", "PASS: 11 Basic button mappings; minimum/resized layout; read-only check; "
                    + "concurrent tap dropped; HTTP/JSON errors; timeout and recovery; no retry; "
                    + "local volume pressed feedback; unchanged alarm during commands; hold/release both directions; release before acknowledgement; cancel; 3s cap; invalid session; "
                    + "native alarm panel; 24-hour selectors; independent Set/Cancel; preserved drafts; NTP/error guards; no alarm write retries.\n");
        } catch (Throwable failure) {
            result.putString("stream", "FAIL: " + android.util.Log.getStackTraceString(failure));
        } finally {
            cancelTestTouch();
            if (host != null) { host.stopListening(); host.deleteHost(); }
            if (target != null && widgetId != 0) WidgetSettings.delete(target, widgetId);
            if (server != null) try { server.close(); } catch (Exception ignored) { }
            if (activity != null) ui(() -> activity.finish());
        }
        finish(result.getString("stream", "").startsWith("PASS:") ? ActivityResult.OK : ActivityResult.FAIL, result);
    }

    private void ui(Runnable action) {
        Throwable[] failure = {null};
        super.runOnMainSync(() -> {
            try { action.run(); } catch (Throwable error) { failure[0] = error; }
        });
        if (failure[0] != null) throw new AssertionError(failure[0]);
    }

    private static String alarmResponse(boolean enabled, boolean clockReady) {
        return "{\"clockReady\":"+clockReady+",\"storageReady\":true,\"localTime\":\"2026-10-07 12:00:00\","+
            "\"on\":{\"enabled\":"+enabled+",\"time\":\"23:59\",\"date\":\"2026-10-08\",\"source\":\"CD\"},"+
            "\"off\":{\"enabled\":false,\"time\":\"02:00\",\"date\":\"\"}}";
    }
    private View find(View root, String value, boolean description) {
        if(description?value.contentEquals(root.getContentDescription()==null?"":root.getContentDescription())
            :root instanceof TextView&&value.contentEquals(((TextView)root).getText()))return root;
        if(root instanceof android.view.ViewGroup){
            android.view.ViewGroup group=(android.view.ViewGroup)root;
            for(int i=0;i<group.getChildCount();i++){View match=find(group.getChildAt(i),value,description);if(match!=null)return match;}
        }
        return null;
    }
    private void awaitAlarmText(AlarmActivity panel,String value) throws Exception {
        long end=System.currentTimeMillis()+6000;
        while(System.currentTimeMillis()<end){
            boolean[] found={false};ui(()->found[0]=find(panel.getWindow().getDecorView(),value,false)!=null);
            if(found[0])return;Thread.sleep(20);
        }
        throw new AssertionError("Alarm panel did not show: "+value);
    }
    private void checkAlarms() throws Exception {
        responseCode=200;delayMs=0;responseBody=alarmResponse(false,true);
        ActivityMonitor monitor=addMonitor(AlarmActivity.class.getName(),null,false);
        int before=requestCount();
        ui(()->hostView.findViewById(R.id.schedule).performClick());
        AlarmActivity panel=(AlarmActivity)waitForMonitorWithTimeout(monitor,5000);
        removeMonitor(monitor);require(panel!=null,"Clock opened browser instead of native activity");
        try{
            awaitAlarmText(panel,"Nie ustawiono.");awaitRequests(before+1);
            require(requestAt(before).equals("GET /alarms "),"Panel open wrote a schedule");
            View root=panel.getWindow().getDecorView();
            AlarmTimeButton onTime=(AlarmTimeButton)find(root,"Godzina włączenia: 23:59",true);
            AlarmTimeButton offTime=(AlarmTimeButton)find(root,"Godzina wyłączenia: 02:00",true);
            require(onTime!=null&&offTime!=null,"Custom time controls missing");
            checkTimeEditor(onTime);
            AlarmSourceButton source=(AlarmSourceButton)find(root,"Źródło pobudki: CD",true);
            int beforePicker=requestCount();ui(source::performClick);
            View sourceRoot=dialogRoot(source);
            if(alarmScreenshotPath!=null) screenshot(alarmScreenshotPath.replace("alarm.png","source-picker.png"));
            ui(()->find(sourceRoot,"Wybierz źródło PHONO",true).performClick());require(source.getValue().equals("PHONO"),"Source picker selection");
            ui(()->{source.setValue("CD");onTime.setTime("23:59");offTime.setTime("00:30");});
            require(requestCount()==beforePicker,"Source picker wrote a schedule");
            responseBody=alarmResponse(true,true);before=requestCount();
            ui(()->find(root,"Ustaw",false).performClick());awaitAlarmText(panel,"Ustawione. Możesz wrócić do widżetu.");
            require(requestCount()==before+1&&requestAt(before).equals("POST /alarms action=on&onTime=23%3A59&source=CD"),"Native wake Set form");
            require(offTime.getTime().equals("00:30"),"Wake save lost off draft");
            ui(()->{
                for(AlarmTimeButton selector:new AlarmTimeButton[]{onTime,offTime}){
                    require(selector.getWidth()-selector.getCompoundPaddingLeft()-selector.getCompoundPaddingRight()>=selector.getPaint().measureText(selector.getText().toString()),"Time digits clipped");
                }
            });
            responseBody=alarmResponse(true,true).replace("\"off\":{\"enabled\":false,\"time\":\"02:00\",\"date\":\"\"}","\"off\":{\"enabled\":true,\"time\":\"00:30\",\"date\":\"2026-10-08\"}");
            before=requestCount();ui(()->find(root,"Ustaw wyłączenie",true).performClick());awaitRequests(before+1);
            awaitAlarmText(panel,"Ustawiono: 08.10 o 00:30.");
            require(requestAt(before).equals("POST /alarms action=off&offTime=00%3A30"),"Native off Set form");
            require(onTime.getTime().equals("23:59"),"Off save changed wake draft");
            if(alarmScreenshotPath!=null){waitForIdleSync();try(java.io.FileOutputStream out=new java.io.FileOutputStream(alarmScreenshotPath)){
                getUiAutomation().takeScreenshot().compress(android.graphics.Bitmap.CompressFormat.PNG,100,out);
            }}
            responseBody=alarmResponse(true,true);before=requestCount();ui(()->find(root,"Anuluj wyłączenie",true).performClick());
            awaitAlarmText(panel,"Anulowano.");require(requestAt(before).equals("POST /alarms action=cancelOff"),"Independent off Cancel form");
            responseBody=alarmResponse(true,false);ui(()->find(root,"Odśwież",false).performClick());
            awaitAlarmText(panel,"Zegar ESP32 czeka na synchronizację.");
            require(!find(root,"Ustaw",false).isEnabled()&&find(root,"Anuluj włączenie",true).isEnabled(),"NTP guards incorrect");
            responseBody=alarmResponse(false,false);before=requestCount();ui(()->find(root,"Anuluj włączenie",true).performClick());
            awaitAlarmText(panel,"Anulowano.");require(requestAt(before).equals("POST /alarms action=cancelOn"),"Cancel form");
            responseCode=500;responseBody="{\"error\":\"test alarm error\"}";before=requestCount();
            ui(()->find(root,"Odśwież",false).performClick());
            awaitAlarmText(panel,"Nie można odczytać budzika. test alarm error Odśwież ustawienia.");
            require(requestCount()==before+1&&!find(root,"Ustaw",false).isEnabled()&&find(root,"Odśwież",false).isEnabled(),"Unknown result retried or left writes enabled");
            responseCode=200;responseBody=alarmResponse(false,true);ui(()->find(root,"Odśwież",false).performClick());awaitAlarmText(panel,"Nie ustawiono.");
            checkAlarmRecreation(panel);
        }finally{ui(panel::finish);responseCode=200;responseBody="{\"ok\":true}";}
    }

    private View dialogRoot(Object picker) throws Exception {
        java.lang.reflect.Field field=picker.getClass().getDeclaredField("editor");field.setAccessible(true);
        android.app.Dialog dialog=(android.app.Dialog)field.get(picker);require(dialog!=null&&dialog.isShowing(),"Picker dialog not shown");
        return dialog.getWindow().getDecorView();
    }
    private void checkAlarmRecreation(AlarmActivity panel) throws Exception {
        View root=panel.getWindow().getDecorView();
        AlarmTimeButton on=(AlarmTimeButton)find(root,"Godzina włączenia: 23:59",true);
        AlarmTimeButton off=(AlarmTimeButton)find(root,"Godzina wyłączenia: 02:00",true);
        AlarmSourceButton source=(AlarmSourceButton)find(root,"Źródło pobudki: CD",true);
        long readyDeadline=System.currentTimeMillis()+6000;
        while(System.currentTimeMillis()<readyDeadline){boolean[] ready={false};ui(()->ready[0]=on.isEnabled());if(ready[0])break;Thread.sleep(20);}
        require(on.isEnabled(),"Refresh did not finish before recreation check");
        ui(()->{on.setTime("07:45");off.setTime("02:10");source.setValue("PHONO");on.performClick();});
        View draft=dialogRoot(on);ui(()->{((android.widget.EditText)find(draft,"Godzina, od 00 do 23",true)).setText("8");((android.widget.EditText)find(draft,"Minuta, od 00 do 59",true)).setText("3");});
        ActivityMonitor monitor=addMonitor(AlarmActivity.class.getName(),null,false);int before=requestCount();ui(panel::recreate);
        AlarmActivity restored=(AlarmActivity)waitForMonitorWithTimeout(monitor,5000);removeMonitor(monitor);require(restored!=null,"Activity was not recreated");
        try{
            awaitAlarmText(restored,"Nie ustawiono.");waitForIdleSync();View restoredRoot=restored.getWindow().getDecorView();
            AlarmTimeButton restoredOn=(AlarmTimeButton)find(restoredRoot,"Godzina włączenia: 07:45",true);
            require(restoredOn!=null&&find(restoredRoot,"Godzina wyłączenia: 02:10",true)!=null&&find(restoredRoot,"Źródło pobudki: PHONO",true)!=null,"Recreation lost alarm drafts");
            View restoredEditor=dialogRoot(restoredOn);
            require(((android.widget.EditText)find(restoredEditor,"Godzina, od 00 do 23",true)).getText().toString().equals("8")&&((android.widget.EditText)find(restoredEditor,"Minuta, od 00 do 59",true)).getText().toString().equals("3"),"Recreation lost open editor input");
            ui(()->find(restoredEditor,"Anuluj",false).performClick());require(restoredOn.getTime().equals("07:45"),"Recreated editor Cancel changed time");
            awaitRequests(before+1);require(requestCount()==before+1&&requestAt(before).equals("GET /alarms "),"Recreation wrote a schedule");
        }finally{ui(restored::finish);}
    }
    private void screenshot(String path) throws Exception {
        waitForIdleSync();Thread.sleep(150);
        try(java.io.FileOutputStream output=new java.io.FileOutputStream(path)){
            getUiAutomation().takeScreenshot().compress(android.graphics.Bitmap.CompressFormat.PNG,100,output);
        }
    }
    private void checkTimeEditor(AlarmTimeButton time) throws Exception {
        int before=requestCount();ui(time::performClick);View root=dialogRoot(time);
        android.widget.EditText hour=(android.widget.EditText)find(root,"Godzina, od 00 do 23",true);
        android.widget.EditText minute=(android.widget.EditText)find(root,"Minuta, od 00 do 59",true);
        ui(()->{find(root,"Zwiększ godzinę",true).performClick();find(root,"Zwiększ minutę",true).performClick();});
        require(hour.getText().toString().equals("00")&&minute.getText().toString().equals("00"),"Down arrows must advance and wrap");
        ui(()->{find(root,"Zmniejsz godzinę",true).performClick();find(root,"Zmniejsz minutę",true).performClick();hour.setText("24");minute.setText("60");find(root,"Gotowe",false).performClick();});
        require(time.getTime().equals("23:59")&&find(root,"Wpisz godzinę 00–23 i minuty 00–59.",false)!=null,"Invalid time committed");
        ui(()->{hour.setText("4");minute.setText("5");find(root,"Gotowe",false).performClick();});
        require(time.getTime().equals("04:05"),"Typed digits not normalized");
        ui(time::performClick);View cancelRoot=dialogRoot(time);
        ui(()->{find(cancelRoot,"Zwiększ godzinę",true).performClick();find(cancelRoot,"Anuluj",false).performClick();});
        require(time.getTime().equals("04:05"),"Cancel changed local time");
        ui(()->{time.setTime("07:00");time.performClick();});
        if(alarmScreenshotPath!=null)screenshot(alarmScreenshotPath.replace("alarm.png","time-editor.png"));
        ui(time::dismissEditor);require(requestCount()==before,"Editing time sent an HTTP request");
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
        server = new ServerSocket(8989, 10, java.net.InetAddress.getByName("127.0.0.1"));
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
                    String reply = first.startsWith("GET /version ") ? "{\"version\":\"test\"}" :
                            first.startsWith("POST /volume/press ") && responseBody.equals("{\"ok\":true}")
                                    ? "{\"ok\":true,\"session\":\"123\"}" : responseBody;
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
        host.deleteHost();
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
                View button = key(id);
                require(button != null && button.isShown(), "Missing/hidden Basic button");
                require(button.getWidth() / density >= 48 && button.getHeight() / density >= 48,
                        "Button smaller than 48dp: " + target.getResources().getResourceEntryName(id)
                        + " " + button.getWidth() / density + "x" + button.getHeight() / density);
            }
        });
    }

    private void clickAndExpectFailure(int button, String command) throws Exception {
        int expected = requestCount() + 1;
        click(button);
        awaitRequests(expected);
        awaitStatus("Brak potwierdzenia · " + command);
        require(requestCount() == expected, "Failed request retried");
    }

    private void awaitStatus(String prefix) throws Exception {
        long limit = android.os.SystemClock.elapsedRealtime() + 5000;
        while (android.os.SystemClock.elapsedRealtime() < limit) {
            final boolean[] done = {false};
            ui(() -> done[0] = ((TextView) hostView.findViewById(R.id.status)).getText().toString().startsWith(prefix)
                    && hostView.findViewById(R.id.power).isEnabled()
                    && WidgetSettings.generation(target, widgetId) > actionGeneration);
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

    private void cancelTestTouch() {
        long now = android.os.SystemClock.uptimeMillis();
        android.view.MotionEvent event = android.view.MotionEvent.obtain(now, now, android.view.MotionEvent.ACTION_CANCEL, 0, 0, 0);
        event.setSource(android.view.InputDevice.SOURCE_TOUCHSCREEN);
        getUiAutomation().injectInputEvent(event, true); event.recycle();
    }
    private View key(int id) {
        return hostView.findViewById(android.os.Build.VERSION.SDK_INT >= 36 && id == R.id.volume_up ? R.id.volume_up_slot
                : android.os.Build.VERSION.SDK_INT >= 36 && id == R.id.volume_down ? R.id.volume_down_slot : id);
    }
    private void click(int id) throws Exception {
        actionGeneration = WidgetSettings.generation(target, widgetId);
        if (android.os.Build.VERSION.SDK_INT >= 36 && (id == R.id.volume_up || id == R.id.volume_down)) {
            touch(id, android.view.MotionEvent.ACTION_DOWN);
            Thread.sleep(60);
            touch(id, android.view.MotionEvent.ACTION_UP);
        } else ui(() -> require(key(id).performClick(), "Button not clickable"));
    }
    private void touch(int id, int action) {
        int[] point = new int[2];
        ui(() -> { View key = key(id); key.getLocationOnScreen(point); point[0] += key.getWidth()/2; point[1] += key.getHeight()/2; });
        if (action == android.view.MotionEvent.ACTION_DOWN) {
            actionGeneration = WidgetSettings.generation(target, widgetId);
            touchDown = android.os.SystemClock.uptimeMillis();
        }
        android.view.MotionEvent event = android.view.MotionEvent.obtain(touchDown, android.os.SystemClock.uptimeMillis(), action, point[0], point[1], 0);
        event.setSource(android.view.InputDevice.SOURCE_TOUCHSCREEN);
        require(getUiAutomation().injectInputEvent(event, true), "Could not inject touch"); event.recycle();
    }
    private android.graphics.Bitmap appearance(int id) {
        android.graphics.Bitmap[] image = {null};
        ui(() -> {
            View view = key(id);
            image[0] = android.graphics.Bitmap.createBitmap(view.getWidth(), view.getHeight(), android.graphics.Bitmap.Config.ARGB_8888);
            view.draw(new android.graphics.Canvas(image[0]));
        });
        return image[0];
    }
    private void checkAlarmKeyFeedback() throws Exception {
        android.graphics.Bitmap idle = appearance(R.id.schedule);
        delayMs = 700;
        int before = requestCount();
        click(R.id.mute); awaitRequests(before + 1);
        ui(() -> require(!key(R.id.schedule).isEnabled(), "Busy alarm key was not guarded"));
        require(idle.sameAs(appearance(R.id.schedule)), "Unrelated command changed alarm appearance");
        awaitStatus("ESP32 przyjęło MUTE"); delayMs = 0;
        require(idle.sameAs(appearance(R.id.schedule)), "Alarm changed after command completion");
        ui(() -> key(R.id.schedule).setPressed(true));
        require(!idle.sameAs(appearance(R.id.schedule)), "Alarm does not react to its own press");
        ui(() -> key(R.id.schedule).setPressed(false));
        require(idle.sameAs(appearance(R.id.schedule)), "Alarm stayed pressed");
        idle.recycle();
    }
    private void checkVolumeFeedback() throws Exception {
        for (int id : new int[]{R.id.volume_down, R.id.volume_up}) {
            for (int release : new int[]{android.view.MotionEvent.ACTION_UP, android.view.MotionEvent.ACTION_CANCEL}) {
                android.graphics.Bitmap idle = appearance(id);
                android.graphics.Bitmap alarm = appearance(R.id.schedule);
                // Feedback must be local even while the network response is still delayed.
                delayMs = 250;
                touch(id, android.view.MotionEvent.ACTION_DOWN);
                Thread.sleep(80);
                if (screenshotPath != null) screenshot(screenshotPath.replace("widget.png", "feedback-debug.png"));
                require(!idle.sameAs(appearance(id)), "Volume has no immediate pressed feedback");
                require(alarm.sameAs(appearance(R.id.schedule)), "Volume press changed alarm appearance");
                if (release == android.view.MotionEvent.ACTION_UP && screenshotPath != null) {
                    screenshot(screenshotPath.replace("widget.png", id == R.id.volume_up ? "volume-up-pressed.png" : "volume-down-pressed.png"));
                }
                touch(id, release); Thread.sleep(80);
                require(idle.sameAs(appearance(id)), "Volume remained pressed after release/cancel");
                awaitStatus("ESP32 przyjęło " + (id == R.id.volume_up ? "VOL+" : "VOL-"));
                delayMs = 0; idle.recycle(); alarm.recycle();
            }
        }
    }
    private void checkHolds() throws Exception {
        for (int key : new int[]{R.id.volume_down, R.id.volume_up}) {
            int before = requestCount();
            touch(key, android.view.MotionEvent.ACTION_DOWN);
            awaitRequests(before + 1);
            Thread.sleep(220);
            require(requestCount() == before + 1, "Repeat began before hold threshold");
            Thread.sleep(800);
            require(requestAt(before + 1).equals("POST /volume/start session=123"), "Hold did not start");
            require(requestAt(before + 2).equals("POST /volume/keepalive session=123"), "No keepalive");
            ui(() -> hostView.findViewById(R.id.power).performClick());
            Thread.sleep(80);
            require(!requestAt(requestCount() - 1).startsWith("POST /command"), "Source command overlapped a hold");
            touch(key, android.view.MotionEvent.ACTION_UP);
            awaitStatus("ESP32 przyjęło " + (key == R.id.volume_up ? "VOL+" : "VOL-"));
            require(requestAt(requestCount() - 1).equals("POST /volume/stop session=123"), "Release did not stop");
            int stopped = requestCount(); Thread.sleep(600);
            require(requestCount() == stopped, "Keepalive continued after release");
        }
        // Release reaches the service while the press acknowledgement is still in flight.
        delayMs = 250;
        int before = requestCount(); click(R.id.volume_up);
        awaitStatus("ESP32 przyjęło VOL+"); delayMs = 0;
        require(requestCount() == before + 2 && requestAt(before + 1).contains("/volume/stop"), "Delayed press started after release");

        before = requestCount(); touch(R.id.volume_down, android.view.MotionEvent.ACTION_DOWN);
        awaitRequests(before + 1); touch(R.id.volume_down, android.view.MotionEvent.ACTION_CANCEL);
        awaitStatus("ESP32 przyjęło VOL-");
        require(requestCount() == before + 2 && requestAt(before + 1).contains("/volume/stop"), "Cancel did not stop");

        touch(R.id.volume_up, android.view.MotionEvent.ACTION_DOWN);
        Thread.sleep(3300); awaitStatus("ESP32 przyjęło VOL+");
        require(requestAt(requestCount() - 1).contains("/volume/stop"), "Three-second cap did not stop");
        int capped = requestCount(); touch(R.id.volume_up, android.view.MotionEvent.ACTION_UP); Thread.sleep(600);
        require(requestCount() == capped, "Cap restarted a held key");

        responseBody = "{\"ok\":true,\"session\":\"18446744073709551616\"}";
        clickAndExpectFailure(R.id.volume_up, "VOL+"); responseBody = "{\"ok\":true}";
    }
}
