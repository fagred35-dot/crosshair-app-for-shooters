#include "CrosshairRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace aimpoint {
namespace {

constexpr float kPi = 3.14159265358979323846F;

struct PointF {
    float x;
    float y;
};

struct Segment {
    PointF from;
    PointF to;
};

COLORREF toColorRef(const RgbColor color) {
    return RGB(color.r, color.g, color.b);
}

PointF rotatePoint(const PointF point, const float angle) {
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    return {point.x * cosine - point.y * sine, point.x * sine + point.y * cosine};
}

POINT toPoint(const POINT center, const PointF point) {
    return {center.x + static_cast<LONG>(std::lround(point.x)),
            center.y + static_cast<LONG>(std::lround(point.y))};
}

void addRadial(std::vector<Segment>& segments, const float angle, const float gap, const float size) {
    const PointF direction{std::cos(angle), std::sin(angle)};
    segments.push_back({{direction.x * gap, direction.y * gap},
                        {direction.x * size, direction.y * size}});
}

void addPolyline(std::vector<Segment>& segments,
                 const std::vector<PointF>& points,
                 const bool closed = false) {
    if (points.size() < 2) {
        return;
    }
    for (std::size_t index = 1; index < points.size(); ++index) {
        segments.push_back({points[index - 1], points[index]});
    }
    if (closed) {
        segments.push_back({points.back(), points.front()});
    }
}

void addEllipse(std::vector<Segment>& segments,
                const PointF center,
                const float radiusX,
                const float radiusY,
                const float rotation = 0.0F,
                const int steps = 20) {
    std::vector<PointF> points;
    points.reserve(static_cast<std::size_t>(steps));
    for (int index = 0; index < steps; ++index) {
        const float angle = static_cast<float>(index) * 2.0F * kPi / static_cast<float>(steps);
        PointF point{std::cos(angle) * radiusX, std::sin(angle) * radiusY};
        point = rotatePoint(point, rotation);
        points.push_back({point.x + center.x, point.y + center.y});
    }
    addPolyline(segments, points, true);
}

void addArc(std::vector<Segment>& segments,
            const PointF center,
            const float radiusX,
            const float radiusY,
            const float startAngle,
            const float endAngle,
            const int steps = 14) {
    std::vector<PointF> points;
    points.reserve(static_cast<std::size_t>(steps + 1));
    for (int index = 0; index <= steps; ++index) {
        const float ratio = static_cast<float>(index) / static_cast<float>(steps);
        const float angle = startAngle + (endAngle - startAngle) * ratio;
        points.push_back({center.x + std::cos(angle) * radiusX,
                          center.y + std::sin(angle) * radiusY});
    }
    addPolyline(segments, points);
}

void drawSegments(HDC dc,
                  const POINT center,
                  const std::vector<Segment>& segments,
                  const float angle,
                  const COLORREF color,
                  const int width) {
    const HPEN pen = CreatePen(PS_SOLID, std::max(1, width), color);
    const HGDIOBJ oldPen = SelectObject(dc, pen);
    for (const auto& segment : segments) {
        const POINT from = toPoint(center, rotatePoint(segment.from, angle));
        const POINT to = toPoint(center, rotatePoint(segment.to, angle));
        MoveToEx(dc, from.x, from.y, nullptr);
        LineTo(dc, to.x, to.y);
    }
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void drawRing(HDC dc,
              const POINT center,
              const float radius,
              const COLORREF color,
              const int width) {
    const HPEN pen = CreatePen(PS_SOLID, std::max(1, width), color);
    const HGDIOBJ oldPen = SelectObject(dc, pen);
    const HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    const int r = std::max(1, static_cast<int>(std::lround(radius)));
    Ellipse(dc, center.x - r, center.y - r, center.x + r + 1, center.y + r + 1);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void drawDot(HDC dc, const POINT center, const float radius, const COLORREF color) {
    const HBRUSH brush = CreateSolidBrush(color);
    const HGDIOBJ oldBrush = SelectObject(dc, brush);
    const HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
    const int r = std::max(1, static_cast<int>(std::lround(radius)));
    Ellipse(dc, center.x - r, center.y - r, center.x + r + 1, center.y + r + 1);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(brush);
}

std::vector<Segment> makeSegments(const CrosshairShape shape, const float size, const float gap) {
    std::vector<Segment> segments;
    switch (shape) {
    case CrosshairShape::Classic:
        addRadial(segments, 0.0F, gap, size);
        addRadial(segments, kPi * 0.5F, gap, size);
        addRadial(segments, kPi, gap, size);
        addRadial(segments, kPi * 1.5F, gap, size);
        break;
    case CrosshairShape::Dot:
    case CrosshairShape::Circle:
        break;
    case CrosshairShape::Tee:
        addRadial(segments, 0.0F, gap, size);
        addRadial(segments, kPi * 0.5F, gap, size);
        addRadial(segments, kPi, gap, size);
        break;
    case CrosshairShape::Chevron:
        segments.push_back({{-size, size * 0.45F}, {0.0F, -size * 0.45F}});
        segments.push_back({{0.0F, -size * 0.45F}, {size, size * 0.45F}});
        segments.push_back({{0.0F, gap}, {0.0F, size * 0.85F}});
        break;
    case CrosshairShape::Diagonal:
        addRadial(segments, kPi * 0.25F, gap, size);
        addRadial(segments, kPi * 0.75F, gap, size);
        addRadial(segments, kPi * 1.25F, gap, size);
        addRadial(segments, kPi * 1.75F, gap, size);
        break;
    case CrosshairShape::Diamond:
        segments.push_back({{0.0F, -size}, {size, 0.0F}});
        segments.push_back({{size, 0.0F}, {0.0F, size}});
        segments.push_back({{0.0F, size}, {-size, 0.0F}});
        segments.push_back({{-size, 0.0F}, {0.0F, -size}});
        break;
    case CrosshairShape::CornerBox: {
        const float corner = size * 0.46F;
        for (const float sx : {-1.0F, 1.0F}) {
            for (const float sy : {-1.0F, 1.0F}) {
                segments.push_back({{sx * size, sy * size}, {sx * corner, sy * size}});
                segments.push_back({{sx * size, sy * size}, {sx * size, sy * corner}});
            }
        }
        break;
    }
    case CrosshairShape::Sniper:
        addRadial(segments, 0.0F, gap, size * 1.35F);
        addRadial(segments, kPi * 0.5F, gap, size * 1.35F);
        addRadial(segments, kPi, gap, size * 1.35F);
        addRadial(segments, kPi * 1.5F, gap, size * 1.35F);
        break;
    case CrosshairShape::Wings:
        segments.push_back({{-gap, 0.0F}, {-size, -size * 0.36F}});
        segments.push_back({{-gap, 0.0F}, {-size, size * 0.36F}});
        segments.push_back({{gap, 0.0F}, {size, -size * 0.36F}});
        segments.push_back({{gap, 0.0F}, {size, size * 0.36F}});
        break;
    case CrosshairShape::Triad:
        addRadial(segments, -kPi * 0.5F, gap, size);
        addRadial(segments, kPi / 6.0F, gap, size);
        addRadial(segments, kPi * 5.0F / 6.0F, gap, size);
        break;
    case CrosshairShape::Hexagon: {
        std::array<PointF, 6> points{};
        for (int i = 0; i < 6; ++i) {
            const float a = -kPi * 0.5F + static_cast<float>(i) * kPi / 3.0F;
            points[static_cast<std::size_t>(i)] = {std::cos(a) * size, std::sin(a) * size};
        }
        for (int i = 0; i < 6; ++i) {
            segments.push_back({points[static_cast<std::size_t>(i)],
                                points[static_cast<std::size_t>((i + 1) % 6)]});
        }
        break;
    }
    case CrosshairShape::Butterfly: {
        segments.push_back({{0.0F, -size * 0.62F}, {0.0F, size * 0.72F}});
        segments.push_back({{0.0F, -size * 0.55F}, {-size * 0.30F, -size}});
        segments.push_back({{0.0F, -size * 0.55F}, {size * 0.30F, -size}});
        for (const float side : {-1.0F, 1.0F}) {
            addPolyline(segments,
                        {{0.0F, -size * 0.36F}, {side * size * 0.48F, -size * 0.85F},
                         {side * size, -size * 0.36F}, {side * size * 0.72F, size * 0.08F},
                         {0.0F, size * 0.10F}}, true);
            addPolyline(segments,
                        {{0.0F, size * 0.10F}, {side * size * 0.68F, size * 0.18F},
                         {side * size * 0.78F, size * 0.72F}, {side * size * 0.22F, size * 0.54F}},
                        true);
        }
        break;
    }
    case CrosshairShape::Saturn:
        addEllipse(segments, {0.0F, 0.0F}, size * 0.58F, size * 0.58F, 0.0F, 24);
        addEllipse(segments, {0.0F, 0.0F}, size * 1.12F, size * 0.34F, -0.32F, 28);
        break;
    case CrosshairShape::Bee:
        addEllipse(segments, {0.0F, size * 0.04F}, size * 0.43F, size * 0.72F, 0.0F, 20);
        addEllipse(segments, {-size * 0.52F, -size * 0.34F}, size * 0.48F, size * 0.30F, -0.35F, 14);
        addEllipse(segments, {size * 0.52F, -size * 0.34F}, size * 0.48F, size * 0.30F, 0.35F, 14);
        segments.push_back({{-size * 0.38F, -size * 0.08F}, {size * 0.38F, -size * 0.08F}});
        segments.push_back({{-size * 0.36F, size * 0.20F}, {size * 0.36F, size * 0.20F}});
        segments.push_back({{-size * 0.16F, -size * 0.65F}, {-size * 0.42F, -size * 0.98F}});
        segments.push_back({{size * 0.16F, -size * 0.65F}, {size * 0.42F, -size * 0.98F}});
        break;
    case CrosshairShape::Crown:
        addPolyline(segments,
                    {{-size, size * 0.55F}, {-size * 0.78F, -size * 0.62F},
                     {-size * 0.28F, size * 0.02F}, {0.0F, -size},
                     {size * 0.28F, size * 0.02F}, {size * 0.78F, -size * 0.62F},
                     {size, size * 0.55F}}, false);
        segments.push_back({{-size, size * 0.55F}, {size, size * 0.55F}});
        segments.push_back({{-size * 0.85F, size * 0.78F}, {size * 0.85F, size * 0.78F}});
        break;
    case CrosshairShape::Heart: {
        std::vector<PointF> heart;
        heart.reserve(36);
        for (int index = 0; index < 36; ++index) {
            const float t = 2.0F * kPi * static_cast<float>(index) / 36.0F;
            const float x = 16.0F * std::pow(std::sin(t), 3.0F);
            const float y = 13.0F * std::cos(t) - 5.0F * std::cos(2.0F * t) -
                            2.0F * std::cos(3.0F * t) - std::cos(4.0F * t);
            heart.push_back({x * size / 18.0F, -y * size / 18.0F + size * 0.08F});
        }
        addPolyline(segments, heart, true);
        break;
    }
    case CrosshairShape::Star: {
        std::vector<PointF> star;
        star.reserve(10);
        for (int index = 0; index < 10; ++index) {
            const float radius = index % 2 == 0 ? size : size * 0.42F;
            const float a = -kPi * 0.5F + static_cast<float>(index) * kPi / 5.0F;
            star.push_back({std::cos(a) * radius, std::sin(a) * radius});
        }
        addPolyline(segments, star, true);
        break;
    }
    case CrosshairShape::Lightning:
        addPolyline(segments,
                    {{size * 0.18F, -size}, {-size * 0.62F, size * 0.08F},
                     {-size * 0.08F, size * 0.08F}, {-size * 0.30F, size},
                     {size * 0.68F, -size * 0.22F}, {size * 0.12F, -size * 0.22F}}, true);
        break;
    case CrosshairShape::Rocket:
        addPolyline(segments,
                    {{0.0F, -size}, {-size * 0.40F, -size * 0.30F},
                     {-size * 0.34F, size * 0.48F}, {0.0F, size * 0.70F},
                     {size * 0.34F, size * 0.48F}, {size * 0.40F, -size * 0.30F}}, true);
        addPolyline(segments, {{-size * 0.34F, size * 0.22F}, {-size * 0.78F, size * 0.72F},
                               {-size * 0.28F, size * 0.58F}}, false);
        addPolyline(segments, {{size * 0.34F, size * 0.22F}, {size * 0.78F, size * 0.72F},
                               {size * 0.28F, size * 0.58F}}, false);
        addPolyline(segments, {{-size * 0.16F, size * 0.66F}, {0.0F, size},
                               {size * 0.16F, size * 0.66F}}, false);
        addEllipse(segments, {0.0F, -size * 0.22F}, size * 0.15F, size * 0.15F, 0.0F, 12);
        break;
    case CrosshairShape::Alien:
        addEllipse(segments, {0.0F, -size * 0.05F}, size * 0.78F, size, 0.0F, 22);
        segments.push_back({{-size * 0.48F, -size * 0.20F}, {-size * 0.12F, size * 0.03F}});
        segments.push_back({{size * 0.48F, -size * 0.20F}, {size * 0.12F, size * 0.03F}});
        addArc(segments, {0.0F, size * 0.28F}, size * 0.28F, size * 0.16F, 0.25F, kPi - 0.25F, 8);
        break;
    case CrosshairShape::Cat:
        addPolyline(segments,
                    {{-size * 0.78F, size * 0.42F}, {-size * 0.76F, -size * 0.72F},
                     {-size * 0.26F, -size * 0.42F}, {0.0F, -size * 0.78F},
                     {size * 0.26F, -size * 0.42F}, {size * 0.76F, -size * 0.72F},
                     {size * 0.78F, size * 0.42F}, {0.0F, size * 0.88F}}, true);
        segments.push_back({{-size * 0.32F, -size * 0.02F}, {-size * 0.12F, size * 0.08F}});
        segments.push_back({{size * 0.32F, -size * 0.02F}, {size * 0.12F, size * 0.08F}});
        segments.push_back({{-size * 0.16F, size * 0.36F}, {-size, size * 0.20F}});
        segments.push_back({{-size * 0.16F, size * 0.46F}, {-size, size * 0.56F}});
        segments.push_back({{size * 0.16F, size * 0.36F}, {size, size * 0.20F}});
        segments.push_back({{size * 0.16F, size * 0.46F}, {size, size * 0.56F}});
        break;
    case CrosshairShape::Spider:
        addEllipse(segments, {0.0F, size * 0.22F}, size * 0.36F, size * 0.54F, 0.0F, 16);
        addEllipse(segments, {0.0F, -size * 0.38F}, size * 0.26F, size * 0.28F, 0.0F, 14);
        for (const float side : {-1.0F, 1.0F}) {
            for (int leg = 0; leg < 4; ++leg) {
                const float y = -size * 0.22F + static_cast<float>(leg) * size * 0.28F;
                segments.push_back({{side * size * 0.28F, y},
                                    {side * size * (0.66F + leg * 0.07F), y - size * 0.25F}});
                segments.push_back({{side * size * (0.66F + leg * 0.07F), y - size * 0.25F},
                                    {side * size, y + (leg < 2 ? -size * 0.42F : size * 0.42F)}});
            }
        }
        break;
    case CrosshairShape::Flower:
        addEllipse(segments, {0.0F, 0.0F}, size * 0.24F, size * 0.24F, 0.0F, 12);
        for (int petal = 0; petal < 6; ++petal) {
            const float a = static_cast<float>(petal) * kPi / 3.0F;
            addEllipse(segments, {std::cos(a) * size * 0.55F, std::sin(a) * size * 0.55F},
                       size * 0.28F, size * 0.50F, a + kPi * 0.5F, 12);
        }
        break;
    case CrosshairShape::Dragonfly:
        segments.push_back({{0.0F, -size * 0.84F}, {0.0F, size}});
        addEllipse(segments, {0.0F, -size * 0.72F}, size * 0.20F, size * 0.20F, 0.0F, 10);
        for (const float side : {-1.0F, 1.0F}) {
            addPolyline(segments, {{0.0F, -size * 0.38F}, {side * size, -size * 0.70F},
                                   {side * size * 0.72F, -size * 0.08F}, {0.0F, -size * 0.08F}}, true);
            addPolyline(segments, {{0.0F, -size * 0.02F}, {side * size * 0.82F, size * 0.15F},
                                   {side * size * 0.62F, size * 0.65F}, {0.0F, size * 0.28F}}, true);
        }
        break;
    case CrosshairShape::Ghost:
        addArc(segments, {0.0F, -size * 0.15F}, size * 0.76F, size * 0.78F, kPi, 2.0F * kPi, 18);
        addPolyline(segments,
                    {{-size * 0.76F, -size * 0.15F}, {-size * 0.76F, size * 0.76F},
                     {-size * 0.40F, size * 0.48F}, {-size * 0.12F, size * 0.82F},
                     {size * 0.18F, size * 0.48F}, {size * 0.46F, size * 0.80F},
                     {size * 0.76F, size * 0.48F}, {size * 0.76F, -size * 0.15F}}, false);
        segments.push_back({{-size * 0.36F, -size * 0.08F}, {-size * 0.16F, size * 0.08F}});
        segments.push_back({{size * 0.36F, -size * 0.08F}, {size * 0.16F, size * 0.08F}});
        break;
    case CrosshairShape::Skull:
        addPolyline(segments,
                    {{-size * 0.66F, size * 0.30F}, {-size * 0.78F, -size * 0.32F},
                     {-size * 0.48F, -size * 0.86F}, {0.0F, -size},
                     {size * 0.48F, -size * 0.86F}, {size * 0.78F, -size * 0.32F},
                     {size * 0.66F, size * 0.30F}, {size * 0.34F, size * 0.48F},
                     {size * 0.28F, size * 0.86F}, {-size * 0.28F, size * 0.86F},
                     {-size * 0.34F, size * 0.48F}}, true);
        addPolyline(segments, {{-size * 0.46F, -size * 0.18F}, {-size * 0.12F, -size * 0.02F},
                               {-size * 0.38F, size * 0.16F}}, true);
        addPolyline(segments, {{size * 0.46F, -size * 0.18F}, {size * 0.12F, -size * 0.02F},
                               {size * 0.38F, size * 0.16F}}, true);
        segments.push_back({{-size * 0.18F, size * 0.55F}, {-size * 0.18F, size * 0.86F}});
        segments.push_back({{size * 0.18F, size * 0.55F}, {size * 0.18F, size * 0.86F}});
        break;
    case CrosshairShape::Sword:
        addPolyline(segments, {{0.0F, -size}, {-size * 0.18F, size * 0.28F},
                               {0.0F, size * 0.50F}, {size * 0.18F, size * 0.28F}}, true);
        segments.push_back({{-size * 0.72F, size * 0.35F}, {size * 0.72F, size * 0.35F}});
        segments.push_back({{0.0F, size * 0.46F}, {0.0F, size * 0.88F}});
        addPolyline(segments, {{-size * 0.20F, size * 0.88F}, {0.0F, size},
                               {size * 0.20F, size * 0.88F}}, false);
        break;
    case CrosshairShape::Moon:
        addArc(segments, {0.0F, 0.0F}, size * 0.78F, size, -kPi * 0.5F, kPi * 0.5F, 20);
        addArc(segments, {-size * 0.28F, 0.0F}, size * 0.66F, size * 0.76F,
               kPi * 0.5F, -kPi * 0.5F, 18);
        addPolyline(segments, {{size * 0.48F, -size * 0.64F}, {size * 0.58F, -size * 0.42F},
                               {size * 0.82F, -size * 0.40F}, {size * 0.64F, -size * 0.24F},
                               {size * 0.70F, 0.0F}, {size * 0.48F, -size * 0.12F},
                               {size * 0.28F, 0.0F}, {size * 0.34F, -size * 0.30F},
                               {size * 0.18F, -size * 0.46F}, {size * 0.42F, -size * 0.48F}}, true);
        break;
    case CrosshairShape::Flame:
        addPolyline(segments,
                    {{0.0F, -size}, {-size * 0.10F, -size * 0.26F},
                     {-size * 0.58F, size * 0.12F}, {-size * 0.72F, size * 0.58F},
                     {-size * 0.32F, size}, {size * 0.40F, size * 0.88F},
                     {size * 0.72F, size * 0.28F}, {size * 0.38F, -size * 0.50F},
                     {size * 0.18F, size * 0.06F}}, true);
        addPolyline(segments, {{0.0F, size * 0.18F}, {-size * 0.24F, size * 0.66F},
                               {0.0F, size * 0.88F}, {size * 0.28F, size * 0.56F}}, true);
        break;
    }
    return segments;
}

} // namespace

void CrosshairRenderer::draw(HDC dc,
                             const POINT center,
                             const CrosshairPreset& preset,
                             const RenderOptions& options) {
    const float seconds = static_cast<float>(options.elapsedMilliseconds) / 1000.0F;
    float animationScale = 1.0F;
    float angle = 0.0F;
    if (preset.animation == Animation::Pulse) {
        animationScale = 0.90F + 0.12F * (std::sin(seconds * 5.0F) * 0.5F + 0.5F);
    } else if (preset.animation == Animation::Rotate) {
        angle = seconds * 1.35F;
    }

    const float scale = std::max(0.1F, options.scale * animationScale);
    const float size = preset.size * scale;
    const float gap = preset.gap * scale;
    const int width = std::max(1, static_cast<int>(std::lround(preset.thickness * scale)));
    const bool outline = options.outline && preset.outline;
    const auto segments = makeSegments(preset.shape, size, gap);

    if (outline) {
        drawSegments(dc, center, segments, angle, RGB(8, 10, 14), width + 3);
    }
    drawSegments(dc, center, segments, angle, toColorRef(preset.primary), width);

    if (preset.shape == CrosshairShape::Circle) {
        if (outline) {
            drawRing(dc, center, size + 1.0F, RGB(8, 10, 14), width + 3);
        }
        drawRing(dc, center, size, toColorRef(preset.primary), width);
    } else if (preset.shape == CrosshairShape::Sniper) {
        if (outline) {
            drawRing(dc, center, gap * 0.68F + 1.0F, RGB(8, 10, 14), width + 2);
        }
        drawRing(dc, center, gap * 0.68F, toColorRef(preset.accent), width);
    }

    const bool dot = preset.shape == CrosshairShape::Dot || preset.centerDot || options.centerDot;
    if (dot) {
        const float dotRadius = preset.shape == CrosshairShape::Dot
                                    ? std::max(2.0F, preset.thickness * 1.35F * scale)
                                    : std::max(1.3F, preset.thickness * 0.75F * scale);
        if (outline) {
            drawDot(dc, center, dotRadius + 1.5F, RGB(8, 10, 14));
        }
        drawDot(dc, center, dotRadius, toColorRef(preset.accent));
    }

    // The orbiting accent keeps rotation visible even for symmetrical shapes such as dots and rings.
    if (preset.animation == Animation::Rotate) {
        const float orbit = std::max(8.0F, size + std::max(3.0F, gap * 0.45F));
        const POINT tracer{center.x + static_cast<LONG>(std::lround(std::cos(angle) * orbit)),
                           center.y + static_cast<LONG>(std::lround(std::sin(angle) * orbit))};
        const float tracerRadius = std::max(1.2F, static_cast<float>(width) * 0.65F);
        if (outline) {
            drawDot(dc, tracer, tracerRadius + 1.2F, RGB(8, 10, 14));
        }
        drawDot(dc, tracer, tracerRadius, toColorRef(preset.accent));
    }
}

} // namespace aimpoint
