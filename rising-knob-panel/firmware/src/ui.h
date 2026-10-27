#pragma once

// Screens: a placeholder dial (the ring turns a 0-100 value), the alignment
// screens, and status messages. Swap the dial for your real UI; it only needs
// LVGL and `addDetents()`.
namespace ui {

enum class Screen { Dial, AlignFlush, AlignHeight, Homing, Fault };

void begin();
void addDetents(int detents);
// `valueMm` is shown on the align screens.
void show(Screen screen, float valueMm);

}  // namespace ui
