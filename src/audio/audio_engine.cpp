#include "audio_engine.h"
#include "util/scopedirqblocker.h"
#include "../dsp/fast_math.h"

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
    if (param_dirty_) { param_read_idx_ ^= 1; param_dirty_ = false; }
    if (hold_dirty_) {
        hold_dirty_ = false;
        if (reverb_mode_) reverb_mode_->SetHold(new_hold_);
    }

    const MultiParamBuf& p = param_buf_[param_read_idx_];
    UpdateMixCoeffs(p);

    // Per-block prepare (expensive coefficient updates, LFO stepping, etc.)
    if (mod_mode_)    mod_mode_->Prepare(p.mod);
    if (delay_mode_)  delay_mode_->Prepare(p.delay);
    if (reverb_mode_) reverb_mode_->Prepare(p.reverb);

    for (size_t i = 0; i < size; ++i) {
        const float dry = IN_L[i];

        // Stage 1: modulation (stereo in/out)
        StereoFrame s1;
        if (mod_mode_) {
            const StereoFrame wet = mod_mode_->Process({dry, dry}, p.mod);
            s1.left  = (dry * mod_dry_ + wet.left  * mod_wet_) * mod_norm_;
            s1.right = (dry * mod_dry_ + wet.right * mod_wet_) * mod_norm_;
        } else {
            s1 = {dry, dry};
        }

        // Stage 2: delay (mono input from s1.left → stereo out)
        StereoFrame s2;
        if (delay_mode_) {
            const StereoFrame wet = delay_mode_->Process(s1.left, p.delay);
            s2.left  = (s1.left * dly_dry_ + wet.left  * dly_wet_) * dly_norm_;
            s2.right = (s1.left * dly_dry_ + wet.right * dly_wet_) * dly_norm_;
        } else {
            s2 = s1;
        }

        // Stage 3: reverb (mono input from s2.left → stereo out)
        StereoFrame s3;
        if (reverb_mode_) {
            const StereoFrame wet = reverb_mode_->Process(s2.left, p.reverb);
            s3.left  = (s2.left * rev_dry_ + wet.left  * rev_wet_) * rev_norm_;
            s3.right = (s2.left * rev_dry_ + wet.right * rev_wet_) * rev_norm_;
        } else {
            s3 = s2;
        }

        OUT_L[i] = s3.left;
        OUT_R[i] = s3.right;
    }
}

} // namespace pedal
