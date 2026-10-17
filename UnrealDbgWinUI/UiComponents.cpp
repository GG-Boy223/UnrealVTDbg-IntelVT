#include "UiComponents.h"

#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Windows.Foundation.Collections.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;

namespace unrealdbg_ui
{
    namespace
    {
        Thickness All(const double value)
        {
            return Thickness{ value, value, value, value };
        }

        Thickness Only(const double left, const double top, const double right, const double bottom)
        {
            return Thickness{ left, top, right, bottom };
        }

        // 图标底片：统一的小方块，页面里所有卡片标题、磁贴都用它。
        Border IconChip(const hstring& glyph, const Color foreground, const Color background,
            const double size, const double radius, const double glyphSize)
        {
            Border chip;
            chip.Width(size);
            chip.Height(size);
            chip.Background(ui::Brush(background));
            chip.CornerRadius(CornerRadius{ radius, radius, radius, radius });
            chip.Child(ui::Icon(glyph, glyphSize, foreground));
            chip.VerticalAlignment(VerticalAlignment::Center);
            return chip;
        }

        void InsertBrush(const Button& button, const hstring& key, const Color color)
        {
            button.Resources().Insert(box_value(key), box_value(ui::Brush(color)));
        }
    }

    void ApplyButtonVisuals(const Button& button, const ButtonVariant variant)
    {
        Color background{};
        Color border{};
        Color foreground{};
        Color hover{};
        Color pressed{};

        switch (variant)
        {
        case ButtonVariant::Primary:
            background = ui::kAccent;
            border = ui::kAccent;
            foreground = ui::kTextOnAccent;
            hover = ui::kAccentHover;
            pressed = ui::kAccentPressed;
            break;
        case ButtonVariant::Danger:
            background = ui::kDangerSoft;
            border = ui::kBorder;
            foreground = ui::kDanger;
            hover = ui::kDangerHover;
            pressed = ui::kDangerPressed;
            break;
        case ButtonVariant::Ghost:
            background = ui::Rgb(0x00, 0x00, 0x00, 0x00);
            border = ui::Rgb(0x00, 0x00, 0x00, 0x00);
            foreground = ui::kTextSecondary;
            hover = ui::kCardHover;
            pressed = ui::kSunken;
            break;
        case ButtonVariant::Secondary:
        default:
            background = ui::kCardInset;
            border = ui::kBorderStrong;
            foreground = ui::kTextPrimary;
            hover = ui::kCardHover;
            pressed = ui::kSunken;
            break;
        }

        button.Background(ui::Brush(background));
        button.BorderBrush(ui::Brush(border));
        button.Foreground(ui::Brush(foreground));

        // 覆盖控件模板的交互状态资源，暗色主题下才有正确的悬停/按下/禁用反馈。
        InsertBrush(button, L"ButtonBackground", background);
        InsertBrush(button, L"ButtonBackgroundPointerOver", hover);
        InsertBrush(button, L"ButtonBackgroundPressed", pressed);
        InsertBrush(button, L"ButtonBackgroundDisabled", ui::kCardInset);
        InsertBrush(button, L"ButtonBorderBrush", border);
        InsertBrush(button, L"ButtonBorderBrushPointerOver", variant == ButtonVariant::Ghost ? ui::Rgb(0, 0, 0, 0) : ui::kBorderStrong);
        InsertBrush(button, L"ButtonBorderBrushPressed", border);
        InsertBrush(button, L"ButtonBorderBrushDisabled", ui::kBorder);
        InsertBrush(button, L"ButtonForeground", foreground);
        InsertBrush(button, L"ButtonForegroundPointerOver", variant == ButtonVariant::Ghost ? ui::kTextPrimary : foreground);
        InsertBrush(button, L"ButtonForegroundPressed", foreground);
        InsertBrush(button, L"ButtonForegroundDisabled", ui::kTextMuted);
    }

    Button MakeButton(const hstring& caption, const hstring& glyph, const ButtonVariant variant, const double fontSize)
    {
        Button button;
        StackPanel content;
        content.Orientation(Orientation::Horizontal);
        content.Spacing(6);
        content.VerticalAlignment(VerticalAlignment::Center);

        Color foreground = ui::kTextPrimary;
        switch (variant)
        {
        case ButtonVariant::Primary: foreground = ui::kTextOnAccent; break;
        case ButtonVariant::Danger: foreground = ui::kDanger; break;
        case ButtonVariant::Ghost: foreground = ui::kTextSecondary; break;
        default: break;
        }

        if (glyph.size() > 0)
        {
            content.Children().Append(ui::Icon(glyph, fontSize + 1.0, foreground));
        }
        content.Children().Append(ui::Text(caption, fontSize, foreground, ui::kWeightMedium));
        button.Content(content);
        winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
            button, caption);
        button.Padding(Only(12, 6, 14, 6));
        button.MinHeight(30);
        button.CornerRadius(CornerRadius{ ui::kRadiusSm + 2, ui::kRadiusSm + 2, ui::kRadiusSm + 2, ui::kRadiusSm + 2 });
        button.BorderThickness(Thickness{ 1, 1, 1, 1 });
        button.VerticalAlignment(VerticalAlignment::Center);
        ApplyButtonVisuals(button, variant);
        return button;
    }

    CardParts MakeSectionCard(const hstring& title, const hstring& subtitle, const hstring& glyph,
        const Color accent, const Color accentSoft)
    {
        CardParts parts;
        Border card = ui::Surface(ui::kCard, ui::kBorder, ui::kRadiusLg, ui::kSpaceMd);
        Grid layout;
        layout.RowSpacing(ui::kSpaceMd);

        RowDefinition headerRow;
        headerRow.Height(GridLength{ 1, GridUnitType::Auto });
        layout.RowDefinitions().Append(headerRow);
        RowDefinition bodyRow;
        bodyRow.Height(GridLength{ 1, GridUnitType::Auto });
        layout.RowDefinitions().Append(bodyRow);

        Grid header;
        ColumnDefinition iconColumn;
        iconColumn.Width(GridLength{ 1, GridUnitType::Auto });
        header.ColumnDefinitions().Append(iconColumn);
        ColumnDefinition textColumn;
        textColumn.Width(GridLength{ 1, GridUnitType::Star });
        header.ColumnDefinitions().Append(textColumn);
        ColumnDefinition actionColumn;
        actionColumn.Width(GridLength{ 1, GridUnitType::Auto });
        header.ColumnDefinitions().Append(actionColumn);

        Border chip = IconChip(glyph, accent, accentSoft, 28, ui::kRadiusMd, 13);
        Grid::SetColumn(chip, 0);
        header.Children().Append(chip);

        StackPanel text;
        text.Spacing(3);
        text.Margin(Only(ui::kSpaceMd, 0, ui::kSpaceMd, 0));
        text.VerticalAlignment(VerticalAlignment::Center);
        text.Children().Append(ui::Text(title, ui::kFontSubtitle, ui::kTextPrimary, ui::kWeightSemiBold));
        parts.Subtitle = ui::TextWrapped(subtitle, ui::kFontCaption, ui::kTextMuted);
        parts.Subtitle.Visibility(subtitle.empty() ? Visibility::Collapsed : Visibility::Visible);
        text.Children().Append(parts.Subtitle);
        Grid::SetColumn(text, 1);
        header.Children().Append(text);

        parts.Actions = StackPanel();
        parts.Actions.Orientation(Orientation::Horizontal);
        parts.Actions.Spacing(5);
        parts.Actions.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetColumn(parts.Actions, 2);
        header.Children().Append(parts.Actions);

        layout.Children().Append(header);

        parts.Body = StackPanel();
        parts.Body.Spacing(9);
        Grid::SetRow(parts.Body, 1);
        layout.Children().Append(parts.Body);

        card.Child(layout);
        parts.Root = card;
        return parts;
    }

    UIElement MakeStatTile(const hstring& glyph, const Color accent, const Color accentSoft, const hstring& label,
        const hstring& value, const hstring& hint, TextBlock* valueOut, TextBlock* hintOut)
    {
        Border tile = ui::Surface(ui::kCard, ui::kBorder, ui::kRadiusLg, ui::kSpaceMd);
        StackPanel body;
        body.Spacing(6.0);

        StackPanel head;
        head.Orientation(Orientation::Horizontal);
        head.Spacing(ui::kSpaceSm);
        head.Children().Append(IconChip(glyph, accent, accentSoft, 26, ui::kRadiusSm, 13));
        TextBlock caption = ui::Text(label, ui::kFontCaption, ui::kTextSecondary, ui::kWeightMedium);
        caption.VerticalAlignment(VerticalAlignment::Center);
        head.Children().Append(caption);
        body.Children().Append(head);

        // 主数值直接取磁贴的语义色：四个磁贴各占一色，数值一眼可辨。
        TextBlock main = ui::Text(value, ui::kFontHero - 3, accent, ui::kWeightSemiBold);
        main.Margin(Only(0, ui::kSpaceSm, 0, 0));
        main.TextTrimming(TextTrimming::CharacterEllipsis);
        body.Children().Append(main);
        if (valueOut != nullptr)
        {
            *valueOut = main;
        }

        TextBlock note = ui::TextWrapped(hint, ui::kFontMicro + 0.5, ui::kTextMuted);
        body.Children().Append(note);
        if (hintOut != nullptr)
        {
            *hintOut = note;
        }

        tile.Child(body);
        return tile;
    }

    UIElement MakePropertyRow(const hstring& label, const hstring& value, const bool monospace, TextBlock* valueOut)
    {
        Grid row;
        ColumnDefinition labelColumn;
        labelColumn.Width(GridLength{ 96 });
        row.ColumnDefinitions().Append(labelColumn);
        ColumnDefinition valueColumn;
        valueColumn.Width(GridLength{ 1, GridUnitType::Star });
        row.ColumnDefinitions().Append(valueColumn);
        row.Padding(Only(0, 5, 0, 5));

        // 标签带冒号并与取值拉开一列宽度，避免“标签取值”连成一串。
        TextBlock caption = ui::Text(label + hstring(L"："), ui::kFontCaption, ui::kTextSecondary, ui::kWeightMedium);
        caption.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetColumn(caption, 0);
        row.Children().Append(caption);

        TextBlock text = monospace ? ui::Mono(value, ui::kFontMono, ui::kTextPrimary)
                                   : ui::Text(value, ui::kFontCaption, ui::kTextPrimary);
        text.TextTrimming(TextTrimming::CharacterEllipsis);
        text.VerticalAlignment(VerticalAlignment::Center);
        text.IsTextSelectionEnabled(true);
        ui::SetToolTip(text, value);
        Grid::SetColumn(text, 1);
        row.Children().Append(text);
        if (valueOut != nullptr)
        {
            *valueOut = text;
        }
        return row;
    }

    UIElement MakeStepRow(const int index, const hstring& title, const hstring& detail)
    {
        Grid row;
        ColumnDefinition indexColumn;
        indexColumn.Width(GridLength{ 1, GridUnitType::Auto });
        row.ColumnDefinitions().Append(indexColumn);
        ColumnDefinition textColumn;
        textColumn.Width(GridLength{ 1, GridUnitType::Star });
        row.ColumnDefinitions().Append(textColumn);

        Border badge;
        badge.Width(23);
        badge.Height(23);
        badge.Background(ui::Brush(ui::kAccentSoft));
        badge.CornerRadius(CornerRadius{ 11.5, 11.5, 11.5, 11.5 });
        badge.VerticalAlignment(VerticalAlignment::Top);
        TextBlock number = ui::Text(to_hstring(index), ui::kFontMicro + 0.5, ui::kAccentText, ui::kWeightSemiBold);
        number.TextAlignment(TextAlignment::Center);
        number.HorizontalAlignment(HorizontalAlignment::Center);
        number.VerticalAlignment(VerticalAlignment::Center);
        badge.Child(number);
        Grid::SetColumn(badge, 0);
        row.Children().Append(badge);

        StackPanel text;
        text.Spacing(4);
        text.Margin(Only(ui::kSpaceMd, 0, 0, 0));
        text.Children().Append(ui::Text(title, ui::kFontCaption, ui::kTextPrimary, ui::kWeightMedium));
        text.Children().Append(ui::TextWrapped(detail, ui::kFontCaption - 0.5, ui::kTextMuted));
        Grid::SetColumn(text, 1);
        row.Children().Append(text);
        return row;
    }

    UIElement MakeToggleRow(const hstring& glyph, const hstring& title, const hstring& detail,
        const Color accent, ToggleSwitch* switchOut)
    {
        Border row = ui::Surface(ui::kCardInset, ui::kBorder, ui::kRadiusMd, ui::kSpaceLg);
        Grid layout;
        ColumnDefinition iconColumn;
        iconColumn.Width(GridLength{ 1, GridUnitType::Auto });
        layout.ColumnDefinitions().Append(iconColumn);
        ColumnDefinition textColumn;
        textColumn.Width(GridLength{ 1, GridUnitType::Star });
        layout.ColumnDefinitions().Append(textColumn);
        ColumnDefinition switchColumn;
        switchColumn.Width(GridLength{ 1, GridUnitType::Auto });
        layout.ColumnDefinitions().Append(switchColumn);

        Border chip = IconChip(glyph, accent, ui::kAccentSoft, 28, ui::kRadiusSm, 14);
        Grid::SetColumn(chip, 0);
        layout.Children().Append(chip);

        StackPanel text;
        text.Spacing(3);
        text.Margin(Only(ui::kSpaceMd, 0, ui::kSpaceMd, 0));
        text.Children().Append(ui::Text(title, ui::kFontCaption, ui::kTextPrimary, ui::kWeightMedium));
        text.Children().Append(ui::TextWrapped(detail, ui::kFontCaption - 0.5, ui::kTextMuted));
        Grid::SetColumn(text, 1);
        layout.Children().Append(text);

        ToggleSwitch toggle;
        toggle.MinWidth(0);
        toggle.OnContent(box_value(hstring(L"")));
        toggle.OffContent(box_value(hstring(L"")));
        toggle.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetColumn(toggle, 2);
        layout.Children().Append(toggle);
        if (switchOut != nullptr)
        {
            *switchOut = toggle;
        }

        row.Child(layout);
        return row;
    }

    UIElement MakeCheckRow(const hstring& label, const hstring& path, const bool ok, TextBlock* statusOut)
    {
        Grid row;
        row.Padding(ui::Insets(0, ui::kSpaceXs, 0, ui::kSpaceXs));
        ColumnDefinition stateColumn;
        stateColumn.Width(GridLength{ 1, GridUnitType::Auto });
        row.ColumnDefinitions().Append(stateColumn);
        ColumnDefinition labelColumn;
        labelColumn.Width(GridLength{ 120 });
        row.ColumnDefinitions().Append(labelColumn);
        ColumnDefinition pathColumn;
        pathColumn.Width(GridLength{ 1, GridUnitType::Star });
        row.ColumnDefinitions().Append(pathColumn);
        ColumnDefinition statusColumn;
        statusColumn.Width(GridLength{ 1, GridUnitType::Auto });
        row.ColumnDefinitions().Append(statusColumn);

        winrt::Microsoft::UI::Xaml::Controls::FontIcon state =
            ui::Icon(ok ? ui::kGlyphCheck : ui::kGlyphError, 12, ok ? ui::kSuccess : ui::kDanger);
        state.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetColumn(state, 0);
        row.Children().Append(state);

        TextBlock caption = ui::Text(label, ui::kFontCaption, ui::kTextPrimary);
        caption.Margin(Only(ui::kSpaceSm, 0, 0, 0));
        caption.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetColumn(caption, 1);
        row.Children().Append(caption);

        TextBlock detail = ui::Mono(path, ui::kFontMono - 0.5, ui::kTextMuted);
        detail.Margin(Only(ui::kSpaceSm, 0, 0, 0));
        detail.TextTrimming(TextTrimming::CharacterEllipsis);
        detail.VerticalAlignment(VerticalAlignment::Center);
        ui::SetToolTip(detail, path);
        Grid::SetColumn(detail, 2);
        row.Children().Append(detail);

        TextBlock status = ui::Text(ok ? L"已就绪" : L"缺失", ui::kFontMicro + 0.5,
            ok ? ui::kSuccess : ui::kDanger, ui::kWeightMedium);
        status.VerticalAlignment(VerticalAlignment::Center);
        status.HorizontalAlignment(HorizontalAlignment::Right);
        status.Margin(Only(ui::kSpaceSm, 0, 0, 0));
        Grid::SetColumn(status, 3);
        row.Children().Append(status);
        if (statusOut != nullptr)
        {
            *statusOut = status;
        }

        return row;
    }

    UIElement MakeEmptyState(const hstring& glyph, const hstring& title, const hstring& detail, Button* actionOut)
    {
        StackPanel panel;
        panel.Spacing(ui::kSpaceMd);
        panel.HorizontalAlignment(HorizontalAlignment::Center);
        panel.VerticalAlignment(VerticalAlignment::Center);
        panel.MaxWidth(300);

        winrt::Microsoft::UI::Xaml::Controls::FontIcon icon = ui::Icon(glyph, 30, ui::kTextMuted);
        icon.HorizontalAlignment(HorizontalAlignment::Center);
        panel.Children().Append(icon);

        TextBlock heading = ui::Text(title, ui::kFontBody, ui::kTextSecondary, ui::kWeightMedium);
        heading.HorizontalAlignment(HorizontalAlignment::Center);
        panel.Children().Append(heading);

        TextBlock body = ui::TextWrapped(detail, ui::kFontCaption - 0.5, ui::kTextMuted);
        body.TextAlignment(TextAlignment::Center);
        body.HorizontalAlignment(HorizontalAlignment::Center);
        panel.Children().Append(body);

        Button action = MakeButton(L"添加调试器", ui::kGlyphAdd, ButtonVariant::Secondary);
        action.Margin(Only(0, ui::kSpaceSm, 0, 0));
        action.HorizontalAlignment(HorizontalAlignment::Center);
        // 只有调用方要接管按钮时才渲染：没人接管时按钮点了也没反应，不如不放。
        if (actionOut != nullptr)
        {
            *actionOut = action;
            panel.Children().Append(action);
        }

        return panel;
    }

    UIElement MakeStatusRow(const hstring& glyph, const hstring& label, const hstring& value,
        const Color valueColor, TextBlock* valueOut)
    {
        Grid row;
        ColumnDefinition iconColumn;
        iconColumn.Width(GridLength{ 16 });
        row.ColumnDefinitions().Append(iconColumn);
        ColumnDefinition labelColumn;
        labelColumn.Width(GridLength{ 1, GridUnitType::Star });
        row.ColumnDefinitions().Append(labelColumn);
        ColumnDefinition valueColumn;
        valueColumn.Width(GridLength{ 1, GridUnitType::Auto });
        row.ColumnDefinitions().Append(valueColumn);
        row.Padding(Only(0, 3, 0, 3));

        FontIcon icon = ui::Icon(glyph, 12, ui::kTextMuted);
        icon.HorizontalAlignment(HorizontalAlignment::Left);
        Grid::SetColumn(icon, 0);
        row.Children().Append(icon);

        TextBlock caption = ui::Text(label, ui::kFontMicro + 1, ui::kTextMuted);
        caption.VerticalAlignment(VerticalAlignment::Center);
        caption.Margin(Only(4, 0, 4, 0));
        Grid::SetColumn(caption, 1);
        row.Children().Append(caption);

        TextBlock text = ui::Text(value, ui::kFontMicro + 1, valueColor, ui::kWeightMedium);
        text.HorizontalAlignment(HorizontalAlignment::Right);
        text.VerticalAlignment(VerticalAlignment::Center);
        text.TextTrimming(TextTrimming::CharacterEllipsis);
        ui::SetToolTip(text, value);
        Grid::SetColumn(text, 2);
        row.Children().Append(text);
        if (valueOut != nullptr)
        {
            *valueOut = text;
        }
        return row;
    }

    UIElement MakeSectionLabel(const hstring& value)
    {
        TextBlock label = ui::Text(value, ui::kFontMicro + 0.5, ui::kTextMuted, ui::kWeightSemiBold);
        label.Margin(Only(0, 0, 0, 8));
        return label;
    }

    Border MakeBadge(const hstring& value, const Color foreground, const Color background)
    {
        return ui::Pill(value, foreground, background, ui::kFontMicro + 0.5);
    }
}
