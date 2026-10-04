package pl.onkyo.remote;

import android.app.Activity;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;

/** Debug-only host surface for instrumentation and screenshot inspection. */
public final class WidgetTestActivity extends Activity {
    private FrameLayout root;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        root = new FrameLayout(this);
        root.setBackgroundColor(0xFFA9A7A2);
        setContentView(root);
    }

    void show(View view, int widthDp, int heightDp) {
        root.removeAllViews();
        float density = getResources().getDisplayMetrics().density;
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                Math.round(widthDp * density), Math.round(heightDp * density), Gravity.CENTER);
        root.addView(view, params);
    }
}
