package pl.onkyo.remote;

import org.json.JSONException;
import org.json.JSONObject;
import java.time.LocalDate;
import java.time.format.DateTimeParseException;
import java.util.Arrays;

final class AlarmSnapshot {
    static final String[] SOURCES = {"TUNER", "CD", "PHONO", "TAPE-1", "TAPE-2", "VIDEO-1", "VIDEO-2"};
    final boolean clockReady, storageReady;
    final String localTime, source;
    final Slot on, off;

    static final class Slot {
        final boolean enabled;
        final String time, date;
        Slot(JSONObject value) throws JSONException {
            enabled = flag(value, "enabled");
            time = value.getString("time"); date = value.getString("date");
            if (!validTime(time) || (!date.isEmpty() && !validDate(date)) || (enabled && date.isEmpty()))
                throw new JSONException("Nieprawidłowy harmonogram ESP32.");
        }
        String status() { return enabled ? "Ustawiono: " + date.substring(8,10) + "." + date.substring(5,7) + " o " + time + "." : "Nie ustawiono."; }
    }

    AlarmSnapshot(String response) throws JSONException {
        JSONObject data = new JSONObject(response);
        clockReady = flag(data, "clockReady"); storageReady = flag(data, "storageReady");
        localTime = data.getString("localTime");
        on = new Slot(data.getJSONObject("on")); off = new Slot(data.getJSONObject("off"));
        source = data.getJSONObject("on").getString("source");
        if (localTime.length() > 40 || !Arrays.asList(SOURCES).contains(source)) throw new JSONException("Nieprawidłowa odpowiedź ESP32.");
    }
    private static boolean flag(JSONObject data, String name) throws JSONException {
        Object value = data.get(name);
        if (!(value instanceof Boolean)) throw new JSONException("Nieprawidłowa odpowiedź ESP32.");
        return (Boolean) value;
    }
    static boolean validTime(String value) {
        return value != null && value.matches("(?:[01][0-9]|2[0-3]):[0-5][0-9]");
    }
    private static boolean validDate(String value) {
        try { return value.length() == 10 && LocalDate.parse(value).toString().equals(value); }
        catch (DateTimeParseException e) { return false; }
    }
}
