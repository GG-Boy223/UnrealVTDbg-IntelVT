#pragma once

// 统一设计令牌（Design Tokens）。
//
// 界面层所有颜色、字号、间距、圆角、控件尺寸都必须来自本文件，页面代码里
// 不允许再出现写死的颜色值或魔法数字。这样后续换肤或调整密度时只需要改一处。

#include <cstdint>
#include <string>
#include <unordered_map>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

namespace unrealdbg_ui
{
    namespace token
    {
        using winrt::Microsoft::UI::Xaml::Controls::Border;
        using winrt::Microsoft::UI::Xaml::Controls::FontIcon;
        using winrt::Microsoft::UI::Xaml::Controls::TextBlock;
        using winrt::Microsoft::UI::Xaml::Media::FontFamily;
        using winrt::Microsoft::UI::Xaml::Media::SolidColorBrush;
        using winrt::Windows::UI::Color;
        using winrt::Windows::UI::Text::FontWeight;

        inline Color Rgb(const uint8_t red, const uint8_t green, const uint8_t blue, const uint8_t alpha = 255) noexcept
        {
            return Color{ alpha, red, green, blue };
        }

        // ---------------------------------------------------------------- 颜色
        // Runtime palettes: dark ink / light rice paper.
        inline bool UseDarkTheme{ false };
        inline Color kWindowTop{ Rgb(0x0E, 0x18, 0x1A) };
        inline Color kWindowBottom{ Rgb(0x0B, 0x13, 0x15) };
        inline Color kTitleBar{ Rgb(0x0A, 0x12, 0x14) };
        inline Color kSidebar{ Rgb(0x0D, 0x18, 0x1A) };
        inline Color kCard{ Rgb(0x14, 0x23, 0x26) };
        inline Color kCardInset{ Rgb(0x10, 0x1D, 0x20) };
        inline Color kCardHover{ Rgb(0x1A, 0x2D, 0x30) };
        inline Color kSunken{ Rgb(0x09, 0x11, 0x13) };
        inline Color kBorder{ Rgb(0x25, 0x38, 0x3B) };
        inline Color kBorderStrong{ Rgb(0x34, 0x4B, 0x4D) };
        inline Color kTextPrimary{ Rgb(0xEF, 0xF1, 0xE9) };
        inline Color kTextSecondary{ Rgb(0xA9, 0xB7, 0xB2) };
        inline Color kTextMuted{ Rgb(0x73, 0x86, 0x83) };
        inline Color kTextOnAccent{ Rgb(0x07, 0x16, 0x12) };
        inline Color kAccent{ Rgb(0x67, 0xB7, 0x9B) };
        inline Color kAccentHover{ Rgb(0x7D, 0xC9, 0xAD) };
        inline Color kAccentPressed{ Rgb(0x50, 0x98, 0x80) };
        inline Color kAccentSoft{ Rgb(0x16, 0x32, 0x2B) };
        inline Color kAccentText{ Rgb(0xC0, 0xE7, 0xD9) };
        inline Color kCinnabar{ Rgb(0xD8, 0x68, 0x52) };
        inline Color kCinnabarSoft{ Rgb(0x32, 0x1C, 0x18) };
        inline Color kGold{ Rgb(0xCF, 0xAD, 0x70) };
        inline Color kGoldSoft{ Rgb(0x2D, 0x27, 0x1A) };
        inline Color kSuccess{ Rgb(0x67, 0xC6, 0x9D) };
        inline Color kSuccessSoft{ Rgb(0x12, 0x2B, 0x24) };
        inline Color kWarning{ Rgb(0xD8, 0xB0, 0x6E) };
        inline Color kWarningSoft{ Rgb(0x30, 0x28, 0x18) };
        inline Color kDanger{ Rgb(0xE0, 0x77, 0x68) };
        inline Color kDangerSoft{ Rgb(0x31, 0x1A, 0x18) };
        inline Color kDangerHover{ Rgb(0x43, 0x21, 0x1F) };
        inline Color kDangerPressed{ Rgb(0x27, 0x13, 0x12) };
        inline Color kInfo{ Rgb(0x86, 0xB9, 0xC2) };
        inline Color kInfoSoft{ Rgb(0x18, 0x2B, 0x2E) };
        inline Color kDebug{ Rgb(0x90, 0xB5, 0xBF) };
        inline Color kDebugSoft{ Rgb(0x16, 0x25, 0x28) };

        inline void SetTheme(const bool dark)
        {
            UseDarkTheme = dark;
            if (dark)
            {
                kWindowTop = Rgb(0x0E, 0x18, 0x1A);
                kWindowBottom = Rgb(0x0B, 0x13, 0x15);
                kTitleBar = Rgb(0x0A, 0x12, 0x14);
                kSidebar = Rgb(0x0D, 0x18, 0x1A);
                kCard = Rgb(0x14, 0x23, 0x26);
                kCardInset = Rgb(0x10, 0x1D, 0x20);
                kCardHover = Rgb(0x1A, 0x2D, 0x30);
                kSunken = Rgb(0x09, 0x11, 0x13);
                kBorder = Rgb(0x25, 0x38, 0x3B);
                kBorderStrong = Rgb(0x34, 0x4B, 0x4D);
                kTextPrimary = Rgb(0xEF, 0xF1, 0xE9);
                kTextSecondary = Rgb(0xA9, 0xB7, 0xB2);
                kTextMuted = Rgb(0x73, 0x86, 0x83);
                kTextOnAccent = Rgb(0x07, 0x16, 0x12);
                kAccent = Rgb(0x67, 0xB7, 0x9B);
                kAccentHover = Rgb(0x7D, 0xC9, 0xAD);
                kAccentPressed = Rgb(0x50, 0x98, 0x80);
                kAccentSoft = Rgb(0x16, 0x32, 0x2B);
                kAccentText = Rgb(0xC0, 0xE7, 0xD9);
                kCinnabar = Rgb(0xD8, 0x68, 0x52);
                kCinnabarSoft = Rgb(0x32, 0x1C, 0x18);
                kGold = Rgb(0xCF, 0xAD, 0x70);
                kGoldSoft = Rgb(0x2D, 0x27, 0x1A);
                kSuccess = Rgb(0x67, 0xC6, 0x9D);
                kSuccessSoft = Rgb(0x12, 0x2B, 0x24);
                kWarning = Rgb(0xD8, 0xB0, 0x6E);
                kWarningSoft = Rgb(0x30, 0x28, 0x18);
                kDanger = Rgb(0xE0, 0x77, 0x68);
                kDangerSoft = Rgb(0x31, 0x1A, 0x18);
                kDangerHover = Rgb(0x43, 0x21, 0x1F);
                kDangerPressed = Rgb(0x27, 0x13, 0x12);
                kInfo = Rgb(0x86, 0xB9, 0xC2);
                kInfoSoft = Rgb(0x18, 0x2B, 0x2E);
                kDebug = Rgb(0x90, 0xB5, 0xBF);
                kDebugSoft = Rgb(0x16, 0x25, 0x28);
            }
            else
            {
                kWindowTop = Rgb(0xF4, 0xF1, 0xE8);
                kWindowBottom = Rgb(0xF8, 0xF6, 0xF0);
                kTitleBar = Rgb(0xEC, 0xE8, 0xDE);
                kSidebar = Rgb(0xEF, 0xEB, 0xE1);
                kCard = Rgb(0xFB, 0xF9, 0xF4);
                kCardInset = Rgb(0xF3, 0xEF, 0xE6);
                kCardHover = Rgb(0xE8, 0xE3, 0xD7);
                kSunken = Rgb(0xE9, 0xE4, 0xD9);
                kBorder = Rgb(0xD5, 0xCF, 0xC1);
                kBorderStrong = Rgb(0xB8, 0xB0, 0xA1);
                kTextPrimary = Rgb(0x1C, 0x27, 0x26);
                kTextSecondary = Rgb(0x4F, 0x5F, 0x5A);
                kTextMuted = Rgb(0x7A, 0x87, 0x82);
                kTextOnAccent = Rgb(0xFF, 0xFF, 0xFF);
                kAccent = Rgb(0x2F, 0x7F, 0x67);
                kAccentHover = Rgb(0x3E, 0x92, 0x78);
                kAccentPressed = Rgb(0x23, 0x66, 0x51);
                kAccentSoft = Rgb(0xDE, 0xEC, 0xE6);
                kAccentText = Rgb(0x23, 0x5F, 0x4D);
                kCinnabar = Rgb(0xB7, 0x4D, 0x3B);
                kCinnabarSoft = Rgb(0xF1, 0xDE, 0xD8);
                kGold = Rgb(0x94, 0x72, 0x35);
                kGoldSoft = Rgb(0xEE, 0xE4, 0xD0);
                kSuccess = Rgb(0x2D, 0x88, 0x66);
                kSuccessSoft = Rgb(0xDD, 0xEF, 0xE7);
                kWarning = Rgb(0x95, 0x6F, 0x27);
                kWarningSoft = Rgb(0xF4, 0xE9, 0xD0);
                kDanger = Rgb(0xB4, 0x4C, 0x42);
                kDangerSoft = Rgb(0xF3, 0xE0, 0xDD);
                kDangerHover = Rgb(0xE8, 0xCF, 0xCB);
                kDangerPressed = Rgb(0xDD, 0xBD, 0xB8);
                kInfo = Rgb(0x45, 0x76, 0x86);
                kInfoSoft = Rgb(0xE0, 0xEB, 0xEE);
                kDebug = Rgb(0x5E, 0x78, 0x82);
                kDebugSoft = Rgb(0xE8, 0xEE, 0xF0);
            }
        }

        // -------------------------------------------------------------- 字号
        // 中文界面优先保证字形清楚：正文不低于 12px，说明文字不低于 10.5px。
        inline constexpr double kFontMicro{ 10.5 };      // 徽标、极小提示
        inline constexpr double kFontCaption{ 12.0 };    // 说明文字、副标题
        inline constexpr double kFontBody{ 13.0 };       // 正文、行标题
        inline constexpr double kFontSubtitle{ 15.0 };   // 区块标题
        inline constexpr double kFontTitle{ 18.0 };      // 页面标题、指标数值
        inline constexpr double kFontHero{ 25.0 };       // 主数值
        inline constexpr double kFontMono{ 12.0 };       // 等宽文本（路径、日志）

        inline constexpr FontWeight kWeightRegular{ 400 };
        inline constexpr FontWeight kWeightMedium{ 500 };
        inline constexpr FontWeight kWeightSemiBold{ 600 };

        // -------------------------------------------------------------- 行高
        // 行高规范：单行文本 1.30 倍字号，多行文本 1.50 倍字号。所有文本工厂
        // （Text / TextWrapped / Mono）统一套用，页面代码不得再各自设置 LineHeight。
        inline constexpr double kLineHeightTight{ 1.3 };
        inline constexpr double kLineHeightRelaxed{ 1.5 };
        inline constexpr double kLineHeightMono{ 1.38 };

        // -------------------------------------------------------------- 间距
        // 紧凑窗口下间距阶梯整体收紧一档，保持留白节奏但不浪费纵向空间。
        inline constexpr double kSpaceXs{ 4.0 };
        inline constexpr double kSpaceSm{ 8.0 };
        inline constexpr double kSpaceMd{ 12.0 };
        inline constexpr double kSpaceLg{ 15.0 };
        inline constexpr double kSpaceXl{ 18.0 };
        inline constexpr double kSpaceXxl{ 22.0 };

        // -------------------------------------------------------------- 圆角
        inline constexpr double kRadiusSm{ 4.0 };
        inline constexpr double kRadiusMd{ 6.0 };
        inline constexpr double kRadiusLg{ 8.0 };

        // -------------------------------------------------------------- 尺寸
        inline constexpr double kTitleBarHeight{ 46.0 };
        inline constexpr double kNavWidth{ 202.0 };
        inline constexpr double kNavCollapsedWidth{ 56.0 };
        inline constexpr double kNavItemHeight{ 38.0 };
        inline constexpr double kSplitterHeight{ 5.0 };
        inline constexpr double kLogHeaderHeight{ 34.0 };
        inline constexpr double kLogDefaultHeight{ 218.0 };
        inline constexpr double kLogMinHeight{ 128.0 };
        inline constexpr double kLogMaxRatio{ 0.6 };
        inline constexpr double kWindowDefaultWidth{ 1180.0 };
        inline constexpr double kWindowDefaultHeight{ 790.0 };
        inline constexpr double kWindowMinWidth{ 1020.0 };
        inline constexpr double kWindowMinHeight{ 680.0 };

        // ------------------------------------------------------------ 图标字形
        // 全部取自 Segoe MDL2 Assets / Segoe Fluent Icons 的常用码位。
        inline const wchar_t* const kGlyphOverview{ L"\uE80F" };   // Home
        inline const wchar_t* const kGlyphProcess{ L"\uE7B8" };    // Devices
        inline const wchar_t* const kGlyphShield{ L"\uE72E" };     // Lock
        inline const wchar_t* const kGlyphAbout{ L"\uE946" };      // Info
        inline const wchar_t* const kGlyphLog{ L"\uE756" };        // CommandPrompt
        inline const wchar_t* const kGlyphAdd{ L"\uE710" };        // Add
        inline const wchar_t* const kGlyphPlay{ L"\uE768" };       // Play
        inline const wchar_t* const kGlyphPause{ L"\uE769" };      // Pause
        inline const wchar_t* const kGlyphRemove{ L"\uE74D" };     // Delete
        inline const wchar_t* const kGlyphCopy{ L"\uE8C8" };       // Copy
        inline const wchar_t* const kGlyphClear{ L"\uE74D" };      // Delete
        inline const wchar_t* const kGlyphRefresh{ L"\uE72C" };    // Refresh
        inline const wchar_t* const kGlyphPin{ L"\uE718" };        // Pin
        inline const wchar_t* const kGlyphChevronDown{ L"\uE70D" };
        inline const wchar_t* const kGlyphChevronUp{ L"\uE70E" };
        inline const wchar_t* const kGlyphChevronLeft{ L"\uE76B" };
        inline const wchar_t* const kGlyphChevronRight{ L"\uE76C" };
        inline const wchar_t* const kGlyphFolder{ L"\uE8B7" };    // Folder
        inline const wchar_t* const kGlyphCheck{ L"\uE73E" };      // CheckMark
        inline const wchar_t* const kGlyphWarning{ L"\uE7BA" };    // Warning
        inline const wchar_t* const kGlyphError{ L"\uEA39" };      // StatusErrorFull
        inline const wchar_t* const kGlyphTimer{ L"\uE916" };      // Stopwatch
        inline const wchar_t* const kGlyphServer{ L"\uE968" };     // Server
        inline const wchar_t* const kGlyphSymbols{ L"\uE943" };    // Code
        inline const wchar_t* const kGlyphDownload{ L"\uE896" };   // Download
        inline const wchar_t* const kGlyphLink{ L"\uE71B" };       // Link
        inline const wchar_t* const kGlyphSun{ L"\uE706" };        // Brightness
        inline const wchar_t* const kGlyphMoon{ L"\uE708" };       // QuietHours
        inline const wchar_t* const kGlyphFilter{ L"\uE71C" };     // Filter

        inline const wchar_t* const kIconFontFamily{ L"Segoe Fluent Icons,Segoe MDL2 Assets" };

        // -------------------------------------------------------------- 字体族
        // 中文优先的字体链。Windows 11 上拉丁字形使用 Segoe UI Variable，
        // 中文字形优先 Microsoft YaHei UI，避免宋体在 10-13px 下发虚。
        inline const wchar_t* const kUiFontFamily{
            L"Microsoft YaHei UI, Segoe UI Variable Text, Microsoft YaHei, Segoe UI" };
        inline const wchar_t* const kUiDisplayFontFamily{
            L"Microsoft YaHei UI, Segoe UI Variable Display, Segoe UI Variable Text, Segoe UI" };
        inline const wchar_t* const kMonoFontFamily{ L"Cascadia Mono, Consolas, Courier New" };

        // 字号 -> 行高（像素）换算，供文本工厂统一套用。
        inline double LineHeightFor(const double size, const double ratio) noexcept
        {
            const double value = size * ratio;
            return static_cast<double>(static_cast<int>(value * 2.0 + 0.5)) / 2.0;  // 取 0.5 的整数倍
        }

        // ------------------------------------------------------------ 基础图元
        inline SolidColorBrush Brush(const Color color)
        {
            static std::unordered_map<std::uint32_t, SolidColorBrush> cache;
            const std::uint32_t key =
                (static_cast<std::uint32_t>(color.A) << 24) |
                (static_cast<std::uint32_t>(color.R) << 16) |
                (static_cast<std::uint32_t>(color.G) << 8) |
                static_cast<std::uint32_t>(color.B);
            const auto existing = cache.find(key);
            if (existing != cache.end())
            {
                return existing->second;
            }
            SolidColorBrush brush(color);
            cache.emplace(key, brush);
            return brush;
        }

        // 所有文本统一走这里：字号（外部传入）+ 字体族（按字号自动切 Display/Text
        // 变体）+ 行高（按行高规范换算），页面代码不再重复设置字体属性。
        inline TextBlock Text(const winrt::hstring& value, const double size, const Color color,
            const FontWeight weight = kWeightRegular)
        {
            TextBlock block;
            block.Text(value);
            block.FontSize(size);
            block.FontFamily(FontFamily(size >= kFontSubtitle ? kUiDisplayFontFamily : kUiFontFamily));
            block.Foreground(Brush(color));
            block.FontWeight(weight);
            block.TextWrapping(winrt::Microsoft::UI::Xaml::TextWrapping::NoWrap);
            block.LineStackingStrategy(winrt::Microsoft::UI::Xaml::LineStackingStrategy::BlockLineHeight);
            block.LineHeight(LineHeightFor(size, kLineHeightTight));
            return block;
        }

        // 多行正文走彻底放宽的行高（1.55），这是"读得舒服"的关键。
        inline TextBlock TextWrapped(const winrt::hstring& value, const double size, const Color color,
            const FontWeight weight = kWeightRegular)
        {
            TextBlock block = Text(value, size, color, weight);
            block.TextWrapping(winrt::Microsoft::UI::Xaml::TextWrapping::Wrap);
            block.LineHeight(LineHeightFor(size, kLineHeightRelaxed));
            return block;
        }

        inline TextBlock Mono(const winrt::hstring& value, const double size, const Color color)
        {
            TextBlock block = Text(value, size, color);
            block.FontFamily(FontFamily(kMonoFontFamily));
            block.LineHeight(LineHeightFor(size, kLineHeightMono));
            return block;
        }

        inline FontIcon Icon(const winrt::hstring& glyph, const double size, const Color color)
        {
            FontIcon icon;
            icon.Glyph(glyph);
            icon.FontSize(size);
            icon.Foreground(Brush(color));
            icon.FontFamily(FontFamily(kIconFontFamily));
            icon.HorizontalAlignment(winrt::Microsoft::UI::Xaml::HorizontalAlignment::Center);
            icon.VerticalAlignment(winrt::Microsoft::UI::Xaml::VerticalAlignment::Center);
            icon.IsHitTestVisible(false);
            return icon;
        }

        // 圆角 + 边框的矩形块，界面里所有"面"都由它拼装。
        inline Border Surface(const Color background, const Color border, const double radius = kRadiusLg,
            const double padding = kSpaceLg, const double borderThickness = 1.0)
        {
            Border surface;
            surface.Background(Brush(background));
            surface.BorderBrush(Brush(border));
            surface.BorderThickness(winrt::Microsoft::UI::Xaml::Thickness{ borderThickness, borderThickness, borderThickness, borderThickness });
            surface.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius{ radius, radius, radius, radius });
            surface.Padding(winrt::Microsoft::UI::Xaml::Thickness{ padding, padding, padding, padding });
            return surface;
        }

        // 带底色的小圆点，用于状态指示。
        inline Border Dot(const Color color, const double size = 8.0)
        {
            Border dot;
            dot.Width(size);
            dot.Height(size);
            dot.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius{ size / 2, size / 2, size / 2, size / 2 });
            dot.Background(Brush(color));
            dot.VerticalAlignment(winrt::Microsoft::UI::Xaml::VerticalAlignment::Center);
            return dot;
        }

        // 胶囊标签：状态、计数、分级都用它。
        inline Border Pill(const winrt::hstring& value, const Color foreground, const Color background,
            const double fontSize = kFontMicro)
        {
            Border pill;
            pill.Background(Brush(background));
            pill.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius{ 999, 999, 999, 999 });
            pill.Padding(winrt::Microsoft::UI::Xaml::Thickness{ 10, 3, 10, 3 });
            pill.VerticalAlignment(winrt::Microsoft::UI::Xaml::VerticalAlignment::Center);
            TextBlock label = Text(value, fontSize, foreground);
            label.TextAlignment(winrt::Microsoft::UI::Xaml::TextAlignment::Center);
            pill.Child(label);
            return pill;
        }

        inline Border Separator(const Color color, const double thickness = 1.0)
        {
            Border line;
            line.Height(thickness);
            line.Background(Brush(color));
            line.HorizontalAlignment(winrt::Microsoft::UI::Xaml::HorizontalAlignment::Stretch);
            return line;
        }

        inline winrt::Microsoft::UI::Xaml::Thickness Insets(const double left, const double top,
            const double right, const double bottom)
        {
            return winrt::Microsoft::UI::Xaml::Thickness{ left, top, right, bottom };
        }

        inline void SetToolTip(const winrt::Microsoft::UI::Xaml::DependencyObject& element, const winrt::hstring& value)
        {
            winrt::Microsoft::UI::Xaml::Controls::ToolTipService::SetToolTip(element, winrt::box_value(value));
        }
    }

    // 兼容简写：页面代码里用 ui::Text(...) 而不是 token::Text(...)。
    namespace ui = token;
}
