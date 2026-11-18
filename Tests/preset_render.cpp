// preset_render.cpp: plays every factory preset through the real engine with a
// guitar-like phrase (A2, E3, A3, E4) and prints name|loudness LUFS|peak|finite|notes.
// Driven by Tests/check_presets.py.
// Renders every preset through the real engine with a guitar-like phrase and
// reports loudness, peak, NaN safety and whether it is audible at all.
#include "../Sources/GSXCore/Engine.hpp"
#include <cstdio>
#include <cmath>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>
using namespace gsx;

struct PluckedString {
    std::vector<float> line; int len = 0; float frac = 0; int w = 0; float last = 0, decay = 0.996f;
    std::vector<float> pick; int pw = 0;
    void pluck(float hz, float fs, float amp, std::mt19937& rng) {
        const float period = fs / hz - 0.5f; len = (int)period; frac = period - len;
        line.assign(len + 2, 0.0f); std::uniform_real_distribution<float> u(-1, 1);
        std::vector<float> ex(len + 2); float lp = 0;
        for (int i = 0; i < len + 2; ++i) { lp += 0.6f * (u(rng) - lp); ex[i] = lp; }
        const int pp = std::max(1, (int)(0.18f * len));
        for (int i = 0; i < len + 2; ++i) line[i] = amp * (ex[i] - (i >= pp ? ex[i - pp] : 0.0f));
        w = 0; decay = std::pow(0.001f, 1.0f / (4.0f * hz));
        pick.assign(std::max(1, (int)(0.12f * (period + 0.5f))) + 1, 0.0f); pw = 0;
    }
    float next() {
        if (!len) return 0; const int n = len + 2;
        const float a = line[w], b = line[(w + n - 1) % n], y = a + frac * (b - a);
        line[(w + len) % n] = decay * (0.5f * y + 0.5f * last); last = y; w = (w + 1) % n;
        const float o = pick[pw]; pick[pw] = y; pw = (pw + 1) % (int)pick.size(); return y - o;
    }
};

int main(int argc, char** argv) {
    const float fs = 48000;
    std::map<std::string, int> ids;
    for (int i = 0; i < P_COUNT; ++i) ids[paramDef(i).key] = i;
    std::ifstream in(argv[1]);
    std::string line;
    // Phrase: four notes across the guitar range, played moderately, 0.9 s each.
    const int notes[4] = {45, 52, 57, 64};
    while (std::getline(in, line)) {
        const auto bar = line.find('|');
        const std::string name = line.substr(0, bar);
        Engine eng; eng.prepare(fs, 1);
        // defaults for sound params already set; apply overrides
        std::stringstream ss(line.substr(bar + 1)); std::string kv;
        while (std::getline(ss, kv, ',')) {
            const auto eq = kv.find('='); if (eq == std::string::npos) continue;
            eng.setParam(ids.at(kv.substr(0, eq)), std::stof(kv.substr(eq + 1)));
        }
        std::mt19937 rng(5);
        PluckedString s;
        const int block = 128; std::vector<float> x(block), L(block), R(block); const float* ch[1] = {x.data()};
        double e = 0; long n = 0; float peak = 0; bool finite = true; int notesOn = 0; uint32_t c0 = 0;
        // K-weighting (ITU-R BS.1770): +4 dB high shelf ~1.7 kHz, then 38 Hz high-pass.
        Biquad shelf, hp;
        { const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
          const double K = std::tan(M_PI * f0 / fs), Vh = std::pow(10.0, G / 20.0), Vb = std::pow(Vh, 0.4996667741545416);
          const double a0 = 1.0 + K / Q + K * K;
          shelf.b0 = (float)((Vh + Vb * K / Q + K * K) / a0); shelf.b1 = (float)(2.0 * (K * K - Vh) / a0);
          shelf.b2 = (float)((Vh - Vb * K / Q + K * K) / a0); shelf.a1 = (float)(2.0 * (K * K - 1.0) / a0);
          shelf.a2 = (float)((1.0 - K / Q + K * K) / a0); }
        hp.setHighpass(38.0f, 0.5f, fs);
        const int win = (int)(0.4f * fs);              // momentary loudness window
        std::vector<float> ring(win, 0.0f); int rp = 0; double wsum = 0; double noteMax = 0, sumMax = 0;
        const int perNote = (int)(0.9f * fs);
        for (int k = 0; k < 4; ++k) {
            s.pluck(440.0f * std::pow(2.0f, (notes[k] - 69) / 12.0f), fs, 0.35f, rng);
            c0 = eng.tracker(0).noteCounter();
            bool counted = false;
            for (int pos = 0; pos < perNote; pos += block) {
                for (int i = 0; i < block; ++i) x[i] = s.next();
                eng.process(ch, 1, L.data(), R.data(), block);
                if (!counted && eng.tracker(0).noteCounter() != c0) { ++notesOn; counted = true; }
                for (int i = 0; i < block; ++i) {
                    if (!std::isfinite(L[i]) || !std::isfinite(R[i])) finite = false;
                    const float m = hp.process(shelf.process(0.5f * (L[i] + R[i])));
                    const float sq = m * m;
                    wsum += sq - ring[rp]; ring[rp] = sq; rp = (rp + 1) % win;
                    noteMax = std::max(noteMax, wsum / win);
                    e += sq; ++n; peak = std::max(peak, std::max(std::fabs(L[i]), std::fabs(R[i])));
                }
            }
            sumMax += noteMax; noteMax = 0;
        }
        (void)e; (void)n;
        const double rmsDb = 10.0 * std::log10(sumMax / 4.0 + 1e-20) - 0.691;   // ~LUFS momentary max
        std::printf("%s|%.2f|%.3f|%d|%d\n", name.c_str(), rmsDb, peak, finite ? 1 : 0, notesOn);
    }
}
