#include "BlackMythWukongPseudoTrainerPanel.h"
#include "resource.h"

#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <objidl.h>
#include <shlwapi.h>

#include <gdiplus.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shlwapi.lib")

namespace
{
    using BlackMythWukong::PseudoTrainerPanel;
    using BlackMythWukong::TrainerSection;

    struct HitRegion
    {
        RECT Rect{};
        enum class Type
        {
            ToggleOption
        } RegionType = Type::ToggleOption;
        std::size_t SectionIndex = 0;
        std::size_t OptionIndex = 0;
    };

    struct AppState
    {
        PseudoTrainerPanel Panel;
        std::vector<HitRegion> HitRegions;
        HFONT TitleFont = nullptr;
        HFONT HeaderFont = nullptr;
        HFONT BodyFont = nullptr;
        HFONT SmallFont = nullptr;
        ULONG_PTR GdiPlusToken = 0;
        std::unique_ptr<Gdiplus::Image> GameIconImage;
        std::unique_ptr<Gdiplus::Image> BackgroundImage;
        std::unique_ptr<Gdiplus::Image> SplashBackgroundImage;
        IStream* GameIconStream = nullptr;
        IStream* BackgroundImageStream = nullptr;
        IStream* SplashBackgroundImageStream = nullptr;
        HBITMAP CachedBackgroundBitmap = nullptr;
        HBITMAP CachedSplashBitmap = nullptr;
        int CachedBackgroundWidth = 0;
        int CachedBackgroundHeight = 0;
        int CachedSplashWidth = 0;
        int CachedSplashHeight = 0;
        float SplashProgress = 0.0f;
        float DisplayedSplashProgress = 0.0f;
        int ScrollOffset = 0;
        int ContentHeight = 0;
        bool IsScrollThumbDragging = false;
        int ScrollDragStartY = 0;
        int ScrollDragStartOffset = 0;
    };

    constexpr int WindowWidth = 760;
    constexpr int WindowHeight = 760;
    constexpr int MinimumWindowWidth = 760;
    constexpr int MaximumWindowWidth = 760;
    constexpr int MinimumWindowHeight = 640;
    constexpr int OuterPadding = 20;
    constexpr int HeaderHeight = 72;
    constexpr int RowHeight = 36;
    constexpr int SectionGap = 10;
    constexpr int SectionTitleHeight = 26;
    constexpr int ToggleWidth = 54;
    constexpr int ToggleHeight = 26;
    constexpr int HotkeyWidth = 132;
    constexpr int CustomScrollBarWidth = 22;
    constexpr int CustomScrollBarMargin = 8;
    constexpr int CustomScrollBarMinThumbHeight = 56;
    constexpr int SplashWidth = 520;
    constexpr int SplashHeight = 280;
    constexpr DWORD SplashDurationMs = 5000;
    constexpr float BackgroundWidthFactor = 0.72f;
    constexpr float BackgroundVerticalOffset = 170.0f;
    AppState g_state;

    void EnsureFontsCreated();

    bool LoadImageFromResource(const UINT resourceId, std::unique_ptr<Gdiplus::Image>& image, IStream*& stream)
    {
        if (image)
        {
            return true;
        }

        if (g_state.GdiPlusToken == 0)
        {
            Gdiplus::GdiplusStartupInput startupInput;
            Gdiplus::GdiplusStartup(&g_state.GdiPlusToken, &startupInput, nullptr);
        }

        const HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
        if (resource == nullptr)
        {
            return false;
        }

        const DWORD resourceSize = SizeofResource(nullptr, resource);
        const HGLOBAL loadedResource = LoadResource(nullptr, resource);
        if (loadedResource == nullptr || resourceSize == 0)
        {
            return false;
        }

        const BYTE* resourceData = static_cast<const BYTE*>(LockResource(loadedResource));
        if (resourceData == nullptr)
        {
            return false;
        }

        IStream* imageStream = SHCreateMemStream(resourceData, resourceSize);
        if (imageStream == nullptr)
        {
            return false;
        }

        auto loadedImage = std::make_unique<Gdiplus::Image>(imageStream);
        if (loadedImage->GetLastStatus() != Gdiplus::Ok)
        {
            imageStream->Release();
            return false;
        }

        image = std::move(loadedImage);
        stream = imageStream;
        return true;
    }

    RECT MakeRect(const int left, const int top, const int right, const int bottom)
    {
        RECT rect{};
        rect.left = left;
        rect.top = top;
        rect.right = right;
        rect.bottom = bottom;
        return rect;
    }

    int RectWidth(const RECT& rect)
    {
        return rect.right - rect.left;
    }

    int RectHeight(const RECT& rect)
    {
        return rect.bottom - rect.top;
    }

    void FillRectColor(HDC dc, const RECT& rect, const COLORREF color)
    {
        const HBRUSH brush = CreateSolidBrush(color);
        FillRect(dc, &rect, brush);
        DeleteObject(brush);
    }

    void FillRoundRectColor(HDC dc, const RECT& rect, const COLORREF color, const int radius)
    {
        const HBRUSH brush = CreateSolidBrush(color);
        const HPEN pen = CreatePen(PS_SOLID, 1, color);
        const HGDIOBJ oldBrush = SelectObject(dc, brush);
        const HGDIOBJ oldPen = SelectObject(dc, pen);
        RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);
    }

    void DrawDiamondMarker(HDC dc, const int centerX, const int centerY, const int radius, const COLORREF color)
    {
        const HBRUSH brush = CreateSolidBrush(color);
        const HPEN pen = CreatePen(PS_SOLID, 1, color);
        const HGDIOBJ oldBrush = SelectObject(dc, brush);
        const HGDIOBJ oldPen = SelectObject(dc, pen);

        POINT points[4]{
            { centerX, centerY - radius },
            { centerX + radius, centerY },
            { centerX, centerY + radius },
            { centerX - radius, centerY }
        };

        Polygon(dc, points, 4);

        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);
    }

    void DrawTextLine(HDC dc, const std::wstring& text, RECT rect, const COLORREF color, HFONT font, const UINT format)
    {
        const HGDIOBJ oldFont = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, color);
        DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rect, format);
        SelectObject(dc, oldFont);
    }

    void DrawBadge(HDC dc, const RECT& rect, const COLORREF fill, const std::wstring& text, HFONT font, const COLORREF textColor)
    {
        FillRoundRectColor(dc, rect, fill, RectHeight(rect));
        DrawTextLine(dc, text, rect, textColor, font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    void EnsureGameIconLoaded()
    {
        LoadImageFromResource(IDR_GAME_ICON_IMAGE, g_state.GameIconImage, g_state.GameIconStream);
    }

    void EnsureSplashBackgroundImageLoaded()
    {
        LoadImageFromResource(IDR_SPLASH_BACKGROUND_IMAGE, g_state.SplashBackgroundImage, g_state.SplashBackgroundImageStream);
    }

    void EnsureBackgroundImageLoaded()
    {
        LoadImageFromResource(IDR_BACKGROUND_IMAGE, g_state.BackgroundImage, g_state.BackgroundImageStream);
    }

    void DrawBackgroundImage(HDC dc, const RECT& rect)
    {
        EnsureBackgroundImageLoaded();
        if (!g_state.BackgroundImage)
        {
            return;
        }

        const int targetWidth = RectWidth(rect);
        const int targetHeight = RectHeight(rect);
        if (targetWidth <= 0 || targetHeight <= 0)
        {
            return;
        }

        if (g_state.CachedBackgroundBitmap == nullptr ||
            g_state.CachedBackgroundWidth != targetWidth ||
            g_state.CachedBackgroundHeight != targetHeight)
        {
            if (g_state.CachedBackgroundBitmap != nullptr)
            {
                DeleteObject(g_state.CachedBackgroundBitmap);
                g_state.CachedBackgroundBitmap = nullptr;
            }

            Gdiplus::Bitmap renderedBitmap(targetWidth, targetHeight, PixelFormat32bppPARGB);
            Gdiplus::Graphics graphics(&renderedBitmap);
            graphics.Clear(Gdiplus::Color(255, 11, 13, 16));
            graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

            const float imageWidth = static_cast<float>(g_state.BackgroundImage->GetWidth());
            const float imageHeight = static_cast<float>(g_state.BackgroundImage->GetHeight());
            const float narrowedWidth = static_cast<float>(targetWidth) * BackgroundWidthFactor;
            const int backgroundLeft = static_cast<int>((static_cast<float>(targetWidth) - narrowedWidth) * 0.5f);
            const int backgroundWidth = static_cast<int>(narrowedWidth);
            const float scale = std::max(narrowedWidth / imageWidth, static_cast<float>(targetHeight) / imageHeight);
            const float drawWidth = imageWidth * scale;
            const float drawHeight = imageHeight * scale;
            const float drawX = (narrowedWidth - drawWidth) * 0.5f;
            const float drawY = (static_cast<float>(targetHeight) - drawHeight) * 0.5f + BackgroundVerticalOffset;

            Gdiplus::ImageAttributes attributes;
            Gdiplus::ColorMatrix matrix = {
                1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.18f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f, 1.0f
            };
            attributes.SetColorMatrix(&matrix, Gdiplus::ColorMatrixFlagsDefault, Gdiplus::ColorAdjustTypeBitmap);

            graphics.DrawImage(
                g_state.BackgroundImage.get(),
                Gdiplus::Rect(backgroundLeft, 0, backgroundWidth, targetHeight),
                static_cast<INT>(-drawX / scale),
                static_cast<INT>(-drawY / scale),
                static_cast<INT>(backgroundWidth / scale),
                static_cast<INT>(drawHeight / scale),
                Gdiplus::UnitPixel,
                &attributes);

            renderedBitmap.GetHBITMAP(Gdiplus::Color(11, 13, 16), &g_state.CachedBackgroundBitmap);
            g_state.CachedBackgroundWidth = targetWidth;
            g_state.CachedBackgroundHeight = targetHeight;
        }

        HDC memoryDc = CreateCompatibleDC(dc);
        HGDIOBJ oldBitmap = SelectObject(memoryDc, g_state.CachedBackgroundBitmap);
        BitBlt(dc, rect.left, rect.top, targetWidth, targetHeight, memoryDc, 0, 0, SRCCOPY);
        SelectObject(memoryDc, oldBitmap);
        DeleteDC(memoryDc);
    }

    void DrawGameIcon(HDC dc, const RECT& rect)
    {
        EnsureGameIconLoaded();

        if (!g_state.GameIconImage)
        {
            DrawBadge(dc, rect, RGB(45, 41, 34), L"B", g_state.TitleFont, RGB(235, 214, 176));
            return;
        }

        Gdiplus::Graphics graphics(dc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

        Gdiplus::GraphicsPath clipPath;
        clipPath.AddEllipse(static_cast<Gdiplus::REAL>(rect.left), static_cast<Gdiplus::REAL>(rect.top), static_cast<Gdiplus::REAL>(RectWidth(rect)), static_cast<Gdiplus::REAL>(RectHeight(rect)));
        graphics.SetClip(&clipPath);
        graphics.DrawImage(g_state.GameIconImage.get(), rect.left, rect.top, RectWidth(rect), RectHeight(rect));
        graphics.ResetClip();

        Gdiplus::Pen borderPen(Gdiplus::Color(90, 60, 56, 50), 1.0f);
        graphics.DrawEllipse(&borderPen, static_cast<Gdiplus::REAL>(rect.left), static_cast<Gdiplus::REAL>(rect.top), static_cast<Gdiplus::REAL>(RectWidth(rect) - 1), static_cast<Gdiplus::REAL>(RectHeight(rect) - 1));
    }

    float EaseSplashProgress(const float progress)
    {
        const float t = std::clamp(progress, 0.0f, 1.0f);
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    void AddRoundedRectPath(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& rect, const float radius)
    {
        const float diameter = radius * 2.0f;
        path.AddArc(rect.X, rect.Y, diameter, diameter, 180.0f, 90.0f);
        path.AddArc(rect.GetRight() - diameter, rect.Y, diameter, diameter, 270.0f, 90.0f);
        path.AddArc(rect.GetRight() - diameter, rect.GetBottom() - diameter, diameter, diameter, 0.0f, 90.0f);
        path.AddArc(rect.X, rect.GetBottom() - diameter, diameter, diameter, 90.0f, 90.0f);
        path.CloseFigure();
    }

    void DrawSplashCoverBackground(HDC dc, const RECT& rect)
    {
        FillRectColor(dc, rect, RGB(11, 13, 16));
        EnsureSplashBackgroundImageLoaded();
        if (!g_state.SplashBackgroundImage)
        {
            return;
        }

        Gdiplus::Graphics graphics(dc);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

        const float targetWidth = static_cast<float>(RectWidth(rect));
        const float targetHeight = static_cast<float>(RectHeight(rect));
        const float imageWidth = static_cast<float>(g_state.SplashBackgroundImage->GetWidth());
        const float imageHeight = static_cast<float>(g_state.SplashBackgroundImage->GetHeight());
        const float scale = std::max(targetWidth / imageWidth, targetHeight / imageHeight);
        const float drawWidth = imageWidth * scale;
        const float drawHeight = imageHeight * scale;
        const float drawX = (targetWidth - drawWidth) * 0.5f;
        const float drawY = (targetHeight - drawHeight) * 0.5f;

        Gdiplus::ImageAttributes attributes;
        Gdiplus::ColorMatrix matrix = {
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.22f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 1.0f
        };
        attributes.SetColorMatrix(&matrix, Gdiplus::ColorMatrixFlagsDefault, Gdiplus::ColorAdjustTypeBitmap);

        graphics.DrawImage(
            g_state.SplashBackgroundImage.get(),
            Gdiplus::Rect(rect.left, rect.top, RectWidth(rect), RectHeight(rect)),
            static_cast<INT>(std::floor((rect.left - drawX) / scale)),
            static_cast<INT>(std::floor((rect.top - drawY) / scale)),
            static_cast<INT>(std::ceil(targetWidth / scale)),
            static_cast<INT>(std::ceil(targetHeight / scale)),
            Gdiplus::UnitPixel,
            &attributes);
    }

    void DrawSplashBackground(HDC dc, const RECT& rect)
    {
        EnsureFontsCreated();
        DrawSplashCoverBackground(dc, rect);

        const RECT iconRect = MakeRect(rect.left + 28, rect.top + 28, rect.left + 100, rect.top + 100);
        DrawGameIcon(dc, iconRect);

        RECT titleRect = MakeRect(rect.left + 120, rect.top + 34, rect.left + 360, rect.top + 72);
        DrawTextLine(dc, L"Black Myth: Wukong", titleRect, RGB(236, 241, 245), g_state.TitleFont, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT statusRect = MakeRect(rect.left + 30, rect.bottom - 78, rect.left + 220, rect.bottom - 50);
        DrawTextLine(dc, L"Loading interface...", statusRect, RGB(188, 197, 204), g_state.SmallFont, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        const RECT progressTrackRect = MakeRect(rect.left + 30, rect.bottom - 40, rect.right - 30, rect.bottom - 24);
        FillRoundRectColor(dc, progressTrackRect, RGB(34, 38, 44), 12);
    }

    void EnsureSplashBackgroundCached(HDC dc, const RECT& rect)
    {
        const int width = RectWidth(rect);
        const int height = RectHeight(rect);
        if (width <= 0 || height <= 0)
        {
            return;
        }

        if (g_state.CachedSplashBitmap != nullptr &&
            g_state.CachedSplashWidth == width &&
            g_state.CachedSplashHeight == height)
        {
            return;
        }

        if (g_state.CachedSplashBitmap != nullptr)
        {
            DeleteObject(g_state.CachedSplashBitmap);
            g_state.CachedSplashBitmap = nullptr;
        }

        HDC memoryDc = CreateCompatibleDC(dc);
        g_state.CachedSplashBitmap = CreateCompatibleBitmap(dc, width, height);
        HGDIOBJ oldBitmap = SelectObject(memoryDc, g_state.CachedSplashBitmap);

        DrawSplashBackground(memoryDc, rect);

        SelectObject(memoryDc, oldBitmap);
        DeleteDC(memoryDc);
        g_state.CachedSplashWidth = width;
        g_state.CachedSplashHeight = height;
    }

    void DrawSplash(HDC dc, const RECT& rect)
    {
        EnsureSplashBackgroundCached(dc, rect);

        if (g_state.CachedSplashBitmap != nullptr)
        {
            HDC memoryDc = CreateCompatibleDC(dc);
            HGDIOBJ oldBitmap = SelectObject(memoryDc, g_state.CachedSplashBitmap);
            BitBlt(dc, rect.left, rect.top, RectWidth(rect), RectHeight(rect), memoryDc, 0, 0, SRCCOPY);
            SelectObject(memoryDc, oldBitmap);
            DeleteDC(memoryDc);
        }

        const Gdiplus::RectF progressTrackRect(
            static_cast<Gdiplus::REAL>(rect.left + 30),
            static_cast<Gdiplus::REAL>(rect.bottom - 40),
            static_cast<Gdiplus::REAL>(rect.right - rect.left - 60),
            16.0f);

        Gdiplus::Graphics progressGraphics(dc);
        progressGraphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        progressGraphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

        Gdiplus::GraphicsPath trackPath;
        AddRoundedRectPath(trackPath, progressTrackRect, progressTrackRect.Height / 2.0f);
        Gdiplus::SolidBrush trackBrush(Gdiplus::Color(255, 34, 38, 44));
        progressGraphics.FillPath(&trackBrush, &trackPath);

        const float renderedProgress = std::clamp(g_state.DisplayedSplashProgress, 0.0f, 1.0f);
        const float minimumVisibleWidth = progressTrackRect.Height;
        const float fillWidth = std::max(minimumVisibleWidth, progressTrackRect.Width * renderedProgress);
        Gdiplus::RectF progressFillRect(
            progressTrackRect.X,
            progressTrackRect.Y,
            std::min(fillWidth, progressTrackRect.Width),
            progressTrackRect.Height);

        Gdiplus::GraphicsPath fillPath;
        AddRoundedRectPath(fillPath, progressFillRect, progressFillRect.Height / 2.0f);
        Gdiplus::SolidBrush fillBrush(Gdiplus::Color(255, 46, 182, 220));
        progressGraphics.FillPath(&fillBrush, &fillPath);

        const Gdiplus::REAL glowDiameter = progressFillRect.Height + 10.0f;
        const Gdiplus::REAL glowX = progressFillRect.GetRight() - progressFillRect.Height * 0.5f - glowDiameter * 0.5f;
        const Gdiplus::REAL glowY = progressFillRect.Y - 5.0f;
        Gdiplus::SolidBrush glowBrush(Gdiplus::Color(110, 86, 210, 238));
        progressGraphics.FillEllipse(&glowBrush, glowX, glowY, glowDiameter, glowDiameter);

        Gdiplus::SolidBrush capBrush(Gdiplus::Color(235, 104, 224, 248));
        progressGraphics.FillEllipse(&capBrush,
            progressFillRect.GetRight() - progressFillRect.Height,
            progressFillRect.Y,
            progressFillRect.Height,
            progressFillRect.Height);
    }

    void DrawCircleMarker(HDC dc, const int centerX, const int centerY, const int radius, const COLORREF color)
    {
        const HBRUSH brush = CreateSolidBrush(color);
        const HPEN pen = CreatePen(PS_SOLID, 1, color);
        const HGDIOBJ oldBrush = SelectObject(dc, brush);
        const HGDIOBJ oldPen = SelectObject(dc, pen);
        Ellipse(dc, centerX - radius, centerY - radius, centerX + radius, centerY + radius);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);
    }

    void DrawToggle(HDC dc, const RECT& rect, const bool enabled)
    {
        const COLORREF background = enabled ? RGB(48, 183, 220) : RGB(56, 60, 68);
        const COLORREF knobColor = RGB(244, 247, 250);
        const COLORREF borderColor = enabled ? RGB(74, 197, 229) : RGB(72, 77, 86);

        Gdiplus::Graphics graphics(dc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

        const int diameter = RectHeight(rect);
        const int trackRadius = diameter / 2;
        Gdiplus::GraphicsPath trackPath;
        trackPath.AddArc(static_cast<Gdiplus::REAL>(rect.left), static_cast<Gdiplus::REAL>(rect.top), static_cast<Gdiplus::REAL>(diameter), static_cast<Gdiplus::REAL>(diameter), 90.0f, 180.0f);
        trackPath.AddArc(static_cast<Gdiplus::REAL>(rect.right - diameter), static_cast<Gdiplus::REAL>(rect.top), static_cast<Gdiplus::REAL>(diameter), static_cast<Gdiplus::REAL>(diameter), 270.0f, 180.0f);
        trackPath.CloseFigure();

        Gdiplus::SolidBrush trackBrush(Gdiplus::Color(235, GetRValue(background), GetGValue(background), GetBValue(background)));
        Gdiplus::Pen trackPen(Gdiplus::Color(255, GetRValue(borderColor), GetGValue(borderColor), GetBValue(borderColor)), 1.0f);
        graphics.FillPath(&trackBrush, &trackPath);
        graphics.DrawPath(&trackPen, &trackPath);

        const int radius = RectHeight(rect) / 2 - 5;
        const int centerY = rect.top + RectHeight(rect) / 2;
        const int knobX = enabled ? rect.right - trackRadius : rect.left + trackRadius;
        Gdiplus::SolidBrush knobBrush(Gdiplus::Color(245, GetRValue(knobColor), GetGValue(knobColor), GetBValue(knobColor)));
        Gdiplus::Pen knobPen(Gdiplus::Color(90, 255, 255, 255), 1.0f);
        graphics.FillEllipse(&knobBrush,
            static_cast<Gdiplus::REAL>(knobX - radius),
            static_cast<Gdiplus::REAL>(centerY - radius),
            static_cast<Gdiplus::REAL>(radius * 2),
            static_cast<Gdiplus::REAL>(radius * 2));
        graphics.DrawEllipse(&knobPen,
            static_cast<Gdiplus::REAL>(knobX - radius),
            static_cast<Gdiplus::REAL>(centerY - radius),
            static_cast<Gdiplus::REAL>(radius * 2),
            static_cast<Gdiplus::REAL>(radius * 2));
    }

    int CalculateContentHeight()
    {
        int height = 0;
        for (const TrainerSection& section : g_state.Panel.GetSections())
        {
            height += SectionTitleHeight + 8;
            height += static_cast<int>(section.Options.size()) * RowHeight;
            height += SectionGap;
        }

        return height + OuterPadding;
    }

    int GetContentViewportHeight(HWND hwnd)
    {
        RECT client{};
        GetClientRect(hwnd, &client);
        return std::max(0, RectHeight(client) - (HeaderHeight + 6));
    }

    int GetMaxScroll(HWND hwnd)
    {
        return std::max(0, g_state.ContentHeight - GetContentViewportHeight(hwnd));
    }

    RECT GetScrollBarRect(const RECT& clientRect)
    {
        return MakeRect(
            clientRect.right - CustomScrollBarMargin - CustomScrollBarWidth,
            clientRect.top + HeaderHeight + CustomScrollBarMargin,
            clientRect.right - CustomScrollBarMargin,
            clientRect.bottom - CustomScrollBarMargin);
    }

    RECT GetScrollThumbRect(const RECT& clientRect)
    {
        const RECT scrollBarRect = GetScrollBarRect(clientRect);
        const int trackHeight = RectHeight(scrollBarRect);
        const int viewportHeight = std::max(1, RectHeight(clientRect) - (HeaderHeight + 6));
        const int maxScroll = std::max(0, g_state.ContentHeight - viewportHeight);
        if (trackHeight <= 0)
        {
            return scrollBarRect;
        }

        const int thumbHeight = std::clamp(
            static_cast<int>((static_cast<float>(viewportHeight) / std::max(g_state.ContentHeight, 1)) * trackHeight),
            CustomScrollBarMinThumbHeight,
            trackHeight);

        if (maxScroll <= 0)
        {
            return MakeRect(scrollBarRect.left, scrollBarRect.top, scrollBarRect.right, scrollBarRect.top + thumbHeight);
        }

        const int travel = std::max(0, trackHeight - thumbHeight);
        const int thumbTop = scrollBarRect.top + static_cast<int>((static_cast<float>(g_state.ScrollOffset) / maxScroll) * travel);
        return MakeRect(scrollBarRect.left, thumbTop, scrollBarRect.right, thumbTop + thumbHeight);
    }

    void DrawCustomScrollBar(HDC dc, const RECT& clientRect)
    {
        const RECT scrollBarRect = GetScrollBarRect(clientRect);
        FillRoundRectColor(dc, scrollBarRect, RGB(28, 31, 36), 10);

        const RECT thumbRect = GetScrollThumbRect(clientRect);
        FillRoundRectColor(dc, thumbRect, RGB(214, 214, 214), 10);
    }

    void UpdateScrollOffsetFromThumb(HWND hwnd, const int mouseY)
    {
        RECT clientRect{};
        GetClientRect(hwnd, &clientRect);
        const RECT scrollBarRect = GetScrollBarRect(clientRect);
        const RECT thumbRect = GetScrollThumbRect(clientRect);
        const int thumbHeight = RectHeight(thumbRect);
        const int trackHeight = RectHeight(scrollBarRect);
        const int travel = std::max(1, trackHeight - thumbHeight);
        const int desiredThumbTop = std::clamp(mouseY - g_state.ScrollDragStartY + thumbRect.top, scrollBarRect.top, scrollBarRect.bottom - thumbHeight);
        const int thumbOffset = desiredThumbTop - scrollBarRect.top;
        const int maxScroll = GetMaxScroll(hwnd);
        g_state.ScrollOffset = maxScroll <= 0 ? 0 : static_cast<int>((static_cast<float>(thumbOffset) / travel) * maxScroll);
        g_state.ScrollOffset = std::clamp(g_state.ScrollOffset, 0, maxScroll);
    }

    void UpdateScrollBar(HWND hwnd)
    {
        g_state.ContentHeight = CalculateContentHeight();
        const int maxScroll = GetMaxScroll(hwnd);
        g_state.ScrollOffset = std::clamp(g_state.ScrollOffset, 0, maxScroll);
    }

    bool HandleHit(const POINT point, HWND hwnd)
    {
        for (const HitRegion& hit : g_state.HitRegions)
        {
            if (!PtInRect(&hit.Rect, point))
            {
                continue;
            }

            g_state.Panel.ToggleOption(hit.SectionIndex, hit.OptionIndex);
            InvalidateRect(hwnd, nullptr, TRUE);
            return true;
        }

        return false;
    }

    void EnsureFontsCreated()
    {
        if (g_state.TitleFont != nullptr)
        {
            return;
        }

        g_state.TitleFont = CreateFontW(28, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY, VARIABLE_PITCH, L"Segoe UI");
        g_state.HeaderFont = CreateFontW(21, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY, VARIABLE_PITCH, L"Segoe UI");
        g_state.BodyFont = CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY, VARIABLE_PITCH, L"Segoe UI");
        g_state.SmallFont = CreateFontW(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY, VARIABLE_PITCH, L"Segoe UI");
    }

    void DrawInterface(HDC dc, const RECT& clientRect)
    {
        EnsureFontsCreated();
        g_state.HitRegions.clear();

        FillRectColor(dc, clientRect, RGB(11, 13, 16));

        RECT panelRect = MakeRect(clientRect.left, clientRect.top, clientRect.right, clientRect.bottom);
        DrawBackgroundImage(dc, panelRect);

        RECT gameIcon = MakeRect(panelRect.left + 12, panelRect.top + 10, panelRect.left + 64, panelRect.top + 62);
        DrawGameIcon(dc, gameIcon);

        RECT titleRect = MakeRect(panelRect.left + 72, panelRect.top + 18, panelRect.left + 356, panelRect.top + 43);
        DrawTextLine(dc, L"Black Myth: Wukong", titleRect, RGB(230, 235, 239), g_state.HeaderFont, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        const RECT scrollBarRect = GetScrollBarRect(panelRect);
        const int contentRight = scrollBarRect.left - 14;
        RECT listClipRect = MakeRect(panelRect.left, panelRect.top + HeaderHeight, contentRight, panelRect.bottom);
        HRGN listClipRegion = CreateRectRgn(listClipRect.left, listClipRect.top, listClipRect.right, listClipRect.bottom);
        SelectClipRgn(dc, listClipRegion);

        int y = panelRect.top + HeaderHeight + 4 - g_state.ScrollOffset;
        const int labelLeft = panelRect.left + 40;
        const int toggleLeft = contentRight - HotkeyWidth - ToggleWidth - 18;
        const int hotkeyLeft = contentRight - HotkeyWidth;

        const std::vector<TrainerSection>& sections = g_state.Panel.GetSections();
        for (std::size_t sectionIndex = 0; sectionIndex < sections.size(); ++sectionIndex)
        {
            const TrainerSection& section = sections[sectionIndex];
            RECT sectionIconRect = MakeRect(panelRect.left + 18, y + 4, panelRect.left + 26, y + 12);
            DrawDiamondMarker(dc, (sectionIconRect.left + sectionIconRect.right) / 2, (sectionIconRect.top + sectionIconRect.bottom) / 2, 3, RGB(186, 193, 199));
            RECT sectionRect = MakeRect(panelRect.left + 34, y, panelRect.left + 320, y + SectionTitleHeight);
            DrawTextLine(dc, section.Title, sectionRect, RGB(189, 197, 204), g_state.SmallFont, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            y += SectionTitleHeight + 4;

            for (std::size_t optionIndex = 0; optionIndex < section.Options.size(); ++optionIndex)
            {
                const auto& option = section.Options[optionIndex];
                const RECT rowRect = MakeRect(panelRect.left + 18, y, contentRight, y + RowHeight);

                if (rowRect.bottom >= panelRect.top + HeaderHeight && rowRect.top <= panelRect.bottom)
                {
                    const int markerY = rowRect.top + RowHeight / 2;
                    DrawDiamondMarker(dc, panelRect.left + 24, markerY, 3, RGB(58, 175, 210));

                    RECT labelRect = MakeRect(labelLeft, rowRect.top, toggleLeft - 18, rowRect.bottom);
                    DrawTextLine(dc, option.Label, labelRect, RGB(214, 221, 227), g_state.BodyFont, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                    RECT toggleRect = MakeRect(toggleLeft, rowRect.top + 3, toggleLeft + ToggleWidth, rowRect.top + 3 + ToggleHeight);
                    DrawToggle(dc, toggleRect, option.Enabled);

                    RECT hotkeyRect = MakeRect(hotkeyLeft, rowRect.top + 3, hotkeyLeft + HotkeyWidth, rowRect.top + 3 + ToggleHeight);
                    FillRoundRectColor(dc, hotkeyRect, RGB(36, 39, 44), 12);
                    DrawTextLine(dc, option.Hotkey, hotkeyRect, RGB(136, 145, 154), g_state.SmallFont, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                    g_state.HitRegions.push_back({ toggleRect, HitRegion::Type::ToggleOption, sectionIndex, optionIndex });
                }

                y += RowHeight;
            }

            y += SectionGap;
        }

        SelectClipRgn(dc, nullptr);
        DeleteObject(listClipRegion);
        DrawCustomScrollBar(dc, panelRect);
    }

    void CleanupFonts()
    {
        g_state.GameIconImage.reset();
        g_state.BackgroundImage.reset();
        g_state.SplashBackgroundImage.reset();
        if (g_state.GameIconStream != nullptr)
        {
            g_state.GameIconStream->Release();
            g_state.GameIconStream = nullptr;
        }
        if (g_state.BackgroundImageStream != nullptr)
        {
            g_state.BackgroundImageStream->Release();
            g_state.BackgroundImageStream = nullptr;
        }
        if (g_state.SplashBackgroundImageStream != nullptr)
        {
            g_state.SplashBackgroundImageStream->Release();
            g_state.SplashBackgroundImageStream = nullptr;
        }
        if (g_state.CachedBackgroundBitmap != nullptr)
        {
            DeleteObject(g_state.CachedBackgroundBitmap);
            g_state.CachedBackgroundBitmap = nullptr;
        }
        if (g_state.CachedSplashBitmap != nullptr)
        {
            DeleteObject(g_state.CachedSplashBitmap);
            g_state.CachedSplashBitmap = nullptr;
        }
        g_state.CachedBackgroundWidth = 0;
        g_state.CachedBackgroundHeight = 0;
        g_state.CachedSplashWidth = 0;
        g_state.CachedSplashHeight = 0;
        if (g_state.GdiPlusToken != 0)
        {
            Gdiplus::GdiplusShutdown(g_state.GdiPlusToken);
            g_state.GdiPlusToken = 0;
        }

        DeleteObject(g_state.TitleFont);
        DeleteObject(g_state.HeaderFont);
        DeleteObject(g_state.BodyFont);
        DeleteObject(g_state.SmallFont);
        g_state.TitleFont = nullptr;
        g_state.HeaderFont = nullptr;
        g_state.BodyFont = nullptr;
        g_state.SmallFont = nullptr;
    }

    void PrepareMainWindow(HWND hwnd)
    {
        EnsureFontsCreated();
        EnsureGameIconLoaded();
        EnsureBackgroundImageLoaded();
        UpdateScrollBar(hwnd);

        RECT clientRect{};
        GetClientRect(hwnd, &clientRect);
        if (RectWidth(clientRect) <= 0 || RectHeight(clientRect) <= 0)
        {
            return;
        }

        HDC windowDc = GetDC(hwnd);
        if (windowDc == nullptr)
        {
            return;
        }

        HDC memoryDc = CreateCompatibleDC(windowDc);
        HBITMAP bitmap = CreateCompatibleBitmap(windowDc, RectWidth(clientRect), RectHeight(clientRect));
        HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);
        DrawInterface(memoryDc, clientRect);
        SelectObject(memoryDc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(memoryDc);
        ReleaseDC(hwnd, windowDc);
    }

    LRESULT CALLBACK SplashWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_CREATE:
        {
            RECT clientRect{};
            GetClientRect(hwnd, &clientRect);
            HRGN region = CreateRoundRectRgn(clientRect.left, clientRect.top, clientRect.right + 1, clientRect.bottom + 1, 28, 28);
            SetWindowRgn(hwnd, region, TRUE);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT paint{};
            HDC windowDc = BeginPaint(hwnd, &paint);

            RECT clientRect{};
            GetClientRect(hwnd, &clientRect);

            HDC memoryDc = CreateCompatibleDC(windowDc);
            HBITMAP bitmap = CreateCompatibleBitmap(windowDc, RectWidth(clientRect), RectHeight(clientRect));
            HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);

            DrawSplash(memoryDc, clientRect);

            BitBlt(windowDc, 0, 0, RectWidth(clientRect), RectHeight(clientRect), memoryDc, 0, 0, SRCCOPY);

            SelectObject(memoryDc, oldBitmap);
            DeleteObject(bitmap);
            DeleteDC(memoryDc);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_LBUTTONDOWN:
        case WM_KEYDOWN:
            DestroyWindow(hwnd);
            return 0;
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    void ShowStartupSplash(HINSTANCE instance)
    {
        const wchar_t* splashClassName = L"BlackMythWukongSplashWindow";
        static bool splashRegistered = false;

        if (!splashRegistered)
        {
            WNDCLASSEXW splashClass{};
            splashClass.cbSize = sizeof(splashClass);
            splashClass.style = CS_HREDRAW | CS_VREDRAW;
            splashClass.lpfnWndProc = SplashWindowProc;
            splashClass.hInstance = instance;
            splashClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            splashClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
            splashClass.hIconSm = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
            splashClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
            splashClass.lpszClassName = splashClassName;
            RegisterClassExW(&splashClass);
            splashRegistered = true;
        }

        const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        const int x = (screenWidth - SplashWidth) / 2;
        const int y = (screenHeight - SplashHeight) / 2;

        HWND splashWindow = CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            splashClassName,
            L"Black Myth: Wukong Splash",
            WS_POPUP | WS_VISIBLE,
            x,
            y,
            SplashWidth,
            SplashHeight,
            nullptr,
            nullptr,
            instance,
            nullptr);

        if (splashWindow == nullptr)
        {
            return;
        }

        ShowWindow(splashWindow, SW_SHOW);
        UpdateWindow(splashWindow);

        timeBeginPeriod(1);
        g_state.SplashProgress = 0.0f;
        g_state.DisplayedSplashProgress = 0.0f;
        LARGE_INTEGER frequency{};
        LARGE_INTEGER startCounter{};
        LARGE_INTEGER previousCounter{};
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&startCounter);
        previousCounter = startCounter;
        MSG message{};
        while (IsWindow(splashWindow))
        {
            LARGE_INTEGER nowCounter{};
            QueryPerformanceCounter(&nowCounter);
            const double elapsedMs = static_cast<double>(nowCounter.QuadPart - startCounter.QuadPart) * 1000.0 / static_cast<double>(frequency.QuadPart);
            const double deltaSeconds = static_cast<double>(nowCounter.QuadPart - previousCounter.QuadPart) / static_cast<double>(frequency.QuadPart);
            previousCounter = nowCounter;

            g_state.SplashProgress = std::clamp(static_cast<float>(elapsedMs / static_cast<double>(SplashDurationMs)), 0.0f, 1.0f);
            const float targetDisplayedProgress = EaseSplashProgress(g_state.SplashProgress);
            const float interpolationFactor = 1.0f - std::exp(static_cast<float>(-14.0 * deltaSeconds));
            g_state.DisplayedSplashProgress += (targetDisplayedProgress - g_state.DisplayedSplashProgress) * interpolationFactor;

            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            {
                if (message.message == WM_QUIT)
                {
                    return;
                }

                TranslateMessage(&message);
                DispatchMessageW(&message);
            }

            RedrawWindow(splashWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
            if (g_state.SplashProgress >= 1.0f && std::abs(g_state.DisplayedSplashProgress - 1.0f) < 0.0025f)
            {
                break;
            }

            Sleep(1);
        }

        if (IsWindow(splashWindow))
        {
            g_state.SplashProgress = 1.0f;
            g_state.DisplayedSplashProgress = 1.0f;
            RedrawWindow(splashWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
            DestroyWindow(splashWindow);
        }

        timeEndPeriod(1);

        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                break;
            }

            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_CREATE:
            UpdateScrollBar(hwnd);
            return 0;

        case WM_SIZE:
            UpdateScrollBar(hwnd);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;

        case WM_GETMINMAXINFO:
        {
            auto* minMaxInfo = reinterpret_cast<MINMAXINFO*>(lParam);
            minMaxInfo->ptMinTrackSize.x = MinimumWindowWidth;
            minMaxInfo->ptMinTrackSize.y = MinimumWindowHeight;
            minMaxInfo->ptMaxTrackSize.x = MaximumWindowWidth;
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEWHEEL:
        {
            const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            g_state.ScrollOffset = std::clamp(g_state.ScrollOffset - (delta / WHEEL_DELTA) * 56, 0, GetMaxScroll(hwnd));
            UpdateScrollBar(hwnd);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }

        case WM_VSCROLL:
            return 0;

        case WM_LBUTTONDOWN:
        {
            const POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            RECT clientRect{};
            GetClientRect(hwnd, &clientRect);
            const RECT scrollBarRect = GetScrollBarRect(clientRect);
            const RECT thumbRect = GetScrollThumbRect(clientRect);

            if (PtInRect(&thumbRect, point))
            {
                g_state.IsScrollThumbDragging = true;
                g_state.ScrollDragStartY = point.y;
                g_state.ScrollDragStartOffset = g_state.ScrollOffset;
                SetCapture(hwnd);
                return 0;
            }

            if (PtInRect(&scrollBarRect, point))
            {
                const int page = std::max(32, GetContentViewportHeight(hwnd) - 40);
                g_state.ScrollOffset += point.y < thumbRect.top ? -page : page;
                g_state.ScrollOffset = std::clamp(g_state.ScrollOffset, 0, GetMaxScroll(hwnd));
                UpdateScrollBar(hwnd);
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }

            HandleHit(point, hwnd);
            return 0;
        }

        case WM_MOUSEMOVE:
            if (g_state.IsScrollThumbDragging)
            {
                RECT clientRect{};
                GetClientRect(hwnd, &clientRect);
                const RECT scrollBarRect = GetScrollBarRect(clientRect);
                const int trackHeight = RectHeight(scrollBarRect);
                const int viewportHeight = std::max(1, GetContentViewportHeight(hwnd));
                const int thumbHeight = std::clamp(
                    static_cast<int>((static_cast<float>(viewportHeight) / std::max(g_state.ContentHeight, 1)) * trackHeight),
                    CustomScrollBarMinThumbHeight,
                    trackHeight);
                const int travel = std::max(1, trackHeight - thumbHeight);
                const int maxScroll = GetMaxScroll(hwnd);
                const int deltaY = GET_Y_LPARAM(lParam) - g_state.ScrollDragStartY;
                const float scrollPerPixel = maxScroll <= 0 ? 0.0f : static_cast<float>(maxScroll) / travel;
                g_state.ScrollOffset = std::clamp(
                    g_state.ScrollDragStartOffset + static_cast<int>(deltaY * scrollPerPixel),
                    0,
                    maxScroll);
                UpdateScrollBar(hwnd);
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }
            return 0;

        case WM_LBUTTONUP:
            if (g_state.IsScrollThumbDragging)
            {
                g_state.IsScrollThumbDragging = false;
                ReleaseCapture();
                return 0;
            }
            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE)
            {
                DestroyWindow(hwnd);
                return 0;
            }
            return 0;

        case WM_PAINT:
        {
            PAINTSTRUCT paint{};
            HDC windowDc = BeginPaint(hwnd, &paint);

            RECT clientRect{};
            GetClientRect(hwnd, &clientRect);

            HDC memoryDc = CreateCompatibleDC(windowDc);
            HBITMAP bitmap = CreateCompatibleBitmap(windowDc, RectWidth(clientRect), RectHeight(clientRect));
            HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);

            DrawInterface(memoryDc, clientRect);

            BitBlt(windowDc, 0, 0, RectWidth(clientRect), RectHeight(clientRect), memoryDc, 0, 0, SRCCOPY);

            SelectObject(memoryDc, oldBitmap);
            DeleteObject(bitmap);
            DeleteDC(memoryDc);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_DESTROY:
            CleanupFonts();
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    const wchar_t* className = L"BlackMythWukongPseudoTrainerWindow";

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    windowClass.hIconSm = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    windowClass.lpszClassName = className;

    RegisterClassExW(&windowClass);

    ShowStartupSplash(instance);

    HWND hwnd = CreateWindowExW(
        0,
        className,
        L"Black Myth: Wukong - Trainer Panel",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        WindowWidth,
        WindowHeight,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (hwnd == nullptr)
    {
        return 0;
    }

    PrepareMainWindow(hwnd);
    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}
