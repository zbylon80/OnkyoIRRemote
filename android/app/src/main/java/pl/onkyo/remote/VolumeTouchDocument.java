package pl.onkyo.remote;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;

/** A small Remote Compose document for Android 16's public DrawInstructions API.
 * Uses the baseline document API 6 format (header 0.3, accepted by 16 and 16 QPR).
 * The native XML supplies the key's appearance and accessible label; this layer
 * supplies down/up/cancel and immediate local pressed feedback. Feedback must
 * not wait for a service launch or HTTP acknowledgement.
 */
final class VolumeTouchDocument {
    static final int DOWN = 1001, UP = 1002, CANCEL = 1003;

    private static final int PRESSED = 50, FILL = 51, BORDER = 52, RADIUS = 53;
    private static final int WINDOW_WIDTH = 5, WINDOW_HEIGHT = 6, DENSITY = 27;

    static byte[] create() {
        try {
            ByteArrayOutputStream bytes = new ByteArrayOutputStream();
            DataOutputStream out = new DataOutputStream(bytes);
            out.writeByte(0); // Header
            out.writeInt(0); out.writeInt(3); out.writeInt(0);
            out.writeInt(400); out.writeInt(400); out.writeLong(0);
            out.writeByte(80); out.writeInt(PRESSED); out.writeFloat(0); // FloatConstant
            color(out, FILL, 0x00101010, 0xFF101010);
            color(out, BORDER, 0x00D1C8BA, 0xFFD1C8BA);
            out.writeByte(81); out.writeInt(RADIUS); out.writeInt(3); // FloatExpression: 6dp
            reference(out, DENSITY); out.writeFloat(6); reference(out, 0x310003); // multiply
            out.writeByte(200); out.writeInt(1); // RootLayout
            out.writeByte(202); // BoxLayout
            out.writeInt(2); out.writeInt(-1); out.writeInt(0); out.writeInt(0);
            dimension(out, 16); dimension(out, 67); // fill width/height
            action(out, 219, DOWN);
            action(out, 220, UP);
            action(out, 225, CANCEL);
            // Transparent at rest; the native XML retains its normal gradient and label.
            out.writeByte(173); // CanvasOperations: drawing inside a layout needs this container.
            paint(out, FILL, false);
            roundRect(out);
            paint(out, BORDER, true);
            roundRect(out);
            out.writeByte(214); // end canvas drawing
            out.writeByte(214); out.writeByte(214); // end box/root
            return bytes.toByteArray();
        } catch (IOException impossible) {
            throw new AssertionError(impossible);
        }
    }

    private static void reference(DataOutputStream out, int id) throws IOException {
        // writeFloat canonicalizes NaN and would erase the Remote Compose variable ID.
        out.writeInt(0xFF800000 | id);
    }

    private static void color(DataOutputStream out, int id, int rest, int pressed) throws IOException {
        out.writeByte(134); out.writeInt(id); out.writeInt(0); // ColorExpression, literal colors
        out.writeInt(rest); out.writeInt(pressed); reference(out, PRESSED);
    }

    private static void paint(DataOutputStream out, int color, boolean stroke) throws IOException {
        out.writeByte(40); out.writeInt(stroke ? 6 : 4); // PaintData bundle length
        out.writeInt(19); out.writeInt(color); // COLOR_ID
        out.writeInt(8 | (stroke ? 1 << 16 : 0)); // STYLE
        out.writeInt(14 | (1 << 16)); // ANTI_ALIAS
        if (stroke) { out.writeInt(5); reference(out, DENSITY); } // STROKE_WIDTH: 1dp
    }

    private static void roundRect(DataOutputStream out) throws IOException {
        out.writeByte(51); // DrawRoundRect, sized by this touch view after layout/resizing
        out.writeFloat(0); out.writeFloat(0);
        reference(out, WINDOW_WIDTH); reference(out, WINDOW_HEIGHT);
        reference(out, RADIUS); reference(out, RADIUS);
    }

    private static void dimension(DataOutputStream out, int operation) throws IOException {
        out.writeByte(operation); out.writeInt(1); out.writeFloat(0);
    }

    private static void action(DataOutputStream out, int operation, int action) throws IOException {
        out.writeByte(operation);
        out.writeByte(222); out.writeInt(PRESSED); out.writeFloat(action == DOWN ? 1 : 0);
        out.writeByte(209); out.writeInt(action); // HostAction -> PendingIntent
        out.writeByte(214); // end action list
    }
}
