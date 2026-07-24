#include "CrosshairPreset.h"

#include <cassert>
#include <iostream>
#include <set>

int main() {
    const auto presets = aimpoint::makePresetCatalog();
    assert(presets.size() == static_cast<std::size_t>(aimpoint::kPresetCount));

    std::set<int> ids;
    std::set<std::wstring> names;
    std::size_t staticCount = 0;
    std::size_t animatedCount = 0;
    std::size_t artisticCount = 0;
    bool hasButterfly = false;
    bool hasSaturn = false;
    bool hasBee = false;

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
        if (preset.family == L"Арт-прицелы") {
            ++artisticCount;
        }
        hasButterfly = hasButterfly || preset.shape == aimpoint::CrosshairShape::Butterfly;
        hasSaturn = hasSaturn || preset.shape == aimpoint::CrosshairShape::Saturn;
        hasBee = hasBee || preset.shape == aimpoint::CrosshairShape::Bee;
    }

    assert(ids.size() == presets.size());
    assert(names.size() == presets.size());
    assert(staticCount == 300);
    assert(animatedCount == 600);
    assert(artisticCount == 540);
    assert(hasButterfly && hasSaturn && hasBee);

    std::cout << "Preset catalog: " << presets.size() << " unique presets ("
              << staticCount << " static, " << animatedCount << " animated, "
              << artisticCount << " artistic)\n";
    return 0;
}
