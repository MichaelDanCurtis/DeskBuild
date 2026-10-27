#include "align.h"

namespace {
bool reached(uint32_t now, uint32_t deadline) { return static_cast<int32_t>(now - deadline) >= 0; }
}  // namespace

void AlignSession::enterFlushPreview(uint32_t nowMs, uint32_t holdMs) {
    atFlush_ = true;
    holdForever_ = holdMs == 0;
    flushUntilMs_ = nowMs + holdMs;
    previewPending_ = false;
}

void AlignSession::begin(uint32_t nowMs, const LiftSettings& current) {
    work_ = clampSettings(current, cfg_.limits);
    phase_ = Phase::Flush;
    finished_ = false;
    lastActivityMs_ = nowMs;
    enterFlushPreview(nowMs, cfg_.previewHoldMs);  // first show where flush is now
}

void AlignSession::adjust(uint32_t nowMs, int steps, Source source) {
    if (phase_ == Phase::Off || steps == 0) return;
    lastActivityMs_ = nowMs;
    if (phase_ == Phase::Flush) {
        work_.flushMm += steps * cfg_.flushStepMm;
        if (source == Source::Ring) {
            atFlush_ = false;  // lift so the ring can be turned
            previewPending_ = true;
            previewAtMs_ = nowMs + cfg_.previewDelayMs;
        } else {
            enterFlushPreview(nowMs, 0);
        }
    } else {
        work_.heightMm += steps * cfg_.heightStepMm;
    }
    work_ = clampSettings(work_, cfg_.limits);
}

void AlignSession::next(uint32_t nowMs) {
    lastActivityMs_ = nowMs;
    if (phase_ == Phase::Flush) {
        phase_ = Phase::Height;
        atFlush_ = previewPending_ = false;
    } else if (phase_ == Phase::Height) {
        phase_ = Phase::Off;
        finished_ = true;
    }
}

void AlignSession::cancel() {
    phase_ = Phase::Off;
    finished_ = false;
}

void AlignSession::update(uint32_t nowMs) {
    if (phase_ == Phase::Flush) {
        if (previewPending_ && reached(nowMs, previewAtMs_)) enterFlushPreview(nowMs, cfg_.previewHoldMs);
        if (atFlush_ && !holdForever_ && reached(nowMs, flushUntilMs_)) atFlush_ = false;
    }
    // Parked at flush on purpose (console nudging) never times out on its own.
    const bool waitingForUser = phase_ == Phase::Flush && atFlush_ && holdForever_;
    const bool busy = previewPending_ || (atFlush_ && !holdForever_);
    if (active() && !waitingForUser && !busy && reached(nowMs, lastActivityMs_ + cfg_.idleAdvanceMs)) next(nowMs);
}

float AlignSession::targetMm() const {
    if (phase_ == Phase::Flush && atFlush_) return work_.flushMm;
    return work_.raisedMm();
}

bool AlignSession::takeFinished(LiftSettings& out) {
    if (!finished_) return false;
    finished_ = false;
    out = work_;
    return true;
}
