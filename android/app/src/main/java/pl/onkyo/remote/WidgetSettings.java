package pl.onkyo.remote;

import android.content.Context;
import android.content.SharedPreferences;

final class WidgetSettings {
    static final String DEFAULT_ENDPOINT = "http://192.168.1.46";

    private static SharedPreferences prefs(Context context) {
        return context.getSharedPreferences("widgets", Context.MODE_PRIVATE);
    }

    static String defaultEndpoint(Context context) {
        return prefs(context).getString("default_endpoint", DEFAULT_ENDPOINT);
    }

    static void saveDefault(Context context, String endpoint) {
        prefs(context).edit().putString("default_endpoint", endpoint).apply();
    }

    static String endpoint(Context context, int id) {
        return prefs(context).getString("endpoint_" + id, "");
    }

    static void save(Context context, int id, String endpoint) {
        prefs(context).edit().putString("endpoint_" + id, endpoint)
                .remove("status_" + id).apply();
    }

    static long generation(Context context, int id) {
        return prefs(context).getLong("generation_" + id, 0);
    }

    static long nextGeneration(Context context, int id) {
        long next = generation(context, id) + 1;
        prefs(context).edit().putLong("generation_" + id, next).apply();
        return next;
    }

    static String status(Context context, int id) {
        return prefs(context).getString("status_" + id, context.getString(R.string.ready));
    }

    static void saveStatus(Context context, int id, String status) {
        prefs(context).edit().putString("status_" + id, status).apply();
    }

    static void delete(Context context, int id) {
        prefs(context).edit().remove("endpoint_" + id).remove("status_" + id)
                .remove("generation_" + id).apply();
    }
}
