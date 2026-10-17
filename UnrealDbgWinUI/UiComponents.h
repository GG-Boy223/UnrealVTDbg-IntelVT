#pragma once

// 可复用界面组件。所有页面只能通过这里的工厂函数拼装界面，
// 保证同一类信息在不同页面上的视觉表达完全一致。

#include "UiTheme.h"

#include <winrt/Windows.Foundation.Collections.h>

#include <vector>

namespace unrealdbg_ui
{
    using winrt::hstring;
    using winrt::Microsoft::UI::Xaml::Controls::Border;
    using winrt::Microsoft::UI::Xaml::Controls::Button;
    using winrt::Microsoft::UI::Xaml::Controls::StackPanel;
    using winrt::Microsoft::UI::Xaml::Controls::TextBlock;
    using winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch;
    using winrt::Microsoft::UI::Xaml::UIElement;
    using winrt::Windows::UI::Color;

    enum class ButtonVariant
    {
        Primary,
        Secondary,
        Ghost,
        Danger,
    };

    // 卡片：标题区（图标 + 标题 + 说明 + 右侧操作位）+ 内容区。
    struct CardParts
    {
        Border Root{ nullptr };
        StackPanel Body{ nullptr };
        StackPanel Actions{ nullptr };
        TextBlock Subtitle{ nullptr };
    };

    [[nodiscard]] CardParts MakeSectionCard(const hstring& title, const hstring& subtitle,
        const hstring& glyph, const Color accent = ui::kAccent, const Color accentSoft = ui::kAccentSoft);

    // 指标磁贴：图标 + 名称 + 主数值 + 说明。
    [[nodiscard]] UIElement MakeStatTile(const hstring& glyph, const Color accent, const Color accentSoft,
        const hstring& label, const hstring& value, const hstring& hint,
        TextBlock* valueOut = nullptr, TextBlock* hintOut = nullptr);

    [[nodiscard]] Button MakeButton(const hstring& caption, const hstring& glyph, const ButtonVariant variant,
        const double fontSize = ui::kFontCaption);
    void ApplyButtonVisuals(const Button& button, const ButtonVariant variant);

    // "标签: 取值" 行，用于环境信息一类的键值对。
    [[nodiscard]] UIElement MakePropertyRow(const hstring& label, const hstring& value, const bool monospace,
        TextBlock* valueOut = nullptr);

    [[nodiscard]] UIElement MakeStepRow(const int index, const hstring& title, const hstring& detail);

    // 开关行：图标 + 标题 + 说明 + 右侧开关。
    [[nodiscard]] UIElement MakeToggleRow(const hstring& glyph, const hstring& title, const hstring& detail,
        const Color accent, ToggleSwitch* switchOut);

    // 自检行：状态图标 + 名称 + 路径 + 结论。
    [[nodiscard]] UIElement MakeCheckRow(const hstring& label, const hstring& path, const bool ok,
        TextBlock* statusOut);

    [[nodiscard]] UIElement MakeEmptyState(const hstring& glyph, const hstring& title, const hstring& detail,
        Button* actionOut);

    [[nodiscard]] UIElement MakeSectionLabel(const hstring& value);

    // 紧凑状态行：图标 + 名称 + 右侧取值。用于导航栏底部的“运行环境”摘要，
    // 一行一个键值，数值文本可被外部持有并就地刷新。
    [[nodiscard]] UIElement MakeStatusRow(const hstring& glyph, const hstring& label, const hstring& value,
        const Color valueColor, TextBlock* valueOut = nullptr);

    [[nodiscard]] Border MakeBadge(const hstring& value, const Color foreground, const Color background);
}
