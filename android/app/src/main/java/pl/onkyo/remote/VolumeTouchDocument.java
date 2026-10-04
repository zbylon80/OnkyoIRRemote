package pl.onkyo.remote;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;

/** A small Remote Compose document for Android 16's public DrawInstructions API.
 * Uses the baseline document API 6 format (header 0.3, accepted by 16 and 16 QPR).
 * The native XML supplies the key's appearance and accessible label; this layer
 * supplies down/up/cancel, which ordinary RemoteViews buttons cannot deliver.
 */
final class VolumeTouchDocument {
    static final int DOWN = 1001, UP = 1002, CANCEL = 1003;

    static byte[] create() {
        try {
            ByteArrayOutputStream bytes = new ByteArrayOutputStream();
            DataOutputStream out = new DataOutputStream(bytes);
            out.writeByte(0); // Header
            out.writeInt(0); out.writeInt(3); out.writeInt(0);
            out.writeInt(400); out.writeInt(400); out.writeLong(0);
            out.writeByte(200); out.writeInt(1); // RootLayout
            out.writeByte(202); // BoxLayout
            out.writeInt(2); out.writeInt(-1); out.writeInt(0); out.writeInt(0);
            dimension(out, 16); dimension(out, 67); // fill width/height
            action(out, 219, DOWN);
            action(out, 220, UP);
            action(out, 225, CANCEL);
            out.writeByte(214); out.writeByte(214); // end box/root
            return bytes.toByteArray();
        } catch (IOException impossible) {
            throw new AssertionError(impossible);
        }
    }

    private static void dimension(DataOutputStream out, int operation) throws IOException {
        out.writeByte(operation); out.writeInt(1); out.writeFloat(0);
    }

    private static void action(DataOutputStream out, int operation, int action) throws IOException {
        out.writeByte(operation);
        out.writeByte(209); out.writeInt(action); // HostAction -> PendingIntent
        out.writeByte(214); // end action list
    }
}
