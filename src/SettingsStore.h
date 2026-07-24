#pragma once

#include "AppSettings.h"

#include <string>

namespace aimpoint {

class SettingsStore {
public:
    SettingsStore();

    [[nodiscard]] AppSettings load() const;
    void save(const AppSettings& settings) const;
    [[nodiscard]] bool setRunAtStartup(bool enabled) const;
    [[nodiscard]] const std::wstring& path() const noexcept { return path_; }

private:
    std::wstring path_;
};

} // namespace aimpoint
