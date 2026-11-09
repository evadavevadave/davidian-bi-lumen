/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* Audio Unit (AUv2 effect) build of the Bi-Lumen for macOS: engine.c behind Apple's
 * AudioUnitSDK (Apache 2.0 licence; see NOTICE.md), with parameters from params.h and a Cocoa
 * view from au_view.m. */
#include <AudioUnitSDK/AUEffectBase.h>
#include <AudioToolbox/AudioUnitProperties.h>

#include <cstring>

extern "C" {
#include "../engine.h"
}
#include "../params.h"

/* Both set by CMakeLists.txt, which also puts them in Info.plist and au_view.m. */
#ifndef BL_AU_BUNDLE_ID
#    error "BL_AU_BUNDLE_ID must name the component's bundle identifier"
#endif
#ifndef BL_AU_VIEW_FACTORY
#    error "BL_AU_VIEW_FACTORY must name the Cocoa view factory class"
#endif
#define BL_STR2(x) #x
#define BL_STR(x) BL_STR2(x)

class DavidianBiLumenAU : public ausdk::AUEffectBase {
public:
    explicit DavidianBiLumenAU(AudioComponentInstance ci) : AUEffectBase(ci, false)
    {
        /* Parameter IDs are the LV2 port numbers. The host's bypass replaces "Enabled". */
        for (int i = 0; i < BL_NUM_PARAMS; ++i) {
            values[BL_PARAMS[i].port] = BL_PARAMS[i].def;
            if (BL_PARAMS[i].port != PORT_ENABLED)
                Globals()->SetParameter((AudioUnitParameterID)BL_PARAMS[i].port, BL_PARAMS[i].def);
        }
    }

    ~DavidianBiLumenAU() override { bl_engine_free(engine); }

    OSStatus Initialize() override
    {
        const OSStatus r = AUEffectBase::Initialize();
        if (r != noErr)
            return r;
        bl_engine_free(engine);
        engine = bl_engine_new(GetSampleRate());
        if (!engine)
            return kAudioUnitErr_FailedInitialization;
        for (int port = BL_FIRST_CONTROL; port < PORT_COUNT; ++port)
            bl_engine_connect(engine, (uint32_t)port, &values[port]);
        pull();
        bl_engine_activate(engine);
        return noErr;
    }

    void Cleanup() override
    {
        bl_engine_free(engine);
        engine = nullptr;
        AUEffectBase::Cleanup();
    }

    OSStatus Reset(AudioUnitScope scope, AudioUnitElement element) override
    {
        if (engine) {
            pull();
            bl_engine_activate(engine);
        }
        return AUEffectBase::Reset(scope, element);
    }

    /* Stereo out, from stereo or mono in (a mono input feeds both phasor inputs). */
    UInt32 SupportedNumChannels(const AUChannelInfo** out) override
    {
        static const AUChannelInfo configs[] = { { 2, 2 }, { 1, 2 } };
        if (out)
            *out = configs;
        return 2;
    }

    bool SupportsTail() override { return true; }
    Float64 GetTailTime() override { return 0.1; }

    OSStatus GetParameterInfo(AudioUnitScope scope, AudioUnitParameterID id, AudioUnitParameterInfo& info) override
    {
        const BlParam* p = bl_param((int)id);
        if (scope != kAudioUnitScope_Global)
            return kAudioUnitErr_InvalidScope;
        if (!p || p->port == PORT_ENABLED)
            return kAudioUnitErr_InvalidParameter;
        std::memset(&info, 0, sizeof info);
        FillInParameterName(info, CFStringCreateWithCString(nullptr, p->name, kCFStringEncodingUTF8), true);
        info.flags |= kAudioUnitParameterFlag_IsReadable;
        if (p->output)
            info.flags |= kAudioUnitParameterFlag_MeterReadOnly;
        else
            info.flags |= kAudioUnitParameterFlag_IsWritable;
        if (p->log)
            info.flags |= kAudioUnitParameterFlag_DisplayLogarithmic;
        if (p->steps) {
            info.unit = kAudioUnitParameterUnit_Indexed;
        } else if (!std::strcmp(p->unit, "Hz")) {
            info.unit = kAudioUnitParameterUnit_Hertz;
        } else if (!std::strcmp(p->unit, "dB")) {
            info.unit = kAudioUnitParameterUnit_Decibels;
        } else if (!std::strcmp(p->unit, "V")) {
            info.unit = kAudioUnitParameterUnit_CustomUnit;
            info.unitName = CFSTR("V");
        } else {
            info.unit = kAudioUnitParameterUnit_Generic;
        }
        info.minValue = p->min;
        info.maxValue = p->max;
        info.defaultValue = p->def;
        return noErr;
    }

    OSStatus GetParameterValueStrings(AudioUnitScope scope, AudioUnitParameterID id, CFArrayRef* out) override
    {
        const BlParam* p = bl_param((int)id);
        CFStringRef names[3];
        if (scope != kAudioUnitScope_Global || !p || !p->steps || p->port == PORT_ENABLED)
            return kAudioUnitErr_InvalidProperty;
        if (!out) /* the SDK asks whether there are any */
            return noErr;
        for (int i = 0; i <= p->steps; ++i)
            names[i] = CFStringCreateWithCString(nullptr, p->opts[i], kCFStringEncodingUTF8);
        *out = CFArrayCreate(nullptr, (const void**)names, p->steps + 1, &kCFTypeArrayCallBacks);
        for (int i = 0; i <= p->steps; ++i)
            CFRelease(names[i]);
        return noErr;
    }

    OSStatus GetPropertyInfo(AudioUnitPropertyID id, AudioUnitScope scope, AudioUnitElement element,
                             UInt32& size, bool& writable) override
    {
        if (scope == kAudioUnitScope_Global && id == kAudioUnitProperty_CocoaUI) {
            size = sizeof(AudioUnitCocoaViewInfo);
            writable = false;
            return noErr;
        }
        return AUEffectBase::GetPropertyInfo(id, scope, element, size, writable);
    }

    OSStatus GetProperty(AudioUnitPropertyID id, AudioUnitScope scope, AudioUnitElement element,
                         void* data) override
    {
        if (scope == kAudioUnitScope_Global && id == kAudioUnitProperty_CocoaUI) {
            CFBundleRef bundle = CFBundleGetBundleWithIdentifier(CFSTR(BL_AU_BUNDLE_ID));
            CFURLRef url = bundle ? CFBundleCopyBundleURL(bundle) : nullptr;
            AudioUnitCocoaViewInfo info;
            if (!url)
                return kAudioUnitErr_InvalidProperty;
            /* The host releases both. */
            info.mCocoaAUViewBundleLocation = url;
            info.mCocoaAUViewClass[0] = CFStringCreateWithCString(nullptr, BL_STR(BL_AU_VIEW_FACTORY),
                                                                  kCFStringEncodingUTF8);
            std::memcpy(data, &info, sizeof info);
            return noErr;
        }
        return AUEffectBase::GetProperty(id, scope, element, data);
    }

    /* The engine fades to the dry signal itself, so the host's bypass goes through it. */
    bool ShouldBypassEffect() override { return false; }

    OSStatus ProcessBufferLists(AudioUnitRenderActionFlags& flags, const AudioBufferList& in,
                                AudioBufferList& out, UInt32 frames) override
    {
        if (!engine || in.mNumberBuffers < 1 || out.mNumberBuffers < 2)
            return kAudioUnitErr_FormatNotSupported;
        pull();
        float* ia = static_cast<float*>(in.mBuffers[0].mData);
        float* ib = in.mNumberBuffers > 1 ? static_cast<float*>(in.mBuffers[1].mData) : ia;
        bl_engine_connect(engine, PORT_IN_A, ia);
        bl_engine_connect(engine, PORT_IN_B, ib);
        bl_engine_connect(engine, PORT_OUT_A, out.mBuffers[0].mData);
        bl_engine_connect(engine, PORT_OUT_B, out.mBuffers[1].mData);
        bl_engine_run(engine, frames);
        /* The opto lamps, for the view. */
        SetParameter(PORT_LED_A, values[PORT_LED_A]);
        SetParameter(PORT_LED_B, values[PORT_LED_B]);
        flags &= ~kAudioUnitRenderAction_OutputIsSilence;
        return noErr;
    }

private:
    void pull()
    {
        for (int i = 0; i < BL_NUM_PARAMS; ++i) {
            const BlParam* p = &BL_PARAMS[i];
            if (!p->output && p->port != PORT_ENABLED)
                values[p->port] = GetParameter((AudioUnitParameterID)p->port);
        }
        values[PORT_ENABLED] = IsBypassEffect() ? 0.0f : 1.0f;
    }

    BlEngine* engine = nullptr;
    float values[PORT_COUNT] = {};
};

AUSDK_COMPONENT_ENTRY(ausdk::AUBaseFactory, DavidianBiLumenAU)
