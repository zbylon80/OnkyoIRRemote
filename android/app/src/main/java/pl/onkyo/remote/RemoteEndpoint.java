package pl.onkyo.remote;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.Locale;

/** One device root, never a command URL or a URL containing credentials. */
final class RemoteEndpoint {
    static String normalize(String input) {
        if (input == null || input.trim().isEmpty()) throw new IllegalArgumentException();
        String value = input.trim();
        if (!value.contains("://")) value = "http://" + value;
        try {
            URI uri = new URI(value);
            String path = uri.getRawPath();
            int port = uri.getPort();
            if (!"http".equalsIgnoreCase(uri.getScheme()) || uri.getHost() == null
                    || uri.getRawUserInfo() != null || uri.getRawQuery() != null
                    || uri.getRawFragment() != null || port == 0 || port > 65535
                    || (path != null && !path.isEmpty() && !path.equals("/"))) {
                throw new IllegalArgumentException();
            }
            String host = uri.getHost().toLowerCase(Locale.ROOT);
            return "http://" + host + (port == -1 || port == 80 ? "" : ":" + port);
        } catch (URISyntaxException e) {
            throw new IllegalArgumentException(e);
        }
    }
}
