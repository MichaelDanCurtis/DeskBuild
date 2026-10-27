#include "display.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_io.h>
#include <lvgl.h>

#include "config.h"

// The AMOLED (CO5300, SH8601-compatible) driven straight through ESP-IDF's esp_lcd
// QSPI panel IO. Pins and init sequence come from ESP32_Display_Panel's
// BOARD_VIEWE_UEDX46460015_MD50ET config. That library isn't used: it links
// ESP-IDF's legacy I2C driver, which aborts at boot next to the new driver that
// Arduino's Wire (distance sensor, expander) uses.

namespace display {
namespace {

constexpr int kWidth = 472;  // controller frame; the round glass is 466 wide
constexpr int kHeight = 466;
constexpr int kPowerPin = 17;
constexpr int kResetPin = 8;
constexpr int kCs = 12, kSck = 10, kD0 = 13, kD1 = 11, kD2 = 14, kD3 = 9;
constexpr uint32_t kQspiHz = 40 * 1000 * 1000;
constexpr int kBufferLines = 40;
constexpr spi_host_device_t kHost = SPI2_HOST;

// QSPI framing: opcode in bits 24..31, register in bits 8..15.
constexpr int cmd(uint8_t reg) { return (0x02 << 24) | (reg << 8); }
constexpr int kRamWrite = (0x32 << 24) | (0x2C << 8);

struct InitCmd {
    uint8_t reg;
    uint8_t data[4];
    uint8_t len;
    uint16_t delayMs;
};
const InitCmd kInit[] = {
    {0x36, {0x00}, 1, 0},                     // MADCTL: no mirroring
    {0x3A, {0x55}, 1, 0},                     // COLMOD: RGB565
    {0xFE, {0x00}, 0, 0},
    {0xC4, {0x80}, 1, 0},
    {0x35, {0x00}, 0, 10},                    // tearing effect line on
    {0x53, {0x20}, 1, 10},                    // brightness control on
    {0x51, {0x00}, 1, 10},                    // start dark; setLevel() brings it up
    {0x63, {0xFF}, 1, 10},
    {0x2A, {0x00, 0x06, 0x01, 0xDD}, 4, 0},
    {0x2B, {0x00, 0x00, 0x01, 0xD1}, 4, 0},
    {0x11, {0x00}, 0, 120},                   // sleep out; display stays off until raised
};

esp_lcd_panel_io_handle_t io = nullptr;
lv_disp_draw_buf_t drawBuf;
lv_disp_drv_t dispDrv;
bool panelOn = false;
int brightness = -1;

bool IRAM_ATTR onDrawDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void* user) {
    auto* drv = static_cast<lv_disp_drv_t*>(user);
    if (drv->draw_buf) lv_disp_flush_ready(drv);  // IRAM via LV_ATTRIBUTE_FLUSH_READY
    return false;
}

void sendParam(uint8_t reg, const uint8_t* data, size_t len) { esp_lcd_panel_io_tx_param(io, cmd(reg), data, len); }

void flush(lv_disp_drv_t*, const lv_area_t* a, lv_color_t* px) {
    const uint8_t caset[] = {uint8_t(a->x1 >> 8), uint8_t(a->x1), uint8_t(a->x2 >> 8), uint8_t(a->x2)};
    const uint8_t raset[] = {uint8_t(a->y1 >> 8), uint8_t(a->y1), uint8_t(a->y2 >> 8), uint8_t(a->y2)};
    sendParam(0x2A, caset, 4);
    sendParam(0x2B, raset, 4);
    const size_t bytes = (a->x2 - a->x1 + 1) * (a->y2 - a->y1 + 1) * sizeof(lv_color_t);
    esp_lcd_panel_io_tx_color(io, kRamWrite, px, bytes);  // async; onDrawDone fires when sent
}

// The controller only accepts windows that start and end on even pixels.
void rounder(lv_disp_drv_t*, lv_area_t* a) {
    a->x1 &= ~1;
    a->x2 |= 1;
    a->y1 &= ~1;
    a->y2 |= 1;
}

void setPanel(bool on) {
    sendParam(on ? 0x29 : 0x28, nullptr, 0);
    panelOn = on;
}

}  // namespace

bool begin() {
    pinMode(kPowerPin, OUTPUT);
    digitalWrite(kPowerPin, HIGH);
    pinMode(kResetPin, OUTPUT);
    digitalWrite(kResetPin, LOW);
    delay(10);
    digitalWrite(kResetPin, HIGH);
    delay(150);

    spi_bus_config_t bus = {};
    bus.sclk_io_num = kSck;
    bus.data0_io_num = kD0;
    bus.data1_io_num = kD1;
    bus.data2_io_num = kD2;
    bus.data3_io_num = kD3;
    bus.data4_io_num = bus.data5_io_num = bus.data6_io_num = bus.data7_io_num = -1;
    bus.max_transfer_sz = kWidth * kBufferLines * sizeof(lv_color_t);
    if (spi_bus_initialize(kHost, &bus, SPI_DMA_CH_AUTO) != ESP_OK) return false;

    esp_lcd_panel_io_spi_config_t ioCfg = {};
    ioCfg.cs_gpio_num = kCs;
    ioCfg.dc_gpio_num = -1;
    ioCfg.spi_mode = 0;
    ioCfg.pclk_hz = kQspiHz;
    ioCfg.trans_queue_depth = 10;
    ioCfg.on_color_trans_done = onDrawDone;
    ioCfg.user_ctx = &dispDrv;
    ioCfg.lcd_cmd_bits = 32;
    ioCfg.lcd_param_bits = 8;
    ioCfg.flags.quad_mode = 1;
    if (esp_lcd_new_panel_io_spi(static_cast<esp_lcd_spi_bus_handle_t>(kHost), &ioCfg, &io) != ESP_OK) {
        return false;
    }
    for (const auto& c : kInit) {
        sendParam(c.reg, c.len ? c.data : nullptr, c.len);
        if (c.delayMs) delay(c.delayMs);
    }

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
    if (!io) return;
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    // Squared so the fade looks even to the eye.
    const int target = static_cast<int>(level * level * config::kDisplayMaxBrightness + 0.5f);
    if (target == brightness) return;
    brightness = target;

    const uint8_t b = static_cast<uint8_t>(target);
    sendParam(0x51, &b, 1);
    if (target > 0 && !panelOn) setPanel(true);
    else if (target == 0 && panelOn) setPanel(false);
}

void service() { lv_timer_handler(); }

}  // namespace display
