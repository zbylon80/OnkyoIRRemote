package pl.onkyo.remote.touchprobe;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;

/** Experimental protocol fixture from Android SDK 36 sources. Not a production widget writer. */
final class ProbeDocument {
    static byte[] create() {
        try {
            ByteArrayOutputStream bytes = new ByteArrayOutputStream();
            DataOutputStream out = new DataOutputStream(bytes);
            // Protocol 0.3 maps to baseline document API 6; compatible with 16 and its QPRs.
            out.writeByte(0); out.writeInt(0); out.writeInt(3); out.writeInt(0);
            out.writeInt(400); out.writeInt(400); out.writeLong(0);
            out.writeByte(40); out.writeInt(2); out.writeInt(4); out.writeInt(0xFF0066CC);
            out.writeByte(42); for (float value : new float[]{0, 0, 400, 400}) out.writeFloat(value);
            out.writeByte(200); out.writeInt(1); // root layout
            out.writeByte(202); out.writeInt(2); out.writeInt(-1); out.writeInt(0); out.writeInt(0); // box
            dimension(out, 16); dimension(out, 67); // fill width/height
            out.writeByte(55); // background modifier
            for (float value : new float[]{0, 0, 400, 400, .2f, .44f, .7f, 1}) out.writeFloat(value);
            out.writeInt(0); // rectangle
            action(out, 219, 1001); // touch down
            action(out, 220, 1002); // touch up
            action(out, 225, 1003); // touch cancel
            out.writeByte(214); // end box container
            out.writeByte(214); // end root container
            out.flush();
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
        out.writeByte(209); out.writeInt(action); // host action routed to a PendingIntent
        out.writeByte(214); // end action list
    }
}
