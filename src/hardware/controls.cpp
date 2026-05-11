#include "controls.h"
#include "../config/pin_map.h"

namespace pedal {

// Alps EC11 quadrature transition table: index = (stable << 2) | raw.
// Values: +1 = CW half-step, -1 = CCW half-step, 0 = invalid/no movement.
static const int8_t kTransitionTable[16] = {
    0, -1,  1,  0,
    1,  0,  0, -1,
   -1,  0,  0,  1,
    0,  1, -1,  0,
};

void Controls::QuadEncoder::Init(daisy::Pin a, daisy::Pin b) {
    a_.Init(a, daisy::GPIO::Mode::INPUT, daisy::GPIO::Pull::PULLUP);
    b_.Init(b, daisy::GPIO::Mode::INPUT, daisy::GPIO::Pull::PULLUP);
    const uint8_t initial = ReadState();
    raw_prev_ = initial;
    stable_   = initial;
    accum_    = 0;
}

void Controls::QuadEncoder::IsrPoll(volatile int8_t& out) {
    // Shift-register debounce: require two consecutive identical reads
    // before accepting a new state (filters contact bounce).
    const uint8_t raw = ReadState();
    if (raw != raw_prev_) {
        raw_prev_ = raw;
        return;
    }
    if (raw == stable_) return;

    const uint8_t index = static_cast<uint8_t>((stable_ << 2) | raw);
    stable_ = raw;
    accum_ += kTransitionTable[index];

    // Alps EC11: 2 transitions per detent → threshold ±2.
    if (accum_ >= 2) {
        accum_ = 0;
        out += 1;
    } else if (accum_ <= -2) {
        accum_ = 0;
        out -= 1;
    }
}

uint8_t Controls::QuadEncoder::ReadState() {
    return static_cast<uint8_t>((a_.Read() ? 1 : 0) | (b_.Read() ? 2 : 0));
}

void Controls::EncoderIsrCallback(void* data) {
    Controls* self = static_cast<Controls*>(data);
    for (int i = 0; i < 4; ++i) {
        self->param_enc_[i].IsrPoll(self->isr_delta_[i]);
    }
}

void Controls::Init(daisy::DaisySeed& hw) {
    (void)hw;

    encoder_.Init(pins::ENC_A, pins::ENC_B, pins::ENC_SW);
    param_enc_[0].Init(pins::PARAM_ENC_0_A, pins::PARAM_ENC_0_B);
    param_enc_[1].Init(pins::PARAM_ENC_1_A, pins::PARAM_ENC_1_B);
    param_enc_[2].Init(pins::PARAM_ENC_2_A, pins::PARAM_ENC_2_B);
    param_enc_[3].Init(pins::PARAM_ENC_3_A, pins::PARAM_ENC_3_B);
    sw_fx_[0].Init(pins::SW_FX_MOD);
    sw_fx_[1].Init(pins::SW_FX_DELAY);
    sw_fx_[2].Init(pins::SW_FX_REVERB);
    sw_tap_.Init(pins::SW_TAP);

    // TIM3 at 500 Hz: ISR_freq = APB1 / ((PSC+1)*(ARR+1)) = 120MHz/(120*2000)
    daisy::TimerHandle::Config tim_cfg;
    tim_cfg.periph     = daisy::TimerHandle::Config::Peripheral::TIM_3;
    tim_cfg.dir        = daisy::TimerHandle::Config::CounterDir::UP;
    tim_cfg.period     = 1999;
    tim_cfg.enable_irq = true;
    enc_timer_.Init(tim_cfg);
    enc_timer_.SetPrescaler(119);
    enc_timer_.SetCallback(EncoderIsrCallback, this);
    enc_timer_.Start();
}

void Controls::Poll() {
    // Drain ISR accumulators atomically. ISR is sole writer.
    int param_delta[4];
    __disable_irq();
    for (int i = 0; i < 4; ++i) {
        param_delta[i] = isr_delta_[i];
        isr_delta_[i]  = 0;
    }
    __enable_irq();

    // Switches debounced at main-loop rate (slow transitions, no ISR needed).
    encoder_.Debounce();
    for (int i = 0; i < 3; ++i) sw_fx_[i].Debounce();
    sw_tap_.Debounce();

    for (int i = 0; i < 4; ++i) {
        state_.param_encoder_increment[i] = param_delta[i];
    }

    state_.mode_encoder_increment = encoder_.Increment();
    state_.mode_encoder_pressed   = encoder_.FallingEdge();
    state_.mode_encoder_held      = encoder_.Pressed();
    for (int i = 0; i < 3; ++i) state_.fx_pressed[i] = sw_fx_[i].RisingEdge();
    state_.tap_pressed     = sw_tap_.RisingEdge();
    state_.tap_released    = sw_tap_.FallingEdge();
    state_.tap_held        = sw_tap_.Pressed();
    state_.tap_held_ms     = static_cast<uint32_t>(sw_tap_.TimeHeldMs());
}

} // namespace pedal
