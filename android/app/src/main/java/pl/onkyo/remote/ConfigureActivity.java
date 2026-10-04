package pl.onkyo.remote;

import android.app.Activity;
import android.appwidget.AppWidgetManager;
import android.content.ComponentName;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;
import org.json.JSONObject;

public final class ConfigureActivity extends Activity {
    private int widgetId;
    private EditText address;
    private TextView result;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        setResult(RESULT_CANCELED);
        widgetId = getIntent().getIntExtra(AppWidgetManager.EXTRA_APPWIDGET_ID,
                AppWidgetManager.INVALID_APPWIDGET_ID);
        setContentView(R.layout.activity_configure);
        View root = findViewById(R.id.config_root);
        // Target SDK 36 uses edge-to-edge; keep fields clear of system bars and the keyboard.
        root.setOnApplyWindowInsetsListener((view, insets) -> {
            int bottom = insets.getSystemWindowInsetBottom();
            view.setPadding(insets.getSystemWindowInsetLeft(), insets.getSystemWindowInsetTop(),
                    insets.getSystemWindowInsetRight(), bottom);
            return insets;
        });
        address = findViewById(R.id.address);
        result = findViewById(R.id.check_result);
        String current = WidgetSettings.endpoint(this, widgetId);
        address.setText(current.isEmpty() ? WidgetSettings.defaultEndpoint(this) : current);
        Button save = findViewById(R.id.save);
        save.setText(widgetId == AppWidgetManager.INVALID_APPWIDGET_ID ? R.string.save_default : R.string.save_widget);
        save.setOnClickListener(view -> save());
        findViewById(R.id.check).setOnClickListener(view -> check());
        View add = findViewById(R.id.add_widget);
        add.setVisibility(widgetId == AppWidgetManager.INVALID_APPWIDGET_ID ? View.VISIBLE : View.GONE);
        add.setOnClickListener(view -> addWidget());
    }

    private String validatedEndpoint() {
        try {
            return RemoteEndpoint.normalize(address.getText().toString());
        } catch (IllegalArgumentException e) {
            address.setError(getString(R.string.invalid_address));
            return null;
        }
    }

    private void save() {
        String endpoint = validatedEndpoint();
        if (endpoint == null) return;
        WidgetSettings.saveDefault(this, endpoint);
        if (widgetId != AppWidgetManager.INVALID_APPWIDGET_ID) {
            WidgetSettings.save(this, widgetId, endpoint);
            OnkyoWidgetProvider.update(this, widgetId);
            setResult(RESULT_OK, new Intent().putExtra(AppWidgetManager.EXTRA_APPWIDGET_ID, widgetId));
            finish();
        } else {
            Toast.makeText(this, R.string.saved, Toast.LENGTH_SHORT).show();
        }
    }

    private void addWidget() {
        String endpoint = validatedEndpoint();
        if (endpoint == null) return;
        WidgetSettings.saveDefault(this, endpoint);
        AppWidgetManager manager = AppWidgetManager.getInstance(this);
        if (manager.isRequestPinAppWidgetSupported()) {
            Intent pinned = new Intent(this, PinWidgetReceiver.class).setAction(PinWidgetReceiver.ACTION)
                    .setData(android.net.Uri.parse("onkyo://pin/" + java.util.UUID.randomUUID()))
                    .putExtra(PinWidgetReceiver.EXTRA_ENDPOINT, endpoint);
            // Launcher supplies EXTRA_APPWIDGET_ID only after the widget has been allocated.
            android.app.PendingIntent callback = android.app.PendingIntent.getBroadcast(this, 0, pinned,
                    android.app.PendingIntent.FLAG_ONE_SHOT | android.app.PendingIntent.FLAG_MUTABLE);
            manager.requestPinAppWidget(new ComponentName(this, OnkyoWidgetProvider.class), null, callback);
        } else {
            result.setText(R.string.manual_add);
        }
    }

    private void check() {
        String endpoint = validatedEndpoint();
        if (endpoint == null) return;
        Button check = findViewById(R.id.check);
        check.setEnabled(false);
        result.setText(R.string.checking);
        new Thread(() -> {
            String message;
            try {
                JSONObject response = new JSONObject(RemoteClient.version(endpoint));
                String version = response.getString("version");
                if (version.isEmpty() || version.length() > 40) throw new IllegalArgumentException();
                message = getString(R.string.connection_ok, version);
            } catch (Exception e) {
                message = getString(R.string.connection_failed);
            }
            String finalMessage = message;
            runOnUiThread(() -> {
                if (isFinishing() || isDestroyed()) return;
                check.setEnabled(true);
                // Do not label a newly edited address using an older connection check.
                try {
                    if (endpoint.equals(RemoteEndpoint.normalize(address.getText().toString()))) {
                        result.setText(finalMessage);
                    } else {
                        result.setText("");
                    }
                } catch (IllegalArgumentException e) {
                    result.setText("");
                }
            });
        }, "onkyo-check").start();
    }
}
