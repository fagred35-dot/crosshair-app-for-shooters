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
