#include "engine/SubsystemManager.h"
#include <iostream>

namespace Genesis::Engine {

SubsystemManager::~SubsystemManager() {
    DestroyAll();
}

std::shared_ptr<ISubsystem> SubsystemManager::CreateSubsystem(const std::string& type, const std::string& name) {
    auto inst = SubsystemRegistry::Instance().Create(type, name);
    if (!inst) {
        std::cerr << "SubsystemManager: no factory for " << type << "/" << name << std::endl;
        return nullptr;
    }
    if (!inst->Init()) {
        std::cerr << "SubsystemManager: Init failed for " << type << "/" << name << std::endl;
        return nullptr;
    }
    std::shared_ptr<ISubsystem> sp(std::move(inst));
    m_instances.push_back(sp);
    std::cout << "SubsystemManager: created instance " << type << "/" << name << std::endl;
    return sp;
}

void SubsystemManager::DestroyAll() {
    for (auto& s : m_instances) {
        if (s) {
            s->Shutdown();
        }
    }
    m_instances.clear();
}

} // namespace Genesis::Engine
