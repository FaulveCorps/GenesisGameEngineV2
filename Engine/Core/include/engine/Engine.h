#pragma once

#include <string>

namespace Genesis::Engine {

bool Init(const std::string& config = "");
void Shutdown();

}
