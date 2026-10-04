package pl.onkyo.remote.touchprobe;

import android.app.Instrumentation;
import android.appwidget.AppWidgetHost;
import android.appwidget.AppWidgetHostView;
import android.appwidget.AppWidgetManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import java.io.FileOutputStream;

public final class TouchProbeRunner extends Instrumentation {
    private Context target;
    private ProbeHostActivity activity;
    private AppWidgetHost host;
    private AppWidgetHostView view;
    private final StringBuilder report = new StringBuilder();
    private boolean production;
    @Override public void onCreate(Bundle args) { super.onCreate(args); production = "true".equals(args.getString("production")); start(); }

    @Override public void onStart() {
        Bundle result = new Bundle();
        try {
            target = getTargetContext();
            if (production) {
                report.append(new ProductionVolumeChecks(this).run());
            } else {
            host = new AppWidgetHost(target, 216);
            int id = host.allocateAppWidgetId();
            AppWidgetManager manager = AppWidgetManager.getInstance(target);
            require(manager.bindAppWidgetIdIfAllowed(id, new ComponentName(target, ProbeProvider.class)), "Grant widget binding first");
            activity = (ProbeHostActivity) startActivitySync(new Intent(target, ProbeHostActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
            ui(() -> {
                host.startListening();
                view = host.createView(activity, id, manager.getAppWidgetInfo(id));
                activity.show(view);
                manager.updateAppWidget(id, ProbeProvider.views(target));
            });
            Thread.sleep(600);
            waitForIdleSync();
            ui(() -> { view.setExecutor(null); view.updateAppWidget(ProbeProvider.views(target)); });
            Thread.sleep(200);
            ui(() -> report.append("Activity view tree: ").append(viewTree(view)).append('\n'));
            screenshot("before-touch");
            int[] location = new int[2];
            ui(() -> view.getLocationOnScreen(location));
            int x = location[0] + view.getWidth() / 2;
            int y = location[1] + view.getHeight() / 2;
            exercise("activity host", x, y);
            screenshot("activity");

            ui(() -> activity.pin());
            Thread.sleep(500);
            AccessibilityNodeInfo button = findText(getUiAutomation().getRootInActiveWindow(), "Add to home screen");
            if (button == null) button = findText(getUiAutomation().getRootInActiveWindow(), "Add");
            require(button != null, "Pin confirmation not found: " + tree(getUiAutomation().getRootInActiveWindow()));
            button.performAction(AccessibilityNodeInfo.ACTION_CLICK);
            Thread.sleep(400);
            getUiAutomation().executeShellCommand("input keyevent KEYCODE_HOME").close();
            Thread.sleep(1600);
            AccessibilityNodeInfo widget = findClass(getUiAutomation().getRootInActiveWindow(), "AppWidgetHostView");
            require(widget != null, "Pinned widget not found: " + tree(getUiAutomation().getRootInActiveWindow()));
            Rect bounds = new Rect(); widget.getBoundsInScreen(bounds);
            report.append("Launcher widget bounds: ").append(bounds).append('\n');
            exercise("launcher host", bounds.centerX(), bounds.centerY());
            screenshot("launcher");
            }
            result.putString("stream", "PASS\n" + report);
        } catch (Throwable error) {
            try { screenshot("failure"); } catch (Exception ignored) { }
            result.putString("stream", "FAIL\n" + report + android.util.Log.getStackTraceString(error));
        } finally {
            if (host != null) { host.stopListening(); host.deleteHost(); }
            if (activity != null) ui(() -> activity.finish());
        }
        finish(result.getString("stream", "").startsWith("PASS") ? -1 : 0, result);
    }

    private void exercise(String label, int x, int y) throws Exception {
        target.getSharedPreferences("probe", 0).edit().putString("events", "").commit();
        long down = SystemClock.uptimeMillis();
        inject(down, MotionEvent.ACTION_DOWN, x, y);
        Thread.sleep(100);
        report.append(label).append(" after press: ").append(events()).append('\n');
        Thread.sleep(1100);
        String held = events();
        report.append(label).append(" while held 1.2s: ").append(held).append('\n');
        inject(down, MotionEvent.ACTION_UP, x, y);
        Thread.sleep(200);
        report.append(label).append(" after release: ").append(events()).append('\n');
        require(held.startsWith("DOWN@") && !held.contains("UP@") && !held.contains("CANCEL@"),
                label + " did not retain a 1.2s hold");
        require(events().contains("UP@"), label + " did not report release");
    }

    private void inject(long down, int action, int x, int y) {
        MotionEvent event = MotionEvent.obtain(down, SystemClock.uptimeMillis(), action, x, y, 0);
        event.setSource(InputDevice.SOURCE_TOUCHSCREEN);
        require(getUiAutomation().injectInputEvent(event, true), "Input injection failed");
        event.recycle();
    }
    private String events() { return target.getSharedPreferences("probe", 0).getString("events", ""); }
    private void screenshot(String name) throws Exception {
        if (target == null) return;
        try (FileOutputStream out = new FileOutputStream(new java.io.File(target.getExternalFilesDir(null), "probe-" + name + ".png"))) {
            getUiAutomation().takeScreenshot().compress(android.graphics.Bitmap.CompressFormat.PNG, 100, out);
        }
    }
    private void ui(Runnable action) {
        Throwable[] failure = {null};
        runOnMainSync(() -> { try { action.run(); } catch (Throwable error) { failure[0] = error; } });
        if (failure[0] != null) throw new AssertionError(failure[0]);
    }
    private static AccessibilityNodeInfo findText(AccessibilityNodeInfo node, String text) {
        if (node == null) return null;
        if (text.contentEquals(node.getText() == null ? "" : node.getText())) return node;
        for (int i = 0; i < node.getChildCount(); i++) {
            AccessibilityNodeInfo found = findText(node.getChild(i), text); if (found != null) return found;
        }
        return null;
    }
    private static AccessibilityNodeInfo findClass(AccessibilityNodeInfo node, String text) {
        if (node == null) return null;
        Rect bounds = new Rect(); node.getBoundsInScreen(bounds);
        if (node.isVisibleToUser() && !bounds.isEmpty() && bounds.centerX() > 0
                && bounds.centerX() < targetWidth(node) && node.getClassName() != null
                && node.getClassName().toString().contains(text)
                && "Onkyo touch probe".contentEquals(node.getContentDescription() == null ? "" : node.getContentDescription())) return node;
        for (int i = 0; i < node.getChildCount(); i++) {
            AccessibilityNodeInfo found = findClass(node.getChild(i), text); if (found != null) return found;
        }
        return null;
    }
    private static int targetWidth(AccessibilityNodeInfo node) {
        AccessibilityNodeInfo root = node;
        while (root.getParent() != null) root = root.getParent();
        Rect bounds = new Rect(); root.getBoundsInScreen(bounds); return bounds.right;
    }
    private static String tree(AccessibilityNodeInfo node) {
        if (node == null) return "null";
        StringBuilder text = new StringBuilder().append(node.getClassName()).append(' ').append(node.getText()).append('\n');
        for (int i = 0; i < node.getChildCount(); i++) text.append(tree(node.getChild(i)));
        return text.toString();
    }
    private static String viewTree(android.view.View view) {
        StringBuilder text = new StringBuilder(view.getClass().getName()).append(' ')
                .append(view.getWidth()).append('x').append(view.getHeight()).append('\n');
        if (view instanceof android.view.ViewGroup group) {
            for (int i = 0; i < group.getChildCount(); i++) text.append(viewTree(group.getChildAt(i)));
        }
        return text.toString();
    }
    private static void require(boolean condition, String message) { if (!condition) throw new AssertionError(message); }
}
