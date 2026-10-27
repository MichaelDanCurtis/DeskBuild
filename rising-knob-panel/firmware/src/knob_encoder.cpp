#include "knob_encoder.h"

#include <driver/gpio.h>
#include <driver/pulse_cnt.h>

#include "config.h"

namespace {
constexpr int kLimit = 10000;
}

bool KnobEncoder::begin(int pinA, int pinB) {
    pcnt_unit_config_t unitCfg = {};
    unitCfg.low_limit = -kLimit;
    unitCfg.high_limit = kLimit;
    unitCfg.flags.accum_count = 1;  // keep counting past the limits
    pcnt_unit_handle_t unit = nullptr;
    if (pcnt_new_unit(&unitCfg, &unit) != ESP_OK) return false;

    pcnt_glitch_filter_config_t filter = {};
    filter.max_glitch_ns = 1000;
    pcnt_unit_set_glitch_filter(unit, &filter);

    pcnt_chan_config_t chanA = {};
    chanA.edge_gpio_num = pinA;
    chanA.level_gpio_num = pinB;
    pcnt_chan_config_t chanB = {};
    chanB.edge_gpio_num = pinB;
    chanB.level_gpio_num = pinA;
    pcnt_channel_handle_t a = nullptr, b = nullptr;
    if (pcnt_new_channel(unit, &chanA, &a) != ESP_OK || pcnt_new_channel(unit, &chanB, &b) != ESP_OK) return false;

    // Standard x4 quadrature decoding.
    pcnt_channel_set_edge_action(a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    pcnt_channel_set_level_action(a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    pcnt_channel_set_edge_action(b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE);
    pcnt_channel_set_level_action(b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    pcnt_unit_add_watch_point(unit, -kLimit);
    pcnt_unit_add_watch_point(unit, kLimit);

    gpio_pullup_en(static_cast<gpio_num_t>(pinA));
    gpio_pullup_en(static_cast<gpio_num_t>(pinB));

    if (pcnt_unit_enable(unit) != ESP_OK || pcnt_unit_clear_count(unit) != ESP_OK ||
        pcnt_unit_start(unit) != ESP_OK) {
        return false;
    }
    unit_ = unit;
    return true;
}

int KnobEncoder::takeDetents() {
    if (!unit_) return 0;
    int count = 0;
    pcnt_unit_get_count(static_cast<pcnt_unit_handle_t>(unit_), &count);
    const int detents = (count - last_) / config::kEncoderCountsPerDetent;
    last_ += detents * config::kEncoderCountsPerDetent;  // keep the remainder for next time
    return config::kEncoderReverse ? -detents : detents;
}
