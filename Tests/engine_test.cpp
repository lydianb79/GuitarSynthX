// engine_test.cpp — off-device test & benchmark for the GSXCore DSP engine.
// Build (Linux/macOS):  see Tests/run_tests.sh
#include "../Sources/GSXCore/Engine.hpp"
#include "../Sources/GSXCore/include/GSXCore.h"
#include <chrono>
#include <cstdio>
#include <random>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <string>

using namespace gsx;

// Guitar-like plucked string: Karplus-Strong with fractional delay, pluck-position
// comb on the excitation and a pickup-position comb on the output (gives the
// weak-fundamental spectra that cause octave errors in naive trackers).
struct PluckedString {
    std::vector<float> line; int len = 0; float frac = 0; int w = 0;
    float last = 0, damp = 0.5f, decay = 0.996f;
    std::vector<float> pick; int pw = 0, pickDelay = 1;
    void pluck(float hz, float fs, float amp, std::mt19937& rng, float pluckPos = 0.18f, float pickupPos = 0.12f) {
        const float period = fs / hz - 0.5f; // loop filter adds half a sample
        len = static_cast<int>(period); frac = period - len;
        line.assign(len + 2, 0.0f);
        std::uniform_real_distribution<float> u(-1.0f, 1.0f);
        std::vector<float> ex(len + 2);
        float lp = 0;
        for (int i = 0; i < len + 2; ++i) { lp += 0.6f * (u(rng) - lp); ex[i] = lp; }
        const int pp = std::max(1, static_cast<int>(pluckPos * len));
        for (int i = 0; i < len + 2; ++i) line[i] = amp * (ex[i] - (i >= pp ? ex[i - pp] : 0.0f));
        w = 0;
        decay = std::pow(0.001f, 1.0f / (4.0f * hz)); // ~4 s T60 per period
        pickDelay = std::max(1, static_cast<int>(pickupPos * (period + 0.5f)));
        pick.assign(pickDelay + 1, 0.0f); pw = 0;
    }
    float next() {
        if (len == 0) return 0.0f;
        const int n = len + 2;
        const float a = line[w];                 // written len steps ago
        const float b = line[(w + n - 1) % n];   // written len+1 steps ago
        const float y = a + frac * (b - a);
        const float fb = decay * (damp * y + (1 - damp) * last);
        last = y;
        line[(w + len) % n] = fb;
        w = (w + 1) % n;
        const float old = pick[pw];
        pick[pw] = y; pw = (pw + 1) % static_cast<int>(pick.size());
        return y - old;
    }
};

static float midiHz(int m) { return 440.0f * std::pow(2.0f, (m - 69) / 12.0f); }

struct Result { int notes = 0, correct = 0, octave = 0, missed = 0; double latSum = 0, latMax = 0; };

static void testDetection(double fs, int stability, Result& res, bool verbose) {
    Engine eng;
    eng.prepare(fs, 1);
    eng.setParam(P_TRK_STABILITY, static_cast<float>(stability));
    std::mt19937 rng(42);
    const int block = 64;
    std::vector<float> in(block), L(block), R(block);
    const float* chans[1] = {in.data()};

    // E2 .. E6 chromatic, each note 300 ms, re-picked; random pluck/pickup positions
    for (int m = 40; m <= 84; ++m) {
        PluckedString s;
        std::uniform_real_distribution<float> pos(0.08f, 0.3f), amp(0.15f, 0.6f);
        s.pluck(midiHz(m), static_cast<float>(fs), amp(rng), rng, pos(rng), pos(rng) * 0.6f);
        const uint32_t startCounter = eng.tracker(0).noteCounter();
        int detectAt = -1; float detectedMidi = 0; float firstMidi = 0;
        const int total = static_cast<int>(0.30 * fs);
        for (int pos0 = 0; pos0 < total; pos0 += block) {
            for (int i = 0; i < block; ++i) in[i] = s.next();
            eng.process(chans, 1, L.data(), R.data(), block);
            const auto& tr = eng.tracker(0);
            if (detectAt < 0 && tr.noteCounter() != startCounter && tr.noteActive()) {
                detectAt = pos0 + block; firstMidi = hzToMidi(tr.frequency());
            }
            if (pos0 > static_cast<int>(0.15 * fs) && tr.noteActive()) detectedMidi = hzToMidi(tr.frequency());
        }
        ++res.notes;
        if (detectAt < 0) { ++res.missed; std::printf("  midi %d: MISSED (no retrigger; legato pitch at 150ms+ = %.2f)\n", m, detectedMidi); continue; }
        const double lat = 1000.0 * detectAt / fs;
        res.latSum += lat; res.latMax = std::max(res.latMax, lat);
        const bool firstOk = std::fabs(firstMidi - m) < 0.5f;
        const bool steadyOk = std::fabs(detectedMidi - m) < 0.5f;
        if (firstOk && steadyOk) ++res.correct;
        else if (std::fabs(std::fabs(firstMidi - m) - 12) < 0.6f || std::fabs(std::fabs(detectedMidi - m) - 12) < 0.6f) ++res.octave;
        if (verbose || !(firstOk && steadyOk))
            std::printf("  midi %d: latency %.1f ms (block-quantised), first %.2f steady %.2f %s\n",
                        m, lat, firstMidi, detectedMidi, (firstOk && steadyOk) ? "" : "<-- WRONG");
    }
}

static void testLegatoAndSilence(double fs) {
    Engine eng; eng.prepare(fs, 1);
    std::mt19937 rng(7);
    const int block = 32;
    std::vector<float> in(block), L(block), R(block);
    const float* chans[1] = {in.data()};
    // Note on A3, then (no new pluck) frequency glide to C4 via a second string
    // crossfade (approximates a slide), then silence -> note off.
    PluckedString a, c;
    a.pluck(midiHz(57), (float)fs, 0.5f, rng);
    c.pluck(midiHz(60), (float)fs, 0.5f, rng);
    for (int i = 0; i < (int)(0.25 * fs); ++i) c.next();   // no pick transient: a slide
    int noteOffAt = -1;
    uint32_t counterAfterA = 0; float midiAtEndOfSlide = 0;
    const int total = static_cast<int>(1.2 * fs);
    for (int p = 0; p < total; p += block) {
        for (int i = 0; i < block; ++i) {
            const float t = (p + i) / (float)fs;
            float x;
            if (t < 0.3f) x = a.next();
            else if (t < 0.32f) { const float k = (t - 0.3f) / 0.02f; x = (1 - k) * a.next() + k * 0.6f * c.next(); }
            else if (t < 0.7f) x = 0.6f * c.next();
            else x = 0.0f;
            in[i] = x;
        }
        eng.process(chans, 1, L.data(), R.data(), block);
        const float t = (p + block) / (float)fs;
        if (t < 0.3f) counterAfterA = eng.tracker(0).noteCounter();
        if (t > 0.6f && t < 0.7f) midiAtEndOfSlide = hzToMidi(eng.tracker(0).frequency());
        if (t > 0.7f && noteOffAt < 0 && !eng.tracker(0).noteActive()) noteOffAt = p + block;
    }
    std::printf("Legato: counter A=%u, after slide=%u (equal => legato, no retrigger), pitch at end %.2f (expect 60)\n",
                counterAfterA, eng.tracker(0).noteCounter(), midiAtEndOfSlide);
    std::printf("Note-off after silence: %.1f ms\n", noteOffAt > 0 ? 1000.0 * noteOffAt / fs - 700.0 : -1.0);
}

static void testRepick(double fs) {
    Engine eng; eng.prepare(fs, 1);
    std::mt19937 rng(3);
    const int block = 32;
    std::vector<float> in(block), L(block), R(block);
    const float* chans[1] = {in.data()};
    // Low E ringing, then pick G3 while E still decays (overlapping) — the
    // retrigger must come from post-onset samples only.
    PluckedString e, g;
    e.pluck(midiHz(40), (float)fs, 0.5f, rng);
    bool gStarted = false; int gStartSample = 0; int detectAt = -1; float detMidi = 0; uint32_t c0 = 0;
    for (int p = 0; p < (int)(0.8 * fs); p += block) {
        for (int i = 0; i < block; ++i) {
            const int n = p + i;
            if (!gStarted && n >= (int)(0.4 * fs)) { g.pluck(midiHz(55), (float)fs, 0.6f, rng); gStarted = true; gStartSample = n; c0 = eng.tracker(0).noteCounter(); }
            in[i] = 0.15f * e.next() + (gStarted ? g.next() : 0.0f);
        }
        eng.process(chans, 1, L.data(), R.data(), block);
        if (gStarted && detectAt < 0 && eng.tracker(0).noteCounter() != c0) { detectAt = p + block; detMidi = hzToMidi(eng.tracker(0).frequency()); }
    }
    std::printf("Re-pick E2 -> G3 (overlapping): detected %.2f (expect 55) after %.1f ms\n",
                detMidi, detectAt > 0 ? 1000.0 * (detectAt - gStartSample) / fs : -1.0);
}

static void testFalseRetrigger(double fs) {
    // Long sustained notes with slow vibrato-like amplitude wobble: expect exactly one note-on each.
    Engine eng; eng.prepare(fs, 1);
    std::mt19937 rng(11);
    const int block = 64; std::vector<float> in(block), L(block), R(block); const float* ch[1] = {in.data()};
    int extra = 0, notes = 0;
    for (int m : {40, 45, 52, 57, 64, 69, 76}) {
        // silence gap so each note starts fresh
        std::fill(in.begin(), in.end(), 0.0f);
        for (int b = 0; b < (int)(0.3 * fs / block); ++b) eng.process(ch, 1, L.data(), R.data(), block);
        PluckedString s; s.pluck(midiHz(m), (float)fs, 0.5f, rng);
        const uint32_t c0 = eng.tracker(0).noteCounter();
        for (int b = 0; b < (int)(2.5 * fs / block); ++b) {
            for (int i = 0; i < block; ++i) {
                const float t = (b * block + i) / (float)fs;
                in[i] = s.next() * (1.0f + 0.3f * std::sin(6.28f * 5.5f * t));
            }
            eng.process(ch, 1, L.data(), R.data(), block);
        }
        const int n = (int)(eng.tracker(0).noteCounter() - c0);
        ++notes; extra += std::max(0, n - 1);
        if (n != 1) std::printf("  midi %d: %d note-ons (expected 1)\n", m, n);
    }
    std::printf("False retrigger test fs=%.0f: %d notes, %d spurious retriggers\n", fs, notes, extra);
}

static void benchmark(double fs, int buffer, bool hex, bool heavy = true) {
    Engine eng; eng.prepare(fs, hex ? 6 : 1);
    eng.setParam(P_IN_MODE, hex ? 1.0f : 0.0f);
    if (heavy) { eng.setParam(P_OSCA_UNISON, 7);
    for (int id : {P_DRV_ON, P_CHO_ON, P_PHA_ON, P_DLY_ON, P_REV_ON}) eng.setParam(id, 1); }
    else eng.setParam(P_MST_SYNTH, -60);
    std::mt19937 rng(1);
    PluckedString s[6];
    const int notes[6] = {40, 45, 50, 55, 59, 64};
    for (int i = 0; i < 6; ++i) s[i].pluck(midiHz(notes[i]), (float)fs, 0.5f, rng);
    std::vector<std::vector<float>> in(6, std::vector<float>(buffer));
    std::vector<const float*> ptr(6);
    for (int i = 0; i < 6; ++i) ptr[i] = in[i].data();
    std::vector<float> L(buffer), R(buffer);
    const int blocks = static_cast<int>(10.0 * fs / buffer);
    double worst = 0, total = 0;
    int maxVoices = 0;
    std::vector<double> times; times.reserve(blocks);
    for (int b = 0; b < blocks; ++b) {
        for (int c = 0; c < 6; ++c)
            for (int i = 0; i < buffer; ++i) in[c][i] = s[c].next();
        if (!hex) for (int i = 0; i < buffer; ++i) in[0][i] = s[0].next();
        const auto t0 = std::chrono::high_resolution_clock::now();
        eng.process(ptr.data(), hex ? 6 : 1, L.data(), R.data(), buffer);
        const auto t1 = std::chrono::high_resolution_clock::now();
        const double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
        total += us; worst = std::max(worst, us); times.push_back(us);
        maxVoices = std::max(maxVoices, eng.activeVoices.load());
        if (b % (blocks / 4) == 0 && b > 0) for (int i = 0; i < 6; ++i) s[i].pluck(midiHz(notes[i] + 2), (float)fs, 0.5f, rng);
    }
    const double budget = 1e6 * buffer / fs;
    std::sort(times.begin(), times.end());
    const double p999 = times[static_cast<size_t>(times.size() * 0.999)];
    std::printf("Bench %s fs=%.0f buf=%3d: avg load %5.1f%%, 99.9%% %5.1f%%, worst %6.1f%% of %.0f us budget (voices %d)\n",
                hex ? "HEX " : "MONO", fs, buffer, 100.0 * total / blocks / budget, 100.0 * p999 / budget,
                100.0 * worst / budget, budget, maxVoices);
}

static void sanity(double fs) {
    // All presets-ish extremes: max resonance, max drive, all FX, check finite & bounded.
    Engine eng; eng.prepare(fs, 1);
    eng.setParam(P_FLT_RES, 1.0f); eng.setParam(P_FLT_DRIVE, 1.0f);
    eng.setParam(P_DLY_FB, 0.95f); eng.setParam(P_REV_DECAY, 20.0f);
    for (int id : {P_DRV_ON, P_CHO_ON, P_PHA_ON, P_DLY_ON, P_REV_ON}) eng.setParam(id, 1);
    eng.setParam(P_PHA_FB, 0.9f); eng.setParam(P_OSC_FM, 1.0f); eng.setParam(P_MST_VOLUME, 6);
    eng.setParam(P_REV_SHIMMER, 1.0f); eng.setParam(P_DLY_MOD, 1.0f); eng.setParam(P_DLY_SYNC, 1.0f); eng.setParam(P_DLY_BPM, 40.0f);
    std::mt19937 rng(9); PluckedString s;
    const int block = 128; std::vector<float> in(block), L(block), R(block); const float* ch[1] = {in.data()};
    float peak = 0; bool finite = true;
    for (int b = 0; b < (int)(20 * fs / block); ++b) {
        if (b % 300 == 0) s.pluck(midiHz(40 + (b / 300) % 30), (float)fs, 0.9f, rng);
        for (int i = 0; i < block; ++i) in[i] = s.next();
        eng.process(ch, 1, L.data(), R.data(), block);
        for (int i = 0; i < block; ++i) { if (!std::isfinite(L[i]) || !std::isfinite(R[i])) finite = false; peak = std::max(peak, std::fabs(L[i])); }
        if (b % 1000 == 0) { eng.setParam(P_FLT_TYPE, (float)((b / 1000) % 4)); eng.setParam(P_DLY_MODE, (float)((b / 1000) % 3)); eng.setParam(P_REV_TYPE, (float)((b / 700) % 3)); eng.setParam(P_MST_DRYFX, (float)((b / 1000) % 2)); eng.setParam(P_MST_DRY, 0.0f); }
    }
    std::printf("Sanity (extreme settings, 20 s): finite=%s, peak=%.3f\n", finite ? "yes" : "NO", peak);

    // Silence in -> silence out (no self-noise / denormal loops)
    Engine e2; e2.prepare(fs, 1);
    std::fill(in.begin(), in.end(), 0.0f); float pk = 0;
    for (int b = 0; b < 500; ++b) { e2.process(ch, 1, L.data(), R.data(), block); for (float v : L) pk = std::max(pk, std::fabs(v)); }
    std::printf("Silence in -> peak out %.6f, voices %d\n", pk, e2.activeVoices.load());

    // Params API
    GSXParamInfo info; int n = gsx_param_count(); int bad = 0;
    for (int i = 0; i < n; ++i) { gsx_param_info(i, &info); if (info.defaultValue < info.minValue || info.defaultValue > info.maxValue || !info.key[0]) { ++bad; std::printf("bad param %d %s\n", i, info.key); } }
    std::printf("Param table: %d params, %d invalid\n", n, bad);
    float sc[1024], sp[96]; gsx_read_visual(sc, 1024, sp, 96);
}

static float rmsOf(const std::vector<float>& v, size_t a, size_t b) {
    double e = 0; for (size_t i = a; i < b; ++i) e += (double)v[i] * v[i];
    return (float)std::sqrt(e / std::max<size_t>(1, b - a));
}

static void testEffects(float fs) {
    // 1) Delay echo timing incl. tempo sync (120 BPM 1/8 dotted = 375 ms)
    for (int mode = 0; mode < 3; ++mode) {
        StereoDelay d; d.prepare(fs);
        DelaySettings ds; ds.mode = mode; ds.timeMs = 60000.0f / 120.0f * 0.75f; ds.feedback = 0.5f;
        ds.mix = 1.0f; ds.pingPong = false; ds.dampHz = 16000; ds.lowCutHz = 20; ds.mod = 0.0f;
        d.set(ds); d.reset();
        const int N = (int)(fs * 1.2f);
        std::vector<float> L(N, 0.0f), R(N, 0.0f);
        L[0] = R[0] = 1.0f;
        d.process(L.data(), R.data(), N);
        int peak = 1; float pv = 0;
        for (int i = (int)(0.05f * fs); i < (int)(0.6f * fs); ++i) if (std::fabs(L[i]) > pv) { pv = std::fabs(L[i]); peak = i; }
        std::printf("Delay %-7s first echo at %.1f ms (expect ~375 ms; tape wobbles a little)\n",
                    mode == 0 ? "Digital" : mode == 1 ? "Tape" : "Analog", 1000.0f * peak / fs);
    }
    // 2) Ducking: echo level while input is loud vs after it stops
    {
        StereoDelay d; d.prepare(fs);
        DelaySettings ds; ds.timeMs = 100; ds.feedback = 0.7f; ds.mix = 1.0f; ds.pingPong = false; ds.duck = 1.0f;
        d.set(ds); d.reset();
        const int N = (int)fs; std::vector<float> L(N), R(N), ref(N);
        for (int i = 0; i < N; ++i) { L[i] = R[i] = (i < N / 2) ? 0.5f * std::sin(kTwoPi * 220.0f * i / fs) : 0.0f; ref[i] = L[i]; }
        d.process(L.data(), R.data(), N);
        std::vector<float> wet(N); for (int i = 0; i < N; ++i) wet[i] = L[i] - ref[i];
        std::printf("Delay ducking: echo RMS while playing %.3f, after stopping %.3f (should rise)\n",
                    rmsOf(wet, N / 4, N / 2), rmsOf(wet, N / 2 + (int)(0.15f * fs), N / 2 + (int)(0.35f * fs)));
    }
    // 3) Reverb types + shimmer: finite, bounded, and the tail must decay
    for (int type = 0; type < 3; ++type) {
        for (float shimmer : {0.0f, 1.0f}) {
            Reverb r; r.prepare(fs);
            ReverbSettings rs; rs.type = type; rs.size = 1.0f; rs.decayS = 20.0f; rs.mix = 1.0f;
            rs.shimmer = shimmer; rs.mod = 1.0f; rs.dampHz = 18000;
            r.set(rs); r.reset();
            const int N = (int)(fs * 40); std::vector<float> L(N, 0.0f), R(N, 0.0f);
            for (int i = 0; i < (int)(0.5f * fs); ++i) L[i] = R[i] = 0.5f * std::sin(kTwoPi * 330.0f * i / fs);
            for (int off = 0; off < N; off += 512) r.process(L.data() + off, R.data() + off, std::min(512, N - off));
            bool finite = true; float pk = 0;
            for (int i = 0; i < N; ++i) { if (!std::isfinite(L[i])) finite = false; pk = std::max(pk, std::fabs(L[i])); }
            const float early = rmsOf(L, (size_t)(1 * fs), (size_t)(3 * fs));
            const float late = rmsOf(L, (size_t)(37 * fs), (size_t)(40 * fs));
            std::printf("Reverb %-5s shimmer %.0f (20 s decay): finite=%s peak=%.2f, tail %.1f dB at 37-40 s vs 1-3 s %s\n",
                        type == 0 ? "Room" : type == 1 ? "Hall" : "Plate", shimmer, finite ? "yes" : "NO", pk,
                        linToDb(late) - linToDb(early), late < early * 0.5f ? "(decays)" : "<-- NOT DECAYING");
        }
    }
    // 4) Param table order: every new key must map to its enum ID
    struct { int id; const char* key; } chk[] = {
        {P_DLY_MODE, "dly.mode"}, {P_DLY_SYNC, "dly.sync"}, {P_DLY_BPM, "dly.bpm"}, {P_DLY_LOWCUT, "dly.lowcut"},
        {P_DLY_MOD, "dly.mod"}, {P_DLY_DUCK, "dly.duck"}, {P_REV_TYPE, "rev.type"}, {P_REV_LOWCUT, "rev.lowcut"},
        {P_REV_MOD, "rev.mod"}, {P_REV_SHIMMER, "rev.shimmer"}, {P_REV_WIDTH, "rev.width"}, {P_REV_DUCK, "rev.duck"},
        {P_MST_DRYFX, "mst.dryfx"}, {P_MST_LIMITER, "mst.limiter"}, {P_AMP_VEL, "amp.vel"}};
    int bad = 0; for (auto& c : chk) if (std::string(paramDef(c.id).key) != c.key) { ++bad; std::printf("  ID mismatch %s\n", c.key); }
    std::printf("Param ID mapping: %d mismatches\n", bad);
}

int main() {
    testEffects(48000.0f);
    for (double fs : {44100.0, 48000.0, 96000.0}) {
        Result r;
        testDetection(fs, 2, r, false);
        std::printf("Detection fs=%.0f: %d notes, %d correct, %d octave errors, %d missed, avg latency %.1f ms, max %.1f ms\n\n",
                    fs, r.notes, r.correct, r.octave, r.missed, r.notes - r.missed ? r.latSum / (r.notes - r.missed) : 0, r.latMax);
    }
    for (double fs : {44100.0, 48000.0, 96000.0}) testFalseRetrigger(fs);
    testLegatoAndSilence(48000);
    testRepick(48000);
    sanity(48000);
    for (int buf : {32, 64, 128}) benchmark(48000, buf, false);
    for (int buf : {32, 64}) benchmark(48000, buf, true);
    benchmark(96000, 64, true);
    std::printf("(light: unison 1, no FX)\n");
    benchmark(48000, 64, false, false);
    benchmark(48000, 64, true, false);
    return 0;
}
