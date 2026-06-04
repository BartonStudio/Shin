#include <Module.hpp>

namespace Shin {

    ModuleManager& ModuleManager::GetInstance() {
        static ModuleManager instance;
        return instance;
    }

} // namespace Shin
