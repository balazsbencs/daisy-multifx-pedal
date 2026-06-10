#include "audio_engine.h"
#include "util/scopedirqblocker.h"
#include "../dsp/fast_math.h"
#include "../config/constants.h"

using namespace daisy;

namespace pedal {

AudioEngine* AudioEngine::instance_ = nullptr;

void AudioEngine::Init(DaisySeed* hw) {
    hw_              = hw;
    mod_mode_        = nullptr;
    delay_mode_      = nullptr;
    reverb_mode_     = nullptr;
    param_read_idx_  = 0;
    param_dirty_     = false;
    hold_dirty_      = false;
    new_hold_        = false;
    param_buf_[0].mod    = mod_fx::ParamSet::make_default();
    param_buf_[0].delay  = delay_fx::ParamSet::make_default();
    param_buf_[0].reverb = reverb_fx::ParamSet::make_default();
    param_buf_[0].hold_active = false;
    param_buf_[1] = param_buf_[0];
    instance_ = this;

    // Enable DWT cycle counter for CPU load measurement
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    // Derive cycles-per-block from the actual system clock so the CPU meter
    // is correct regardless of whether the hardware runs at 400 or 480 MHz.
    // Divide before multiplying to avoid 32-bit overflow.
    cycles_per_block_ = (daisy::System::GetSysClkFreq() / static_cast<uint32_t>(SAMPLE_RATE))
                        * static_cast<uint32_t>(BLOCK_SIZE);
}

void AudioEngine::SetModMode(ModMode* m) {
    ScopedIrqBlocker irq;
    mod_mode_ = m;
}

void AudioEngine::SetDelayMode(DelayMode* m) {
    ScopedIrqBlocker irq;
    delay_mode_ = m;
}

void AudioEngine::SetReverbMode(ReverbMode* m) {
    ScopedIrqBlocker irq;
    reverb_mode_ = m;
}

void AudioEngine::SetParams(const MultiParamBuf& params) {
    ScopedIrqBlocker irq;
    const int write_idx   = 1 - param_read_idx_;
    param_buf_[write_idx] = params;
    param_dirty_          = true;
}

void AudioEngine::SetHold(bool hold) {
    ScopedIrqBlocker irq;
    new_hold_   = hold;
    hold_dirty_ = true;
}

void AudioEngine::AudioCallback(AudioHandle::InputBuffer  in,
                                AudioHandle::OutputBuffer out,
                                size_t                    size) {
    if (instance_) instance_->ProcessBlock(in, out, size);
}

void AudioEngine::UpdateMixCoeffs(const MultiParamBuf& p) {
    auto recompute = [](float mix, float& last, float& dry, float& wet, float& norm) {
        if (mix == last) return;
        last              = mix;
        const float angle = mix * 1.57079632679f;
        dry               = fast_cos(angle);
        wet               = fast_sin(angle);
        const float sum   = dry + wet;
        norm              = (sum > 1.0f) ? (1.0f / sum) : 1.0f;
    };
    recompute(p.mod.mix,    last_mod_mix_, mod_dry_, mod_wet_, mod_norm_);
    recompute(p.delay.mix,  last_dly_mix_, dly_dry_, dly_wet_, dly_norm_);
    recompute(p.reverb.mix, last_rev_mix_, rev_dry_, rev_wet_, rev_norm_);
}

void AudioEngine::ProcessBlock(AudioHandle::InputBuffer  in,
                               AudioHandle::OutputBuffer out,
                               size_t                    size) {
    const uint32_t t0 = DWT->CYCCNT;

    if (param_dirty_) { param_read_idx_ ^= 1; param_dirty_ = false; }
    if (hold_dirty_) {
        hold_dirty_ = false;
        if (reverb_mode_) reverb_mode_->SetHold(new_hold_);
    }

    const MultiParamBuf& p = param_buf_[param_read_idx_];
    UpdateMixCoeffs(p);

    // Per-block prepare (expensive coefficient updates, LFO stepping, etc.)
    if (mod_mode_    && p.fx_enabled[0]) mod_mode_->Prepare(p.mod);
    if (delay_mode_  && p.fx_enabled[1]) delay_mode_->Prepare(p.delay);
    if (reverb_mode_ && p.fx_enabled[2]) reverb_mode_->Prepare(p.reverb);

    for (size_t i = 0; i < size; ++i) {
        const float dry = IN_L[i];

        // Stage 1: modulation (stereo in/out)
        StereoFrame s1;
        if (mod_mode_ && p.fx_enabled[0]) {
            const StereoFrame wet = mod_mode_->Process({dry, dry}, p.mod);
            s1.left  = (dry * mod_dry_ + wet.left  * mod_wet_) * mod_norm_;
            s1.right = (dry * mod_dry_ + wet.right * mod_wet_) * mod_norm_;
        } else {
            s1 = {dry, dry};
        }

        // Stage 2: delay (mono input from s1.left → stereo out)
        StereoFrame s2;
        if (delay_mode_ && p.fx_enabled[1]) {
            const StereoFrame wet = delay_mode_->Process(s1.left, p.delay);
            s2.left  = (s1.left * dly_dry_ + wet.left  * dly_wet_) * dly_norm_;
            s2.right = (s1.right * dly_dry_ + wet.right * dly_wet_) * dly_norm_;
        } else {
            s2 = s1;
        }

        // Stage 3: reverb sub-sampled at 24 kHz (every other sample).
        // Average input pairs for cheap anti-aliasing and interpolate the output
        // pair instead of a zero-order hold.
        StereoFrame s3;
        if (reverb_mode_ && p.fx_enabled[2]) {
            reverb_phase_ = !reverb_phase_;
            reverb_input_sum_ += s2.left;
            StereoFrame wet;
            if (reverb_phase_) {
                wet = {
                    0.5f * (reverb_prev_.left + reverb_last_.left),
                    0.5f * (reverb_prev_.right + reverb_last_.right)
                };
            } else {
                reverb_prev_ = reverb_last_;
                reverb_last_ = reverb_mode_->Process(0.5f * reverb_input_sum_, p.reverb);
                reverb_input_sum_ = 0.0f;
                wet = reverb_last_;
            }
            s3.left  = (s2.left  * rev_dry_ + wet.left  * rev_wet_) * rev_norm_;
            s3.right = (s2.right * rev_dry_ + wet.right * rev_wet_) * rev_norm_;
        } else {
            s3 = s2;
            reverb_input_sum_ = 0.0f;
        }

        OUT_L[i] = s3.left;
        OUT_R[i] = s3.right;
    }

    const uint32_t t1 = DWT->CYCCNT;
    const uint32_t elapsed = t1 - t0;
}

} // namespace pedal
