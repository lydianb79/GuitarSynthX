// replay.cpp — plays a GuitarSynthX input recording (Audio > Record Input) through
// the engine offline and prints every note start and end.
//
//   ./Tests/replay.sh "Input 2026-11-18 12.00.00.wav" [key=value ...]
//
// The settings file saved next to the WAV (same name, .txt) is applied first;
// key=value arguments override it, e.g. trk.release=0.
#include "../Sources/GSXCore/Engine.hpp"
#include "../Sources/GSXCore/Params.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace gsx;

static bool readWav(const char* path, std::vector<float>& out, int& channels, double& rate) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<char> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (d.size() < 44 || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4)) return false;
    auto u16 = [&](size_t o) { uint16_t v; std::memcpy(&v, &d[o], 2); return v; };
    auto u32 = [&](size_t o) { uint32_t v; std::memcpy(&v, &d[o], 4); return v; };
    int fmt = 0, bits = 0;
    for (size_t o = 12; o + 8 <= d.size();) {
        const uint32_t len = u32(o + 4);
        if (!std::memcmp(&d[o], "fmt ", 4)) {
            fmt = u16(o + 8); channels = u16(o + 10); rate = u32(o + 12); bits = u16(o + 22);
        } else if (!std::memcmp(&d[o], "data", 4)) {
            if (fmt != 3 || bits != 32) { std::fprintf(stderr, "expected 32-bit float WAV\n"); return false; }
            const size_t n = std::min<size_t>(len, d.size() - o - 8) / 4;
            out.resize(n);
            std::memcpy(out.data(), &d[o + 8], n * 4);
            return true;
        }
        o += 8 + len + (len & 1);
    }
    return false;
}

static void applySetting(Engine& eng, const std::string& line) {
    const size_t eq = line.find('=');
    if (eq == std::string::npos) return;
    auto trim = [](std::string s) {
        const size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t\r");
        return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
    };
    const std::string key = trim(line.substr(0, eq)), val = trim(line.substr(eq + 1));
    for (int id = 0; id < P_COUNT; ++id)
        if (key == paramDef(id).key) { eng.setParam(id, std::stof(val)); return; }
}

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: replay file.wav [key=value ...]\n"); return 1; }
    std::vector<float> data; int ch = 1; double fs = 48000;
    if (!readWav(argv[1], data, ch, fs)) { std::fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    Engine eng;
    eng.prepare(fs, ch);
    std::string txt = argv[1];
    if (txt.size() > 4) txt = txt.substr(0, txt.size() - 4) + ".txt";
    std::ifstream sf(txt);
    bool inParams = false;
    for (std::string line; std::getline(sf, line);) {
        if (line.rfind("[params]", 0) == 0) { inParams = true; continue; }
        if (inParams) applySetting(eng, line);
    }
    eng.setParam(P_IN_GAIN, 0.0f);            // the recording is already after Input Gain
    eng.setParam(P_IN_CHANNEL, 1.0f);
    for (int i = 2; i < argc; ++i) applySetting(eng, argv[i]);

    const int frames = static_cast<int>(data.size() / ch), block = 32;
    std::vector<std::vector<float>> planes(ch, std::vector<float>(block));
    std::vector<const float*> ptrs(ch);
    for (int c = 0; c < ch; ++c) ptrs[c] = planes[c].data();
    std::vector<float> L(block), R(block);
    const int nt = ch > 1 ? Engine::kMaxTrackers : 1;
    std::vector<int> active(nt, 0); std::vector<uint32_t> counter(nt, 0); std::vector<double> onAt(nt, 0);
    int notes = 0;
    std::printf("%s: %.1f s, %d ch, %.0f Hz, Note Off mode %.0f\n", argv[1], frames / fs, ch, fs, eng.getParam(P_TRK_RELEASE));
    for (int p = 0; p + block <= frames; p += block) {
        for (int i = 0; i < block; ++i)
            for (int c = 0; c < ch; ++c) planes[c][i] = data[static_cast<size_t>(p + i) * ch + c];
        eng.process(ptrs.data(), ch, L.data(), R.data(), block);
        const double t = (p + block) / fs;
        for (int k = 0; k < nt; ++k) {
            const PitchTracker& tr = eng.tracker(k);
            const bool a = tr.noteActive();
            if (a && (!active[k] || tr.noteCounter() != counter[k])) {
                if (active[k]) std::printf("%8.3f  [%d] end   (%.0f ms, re-picked)\n", t, k, (t - onAt[k]) * 1000);
                const float m = hzToMidi(tr.frequency());
                std::printf("%8.3f  [%d] start %6.1f Hz  midi %5.2f\n", t, k, tr.frequency(), m);
                onAt[k] = t; ++notes;
            } else if (!a && active[k]) {
                std::printf("%8.3f  [%d] end   (%.0f ms)\n", t, k, (t - onAt[k]) * 1000);
            }
            active[k] = a; counter[k] = tr.noteCounter();
        }
    }
    std::printf("%d notes\n", notes);
    return 0;
}
