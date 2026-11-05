#ifndef PIM_RUNTIME_PATHS_H
#define PIM_RUNTIME_PATHS_H

#include <cstdlib>
#include <filesystem>
#include <string>

namespace pim::runtime_paths {

// Defaults belong to the checkout. Explicit relative overrides use caller cwd.
inline std::filesystem::path artifacts_dir() {
    if (const char* override_dir = std::getenv("PIM_ARTIFACTS_DIR");
        override_dir != nullptr && *override_dir != '\0') {
        return std::filesystem::absolute(override_dir);
    }
#ifdef PIM_SIM_DEFAULT_ARTIFACTS_DIR
    return std::filesystem::path(PIM_SIM_DEFAULT_ARTIFACTS_DIR);
#else
    return std::filesystem::current_path() / "artifacts";
#endif
}

inline std::string artifact_file(const std::string& group, const std::string& filename) {
    auto directory = artifacts_dir() / group;
    std::filesystem::create_directories(directory);
    return (directory / filename).string();
}

inline std::string run_file(const std::string& filename) {
    return artifact_file("runs", filename);
}

inline std::string log_file(const std::string& filename) {
    return artifact_file("logs", filename);
}

}  // namespace pim::runtime_paths
#endif
