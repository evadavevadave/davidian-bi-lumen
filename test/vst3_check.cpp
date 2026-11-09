/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Davidian. See LICENSE and NOTICE.md. */
/* Hosts the built VST3 the way a Windows DAW does and checks what Steinberg's validator does
 * not: that the editor turns mouse clicks and drags into begin/perform/end edits, and that
 * the VST3 sounds the same as the LV2 build, sample for sample, with the same settings
 * (including a bypass). Windows only.
 * Usage: vst3_check <Davidian Bi-Lumen.vst3 folder> <bi_lumen.lv2 folder> */
#ifndef _CRT_SECURE_NO_WARNINGS
#    define _CRT_SECURE_NO_WARNINGS
#endif
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <windows.h>

#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/processdata.h"

#include "compare.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

static int failures;

static void check(const char* what, bool ok)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        ++failures;
}

/* Records the edits the editor sends to the host. */
struct Handler : IComponentHandler {
    struct Edit { char kind; ParamID id; ParamValue v; };
    std::vector<Edit> log;
    tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override
    {
        if (FUnknownPrivate::iidEqual(iid, IComponentHandler::iid) || FUnknownPrivate::iidEqual(iid, FUnknown::iid)) {
            *obj = this;
            return kResultOk;
        }
        *obj = nullptr;
        return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
    tresult PLUGIN_API beginEdit(ParamID id) override { log.push_back({ 'b', id, 0 }); return kResultOk; }
    tresult PLUGIN_API performEdit(ParamID id, ParamValue v) override { log.push_back({ 'p', id, v }); return kResultOk; }
    tresult PLUGIN_API endEdit(ParamID id) override { log.push_back({ 'e', id, 0 }); return kResultOk; }
    tresult PLUGIN_API restartComponent(int32) override { return kResultOk; }
};

/* An edit of one parameter: begin, at least one perform, end; returns the last value. */
static bool one_edit(const std::vector<Handler::Edit>& log, ParamID id, ParamValue* last)
{
    if (log.size() < 3 || log.front().kind != 'b' || log.back().kind != 'e')
        return false;
    for (const auto& e : log) {
        if (e.id != id)
            return false;
        if (e.kind == 'p')
            *last = e.v;
    }
    return true;
}

static LRESULT CALLBACK frame_proc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcA(h, m, w, l); }

static void test_editor(IEditController* ctrl, Handler& handler)
{
    WNDCLASSA wc = {};
    wc.lpfnWndProc = frame_proc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "BiLumenTestFrame";
    RegisterClassA(&wc);
    HWND frame = CreateWindowA("BiLumenTestFrame", "host", WS_OVERLAPPEDWINDOW, 0, 0, 1000, 520, nullptr,
                               nullptr, wc.hInstance, nullptr);
    IPlugView* view = ctrl->createView(ViewType::kEditor);
    check("controller creates an editor", view != nullptr);
    if (!view)
        return;
    ViewRect r;
    check("editor is 960 x 470", view->getSize(&r) == kResultOk && r.getWidth() == 960 && r.getHeight() == 470);
    check("editor supports HWND", view->isPlatformTypeSupported(kPlatformTypeHWND) == kResultTrue);
    check("editor attaches to a host window", view->attached(frame, kPlatformTypeHWND) == kResultOk);
    HWND panel = FindWindowExA(frame, nullptr, "DavidianBiLumenVST3", nullptr);
    check("editor window is a child of the host window", panel != nullptr);
    if (panel) {
        ParamValue v = -1;
        /* Phasor A footswitch at (300, 404): one click turns it off. */
        handler.log.clear();
        SendMessageA(panel, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(300, 404));
        SendMessageA(panel, WM_LBUTTONUP, 0, MAKELPARAM(300, 404));
        check("footswitch click is one begin/perform/end edit", one_edit(handler.log, PORT_ON_A, &v) && v == 0.0);
        check("controller follows the click", ctrl->getParamNormalized(PORT_ON_A) == 0.0);

        /* Phasor A depth knob at (281, 150): dragging up 36 px adds 0.2, from 0.8 to 1.0. */
        handler.log.clear();
        SendMessageA(panel, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(281, 150));
        SendMessageA(panel, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(281, 132));
        SendMessageA(panel, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(281, 114));
        SendMessageA(panel, WM_LBUTTONUP, 0, MAKELPARAM(281, 114));
        check("knob drag is one edit with several performs", one_edit(handler.log, PORT_DEPTH_A, &v)
                                                                && handler.log.size() >= 4);
        check("knob drag ends at the top of its range", std::fabs(v - 1.0) < 1e-6);

        /* Phasor B input switch at (593, 272): the top position is In A. */
        handler.log.clear();
        SendMessageA(panel, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(593, 250));
        SendMessageA(panel, WM_LBUTTONUP, 0, MAKELPARAM(593, 250));
        check("switch click selects In A", one_edit(handler.log, PORT_B_INPUT, &v) && v == 0.0);

        handler.log.clear();
        SendMessageA(panel, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(5, 5));
        SendMessageA(panel, WM_LBUTTONUP, 0, MAKELPARAM(5, 5));
        check("clicking empty panel sends nothing", handler.log.empty());

        /* Values from the host (automation, the opto lamps) reach the open editor without trouble. */
        check("host values reach the editor", ctrl->setParamNormalized(PORT_FEEDBACK_B, 0.5) == kResultOk
                                                  && ctrl->setParamNormalized(PORT_LED_A, 0.7) == kResultOk);
    }
    check("editor detaches", view->removed() == kResultOk);
    check("editor window is gone", !panel || !IsWindow(panel));
    view->release();
    DestroyWindow(frame);
}

/* ---- Sound: the VST3 against the LV2 build (see compare.h) ---- */

static std::vector<float> run_vst3(IComponent* comp, int* led_updates)
{
    std::vector<float> out;
    FUnknownPtr<IAudioProcessor> proc(comp);
    if (!proc)
        return out;
    SpeakerArrangement stereo = SpeakerArr::kStereo;
    ProcessSetup setup { kRealtime, kSample32, CMP_BLOCK, CMP_RATE };
    check("processor takes stereo in and out", proc->setBusArrangements(&stereo, 1, &stereo, 1) == kResultTrue);
    check("processor sets up at 48 kHz", proc->setupProcessing(setup) == kResultOk);
    comp->activateBus(kAudio, kInput, 0, true);
    comp->activateBus(kAudio, kOutput, 0, true);
    check("processor activates", comp->setActive(true) == kResultOk);
    proc->setProcessing(true);

    HostProcessData data;
    ParameterChanges changes(8), outs(8);
    data.prepare(*comp, CMP_BLOCK, kSample32);
    data.numSamples = CMP_BLOCK;
    data.inputParameterChanges = &changes;
    data.outputParameterChanges = &outs;
    for (int b = 0; b < CMP_BLOCKS; ++b) {
        int32 index = 0;
        changes.clearQueue();
        outs.clearQueue();
        if (b == 0)
            for (const auto& c : CMP_CHANGES)
                if (IParamValueQueue* q = changes.addParameterData(c.port, index))
                    q->addPoint(0, bl_to_norm(bl_param(c.port), c.plain), index);
        if (b == CMP_BYPASS)
            if (IParamValueQueue* q = changes.addParameterData(PORT_ENABLED, index)) /* the bypass parameter */
                q->addPoint(0, 1.0, index);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < CMP_BLOCK; ++i)
                data.inputs[0].channelBuffers32[c][i] = cmp_input(c, b * CMP_BLOCK + i);
        if (proc->process(data) != kResultOk)
            break;
        for (int32 k = 0; k < outs.getParameterCount(); ++k)
            if (IParamValueQueue* q = outs.getParameterData(k))
                if (q->getParameterId() == PORT_LED_A || q->getParameterId() == PORT_LED_B)
                    ++*led_updates;
        for (int i = 0; i < CMP_BLOCK; ++i)
            out.push_back(data.outputs[0].channelBuffers32[0][i]), out.push_back(data.outputs[0].channelBuffers32[1][i]);
    }
    proc->setProcessing(false);
    comp->setActive(false);
    data.unprepare();
    return out;
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::fprintf(stderr, "usage: vst3_check <Davidian Bi-Lumen.vst3> <bi_lumen.lv2>\n");
        return 2;
    }
    HostApplication host_app;
    PluginContextFactory::instance().setPluginContext(&host_app);
    std::string error;
    auto module = VST3::Hosting::Module::create(argv[1], error);
    check("loads the VST3 bundle", module != nullptr);
    if (!module) {
        std::printf("  %s\n", error.c_str());
        return 1;
    }
    const auto& factory = module->getFactory();
    check("factory vendor is Davidian", factory.info().vendor() == "Davidian");
    IPtr<PlugProvider> provider;
    for (const auto& ci : factory.classInfos())
        if (ci.category() == kVstAudioEffectClass) {
            check("one effect class named Bi-Lumen", ci.name() == "Bi-Lumen" && !provider);
            provider = owned(new PlugProvider(factory, ci, true));
        }
    if (!provider || !provider->initialize()) {
        check("component and controller start", false);
        return 1;
    }
    IPtr<IComponent> comp = provider->getComponentPtr();
    IPtr<IEditController> ctrl = provider->getControllerPtr();
    Handler handler;
    ctrl->setComponentHandler(&handler);

    {
        int bypass = 0, readonly = 0;
        ParameterInfo info;
        for (int32 i = 0; i < ctrl->getParameterCount(); ++i)
            if (ctrl->getParameterInfo(i, info) == kResultOk) {
                bypass += (info.flags & ParameterInfo::kIsBypass) && info.id == PORT_ENABLED;
                readonly += (info.flags & ParameterInfo::kIsReadOnly) != 0;
            }
        check("23 parameters: 20 controls, bypass and two lamps", ctrl->getParameterCount() == 23);
        check("one bypass parameter", bypass == 1);
        check("the two opto lamps are read only", readonly == 2);
        String128 text;
        ctrl->getParamStringByValue(PORT_RATE1, bl_to_norm(bl_param(PORT_RATE1), 0.5f), text);
        check("rate shows as 0.50", text[0] == '0' && text[1] == '.' && text[2] == '5' && text[3] == '0');
    }

    test_editor(ctrl, handler);

    int led_updates = 0;
    const std::vector<float> vst3 = run_vst3(comp, &led_updates);
    float* lv2 = cmp_run_lv2(argv[2]);
    check("both formats produce a full run of audio", vst3.size() == 2 * CMP_FRAMES && lv2);
    if (vst3.size() == 2 * CMP_FRAMES && lv2) {
        double peak, diff, wet;
        cmp_measure(vst3.data(), lv2, &peak, &diff, &wet);
        std::printf("  peak %.3f, largest VST3/LV2 difference %.2e, bypassed difference from input %.2e\n",
                    peak, diff, wet);
        check("VST3 output matches the LV2 build", peak > 0.05 && diff < 1e-5);
        check("bypass passes the input through", wet < 1e-6);
    }
    free(lv2);
    check("opto lamps reach the controller", led_updates > 0);

    ctrl->setComponentHandler(nullptr);
    provider = nullptr;
    std::printf(failures ? "\n%d failed\n" : "\nAll passed\n", failures);
    return failures ? 1 : 0;
}
