#include "CrosshairPreset.h"

#include <cassert>
#include <iostream>
#include <set>

int main() {
    const auto presets = aimpoint::makePresetCatalog();
    assert(presets.size() == 360);

    std::set<int> ids;
    std::set<std::wstring> names;
    std::size_t staticCount = 0;
    std::size_t animatedCount = 0;

    for (const auto& preset : presets) {
        assert(preset.size > 0.0F);
        assert(preset.thickness > 0.0F);
        assert(!preset.name.empty());
        ids.insert(preset.id);
        names.insert(preset.name);
        if (preset.animation == aimpoint::Animation::None) {
            ++staticCount;
        } else {
            ++animatedCount;
        }
    }

    assert(ids.size() == presets.size());
    assert(names.size() == presets.size());
    assert(staticCount == 120);
    assert(animatedCount == 240);

    std::cout << "Preset catalog: " << presets.size() << " unique presets ("
              << staticCount << " static, " << animatedCount << " animated)\n";
    return 0;
}
