#pragma once

#include <atomic>

namespace Genesis::Engine::Stats {

void Reset();
void AddDrawCalls(int n=1);
int GetDrawCalls();

} // namespace Genesis::Engine::Stats
