#pragma once

#include <stdint.h>

constexpr uint32_t VOLUME_UP_MAX_HOLD_MS = 2000;

// A prepared hold never repeats until started. Closing it also rejects a start
// which arrives later. The boot identifier prevents reuse across reboots.
class VolumeHoldSafety {
public:
  enum class StartResult { Rejected, Started, AlreadyActive };

  void begin(uint32_t bootId) {
    bootId_ = bootId;
    issued_ = closedThrough_ = active_ = startedMs_ = 0;
    limited_ = false;
    preparedMs_ = 0;
    preparedLimited_ = false;
  }

  uint64_t prepare(bool volumeUp, uint32_t nowMs) {
    if (issued_ == UINT32_MAX) return 0;
    preparedMs_ = nowMs;
    preparedLimited_ = volumeUp;
    return (static_cast<uint64_t>(bootId_) << 32) | ++issued_;
  }

  StartResult start(uint64_t token, uint32_t nowMs) {
    const uint32_t sequence = validSequence(token);
    if (sequence == 0 || sequence <= closedThrough_) return StartResult::Rejected;
    if (sequence == active_) {
      if (!reachedLimit(nowMs)) return StartResult::AlreadyActive;
      closeActive();
      return StartResult::Rejected;
    }
    // A newer press supersedes all pending starts from older presses.
    if (sequence != issued_) return StartResult::Rejected;
    closeActive();
    if (preparedLimited_ && static_cast<uint32_t>(nowMs - preparedMs_) >= VOLUME_UP_MAX_HOLD_MS) {
      closedThrough_ = sequence;
      return StartResult::Rejected;
    }
    active_ = sequence;
    startedMs_ = preparedMs_;
    limited_ = preparedLimited_;
    return StartResult::Started;
  }

  bool matches(uint64_t token) const {
    return active_ != 0 && validSequence(token) == active_;
  }

  bool reachedLimit(uint32_t nowMs) const {
    return active_ != 0 && limited_ &&
           static_cast<uint32_t>(nowMs - startedMs_) >= VOLUME_UP_MAX_HOLD_MS;
  }

  void closeActive() {
    if (active_ > closedThrough_) closedThrough_ = active_;
    active_ = 0;
  }

  void closeAll() {
    closedThrough_ = issued_;
    active_ = 0;
  }

  // Return true only when this cancellation closes the currently active hold.
  bool cancel(uint64_t token) {
    const uint32_t sequence = validSequence(token);
    if (sequence == 0) return false;
    if (sequence > closedThrough_) closedThrough_ = sequence;
    if (active_ != 0 && active_ <= closedThrough_) {
      closeActive();
      return true;
    }
    return false;
  }

private:
  uint32_t validSequence(uint64_t token) const {
    const uint32_t sequence = static_cast<uint32_t>(token);
    if (static_cast<uint32_t>(token >> 32) != bootId_ ||
        sequence == 0 || sequence > issued_) return 0;
    return sequence;
  }

  uint32_t bootId_ = 0;
  uint32_t issued_ = 0;
  uint32_t closedThrough_ = 0;
  uint32_t active_ = 0;
  uint32_t startedMs_ = 0;
  uint32_t preparedMs_ = 0;
  bool limited_ = false;
  bool preparedLimited_ = false;
};
