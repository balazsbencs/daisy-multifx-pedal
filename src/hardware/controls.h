#pragma once
#include "daisy_seed.h"
#include "../config/constants.h"

namespace pedal {

struct ControlState {
    int  mode_encoder_increment;       // -1, 0, +1 since last poll
    int  param_encoder_increment[4]{}; // 4 parameter encoders
    bool mode_encoder_pressed;         // falling edge
    bool mode_encoder_held;             // currently held
    bool fx_pressed[3]{};              // rising edge per effect (0=mod,1=delay,2=reverb)
    bool tap_pressed;                  // rising edge
    bool tap_released;                 // falling edge
    bool tap_held;                     // currently held
    uint32_t tap_held_ms;              // held duration while pressed
};

class Controls {
public:
    enum class EncoderType {
        ALPS_EC11     = 2,  // 2 transitions per detent
        BOURNS_PEC11R = 4,  // 4 transitions per detent
    };

    void Init(daisy::DaisySeed& hw, EncoderType type = EncoderType::ALPS_EC11);
    // Call each main loop iteration
    void Poll();
    const ControlState& state() const { return state_; }

private:
    class QuadEncoder {
      public:
        void Init(daisy::Pin a, daisy::Pin b, EncoderType type = EncoderType::ALPS_EC11);
        // Called from TIM3 ISR only. Increments `out` by ±1 on completed detent.
        void IsrPoll(volatile int8_t& out);

      private:
        uint8_t ReadState();
        daisy::GPIO a_;
        daisy::GPIO b_;
        uint8_t raw_prev_  = 0;
        uint8_t stable_    = 0;
        int8_t  accum_     = 0;
        int8_t  threshold_ = 2;  // transitions required per detent
    };

    // TIM3 ISR: polls the 4 parameter encoders only.
    static void EncoderIsrCallback(void* data);

    daisy::Encoder     encoder_;
    QuadEncoder        param_enc_[4];
    daisy::Switch      sw_fx_[3];
    daisy::Switch      sw_tap_;
    ControlState       state_{};
    daisy::TimerHandle enc_timer_;
    volatile int8_t    isr_delta_[4]{};       // written by ISR, drained by Poll()
    int8_t             mode_enc_accum_ = 0;   // sub-detent accumulator for half-speed
};

} // namespace pedal
