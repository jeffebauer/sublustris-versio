// Finish, then go (owner, 9 Nov 2026): a SPACE change during a morph or a reshape waits for it to end. Real switch moves through
// setParam, read back from the Engine's own event log (ADR 0054).
#include <cstdio>
#include <vector>
#include "dsp/Engine.h"
#include "params/ParamSpec.h"

using namespace iv;
namespace {
constexpr int kBlock = 48;
constexpr int kBlocksPerMs = 1; // 48 samples at 48 kHz
struct Move { int ms, space; };
int failures = 0;

void check(bool ok, const char* what)
{
    if (!ok) { std::printf("FAIL: %s\n", what); ++failures; }
}

std::vector<Engine::MorphEvent> run(int from, const std::vector<Move>& moves, int ms)
{
    static Engine e;
    e.prepare(48000.0f, kBlock);
    e.setParam(ParamId::Space, switchToNormalised(from));
    e.reset();
    float in[kBlock], l[kBlock], r[kBlock];
    for (float& x : in) x = 0.01f;
    const int settle = 500 * kBlocksPerMs;
    size_t next = 0;
    for (int b = 0; b < settle + ms * kBlocksPerMs; ++b) {
        if (next < moves.size() && b - settle == moves[next].ms * kBlocksPerMs) e.setParam(ParamId::Space, switchToNormalised(moves[next++].space));
        e.process(in, in, l, r, kBlock);
    }
    const auto& s = e.morphLogForTests();
    return {s.events, s.events + s.count};
}

int count(const std::vector<Engine::MorphEvent>& ev, int kind)
{
    int n = 0;
    for (const auto& e : ev) n += e.kind == kind;
    return n;
}

// Everything in the log, in order: a start, its end, then the next start.
void expectTwoInTurn(const std::vector<Engine::MorphEvent>& ev, int startKind1, int endKind1, int startKind2, int endKind2, int firstTo, int secondTo, const char* what)
{
    check(ev.size() == 4 && ev[0].kind == startKind1 && ev[1].kind == endKind1 && ev[2].kind == startKind2 && ev[3].kind == endKind2, what);
    if (ev.size() == 4) {
        check(ev[0].to == firstTo && ev[2].to == secondTo, "goes to the right spaces");
        check(ev[2].t >= ev[1].t, "the second starts only after the first ends");
    }
}
} // namespace

int main()
{
    const int plate = 0, hall = 1, canyon = 2;
    expectTwoInTurn(run(hall, {{0, canyon}, {700, hall}}, 4000), 3, 5, 3, 5, canyon, hall, "HALL > CANYON > HALL: reshape, end, reshape");
    expectTwoInTurn(run(canyon, {{0, plate}, {1200, canyon}}, 7000), 1, 4, 1, 4, plate, canyon, "CANYON > PLATE > CANYON: morph, end, morph");
    expectTwoInTurn(run(canyon, {{0, plate}, {1200, hall}}, 7000), 1, 4, 1, 4, plate, hall, "CANYON > PLATE > HALL: morph, end, morph");

    auto wiggle = run(canyon, {{0, plate}, {700, canyon}, {1400, plate}}, 7000); // the switch is back at the space now playing by the end
    check(wiggle.size() == 2 && count(wiggle, 1) == 1 && count(wiggle, 4) == 1, "a switch back at the space now playing starts nothing");
    auto latest = run(canyon, {{0, plate}, {700, hall}, {1400, canyon}}, 7000); // the latest position, once
    expectTwoInTurn(latest, 1, 4, 1, 4, plate, canyon, "the latest position wins, once");
    check(count(run(canyon, {{0, plate}}, 4000), 2) == 0, "no swap without a freeze");

    if (failures == 0) std::puts("test_space_morph_waits: ok");
    return failures == 0 ? 0 : 1;
}
