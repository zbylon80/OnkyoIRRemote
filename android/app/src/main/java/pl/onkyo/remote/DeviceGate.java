package pl.onkyo.remote;

import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

/** One operation per ESP32; stale taps are discarded rather than replayed later. */
final class DeviceGate {
    private static final Set<String> BUSY = ConcurrentHashMap.newKeySet();
    static boolean acquire(String endpoint) { return BUSY.add(endpoint); }
    static void release(String endpoint) { BUSY.remove(endpoint); }
}
