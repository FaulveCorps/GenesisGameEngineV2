#pragma once

#include "engine/ISubsystem.h"
#include <string>
#include <vector>

namespace Genesis::Engine {

class ISave : public ISubsystem {
public:
    virtual ~ISave() = default;

    // Save raw string data to a named slot (returns true on success)
    virtual bool Save(const std::string& slot, const std::string& data) = 0;

    // Load data from a named slot; returns true if the slot exists and data was loaded
    virtual bool Load(const std::string& slot, std::string& out) = 0;

    // Delete a named slot; returns true if deletion succeeded (or slot didn't exist)
    virtual bool Delete(const std::string& slot) = 0;

    // List available save slot names
    virtual std::vector<std::string> ListSlots() const = 0;
};

} // namespace Genesis::Engine
