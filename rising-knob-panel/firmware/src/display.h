#pragma once

// The 1.5" AMOLED: panel bring-up, a minimal LVGL v8 port and brightness.
// Everything here must be called from the one task that runs LVGL (Arduino's loop()).
namespace display {

bool begin();

// 0 = flush (panel off, pixels dark like the wood), 1 = fully raised (full brightness).
// The screen fades in as the knob emerges and fades out as it sinks.
void setLevel(float level);

// Let LVGL redraw; call every few milliseconds.
void service();

}  // namespace display
