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
#include <cstdint>

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


// ---------------------------------------------------------------- note-off / fast playing
// Feeds `in` (mono) through a fresh engine in 32-sample blocks and records, per
// block, whether the tracker holds a note, its counter, its pitch and the synth
// output level. Returns the recorded timeline.
struct Frame { bool active; uint32_t counter; float midi; float outRms; };
static std::vector<Frame> runTimeline(Engine& eng, const std::vector<float>& in, int block = 32) {
    std::vector<Frame> tl;
    std::vector<float> x(block), L(block), R(block);
    const float* ch[1] = {x.data()};
    for (size_t p = 0; p + block <= in.size(); p += block) {
        for (int i = 0; i < block; ++i) x[i] = in[p + i];
        eng.process(ch, 1, L.data(), R.data(), block);
        double e = 0; for (int i = 0; i < block; ++i) e += L[i] * L[i];
        const auto& tr = eng.tracker(0);
        tl.push_back({tr.noteActive(), tr.noteCounter(), tr.noteActive() ? hzToMidi(tr.frequency()) : 0.0f,
                      (float)std::sqrt(e / block)});
    }
    return tl;
}

static const char* kReleaseNames[3] = {"Gate only", "Normal", "Fast"};

static void testNoteOff(double fs, int releaseMode) {
    std::printf("Note-off [%s] fs=%.0f (time from the mute/stop until the tracker releases the note):\n", kReleaseNames[releaseMode], fs);
    const int block = 32;
    const float msPerBlock = 1000.0f * block / (float)fs;
    struct Case { const char* name; int kind; };
    const Case cases[] = {{"mute -> silence", 0}, {"mute -> noise -50 dBFS", 1},
                          {"mute -> leaked tone -40 dBFS", 2}, {"let ring 8 s", 3}};
    for (int note : {40, 52, 64}) {
        for (const auto& c : cases) {
            Engine eng; eng.prepare(fs, 1); eng.setParam(P_TRK_RELEASE, (float)releaseMode);
            std::mt19937 rng(21); PluckedString s; s.pluck(midiHz(note), (float)fs, 0.5f, rng);
            Noise nz;
            const int ring = (int)(0.6 * fs);
            const int total = (int)((c.kind == 3 ? 8.6 : 1.6) * fs);
            std::vector<float> in(total);
            const float muteTau = 0.004f * (float)fs;   // fingers damp the string within a few ms
            for (int i = 0; i < total; ++i) {
                float v = s.next();
                if (c.kind != 3 && i >= ring) v *= std::exp(-(i - ring) / muteTau);
                if (c.kind != 3 && i >= ring) {
                    if (c.kind == 1) v += 0.0032f * nz.next() * 1.7f;                  // ~ -50 dBFS noise
                    if (c.kind == 2) v += 0.01f * std::sin(kTwoPi * midiHz(note) * i / (float)fs); // -40 dBFS tone
                }
                in[i] = v;
            }
            auto tl = runTimeline(eng, in, block);
            const int muteBlock = ring / block;
            int offAt = -1; uint32_t counterAtMute = tl[muteBlock].counter; int extraNotes = 0;
            for (size_t b = muteBlock; b < tl.size(); ++b) {
                if (offAt < 0 && !tl[b].active) offAt = (int)b;
                if (tl[b].counter != counterAtMute) { extraNotes = (int)(tl[b].counter - counterAtMute); }
            }
            if (c.kind == 3)
                std::printf("  midi %d %-30s note ends after %s\n", note, c.name,
                            offAt < 0 ? "never (8 s)" : (std::to_string((int)((offAt - muteBlock) * msPerBlock + 600)) + " ms").c_str());
            else
                std::printf("  midi %d %-30s %s%s\n", note, c.name,
                            offAt < 0 ? "NEVER released" : (std::to_string((int)((offAt - muteBlock) * msPerBlock)) + " ms").c_str(),
                            extraNotes ? "  (+spurious new note)" : "");
        }
    }
}

// A sustained note that behaves like a real string: a loud attack that settles
// 12..18 dB lower within ~100 ms, two vibration planes a fraction of a Hz apart
// (deep, slow level beating), vibrato, a slow decay and some hum and hiss. None of
// this is a mute, so the note must keep sounding.
static void testRealSustain(double fs) {
    std::printf("Sustain realism fs=%.0f (loud attack, beating 5..16 dB, vibrato, hum; no cut in the first 1.2 s):\n", fs);
    const int block = 32;
    const float msPerBlock = 1000.0f * block / (float)fs;
    for (int mode = 0; mode < 3; ++mode) {
        int cut = 0, total = 0; float earliest = 1e9f;
        for (int note : {40, 45, 50, 55, 59, 64, 69, 76}) {
            for (int variant = 0; variant < 3; ++variant) {
                Engine eng; eng.prepare(fs, 1); eng.setParam(P_TRK_RELEASE, (float)mode);
                std::mt19937 rng(note * 7 + variant);
                std::uniform_real_distribution<float> u(0.0f, 1.0f);
                const int len = (int)(3.2 * fs);
                std::vector<float> in(len);
                const float f0 = midiHz(note);
                const float beatHz = 0.4f + 1.6f * u(rng);            // plane-to-plane detuning
                const float planeB = 0.3f + 0.42f * u(rng);           // beating 5..16 dB deep
                const float attackDb = 12.0f + 6.0f * u(rng);
                const float vibDepth = variant == 2 ? 0.006f : 0.0f;  // ~10 cents
                float ph[12] = {}, phb[12] = {};
                for (int i = 0; i < len; ++i) {
                    const float t = (float)i / (float)fs;
                    const float env = std::exp(-t * (0.8f + note * 0.02f)) *
                                      (1.0f + (std::pow(10.0f, attackDb / 20.0f) - 1.0f) * std::exp(-t / 0.035f));
                    const float vib = 1.0f + vibDepth * std::sin(kTwoPi * 5.5f * t) * std::min(1.0f, t * 2.0f);
                    float v = 0.0f;
                    for (int h = 1; h <= 12; ++h) {
                        const float fh = f0 * h * vib;
                        if (fh > fs * 0.45f) break;
                        const float ah = (h == 1 ? 0.5f : 1.0f / h) * std::exp(-t * h * 0.4f) * (1.0f + std::sin(h * 1.7f) * 0.5f);
                        ph[h - 1] += kTwoPi * fh / (float)fs;
                        phb[h - 1] += kTwoPi * (fh + beatHz * h) / (float)fs;
                        v += ah * (std::sin(ph[h - 1]) + planeB * std::sin(phb[h - 1] + h));
                    }
                    v *= 0.12f * env;
                    v += 0.002f * std::sin(kTwoPi * 50.0f * t) + 0.0005f * (u(rng) * 2.0f - 1.0f);
                    in[i] = v;
                }
                auto tl = runTimeline(eng, in, block);
                int on = -1, off = -1;
                for (size_t b = 0; b < tl.size(); ++b) {
                    if (on < 0 && tl[b].active) on = (int)b;
                    if (on >= 0 && off < 0 && !tl[b].active) off = (int)b;
                }
                ++total;
                if (off >= 0 && (off - on) * msPerBlock < 1200.0f) { ++cut; earliest = std::min(earliest, (off - on) * msPerBlock); }
            }
        }
        std::printf("  %-10s notes cut early %d/%d%s\n", kReleaseNames[mode], cut, total,
                    cut ? (" (earliest after " + std::to_string((int)earliest) + " ms)").c_str() : "");
    }
}

static void testRecorder() {
    Engine eng; eng.prepare(48000, 1);
    std::vector<float> x(64), L(64), R(64);
    const float* ch[1] = {x.data()};
    eng.recordStart(1.0);
    int k = 0;
    for (int b = 0; b < 10; ++b) {
        for (auto& v : x) v = 0.001f * (float)(k++);
        eng.process(ch, 1, L.data(), R.data(), 64);
    }
    const int frames = eng.recordStop();
    bool ok = frames == 640 && eng.recordChannels() == 1;
    for (int i = 0; ok && i < frames; ++i) ok = std::fabs(eng.recordData()[i] - 0.001f * i) < 1e-6f;
    std::printf("Recorder: %d frames, data %s\n", frames, ok ? "OK" : "WRONG");
}

static void testFastSequence(double fs) {
    std::printf("Fast playing fs=%.0f (16th notes; hit = right note triggered within 40 ms of the pick):\n", fs);
    const int block = 32;
    for (double bpm : {150.0, 180.0}) {
        for (int mode = 0; mode < 2; ++mode) {    // 0 = same string (previous note cut), 1 = two strings alternating (previous rings on)
            Engine eng; eng.prepare(fs, 1);
            std::mt19937 rng(77);
            const int step = (int)(fs * 60.0 / bpm / 4.0);
            const int scale[] = {45, 47, 48, 50, 52, 53, 55, 57, 59, 60, 62, 64};
            std::uniform_int_distribution<int> pick(0, 11);
            std::uniform_real_distribution<float> amp(0.3f, 0.5f);
            const int nNotes = 48;
            std::vector<int> notes(nNotes);
            int prev = -1;
            for (int k = 0; k < nNotes; ++k) { int n; do { n = scale[pick(rng)]; } while (n == prev); notes[k] = prev = n; }
            PluckedString a, b;
            std::vector<float> in((size_t)step * nNotes + (size_t)(0.3 * fs));
            const int dampLen = (int)(0.004 * fs);   // the pick touches (damps) the string before plucking it
            for (int k = 0; k < nNotes; ++k) {
                PluckedString& cur = (mode == 1 && (k & 1)) ? b : a;
                if (k > 0 && mode == 0) {
                    size_t base = (size_t)k * step;
                    for (int i = 0; i < dampLen && base >= (size_t)dampLen; ++i)
                        in[base - dampLen + i] *= 1.0f - 0.85f * (float)i / dampLen;
                }
                cur.pluck(midiHz(notes[k]), (float)fs, amp(rng), rng);
                for (int i = 0; i < step; ++i) {
                    float v = cur.next();
                    if (mode == 1) { PluckedString& other = (k & 1) ? a : b; v += 0.6f * other.next(); }
                    in[(size_t)k * step + i] = v;
                }
            }
            auto tl = runTimeline(eng, in, block);
            int hits = 0; double latSum = 0; int wrongEvents = 0;
            for (int k = 0; k < nNotes; ++k) {
                const size_t b0 = (size_t)k * step / block, b1 = std::min(tl.size(), ((size_t)(k + 1) * step) / block);
                const uint32_t c0 = b0 > 0 ? tl[b0 - 1].counter : 0;
                int hitAt = -1;
                for (size_t bb = b0; bb < b1; ++bb) {
                    if (tl[bb].counter != c0 && tl[bb].active && std::fabs(tl[bb].midi - notes[k]) < 0.5f) {
                        if ((bb - b0) * block * 1000.0 / fs <= 40.0) hitAt = (int)(bb - b0);
                        break;
                    }
                }
                // wrong pitch held for a noticeable time (> 8 ms) within the note slot
                int wrongRun = 0, maxWrong = 0;
                for (size_t bb = b0; bb < b1; ++bb) {
                    const bool wrong = tl[bb].active && std::fabs(tl[bb].midi - notes[k]) >= 0.5f
                                       && (bb - b0) * block * 1000.0 / fs > 40.0;
                    wrongRun = wrong ? wrongRun + 1 : 0; maxWrong = std::max(maxWrong, wrongRun);
                }
                if (maxWrong * block * 1000.0 / fs > 8.0) ++wrongEvents;
                if (hitAt >= 0) { ++hits; latSum += (hitAt + 1) * block * 1000.0 / fs; }
            }
            std::printf("  %3.0f bpm %-16s hits %2d/%d, avg latency %.1f ms, wrong-note stretches %d\n",
                        bpm, mode ? "two strings" : "same string", hits, nNotes, hits ? latSum / hits : 0.0, wrongEvents);
        }
    }
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
    for (int m = 0; m < 3; ++m) testNoteOff(48000, m);
    testRealSustain(48000);
    testRecorder();
    testFastSequence(48000);
    if (std::getenv("GSX_QUICK")) return 0;
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
