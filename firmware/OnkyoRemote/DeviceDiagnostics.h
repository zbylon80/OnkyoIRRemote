#pragma once

#include <atomic>
#include <stdint.h>

// Updated by the network event task; read on demand by the HTTP task.
// Counters describe this boot and never write flash or include credentials.
struct DeviceDiagnostics {
  std::atomic<uint32_t> connections{0};
  std::atomic<uint32_t> disconnects{0};
  std::atomic<uint32_t> lastDisconnectReason{0};
  std::atomic<uint32_t> lastDisconnectUptimeSeconds{0};
  std::atomic<bool> disconnectPending{false};
  uint32_t maxLoopMs = 0;  // Read and written only in the main loop task.

  void connected() { connections.fetch_add(1); }
  void disconnected(uint32_t reason, uint32_t uptimeSeconds) {
    disconnectPending.store(true);
    lastDisconnectReason.store(reason);
    lastDisconnectUptimeSeconds.store(uptimeSeconds);
    disconnects.fetch_add(1);
  }
};
