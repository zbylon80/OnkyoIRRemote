package pl.onkyo.remote.touchprobe;

import android.app.Instrumentation;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import java.io.BufferedReader;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/** Separate UID from the production app: proves cold/background widget service launches. */
final class ProductionVolumeChecks {
    private final Instrumentation test;
    private final List<String> requests = new ArrayList<>();
    private ServerSocket server;
    private long down;
    private boolean fingerDown;
    private Rect lastKey = new Rect(0, 0, 2, 2);
    ProductionVolumeChecks(Instrumentation test) { this.test = test; }

    String run() throws Exception {
        cancelFinger(); // Also clear a touch left by an interrupted instrumentation run.
        server = new ServerSocket(8989, 10, java.net.InetAddress.getByName("127.0.0.1"));
        new Thread(this::serve, "launcher-fake-esp32").start();
        try {
            shell("am start -n pl.onkyo.remote/.ConfigureActivity");
            AccessibilityNodeInfo address = await("pl.onkyo.remote:id/address", false);
            Bundle text = new Bundle(); text.putCharSequence(AccessibilityNodeInfo.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE, "http://127.0.0.1:8989");
            require(address.performAction(AccessibilityNodeInfo.ACTION_SET_TEXT, text), "Cannot set test endpoint");
            await("pl.onkyo.remote:id/add_widget", false).performAction(AccessibilityNodeInfo.ACTION_CLICK);
            await("Add", true).performAction(AccessibilityNodeInfo.ACTION_CLICK);
            shell("input keyevent KEYCODE_HOME"); Thread.sleep(1600);
            screenshot("production-launcher");
            for (String direction : new String[]{"up", "down"}) {
                cold();
                Rect key = bounds(direction);
                int before = count();
                inject(MotionEvent.ACTION_DOWN, key);
                waitCount(before + 1);
                Thread.sleep(1150);
                List<String> held = snapshot(before);
                require(held.get(0).equals("/volume/press direction=" + direction), "Wrong direction: " + held);
                require(held.stream().anyMatch(s -> s.startsWith("/volume/start ")), "No hold start: " + held);
                require(held.stream().anyMatch(s -> s.startsWith("/volume/keepalive ")), "No keepalive: " + held);
                require(held.stream().noneMatch(s -> s.startsWith("/volume/stop ")), "Launcher cancelled hold: " + held);
                require(test.getUiAutomation().getRootInActiveWindow().getPackageName().toString().contains("launcher"), "Hold opened another window");
                inject(MotionEvent.ACTION_UP, key);
                waitStop(before); int stopped = count(); Thread.sleep(650);
                require(count() == stopped, "Requests continued after release");
                Thread.sleep(200);
            }
            cold();
            int before = count(); Rect key = bounds("up");
            inject(MotionEvent.ACTION_DOWN, key); Thread.sleep(60); inject(MotionEvent.ACTION_UP, key);
            waitStop(before); Thread.sleep(500);
            List<String> tap = snapshot(before);
            require(tap.size() == 2 && tap.get(0).equals("/volume/press direction=up")
                    && tap.get(1).startsWith("/volume/stop "), "Tap repeated: " + tap);
            screenshot("production-launcher");
            return "Production widget on Pixel Launcher: cold/idle app; VOL+/VOL- held 1.2s; "
                    + "keepalive only while held; release stops; tap is one step; no activity opened.\n" + snapshot(0) + "\n";
        } finally { if (fingerDown) cancelFinger(); server.close(); }
    }

    private void cold() throws Exception {
        shell("am make-uid-idle pl.onkyo.remote");
        shell("am kill pl.onkyo.remote");
        Thread.sleep(300);
        require(shell("pidof pl.onkyo.remote").trim().isEmpty(), "App remained running before cold widget interaction");
    }
    private Rect bounds(String direction) throws Exception {
        AccessibilityNodeInfo key = await(direction.equals("up") ? "pl.onkyo.remote:id/volume_up_slot" : "pl.onkyo.remote:id/volume_down_slot", false);
        Rect bounds = new Rect(); key.getBoundsInScreen(bounds);
        require(!bounds.isEmpty(), "Volume key off screen"); return bounds;
    }
    private void inject(int action, Rect key) {
        if (action == MotionEvent.ACTION_DOWN) down = SystemClock.uptimeMillis();
        MotionEvent event = MotionEvent.obtain(down, SystemClock.uptimeMillis(), action, key.centerX(), key.centerY(), 0);
        event.setSource(InputDevice.SOURCE_TOUCHSCREEN);
        require(test.getUiAutomation().injectInputEvent(event, true), "Touch injection failed"); event.recycle();
        lastKey = new Rect(key); fingerDown = action == MotionEvent.ACTION_DOWN;
    }
    private void cancelFinger() {
        MotionEvent event = MotionEvent.obtain(SystemClock.uptimeMillis(), SystemClock.uptimeMillis(), MotionEvent.ACTION_CANCEL, lastKey.centerX(), lastKey.centerY(), 0);
        event.setSource(InputDevice.SOURCE_TOUCHSCREEN);
        test.getUiAutomation().injectInputEvent(event, true); event.recycle(); fingerDown = false;
    }
    private AccessibilityNodeInfo await(String value, boolean byText) throws Exception {
        long end = SystemClock.elapsedRealtime() + 5000;
        do {
            AccessibilityNodeInfo node = find(test.getUiAutomation().getRootInActiveWindow(), value, byText);
            if (node != null) return node;
            Thread.sleep(100);
        } while (SystemClock.elapsedRealtime() < end);
        throw new AssertionError("UI control not found: " + value);
    }
    private AccessibilityNodeInfo find(AccessibilityNodeInfo node, String value, boolean byText) {
        if (node == null) return null;
        Rect bounds = new Rect(); node.getBoundsInScreen(bounds);
        boolean matches = byText ? value.contentEquals(node.getText() == null ? "" : node.getText())
                || value.equals("Add") && "Add to home screen".contentEquals(node.getText() == null ? "" : node.getText())
                : value.equals(node.getViewIdResourceName());
        if (matches && node.isVisibleToUser() && !bounds.isEmpty()) return node;
        for (int i=0; i<node.getChildCount(); i++) {
            AccessibilityNodeInfo found = find(node.getChild(i), value, byText); if (found != null) return found;
        }
        return null;
    }
    private String shell(String command) throws Exception {
        try (ParcelFileDescriptor descriptor = test.getUiAutomation().executeShellCommand(command);
             java.io.FileInputStream input = new java.io.FileInputStream(descriptor.getFileDescriptor())) {
            return new String(input.readAllBytes(), StandardCharsets.UTF_8);
        }
    }
    private void screenshot(String name) throws Exception {
        try (FileOutputStream output = new FileOutputStream(new java.io.File(test.getTargetContext().getExternalFilesDir(null), "probe-" + name + ".png"))) {
            test.getUiAutomation().takeScreenshot().compress(android.graphics.Bitmap.CompressFormat.PNG, 100, output);
        }
    }
    private int count() { synchronized (requests) { return requests.size(); } }
    private List<String> snapshot(int from) { synchronized (requests) { return new ArrayList<>(requests.subList(from, requests.size())); } }
    private void waitCount(int expected) throws Exception {
        long end = SystemClock.elapsedRealtime() + 3000;
        while(count() < expected && SystemClock.elapsedRealtime() < end) Thread.sleep(20);
        require(count() >= expected, "Missing HTTP request: " + snapshot(0));
    }
    private void waitStop(int from) throws Exception {
        long end = SystemClock.elapsedRealtime() + 2000;
        while(SystemClock.elapsedRealtime() < end) {
            List<String> calls = snapshot(from);
            if (!calls.isEmpty() && calls.get(calls.size()-1).startsWith("/volume/stop ")) return;
            Thread.sleep(20);
        }
        throw new AssertionError("Release did not stop: " + snapshot(from));
    }
    private void serve() {
        while (!server.isClosed()) {
            try (Socket socket = server.accept()) {
                BufferedReader in = new BufferedReader(new InputStreamReader(socket.getInputStream(), StandardCharsets.UTF_8));
                String first = in.readLine(), header; int length=0;
                while ((header=in.readLine()) != null && !header.isEmpty()) {
                    if (header.toLowerCase(java.util.Locale.ROOT).startsWith("content-length:")) length=Integer.parseInt(header.substring(15).trim());
                }
                char[] body = new char[length]; int read=0;
                while(read < length) { int n=in.read(body,read,length-read); if(n<0)break; read+=n; }
                String path=first.split(" ")[1];
                synchronized(requests) { requests.add(path + " " + new String(body)); }
                byte[] reply = (path.equals("/volume/press") ? "{\"ok\":true,\"session\":\"987\"}" : "{\"ok\":true}").getBytes(StandardCharsets.UTF_8);
                socket.getOutputStream().write(("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + reply.length + "\r\nConnection: close\r\n\r\n").getBytes(StandardCharsets.US_ASCII));
                socket.getOutputStream().write(reply);
            } catch(Exception ignored) { }
        }
    }
    private static void require(boolean condition, String message) { if(!condition)throw new AssertionError(message); }
}
