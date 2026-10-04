package pl.onkyo.remote.touchprobe;

import android.app.Activity;
import android.appwidget.AppWidgetManager;
import android.content.ComponentName;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;

public final class ProbeHostActivity extends Activity {
    private FrameLayout root;
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        root = new FrameLayout(this);
        root.setBackgroundColor(0xFFAAAAAA);
        setContentView(root);
    }
    void show(View view) {
        int size = (int) (240 * getResources().getDisplayMetrics().density);
        root.addView(view, new FrameLayout.LayoutParams(size, size, Gravity.CENTER));
    }
    void pin() {
        AppWidgetManager.getInstance(this).requestPinAppWidget(new ComponentName(this, ProbeProvider.class), null, null);
    }
}
