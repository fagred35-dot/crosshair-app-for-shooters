#include "CrosshairPreset.h"

#include <array>
#include <utility>

namespace aimpoint {
namespace {

struct ShapeDefinition {
    CrosshairShape shape;
    const wchar_t* name;
    const wchar_t* family;
    float size;
    float gap;
    float thickness;
    bool outline;
    bool dot;
};

struct PaletteDefinition {
    const wchar_t* name;
    RgbColor primary;
    RgbColor accent;
};

constexpr std::array<ShapeDefinition, 30> kShapes{{
    {CrosshairShape::Classic, L"Классика", L"Тактические", 18.0F, 5.0F, 2.0F, true, false},
    {CrosshairShape::Dot, L"Точка", L"Минимализм", 5.0F, 0.0F, 3.0F, true, true},
    {CrosshairShape::Circle, L"Кольцо", L"Минимализм", 14.0F, 3.0F, 2.0F, true, true},
    {CrosshairShape::Tee, L"T-прицел", L"Тактические", 19.0F, 5.0F, 2.0F, true, false},
    {CrosshairShape::Chevron, L"Шеврон", L"Динамические", 18.0F, 4.0F, 2.5F, true, true},
    {CrosshairShape::Diagonal, L"Диагональ", L"Минимализм", 17.0F, 5.0F, 2.0F, true, false},
    {CrosshairShape::Diamond, L"Ромб", L"Динамические", 15.0F, 2.0F, 2.0F, true, true},
    {CrosshairShape::CornerBox, L"Уголки", L"Тактические", 21.0F, 4.0F, 2.0F, true, false},
    {CrosshairShape::Sniper, L"Снайпер", L"Тактические", 25.0F, 7.0F, 1.5F, true, true},
    {CrosshairShape::Wings, L"Крылья", L"Динамические", 22.0F, 6.0F, 2.5F, true, true},
    {CrosshairShape::Triad, L"Триада", L"Динамические", 18.0F, 5.0F, 2.0F, true, true},
    {CrosshairShape::Hexagon, L"Гексагон", L"Динамические", 16.0F, 2.0F, 2.0F, true, false},
    {CrosshairShape::Butterfly, L"Бабочка", L"Арт-прицелы", 20.0F, 2.0F, 1.8F, true, true},
    {CrosshairShape::Saturn, L"Сатурн", L"Арт-прицелы", 19.0F, 2.0F, 1.8F, true, true},
    {CrosshairShape::Bee, L"Пчела", L"Арт-прицелы", 19.0F, 2.0F, 1.8F, true, true},
    {CrosshairShape::Crown, L"Корона", L"Арт-прицелы", 19.0F, 2.0F, 2.0F, true, true},
    {CrosshairShape::Heart, L"Сердце", L"Арт-прицелы", 18.0F, 2.0F, 2.0F, true, true},
    {CrosshairShape::Star, L"Звезда", L"Арт-прицелы", 18.0F, 2.0F, 2.0F, true, true},
    {CrosshairShape::Lightning, L"Молния", L"Арт-прицелы", 19.0F, 2.0F, 2.2F, true, true},
    {CrosshairShape::Rocket, L"Ракета", L"Арт-прицелы", 20.0F, 2.0F, 1.8F, true, true},
    {CrosshairShape::Alien, L"Пришелец", L"Арт-прицелы", 18.0F, 2.0F, 1.8F, true, true},
    {CrosshairShape::Cat, L"Кот", L"Арт-прицелы", 19.0F, 2.0F, 1.8F, true, true},
    {CrosshairShape::Spider, L"Паук", L"Арт-прицелы", 20.0F, 2.0F, 1.7F, true, true},
    {CrosshairShape::Flower, L"Цветок", L"Арт-прицелы", 19.0F, 2.0F, 1.7F, true, true},
    {CrosshairShape::Dragonfly, L"Стрекоза", L"Арт-прицелы", 20.0F, 2.0F, 1.7F, true, true},
    {CrosshairShape::Ghost, L"Призрак", L"Арт-прицелы", 18.0F, 2.0F, 1.8F, true, true},
    {CrosshairShape::Skull, L"Череп", L"Арт-прицелы", 18.0F, 2.0F, 1.8F, true, true},
    {CrosshairShape::Sword, L"Меч", L"Арт-прицелы", 20.0F, 2.0F, 1.8F, true, true},
    {CrosshairShape::Moon, L"Луна", L"Арт-прицелы", 18.0F, 2.0F, 1.8F, true, true},
    {CrosshairShape::Flame, L"Пламя", L"Арт-прицелы", 19.0F, 2.0F, 1.8F, true, true},
}};

constexpr std::array<PaletteDefinition, 10> kPalettes{{
    {L"Неон", {67, 255, 168}, {16, 185, 129}},
    {L"Лёд", {75, 224, 255}, {59, 130, 246}},
    {L"Лайм", {190, 255, 45}, {101, 210, 80}},
    {L"Янтарь", {255, 190, 60}, {249, 115, 22}},
    {L"Коралл", {255, 90, 105}, {239, 68, 68}},
    {L"Розовый", {255, 102, 221}, {192, 38, 211}},
    {L"Фиолет", {174, 126, 255}, {124, 58, 237}},
    {L"Белый", {245, 247, 250}, {180, 190, 205}},
    {L"Лазурь", {45, 212, 191}, {14, 116, 144}},
    {L"Красный", {255, 67, 67}, {185, 28, 28}},
}};

constexpr std::array<std::pair<Animation, const wchar_t*>, 3> kAnimations{{
    {Animation::None, L"Статика"},
    {Animation::Pulse, L"Пульс"},
    {Animation::Rotate, L"Вращение"},
}};

} // namespace

std::vector<CrosshairPreset> makePresetCatalog() {
    std::vector<CrosshairPreset> result;
    result.reserve(kShapes.size() * kPalettes.size() * kAnimations.size());

    int id = 0;
    for (const auto& shape : kShapes) {
        for (const auto& palette : kPalettes) {
            for (const auto& animationDefinition : kAnimations) {
                const Animation animation = animationDefinition.first;
                const wchar_t* animationLabel = animationDefinition.second;
                CrosshairPreset preset;
                preset.id = id++;
                preset.name = std::wstring(shape.name) + L" · " + palette.name + L" · " + animationLabel;
                preset.family = shape.family;
                preset.shape = shape.shape;
                preset.animation = animation;
                preset.primary = palette.primary;
                preset.accent = palette.accent;
                preset.size = shape.size;
                preset.gap = shape.gap;
                preset.thickness = shape.thickness;
                preset.outline = shape.outline;
                preset.centerDot = shape.dot;
                result.push_back(std::move(preset));
            }
        }
    }
    return result;
}

const wchar_t* animationName(const Animation animation) noexcept {
    switch (animation) {
    case Animation::None:
        return L"Без анимации";
    case Animation::Pulse:
        return L"Пульс";
    case Animation::Rotate:
        return L"Вращение";
    }
    return L"—";
}

bool isMinimalShape(const CrosshairShape shape) noexcept {
    return shape == CrosshairShape::Dot || shape == CrosshairShape::Circle ||
           shape == CrosshairShape::Diagonal;
}

} // namespace aimpoint
