package pl.onkyo.remote;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.Proxy;
import java.net.URL;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/** Local HTTP API. No automatic retries of IR commands or volume session requests. */
final class RemoteClient {
    static final Set<String> COMMANDS = Collections.unmodifiableSet(new HashSet<>(Arrays.asList(
            "POWER", "VOL-", "VOL+", "MUTE", "TAPE-1", "CD", "PHONO", "TUNER",
            "VIDEO-1", "PRESET-", "PRESET+")));

    static String command(String endpoint, String command) throws IOException {
        if (!COMMANDS.contains(command)) throw new IllegalArgumentException("Unknown Basic command");
        byte[] body = ("name=" + URLEncoder.encode(command, StandardCharsets.UTF_8.name()))
                .getBytes(StandardCharsets.UTF_8);
        return request(endpoint, "/command", body);
    }

    static String version(String endpoint) throws IOException {
        return request(endpoint, "/version", null);
    }

    static String alarms(String endpoint, String action, String time, String source) throws IOException {
        if (action == null) return request(endpoint, "/alarms", null);
        if (!Arrays.asList("on", "off", "cancelOn", "cancelOff").contains(action))
            throw new IllegalArgumentException("Nieznana akcja budzika.");
        StringBuilder form = new StringBuilder("action=").append(action);
        if (action.equals("on") || action.equals("off")) {
            if (!AlarmSnapshot.validTime(time)) throw new IllegalArgumentException("Wybierz poprawną godzinę.");
            form.append('&').append(action).append("Time=")
                    .append(URLEncoder.encode(time, StandardCharsets.UTF_8.name()));
            if (action.equals("on")) {
                if (!Arrays.asList(AlarmSnapshot.SOURCES).contains(source)) throw new IllegalArgumentException("Wybierz źródło.");
                form.append("&source=").append(URLEncoder.encode(source, StandardCharsets.UTF_8.name()));
            }
        }
        return request(endpoint, "/alarms", form.toString().getBytes(StandardCharsets.UTF_8));
    }

    static String volume(String endpoint, String operation, String value) throws IOException {
        if (!Arrays.asList("press", "start", "keepalive", "stop").contains(operation)) {
            throw new IllegalArgumentException("Unknown volume operation");
        }
        String field = operation.equals("press") ? "direction" : "session";
        byte[] body = (field + "=" + URLEncoder.encode(value, StandardCharsets.UTF_8.name()))
                .getBytes(StandardCharsets.UTF_8);
        return request(endpoint, "/volume/" + operation, body);
    }

    private static String request(String endpoint, String path, byte[] body) throws IOException {
        HttpURLConnection connection = (HttpURLConnection) new URL(RemoteEndpoint.normalize(endpoint) + path)
                .openConnection(Proxy.NO_PROXY);
        int timeout = path.startsWith("/volume/") ? 400 : 2000;
        connection.setConnectTimeout(timeout);
        connection.setReadTimeout(timeout);
        connection.setUseCaches(false);
        connection.setInstanceFollowRedirects(false);
        connection.setRequestProperty("Accept", "application/json");
        connection.setRequestProperty("Connection", "close");
        long deadline = System.nanoTime() + 5_000_000_000L;
        try {
            if (body != null) {
                connection.setRequestMethod("POST");
                connection.setDoOutput(true);
                // Streaming mode prevents replaying a POST on redirects/authentication.
                connection.setFixedLengthStreamingMode(body.length);
                connection.setRequestProperty("Content-Type", "application/x-www-form-urlencoded");
                try (OutputStream out = connection.getOutputStream()) {
                    if (System.nanoTime() > deadline) throw new IOException("Request expired");
                    out.write(body);
                }
            }
            int status = connection.getResponseCode();
            if (status != 200 && !path.equals("/alarms")) throw new IOException("Device rejected request");
            InputStream response = status == 200 ? connection.getInputStream() : connection.getErrorStream();
            if (response == null) throw new IOException("Brak odpowiedzi ESP32.");
            try (InputStream in = response; ByteArrayOutputStream out = new ByteArrayOutputStream()) {
                byte[] buffer = new byte[256];
                int count;
                while ((count = in.read(buffer)) != -1) {
                    if (out.size() + count > 2048 || System.nanoTime() > deadline) {
                        throw new IOException("Invalid device response");
                    }
                    out.write(buffer, 0, count);
                }
                String result = out.toString(StandardCharsets.UTF_8.name());
                if (status != 200) {
                    String message = "Nie można odczytać budzika. Wymagane firmware 1.3.0 lub nowsze.";
                    try { message = new org.json.JSONObject(result).optString("error", message); }
                    catch (org.json.JSONException ignored) { }
                    throw new IOException(message);
                }
                return result;
            }
        } finally {
            connection.disconnect();
        }
    }
}
