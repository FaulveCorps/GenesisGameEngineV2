#include "engine/Stats.h"
#include <atomic>

namespace Genesis::Engine::Stats {

static std::atomic<int> s_drawCalls{0};

void Reset() { s_drawCalls.store(0); }
void AddDrawCalls(int n) { s_drawCalls.fetch_add(n); }
int GetDrawCalls() { return s_drawCalls.load(); }

} // namespace Genesis::Engine::Stats
