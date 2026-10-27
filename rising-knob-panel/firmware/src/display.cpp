#include "display.h"

#include <Arduino.h>
#include <esp_display_panel.hpp>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_io.h>
#include <lvgl.h>

#include "config.h"

using namespace esp_panel::drivers;

namespace display {
namespace {

// Panel wiring and init sequence, from ESP32_Display_Panel's
// BOARD_VIEWE_UEDX46460015_MD50ET config. Touch is deliberately not started: its
// I2C driver would conflict with the Wire bus the distance sensor uses.
constexpr int kWidth = 472;  // controller frame; the round glass is 466 wide
constexpr int kHeight = 466;
constexpr int kPowerPin = 17;
constexpr int kResetPin = 8;
constexpr int kCs = 12, kSck = 10, kD0 = 13, kD1 = 11, kD2 = 14, kD3 = 9;
constexpr uint32_t kQspiHz = 40 * 1000 * 1000;
constexpr int kBufferLines = 40;

const uint8_t kC4[] = {0x80};
const uint8_t k3A[] = {0x55};          // RGB565
const uint8_t k53[] = {0x20};          // brightness control on
const uint8_t k51[] = {0x00};          // start dark; setLevel() brings it up
const uint8_t k63[] = {0xFF};
const uint8_t k2A[] = {0x00, 0x06, 0x01, 0xDD};
const uint8_t k2B[] = {0x00, 0x00, 0x01, 0xD1};
const esp_panel_lcd_vendor_init_cmd_t kInitCmds[] = {
    {0xFE, nullptr, 0, 0}, {0xC4, kC4, 1, 0},   {0x3A, k3A, 1, 0},   {0x35, nullptr, 0, 10},
    {0x53, k53, 1, 10},    {0x51, k51, 1, 10},  {0x63, k63, 1, 10},  {0x2A, k2A, 4, 0},
    {0x2B, k2B, 4, 0},     {0x11, nullptr, 0, 60},
};

// SH8601/CO5300 QSPI command framing: opcode 0x02, register in bits 8..15.
constexpr int qspiCmd(uint8_t reg) { return (0x02 << 24) | (reg << 8); }

LCD* lcd = nullptr;
lv_disp_draw_buf_t drawBuf;
lv_disp_drv_t dispDrv;
bool panelOn = false;
int brightness = -1;

bool IRAM_ATTR onDrawDone(void* user) {
    auto* drv = static_cast<lv_disp_drv_t*>(user);
    if (drv->draw_buf) lv_disp_flush_ready(drv);  // IRAM via LV_ATTRIBUTE_FLUSH_READY
    return false;
}

void flush(lv_disp_drv_t* drv, const lv_area_t* a, lv_color_t* px) {
    lcd->drawBitmap(a->x1, a->y1, a->x2 - a->x1 + 1, a->y2 - a->y1 + 1, reinterpret_cast<const uint8_t*>(px));
}

// The controller only accepts windows that start and end on even pixels.
void rounder(lv_disp_drv_t*, lv_area_t* a) {
    const auto& spec = lcd->getBasicAttributes().basic_bus_spec;
    const int xa = spec.x_coord_align > 1 ? spec.x_coord_align : 2;
    const int ya = spec.y_coord_align > 1 ? spec.y_coord_align : 2;
    a->x1 &= ~(xa - 1);
    a->x2 = (a->x2 & ~(xa - 1)) + xa - 1;
    a->y1 &= ~(ya - 1);
    a->y2 = (a->y2 & ~(ya - 1)) + ya - 1;
}

void sendBrightness(uint8_t value) {
    esp_lcd_panel_io_tx_param(lcd->getBus()->getControlPanelHandle(), qspiCmd(0x51), &value, 1);
}

}  // namespace

bool begin() {
    pinMode(kPowerPin, OUTPUT);
    digitalWrite(kPowerPin, HIGH);

    auto* bus = new BusQSPI(kCs, kSck, kD0, kD1, kD2, kD3);
    bus->configQSPI_FreqHz(kQspiHz);
    lcd = new LCD_SH8601(bus, kWidth, kHeight, 16, kResetPin);
    lcd->configVendorCommands(kInitCmds, sizeof(kInitCmds) / sizeof(kInitCmds[0]));
    lcd->attachDrawBitmapFinishCallback(onDrawDone, &dispDrv);
    if (!lcd->begin()) return false;
    lcd->setDisplayOnOff(false);

    lv_init();
    const size_t pixels = kWidth * kBufferLines;
    auto* buf1 = static_cast<lv_color_t*>(heap_caps_malloc(pixels * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    auto* buf2 = static_cast<lv_color_t*>(heap_caps_malloc(pixels * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    if (!buf1 || !buf2) return false;
    lv_disp_draw_buf_init(&drawBuf, buf1, buf2, pixels);

    lv_disp_drv_init(&dispDrv);
    dispDrv.hor_res = kWidth;
    dispDrv.ver_res = kHeight;
    dispDrv.flush_cb = flush;
    dispDrv.rounder_cb = rounder;
    dispDrv.draw_buf = &drawBuf;
    lv_disp_drv_register(&dispDrv);
    return true;
}

void setLevel(float level) {
    if (!lcd) return;
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    // Squared so the fade looks even to the eye.
    const int target = static_cast<int>(level * level * config::kDisplayMaxBrightness + 0.5f);
    if (target == brightness) return;
    brightness = target;

    if (target > 0 && !panelOn) {
        sendBrightness(static_cast<uint8_t>(target));
        lcd->setDisplayOnOff(true);
        panelOn = true;
    } else if (target == 0 && panelOn) {
        sendBrightness(0);
        lcd->setDisplayOnOff(false);
        panelOn = false;
    } else if (panelOn) {
        sendBrightness(static_cast<uint8_t>(target));
    }
}

void service() { lv_timer_handler(); }

}  // namespace display
