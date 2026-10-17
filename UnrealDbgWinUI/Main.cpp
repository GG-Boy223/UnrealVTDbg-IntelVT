//VT Debugger - native WinUI 3 front end.
//
// The UI never calls the legacy backend directly. All shared state lives in
// Bridge/Runtime; this file renders state and sends narrow user actions.

#include "Bridge.h"
#include "UiComponents.h"
#include "UiTheme.h"

#include <commdlg.h>
#include <microsoft.ui.xaml.window.h>
#include <MddBootstrap.h>
#include <Psapi.h>
#include <shellapi.h>
#include <ShlObj.h>
#include <wincodec.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Foundation.Collections.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;
using winrt::Windows::Foundation::IInspectable;
using winrt::Windows::UI::Color;

namespace ui = unrealdbg_ui;
namespace tok = unrealdbg_ui::token;

namespace
{
    constexpr wchar_t kProductName[] = L"F3";
    constexpr wchar_t kProductSubtitle[] = L"VT 调试台";
    constexpr int kPageCount = 8;
    int g_smokeExitCode = 0;

    void StartupTrace(const std::wstring& text) noexcept
    {
        try
        {
            unrealdbg_native::WriteLog(
                unrealdbg_native::LogLevel::Info, text);
        }
        catch (...)
        {
        }
    }

    enum class PageId : int32_t
    {
        Overview = 0,
        Debuggers = 1,
        Target = 2,
        Hooks = 3,
        Logs = 4,
        System = 5,
        Symbols = 6,
        Diagnostics = 7,
    };

    struct HookUiDescriptor
    {
        std::uint64_t mask;
        const wchar_t* symbol;
        const wchar_t* description;
        int category;
        bool windows11;
    };

    constexpr std::array<HookUiDescriptor, 36> kHookDescriptors = {{
        { HOOK_SELECTION_NT_CREATE_DEBUG_OBJECT, L"NtCreateDebugObject",
            L"创建虚拟调试对象，是调试会话入口，安装顺序必须最先。", 0, true },
        { HOOK_SELECTION_PSP_INSERT_PROCESS, L"PspInsertProcess",
            L"接管进程插入路径，关联调试对象的进程生命周期。", 0, true },
        { HOOK_SELECTION_NT_SET_INFORMATION_DEBUG_OBJECT, L"NtSetInformationDebugObject",
            L"处理调试对象信息设置，接管附加参数与目标关联。", 0, true },
        { HOOK_SELECTION_NT_REMOVE_PROCESS_DEBUG, L"NtRemoveProcessDebug",
            L"接管移除调试对象与分离流程。", 0, true },
        { HOOK_SELECTION_NT_DEBUG_ACTIVE_PROCESS, L"NtDebugActiveProcess",
            L"接管调试附加请求，进入虚拟调试会话。", 0, true },
        { HOOK_SELECTION_NT_WAIT_FOR_DEBUG_EVENT, L"NtWaitForDebugEvent",
            L"接管调试事件等待与内核事件投递。", 0, true },
        { HOOK_SELECTION_NT_DEBUG_CONTINUE, L"NtDebugContinue",
            L"接管继续调试事件，恢复目标线程执行。", 0, true },
        { HOOK_SELECTION_DBGK_MAP_VIEW_OF_SECTION, L"DbgkMapViewOfSection",
            L"生成 DLL 加载调试事件。", 0, true },
        { HOOK_SELECTION_DBGK_UNMAP_VIEW_OF_SECTION, L"DbgkUnMapViewOfSection",
            L"生成 DLL 卸载调试事件。", 0, true },
        { HOOK_SELECTION_DBGK_CREATE_THREAD, L"DbgkCreateThread",
            L"生成线程创建调试事件，缺失会少线程事件。", 0, true },
        { HOOK_SELECTION_DBGK_EXIT_THREAD, L"DbgkExitThread",
            L"生成线程退出调试事件并完成状态清理。", 0, true },
        { HOOK_SELECTION_DBGK_EXIT_PROCESS, L"DbgkExitProcess",
            L"生成进程退出调试事件。", 0, true },
        { HOOK_SELECTION_DBGK_FORWARD_EXCEPTION, L"DbgkForwardException",
            L"接管调试异常转发与事件注入路径。", 0, true },
        { HOOK_SELECTION_DBGKP_QUEUE_MESSAGE, L"DbgkpQueueMessage",
            L"接管调试消息队列，控制事件投递。", 0, true },
        { HOOK_SELECTION_PSP_CALL_THREAD_NOTIFY_ROUTINES, L"PspCallThreadNotifyRoutines",
            L"接管线程通知回调，补充底层调试事件兼容路径。", 0, false },
        { HOOK_SELECTION_PSP_EXIT_THREAD, L"PspExitThread",
            L"接管线程退出与调试端口比较，清理挂起调试状态。", 0, true },
        { HOOK_SELECTION_OBP_REFERENCE_OBJECT_BY_HANDLE_TAG, L"ObpReferenceObjectByHandleWithTag",
            L"保护带 Tag 的句柄引用，隐藏虚拟调试对象关联。", 1, true },
        { HOOK_SELECTION_NT_CLOSE, L"NtClose",
            L"接管调试对象句柄关闭，避免会话提前失效。", 1, true },
        { HOOK_SELECTION_NT_DUPLICATE_OBJECT, L"NtDuplicateObject",
            L"保护调试对象句柄复制与查询。", 1, true },
        { HOOK_SELECTION_DBGK_OPEN_PROCESS_DEBUG_PORT, L"DbgkOpenProcessDebugPort",
            L"保护调试端口打开路径，处理调试对象查询。", 1, true },
        { HOOK_SELECTION_NT_QUERY_INFORMATION_PROCESS, L"NtQueryInformationProcess",
            L"伪装 DebugPort、DebugObject 等进程调试信息。", 1, true },
        { HOOK_SELECTION_NT_SET_INFORMATION_PROCESS, L"NtSetInformationProcess",
            L"接管进程调试信息设置。", 1, true },
        { HOOK_SELECTION_NT_QUERY_OBJECT, L"NtQueryObject",
            L"处理对象类型与调试对象查询。", 1, true },
        { HOOK_SELECTION_NT_QUERY_SYSTEM_INFORMATION, L"NtQuerySystemInformation",
            L"接管系统信息枚举，减少调试状态暴露。", 1, true },
        { HOOK_SELECTION_NT_SYSTEM_DEBUG_CONTROL, L"NtSystemDebugControl",
            L"接管系统调试控制请求。", 1, true },
        { HOOK_SELECTION_NT_SET_INFORMATION_THREAD, L"NtSetInformationThread",
            L"处理 HideFromDebugger 等线程调试标志。", 1, true },
        { HOOK_SELECTION_NT_QUERY_INFORMATION_THREAD, L"NtQueryInformationThread",
            L"接管线程调试相关信息查询。", 1, true },
        { HOOK_SELECTION_NT_GET_CONTEXT_THREAD, L"NtGetContextThread",
            L"保护线程上下文读取。", 1, true },
        { HOOK_SELECTION_NT_SET_CONTEXT_THREAD, L"NtSetContextThread",
            L"接管线程上下文写入。", 1, true },
        { HOOK_SELECTION_NT_CREATE_THREAD_EX, L"NtCreateThreadEx",
            L"接管远程线程创建与调试事件关联。", 1, true },
        { HOOK_SELECTION_VALIDATE_HWND, L"ValidateHwnd",
            L"保护窗口句柄验证，是 win32k 窗口 Hook 的入口。", 2, true },
        { HOOK_SELECTION_NT_USER_FIND_WINDOW_EX, L"NtUserFindWindowEx",
            L"接管按类名和标题查找窗口。", 2, true },
        { HOOK_SELECTION_NT_USER_WINDOW_FROM_POINT, L"NtUserWindowFromPoint",
            L"接管坐标窗口查询，减少窗口指纹检测。", 2, true },
        { HOOK_SELECTION_NT_USER_SET_WINDOWS_HOOK_EX, L"NtUserSetWindowsHookEx",
            L"接管全局窗口 Hook 注册。", 2, true },
        { HOOK_SELECTION_NT_USER_BUILD_HWND_LIST, L"NtUserBuildHwndList",
            L"接管窗口列表枚举，可过滤受保护窗口。", 2, false },
        { HOOK_SELECTION_NT_USER_GET_CLASS_NAME, L"NtUserGetClassName",
            L"接管窗口类名查询，减少窗口指纹暴露。", 2, true },
    }};

    hstring H(const std::wstring& text)
    {
        return hstring(text);
    }

    std::wstring ByteCountText(const std::uint64_t bytes)
    {
        wchar_t buffer[64]{};
        if (bytes < 1024)
        {
            swprintf_s(buffer, L"%llu B",
                static_cast<unsigned long long>(bytes));
        }
        else if (bytes < 1024ull * 1024ull)
        {
            swprintf_s(buffer, L"%.1f KB",
                static_cast<double>(bytes) / 1024.0);
        }
        else
        {
            swprintf_s(buffer, L"%.1f MB",
                static_cast<double>(bytes) / (1024.0 * 1024.0));
        }
        return buffer;
    }

    std::wstring HexAddress(const std::uint64_t value)
    {
        wchar_t buffer[24]{};
        swprintf_s(buffer, L"0x%llX",
            static_cast<unsigned long long>(value));
        return buffer;
    }

    std::wstring LowercaseText(std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](const wchar_t character)
            {
                return static_cast<wchar_t>(std::towlower(character));
            });
        return value;
    }

    bool ContainsInsensitive(
        const std::wstring_view text, const std::wstring_view needle) noexcept
    {
        if (needle.empty())
        {
            return true;
        }
        if (needle.size() > text.size())
        {
            return false;
        }
        for (size_t start = 0; start + needle.size() <= text.size(); ++start)
        {
            size_t index = 0;
            for (; index < needle.size(); ++index)
            {
                if (std::towlower(text[start + index]) != needle[index])
                {
                    break;
                }
            }
            if (index == needle.size())
            {
                return true;
            }
        }
        return false;
    }

    std::wstring ShortPdbName(const std::wstring& path)
    {
        const std::filesystem::path value(path);
        std::wstring result = value.filename().wstring();
        const std::wstring parent = value.parent_path().filename().wstring();
        if (parent.size() >= 8 &&
            std::all_of(parent.begin(), parent.begin() + 8,
                [](const wchar_t character)
                {
                    return std::iswxdigit(character) != 0;
                }))
        {
            result += L" · ";
            result += parent.substr(0, 8);
        }
        return result;
    }

    void SetButtonCaption(const Button& button, const hstring& caption)
    {
        if (button == nullptr)
        {
            return;
        }
        const StackPanel panel = button.Content().try_as<StackPanel>();
        if (panel == nullptr)
        {
            return;
        }
        for (const auto& child : panel.Children())
        {
            TextBlock text = child.try_as<TextBlock>();
            if (text != nullptr)
            {
                text.Text(caption);
                return;
            }
        }
    }

    void SetButtonEnabled(const Button& button, const bool enabled)
    {
        if (button == nullptr)
        {
            return;
        }
        button.IsEnabled(enabled);
        button.Opacity(enabled ? 1.0 : 0.42);
    }

    Button MakeIconButton(const hstring& glyph, const hstring& tooltip,
        ui::ButtonVariant variant = ui::ButtonVariant::Ghost)
    {
        Button button;
        button.Content(tok::Icon(glyph, 14, variant == ui::ButtonVariant::Primary
            ? tok::kTextOnAccent : tok::kTextSecondary));
        winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
            button, tooltip);
        button.Width(34);
        button.Height(30);
        button.Padding(Thickness{ 0, 0, 0, 0 });
        button.CornerRadius(CornerRadius{
            tok::kRadiusSm, tok::kRadiusSm, tok::kRadiusSm, tok::kRadiusSm });
        button.BorderThickness(Thickness{ 1, 1, 1, 1 });
        button.VerticalAlignment(VerticalAlignment::Center);
        ui::ApplyButtonVisuals(button, variant);
        tok::SetToolTip(button, tooltip);
        return button;
    }

    Border MakeSeal(const hstring& value, const double size = 30.0)
    {
        Border seal;
        seal.Width(size);
        seal.Height(size);
        seal.CornerRadius(CornerRadius{
            tok::kRadiusSm, tok::kRadiusSm, tok::kRadiusSm, tok::kRadiusSm });
        seal.Background(tok::Brush(tok::kCinnabarSoft));
        seal.BorderBrush(tok::Brush(tok::kCinnabar));
        seal.BorderThickness(Thickness{ 1, 1, 1, 1 });
        TextBlock text = tok::Text(value, size * 0.45, tok::kCinnabar,
            tok::kWeightSemiBold);
        text.HorizontalAlignment(HorizontalAlignment::Center);
        text.VerticalAlignment(VerticalAlignment::Center);
        text.TextAlignment(TextAlignment::Center);
        seal.Child(text);
        return seal;
    }

    Border MakeDivider(const Color color, const double thickness = 1.0)
    {
        Border line;
        line.Height(thickness);
        line.Background(tok::Brush(color));
        line.HorizontalAlignment(HorizontalAlignment::Stretch);
        return line;
    }

    Grid MakeSectionHeading(const hstring& title, const hstring& subtitle = L"")
    {
        Grid row;
        row.ColumnDefinitions().Append(ColumnDefinition());
        row.ColumnDefinitions().GetAt(0).Width(GridLength{ 3 });
        ColumnDefinition textColumn;
        textColumn.Width(GridLength{ 1, GridUnitType::Star });
        row.ColumnDefinitions().Append(textColumn);
        row.ColumnSpacing(9);

        Border marker;
        marker.Width(3);
        marker.Height(subtitle.empty() ? 16 : 30);
        marker.Background(tok::Brush(tok::kAccent));
        marker.CornerRadius(CornerRadius{ 1.5, 1.5, 1.5, 1.5 });
        marker.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetColumn(marker, 0);
        row.Children().Append(marker);

        StackPanel labels;
        labels.Spacing(2);
        labels.Children().Append(tok::Text(title, tok::kFontSubtitle,
            tok::kTextPrimary, tok::kWeightSemiBold));
        if (!subtitle.empty())
        {
            labels.Children().Append(tok::Text(subtitle, tok::kFontMicro + 0.5,
                tok::kTextMuted));
        }
        Grid::SetColumn(labels, 1);
        row.Children().Append(labels);
        return row;
    }

    Border MakeSurface(const double radius = tok::kRadiusLg,
        const double padding = tok::kSpaceMd)
    {
        return tok::Surface(tok::kCard, tok::kBorder, radius, padding);
    }

    void OpenExplorer(const std::wstring& path, const bool selectFile = false)
    {
        if (path.empty())
        {
            return;
        }
        if (selectFile)
        {
            const std::wstring arguments = L"/select,\"" + path + L"\"";
            ::ShellExecuteW(nullptr, L"open", L"explorer.exe",
                arguments.c_str(), nullptr, SW_SHOWNORMAL);
            return;
        }
        ::ShellExecuteW(nullptr, L"open", path.c_str(),
            nullptr, nullptr, SW_SHOWNORMAL);
    }

    void OpenExternalLink(const std::wstring& url)
    {
        if (url.empty())
        {
            return;
        }
        const HINSTANCE result = ::ShellExecuteW(nullptr, L"open",
            url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) <= 32)
        {
            unrealdbg_native::WriteLog(
                unrealdbg_native::LogLevel::Error,
                L"无法打开外部链接：" + url);
        }
    }

    bool PickExecutable(const HWND owner, std::wstring& path)
    {
        std::vector<wchar_t> buffer(32768);
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = owner;
        dialog.lpstrFilter =
            L"可执行文件 (*.exe)\0*.exe\0所有文件 (*.*)\0*.*\0\0";
        dialog.lpstrFile = buffer.data();
        dialog.nMaxFile = static_cast<DWORD>(buffer.size());
        dialog.lpstrTitle = L"选择外部调试器";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
            OFN_NOCHANGEDIR | OFN_DONTADDTORECENT;
        if (!::GetOpenFileNameW(&dialog))
        {
            return false;
        }
        path.assign(buffer.data());
        return true;
    }

    bool PickPdb(const HWND owner, std::wstring& path)
    {
        std::vector<wchar_t> buffer(32768);
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = owner;
        dialog.lpstrFilter =
            L"符号文件 (*.pdb)\0*.pdb\0所有文件 (*.*)\0*.*\0\0";
        dialog.lpstrFile = buffer.data();
        dialog.nMaxFile = static_cast<DWORD>(buffer.size());
        dialog.lpstrTitle = L"选择自定义 PDB";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
            OFN_NOCHANGEDIR | OFN_DONTADDTORECENT;
        if (!::GetOpenFileNameW(&dialog))
        {
            return false;
        }
        path.assign(buffer.data());
        return true;
    }

    void ConfigureListView(ListView& list)
    {
        list.SelectionMode(ListViewSelectionMode::Single);
        list.IsItemClickEnabled(true);
        list.Background(tok::Brush(tok::Rgb(0, 0, 0, 0)));
        list.BorderThickness(Thickness{ 0, 0, 0, 0 });
        list.Padding(Thickness{ 0, 0, 0, 0 });
        list.ItemContainerTransitions(
            winrt::Microsoft::UI::Xaml::Media::Animation::TransitionCollection());
    }

    WriteableBitmap LoadExecutableIconBitmap(const std::wstring& path)
    {
        SHFILEINFOW fileInfo{};
        if (!::SHGetFileInfoW(path.c_str(), 0, &fileInfo, sizeof(fileInfo),
            SHGFI_ICON | SHGFI_LARGEICON) || fileInfo.hIcon == nullptr)
        {
            return nullptr;
        }

        struct IconGuard
        {
            HICON icon;
            ~IconGuard()
            {
                if (icon != nullptr)
                {
                    ::DestroyIcon(icon);
                }
            }
        } iconGuard{ fileInfo.hIcon };

        winrt::com_ptr<IWICImagingFactory> factory;
        if (FAILED(::CoCreateInstance(CLSID_WICImagingFactory, nullptr,
            CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory),
            factory.put_void())))
        {
            return nullptr;
        }

        winrt::com_ptr<IWICBitmap> source;
        if (FAILED(factory->CreateBitmapFromHICON(
            fileInfo.hIcon, source.put())))
        {
            return nullptr;
        }

        winrt::com_ptr<IWICFormatConverter> converter;
        if (FAILED(factory->CreateFormatConverter(converter.put())) ||
            FAILED(converter->Initialize(source.get(),
                GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                nullptr, 0.0, WICBitmapPaletteTypeCustom)))
        {
            return nullptr;
        }

        winrt::com_ptr<IWICBitmap> converted;
        if (FAILED(factory->CreateBitmapFromSource(converter.get(),
            WICBitmapCacheOnLoad, converted.put())))
        {
            return nullptr;
        }

        UINT width{};
        UINT height{};
        if (FAILED(converted->GetSize(&width, &height)) ||
            width == 0 || height == 0)
        {
            return nullptr;
        }

        WICRect rect{ 0, 0, static_cast<INT>(width), static_cast<INT>(height) };
        winrt::com_ptr<IWICBitmapLock> lock;
        if (FAILED(converted->Lock(
            &rect, WICBitmapLockRead, lock.put())))
        {
            return nullptr;
        }

        UINT sourceStride{};
        UINT sourceBytes{};
        BYTE* sourcePixels{};
        if (FAILED(lock->GetStride(&sourceStride)) ||
            FAILED(lock->GetDataPointer(&sourceBytes, &sourcePixels)) ||
            sourcePixels == nullptr || sourceStride < width * 4)
        {
            return nullptr;
        }

        WriteableBitmap bitmap(static_cast<int32_t>(width),
            static_cast<int32_t>(height));
        const auto pixelBuffer = bitmap.PixelBuffer();
        const auto access =
            pixelBuffer.as<winrt::impl::IBufferByteAccess>();
        BYTE* destinationPixels{};
        if (access == nullptr ||
            FAILED(access->Buffer(&destinationPixels)) ||
            destinationPixels == nullptr)
        {
            return nullptr;
        }

        const size_t destinationStride = static_cast<size_t>(width) * 4;
        for (UINT row = 0; row < height; ++row)
        {
            memcpy(destinationPixels + row * destinationStride,
                sourcePixels + row * sourceStride, destinationStride);
        }
        bitmap.Invalidate();
        return bitmap;
    }

    UIElement MakeExecutableIcon(const hstring& path, const double size)
    {
        Grid host;
        host.Width(size);
        host.Height(size);
        host.VerticalAlignment(VerticalAlignment::Center);

        Border background = tok::Surface(tok::kCardInset, tok::kBorder,
            tok::kRadiusSm, 0);
        background.Width(size);
        background.Height(size);
        host.Children().Append(background);

        FontIcon fallback = tok::Icon(tok::kGlyphProcess,
            size * 0.48, tok::kGold);
        fallback.HorizontalAlignment(HorizontalAlignment::Center);
        fallback.VerticalAlignment(VerticalAlignment::Center);
        host.Children().Append(fallback);

        Image image;
        image.Width(size - 4);
        image.Height(size - 4);
        image.Stretch(Stretch::Uniform);
        image.HorizontalAlignment(HorizontalAlignment::Center);
        image.VerticalAlignment(VerticalAlignment::Center);
        image.Visibility(Visibility::Collapsed);
        host.Children().Append(image);

        const WriteableBitmap bitmap =
            LoadExecutableIconBitmap(std::wstring(path.c_str(), path.size()));
        if (bitmap != nullptr)
        {
            image.Source(bitmap);
            image.Visibility(Visibility::Visible);
            fallback.Visibility(Visibility::Collapsed);
        }
        return host;
    }
}

class Shell final
{
public:
    Shell(std::wstring, const bool smokeTest)
        : smokeTest_(smokeTest)
    {
    }

    void Launch()
    {
        window_ = Window();
        window_.Title(L"F3 VT 调试台");
        window_.Closed({ this, &Shell::OnClosed });

        controller_.Start();
        const wchar_t* launchCommandLine = ::GetCommandLineW();
        darkTheme_ = controller_.DarkTheme();
        if (launchCommandLine != nullptr &&
            std::wcsstr(launchCommandLine, L"--ui-theme=light") != nullptr)
        {
            darkTheme_ = false;
        }
        else if (launchCommandLine != nullptr &&
            std::wcsstr(launchCommandLine, L"--ui-theme=dark") != nullptr)
        {
            darkTheme_ = true;
        }
        if (launchCommandLine != nullptr)
        {
            const wchar_t* customPdb = std::wcsstr(
                launchCommandLine, L"--ui-symbol-pdb=");
            if (customPdb != nullptr)
            {
                customPdb += std::wcslen(L"--ui-symbol-pdb=");
                const wchar_t quote = *customPdb == L'"' ? L'"' : L'\0';
                if (quote != L'\0')
                {
                    ++customPdb;
                }
                const wchar_t* end = customPdb;
                while (*end != L'\0' &&
                    (quote != L'\0' ? *end != quote : !iswspace(*end)))
                {
                    ++end;
                }
                customSymbolPdbPath_.assign(customPdb, end);
            }
            const wchar_t* symbolSearch = std::wcsstr(
                launchCommandLine, L"--ui-symbol-search=");
            if (symbolSearch != nullptr)
            {
                symbolSearch += std::wcslen(L"--ui-symbol-search=");
                const wchar_t* end = symbolSearch;
                while (*end != L'\0' && !iswspace(*end))
                {
                    ++end;
                }
                symbolSearchText_.assign(symbolSearch, end);
            }
            if (std::wcsstr(launchCommandLine,
                L"--ui-symbol-used-only") != nullptr)
            {
                symbolUsedOnlyFilter_ = true;
            }
        }
        tok::SetTheme(darkTheme_);
        BuildLayout();
        window_.Content(root_);
        window_.ExtendsContentIntoTitleBar(true);
        window_.SetTitleBar(titleBar_);
        ApplyWindowTheme();
        window_.Activate();
        ApplyInitialWindowSize();

        RefreshAll();
        AppendPendingLogs();
        PageId initialPage = PageId::Overview;
        const wchar_t* commandLine = ::GetCommandLineW();
        if (commandLine != nullptr &&
            std::wcsstr(commandLine, L"--ui-page=debuggers") != nullptr)
        {
            initialPage = PageId::Debuggers;
        }
        else if (commandLine != nullptr &&
            std::wcsstr(commandLine, L"--ui-page=target") != nullptr)
        {
            initialPage = PageId::Target;
        }
        else if (commandLine != nullptr &&
            std::wcsstr(commandLine, L"--ui-page=diagnostics") != nullptr)
        {
            initialPage = PageId::Diagnostics;
        }
        else if (commandLine != nullptr &&
            std::wcsstr(commandLine, L"--ui-page=hooks") != nullptr)
        {
            initialPage = PageId::Hooks;
        }
        else if (commandLine != nullptr &&
            std::wcsstr(commandLine, L"--ui-page=logs") != nullptr)
        {
            initialPage = PageId::Logs;
        }
        else if (commandLine != nullptr &&
            std::wcsstr(commandLine, L"--ui-page=system") != nullptr)
        {
            initialPage = PageId::System;
        }
        else if (commandLine != nullptr &&
            std::wcsstr(commandLine, L"--ui-page=symbols") != nullptr)
        {
            initialPage = PageId::Symbols;
        }
        SelectPage(initialPage);

        timer_ = DispatcherTimer();
        timer_.Interval(std::chrono::milliseconds(500));
        timer_.Tick({ this, &Shell::OnTimer });
        timer_.Start();
        UpdatePollingInterval();

        const bool symbolAnalysisTest = launchCommandLine != nullptr &&
            std::wcsstr(launchCommandLine,
                L"--ui-symbol-analysis-test") != nullptr;
        if (smokeTest_ && !symbolAnalysisTest)
        {
            smokeTimer_ = DispatcherTimer();
            smokeTimer_.Interval(std::chrono::milliseconds(1600));
            smokeTimer_.Tick([this](const IInspectable& sender,
                const IInspectable&)
            {
                auto timer = sender.try_as<DispatcherTimer>();
                if (timer != nullptr)
                {
                    timer.Stop();
                }
                const bool ready = overviewPage_ != nullptr &&
                    debuggerPage_ != nullptr && targetPage_ != nullptr &&
                    hooksPage_ != nullptr && logsPage_ != nullptr &&
                    systemPage_ != nullptr && symbolsPage_ != nullptr &&
                    diagnosticsPage_ != nullptr && initButton_ != nullptr;
                StartupTrace(L"[ui] smoke ready=" + std::to_wstring(ready));
                ::ExitProcess(ready ? 0u : 1u);
            });
            smokeTimer_.Start();
        }

        if (symbolAnalysisTest)
        {
            symbolTestDeadline_ = ::GetTickCount64() + 120000ULL;
            symbolTestTimer_ = DispatcherTimer();
            symbolTestTimer_.Interval(std::chrono::milliseconds(300));
            symbolTestTimer_.Tick([this](const IInspectable& sender,
                const IInspectable&)
            {
                auto timer = sender.try_as<DispatcherTimer>();
                const auto state = controller_.Symbols().AnalysisState();
                if (state == unrealdbg_native::SymbolAnalysisState::Idle &&
                    controller_.Symbols().IsReady())
                {
                    StartSymbolAnalysis();
                }
                if (state == unrealdbg_native::SymbolAnalysisState::Ready)
                {
                    std::wstring missing;
                    const auto symbolSnapshot =
                        controller_.Symbols().SystemSymbols();
                    if (symbolSnapshot != nullptr)
                    {
                        for (const auto& item : *symbolSnapshot)
                        {
                            if (!item.found)
                            {
                                missing += L" " + item.symbolName;
                            }
                        }
                    }
                    StartupTrace(L"[ui] symbol analysis ready=" +
                        controller_.Symbols().AnalysisSummary() +
                        L" details=" + missing);
                    if (timer != nullptr)
                    {
                        timer.Stop();
                    }
                    ::ExitProcess(0);
                }
                if (state == unrealdbg_native::SymbolAnalysisState::Failed)
                {
                    StartupTrace(L"[ui] symbol analysis failed=" +
                        controller_.Symbols().AnalysisError());
                    if (timer != nullptr)
                    {
                        timer.Stop();
                    }
                    ::ExitProcess(1);
                }
                if (::GetTickCount64() >= symbolTestDeadline_)
                {
                    StartupTrace(L"[ui] symbol analysis timeout");
                    if (timer != nullptr)
                    {
                        timer.Stop();
                    }
                    ::ExitProcess(1);
                }
            });
            symbolTestTimer_.Start();
        }

        if (launchCommandLine != nullptr &&
            std::wcsstr(launchCommandLine, L"--ui-theme-toggle-test") != nullptr)
        {
            themeTestTimer_ = DispatcherTimer();
            themeTestTimer_.Interval(std::chrono::milliseconds(800));
            themeTestTimer_.Tick([this](const IInspectable& sender,
                const IInspectable&)
            {
                auto timer = sender.try_as<DispatcherTimer>();
                if (timer != nullptr)
                {
                    timer.Stop();
                }
                ToggleTheme(false);
            });
            themeTestTimer_.Start();
        }

        if (launchCommandLine != nullptr &&
            std::wcsstr(launchCommandLine,
                L"--ui-navigation-stress-test") != nullptr)
        {
            struct NavigationStressState
            {
                int page{};
                int cycles{};
                bool warmupShown{};
                bool baselineCaptured{};
                bool midpointCaptured{};
                SIZE_T baselineWorkingSet{};
                SIZE_T baselinePrivateBytes{};
                SIZE_T midpointWorkingSet{};
                SIZE_T midpointPrivateBytes{};
                ULONGLONG baselineCpu100ns{};
                ULONGLONG baselineWallTick{};
                ULONGLONG deadline{};
            };
            const auto stress = std::make_shared<NavigationStressState>();
            stress->deadline = ::GetTickCount64() + 30000ULL;
            StartSymbolAnalysis();
            navigationTestTimer_ = DispatcherTimer();
            navigationTestTimer_.Interval(std::chrono::milliseconds(80));
            navigationTestTimer_.Tick([this, stress](
                const IInspectable& sender, const IInspectable&)
            {
                const auto analysisState =
                    controller_.Symbols().AnalysisState();
                if (analysisState ==
                    unrealdbg_native::SymbolAnalysisState::Idle)
                {
                    StartSymbolAnalysis();
                }
                if (analysisState ==
                    unrealdbg_native::SymbolAnalysisState::Analyzing)
                {
                    if (::GetTickCount64() >= stress->deadline)
                    {
                        StartupTrace(
                            L"[ui] navigation stress analysis timeout");
                        ::ExitProcess(1);
                    }
                    return;
                }
                if (analysisState ==
                    unrealdbg_native::SymbolAnalysisState::Idle)
                {
                    if (::GetTickCount64() >= stress->deadline)
                    {
                        StartupTrace(
                            L"[ui] navigation stress analysis timeout");
                        ::ExitProcess(1);
                    }
                    return;
                }
                if (analysisState ==
                    unrealdbg_native::SymbolAnalysisState::Failed)
                {
                    StartupTrace(
                        L"[ui] navigation stress analysis failed");
                    ::ExitProcess(1);
                }
                if (!stress->warmupShown)
                {
                    SelectPage(PageId::Symbols);
                    stress->warmupShown = true;
                    return;
                }
                if (!symbolRenderingComplete_)
                {
                    if (::GetTickCount64() >= stress->deadline)
                    {
                        StartupTrace(
                            L"[ui] navigation stress render timeout");
                        ::ExitProcess(1);
                    }
                    return;
                }

                SelectPage(static_cast<PageId>(stress->page));
                stress->page = (stress->page + 1) % kPageCount;
                if (stress->page == 0)
                {
                    ++stress->cycles;
                }

                PROCESS_MEMORY_COUNTERS_EX memory{};
                memory.cb = sizeof(memory);
                if (!::GetProcessMemoryInfo(::GetCurrentProcess(),
                    reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
                    sizeof(memory)))
                {
                    return;
                }
                if (!stress->baselineCaptured && stress->cycles >= 2)
                {
                    stress->baselineCaptured = true;
                    stress->baselineWorkingSet = memory.WorkingSetSize;
                    stress->baselinePrivateBytes = memory.PrivateUsage;
                    FILETIME creation{}, exit{}, kernel{}, user{};
                    if (::GetProcessTimes(::GetCurrentProcess(),
                        &creation, &exit, &kernel, &user))
                    {
                        stress->baselineCpu100ns =
                            (static_cast<ULONGLONG>(kernel.dwHighDateTime) << 32) |
                            kernel.dwLowDateTime;
                        stress->baselineCpu100ns +=
                            (static_cast<ULONGLONG>(user.dwHighDateTime) << 32) |
                            user.dwLowDateTime;
                        stress->baselineWallTick = ::GetTickCount64();
                    }
                }
                if (stress->baselineCaptured &&
                    !stress->midpointCaptured && stress->cycles >= 12)
                {
                    stress->midpointCaptured = true;
                    stress->midpointWorkingSet = memory.WorkingSetSize;
                    stress->midpointPrivateBytes = memory.PrivateUsage;
                }
                if (stress->midpointCaptured && stress->cycles >= 22)
                {
                    auto timer = sender.try_as<DispatcherTimer>();
                    if (timer != nullptr)
                    {
                        timer.Stop();
                    }
                    const auto deltaWorkingSet =
                        static_cast<LONG64>(memory.WorkingSetSize) -
                        static_cast<LONG64>(stress->baselineWorkingSet);
                    const auto deltaPrivate =
                        static_cast<LONG64>(memory.PrivateUsage) -
                        static_cast<LONG64>(stress->baselinePrivateBytes);
                    const auto midpointDeltaPrivate =
                        static_cast<LONG64>(memory.PrivateUsage) -
                        static_cast<LONG64>(stress->midpointPrivateBytes);
                    const auto midpointDeltaWorkingSet =
                        static_cast<LONG64>(memory.WorkingSetSize) -
                        static_cast<LONG64>(stress->midpointWorkingSet);
                    FILETIME creation{}, exit{}, kernel{}, user{};
                    ULONGLONG cpu100ns = 0;
                    if (::GetProcessTimes(::GetCurrentProcess(),
                        &creation, &exit, &kernel, &user))
                    {
                        cpu100ns =
                            (static_cast<ULONGLONG>(kernel.dwHighDateTime) << 32) |
                            kernel.dwLowDateTime;
                        cpu100ns +=
                            (static_cast<ULONGLONG>(user.dwHighDateTime) << 32) |
                            user.dwLowDateTime;
                    }
                    const ULONGLONG wallDelta =
                        ::GetTickCount64() - stress->baselineWallTick;
                    const double cpuPercent = wallDelta == 0
                        ? 0.0
                        : (100.0 * static_cast<double>(
                            cpu100ns - stress->baselineCpu100ns) /
                            (static_cast<double>(wallDelta) * 10000.0));
                    StartupTrace(
                        L"[ui] navigation stress baseline=" +
                        std::to_wstring(stress->baselinePrivateBytes / 1024) +
                        L"KB final=" +
                        std::to_wstring(memory.PrivateUsage / 1024) +
                        L"KB totalDelta=" +
                        std::to_wstring(deltaPrivate / 1024) +
                        L"KB steadyDelta=" +
                        std::to_wstring(midpointDeltaPrivate / 1024) +
                        L"KB totalWorkingDelta=" +
                        std::to_wstring(deltaWorkingSet / 1024) +
                        L"KB steadyWorkingDelta=" +
                        std::to_wstring(midpointDeltaWorkingSet / 1024) +
                        L"KB cpu=" +
                        std::to_wstring(static_cast<int>(
                            cpuPercent * 10.0 + 0.5) / 10.0) +
                        L"% cycles=" + std::to_wstring(stress->cycles));
                    ::ExitProcess(0);
                }
            });
            navigationTestTimer_.Start();
        }
    }

private:
    using Clock = std::chrono::steady_clock;

    class SymbolRowElementFactory :
        public implements<SymbolRowElementFactory,
            winrt::Microsoft::UI::Xaml::IElementFactory>
    {
    public:
        explicit SymbolRowElementFactory(Shell* owner)
            : owner_(owner)
        {
        }

        UIElement GetElement(
            const ElementFactoryGetArgs& args)
        {
            if (owner_ == nullptr || args.Data() == nullptr)
            {
                return nullptr;
            }
            const std::uint64_t item = unbox_value<std::uint64_t>(
                args.Data());
            const auto generation =
                static_cast<std::uint32_t>(item >> 32);
            const auto index = static_cast<std::uint32_t>(item);
            if (generation != static_cast<std::uint32_t>(
                    owner_->symbolRenderGeneration_) ||
                static_cast<size_t>(index) >=
                    owner_->symbolRenderQueue_.size())
            {
                return nullptr;
            }
            const auto* entry =
                owner_->symbolRenderQueue_[static_cast<size_t>(index)];
            if (entry == nullptr)
            {
                return nullptr;
            }
            if (!recycledRows_.empty())
            {
                UIElement element = recycledRows_.back();
                recycledRows_.pop_back();
                Grid row = element.try_as<Grid>();
                if (row != nullptr)
                {
                    owner_->PopulateSymbolRow(row, *entry);
                    return row;
                }
            }
            return owner_->MakeSymbolRow(*entry);
        }

        void RecycleElement(const ElementFactoryRecycleArgs& args)
        {
            Grid row = args.Element() == nullptr
                ? Grid{ nullptr } : args.Element().try_as<Grid>();
            if (row == nullptr)
            {
                return;
            }
            row.Children().Clear();
            if (recycledRows_.size() < kMaximumRecycledRows)
            {
                recycledRows_.push_back(row);
            }
        }

        void Reset()
        {
            recycledRows_.clear();
        }

    private:
        static constexpr size_t kMaximumRecycledRows = 64;
        Shell* owner_{};
        std::vector<UIElement> recycledRows_;
    };

    Grid MakePageRoot(const double top = 20.0)
    {
        Grid page;
        page.Padding(Thickness{ 24, top, 24, 16 });
        page.RowSpacing(14);
        page.HorizontalAlignment(HorizontalAlignment::Stretch);
        page.VerticalAlignment(VerticalAlignment::Stretch);
        return page;
    }

    Grid MakePageTitle(const hstring& eyebrow, const hstring& title,
        StackPanel actions = nullptr)
    {
        Grid header;
        ColumnDefinition labelColumn;
        labelColumn.Width(GridLength{ 1, GridUnitType::Star });
        header.ColumnDefinitions().Append(labelColumn);
        ColumnDefinition actionColumn;
        actionColumn.Width(GridLength{ 1, GridUnitType::Auto });
        header.ColumnDefinitions().Append(actionColumn);

        StackPanel labels;
        labels.Spacing(3);
        TextBlock kicker = tok::Text(eyebrow, tok::kFontMicro,
            tok::kGold, tok::kWeightSemiBold);
        kicker.CharacterSpacing(80);
        labels.Children().Append(kicker);
        labels.Children().Append(tok::Text(title, tok::kFontTitle,
            tok::kTextPrimary, tok::kWeightSemiBold));
        Grid::SetColumn(labels, 0);
        header.Children().Append(labels);

        if (actions != nullptr)
        {
            actions.VerticalAlignment(VerticalAlignment::Bottom);
            Grid::SetColumn(actions, 1);
            header.Children().Append(actions);
        }
        return header;
    }

    void BuildLayout()
    {
        navSelectionInitialized_ = false;
        root_ = Grid();
        root_.RequestedTheme(darkTheme_ ? ElementTheme::Dark : ElementTheme::Light);
        root_.Background(tok::Brush(tok::kWindowTop));

        RowDefinition titleRow;
        titleRow.Height(GridLength{ tok::kTitleBarHeight });
        root_.RowDefinitions().Append(titleRow);
        RowDefinition bodyRow;
        bodyRow.Height(GridLength{ 1, GridUnitType::Star });
        root_.RowDefinitions().Append(bodyRow);

        titleBar_ = BuildTitleBar();
        Grid::SetRow(titleBar_, 0);
        root_.Children().Append(titleBar_);

        Grid body;
        ColumnDefinition navColumn;
        navColumn.Width(GridLength{ tok::kNavWidth });
        body.ColumnDefinitions().Append(navColumn);
        ColumnDefinition contentColumn;
        contentColumn.Width(GridLength{ 1, GridUnitType::Star });
        body.ColumnDefinitions().Append(contentColumn);

        Border sidebar = BuildSidebar();
        Grid::SetColumn(sidebar, 0);
        body.Children().Append(sidebar);

        overviewPage_ = BuildOverviewPage();
        debuggerPage_ = BuildDebuggerPage();
        targetPage_ = BuildTargetPage();
        hooksPage_ = BuildHookPage();
        logsPage_ = BuildLogsPage();
        systemPage_ = BuildSystemPage();
        symbolsPage_ = BuildSymbolsPage();
        diagnosticsPage_ = BuildDiagnosticsPage();
        pages_ = {
            overviewPage_, debuggerPage_, targetPage_, hooksPage_,
            logsPage_, systemPage_, symbolsPage_, diagnosticsPage_
        };

        pageHost_ = Grid();
        pageHost_.Background(tok::Brush(tok::kWindowTop));
        for (const Grid& page : pages_)
        {
            page.Visibility(Visibility::Collapsed);
            pageHost_.Children().Append(page);
        }
        Grid::SetColumn(pageHost_, 1);
        body.Children().Append(pageHost_);

        Grid::SetRow(body, 1);
        root_.Children().Append(body);

    }

    Grid BuildTitleBar()
    {
        Grid bar;
        bar.Background(tok::Brush(tok::kTitleBar));
        bar.Padding(Thickness{ 15, 0, 140, 0 });
        bar.BorderBrush(tok::Brush(tok::kBorder));
        bar.BorderThickness(Thickness{ 0, 0, 0, 1 });

        ColumnDefinition brandColumn;
        brandColumn.Width(GridLength{ 1, GridUnitType::Auto });
        bar.ColumnDefinitions().Append(brandColumn);
        ColumnDefinition statusColumn;
        statusColumn.Width(GridLength{ 1, GridUnitType::Star });
        bar.ColumnDefinitions().Append(statusColumn);
        ColumnDefinition toolsColumn;
        toolsColumn.Width(GridLength{ 1, GridUnitType::Auto });
        bar.ColumnDefinitions().Append(toolsColumn);

        StackPanel brand;
        brand.Orientation(Orientation::Horizontal);
        brand.Spacing(10);
        brand.VerticalAlignment(VerticalAlignment::Center);
        brand.Children().Append(MakeSeal(L"F3", 28));

        StackPanel names;
        names.Spacing(0);
        names.VerticalAlignment(VerticalAlignment::Center);
        StackPanel nameRow;
        nameRow.Orientation(Orientation::Horizontal);
        nameRow.Spacing(8);
        nameRow.Children().Append(tok::Text(kProductName, tok::kFontBody + 1,
            tok::kTextPrimary, tok::kWeightSemiBold));
        nameRow.Children().Append(tok::Text(kProductSubtitle, tok::kFontCaption,
            tok::kTextSecondary, tok::kWeightMedium));
        names.Children().Append(nameRow);
        names.Children().Append(tok::Text(L"UNREALDBG / RING -1",
            tok::kFontMicro - 1, tok::kTextMuted, tok::kWeightMedium));
        brand.Children().Append(names);
        Grid::SetColumn(brand, 0);
        bar.Children().Append(brand);

        titleStatusPill_ = Border();
        titleStatusPill_.Padding(Thickness{ 10, 4, 11, 4 });
        titleStatusPill_.CornerRadius(CornerRadius{
            tok::kRadiusSm, tok::kRadiusSm, tok::kRadiusSm, tok::kRadiusSm });
        titleStatusPill_.BorderThickness(Thickness{ 1, 1, 1, 1 });
        titleStatusPill_.HorizontalAlignment(HorizontalAlignment::Right);
        titleStatusPill_.VerticalAlignment(VerticalAlignment::Center);
        StackPanel titleStatusContent;
        titleStatusContent.Orientation(Orientation::Horizontal);
        titleStatusContent.Spacing(7);
        titleStatusDot_ = tok::Dot(tok::kGold, 7);
        titleStatusText_ = tok::Text(L"正在载入后端", tok::kFontCaption,
            tok::kTextPrimary, tok::kWeightMedium);
        titleStatusText_.VerticalAlignment(VerticalAlignment::Center);
        titleStatusContent.Children().Append(titleStatusDot_);
        titleStatusContent.Children().Append(titleStatusText_);
        titleStatusPill_.Child(titleStatusContent);
        Grid::SetColumn(titleStatusPill_, 1);
        bar.Children().Append(titleStatusPill_);

        StackPanel tools;
        tools.Orientation(Orientation::Horizontal);
        tools.Spacing(6);
        tools.VerticalAlignment(VerticalAlignment::Center);
        Button refresh = MakeIconButton(tok::kGlyphRefresh, L"刷新环境");
        refresh.Click({ this, &Shell::OnRefreshClicked });
        Button directory = MakeIconButton(tok::kGlyphFolder, L"打开运行目录");
        directory.Click([this](const IInspectable&, const RoutedEventArgs&)
        {
            OpenExplorer(controller_.ApplicationDirectory());
        });
        themeButton_ = MakeIconButton(
            darkTheme_ ? tok::kGlyphSun : tok::kGlyphMoon,
            darkTheme_ ? L"切换到明亮主题" : L"切换到暗黑主题");
        themeButton_.Click([this](const IInspectable&,
            const RoutedEventArgs&)
        {
            ToggleTheme();
        });
        themeButtonIcon_ = themeButton_.Content().try_as<FontIcon>();
        tools.Children().Append(themeButton_);
        tools.Children().Append(refresh);
        tools.Children().Append(directory);
        Grid::SetColumn(tools, 2);
        bar.Children().Append(tools);
        return bar;
    }

    Border BuildSidebar()
    {
        Border sidebar;
        sidebar.Background(tok::Brush(tok::kSidebar));
        sidebar.BorderBrush(tok::Brush(tok::kBorder));
        sidebar.BorderThickness(Thickness{ 0, 0, 1, 0 });

        Grid layout;
        RowDefinition headingRow;
        headingRow.Height(GridLength{ 1, GridUnitType::Auto });
        layout.RowDefinitions().Append(headingRow);
        RowDefinition navRow;
        navRow.Height(GridLength{ 1, GridUnitType::Star });
        layout.RowDefinitions().Append(navRow);
        RowDefinition footerRow;
        footerRow.Height(GridLength{ 1, GridUnitType::Auto });
        layout.RowDefinitions().Append(footerRow);

        StackPanel heading;
        heading.Padding(Thickness{ 18, 18, 18, 10 });
        heading.Spacing(3);
        heading.Children().Append(tok::Text(L"调试工作区", tok::kFontSubtitle,
            tok::kTextPrimary, tok::kWeightSemiBold));
        heading.Children().Append(tok::Text(L"LOCAL CONSOLE", tok::kFontMicro - 1,
            tok::kTextMuted, tok::kWeightMedium));
        Grid::SetRow(heading, 0);
        layout.Children().Append(heading);

        StackPanel nav;
        nav.Padding(Thickness{ 10, 4, 10, 8 });
        nav.Spacing(3);
        nav.Children().Append(BuildNavItem(PageId::Overview,
            tok::kGlyphOverview, L"总览"));
        nav.Children().Append(BuildNavItem(PageId::Debuggers,
            tok::kGlyphProcess, L"调试器"));
        nav.Children().Append(BuildNavItem(PageId::Target,
            tok::kGlyphShield, L"目标与防护"));
        nav.Children().Append(BuildNavItem(PageId::Hooks,
            tok::kGlyphShield, L"驱动 Hook"));
        nav.Children().Append(BuildNavItem(PageId::Logs,
            tok::kGlyphLog, L"日志中心"));
        nav.Children().Append(BuildNavItem(PageId::System,
            tok::kGlyphServer, L"系统详情"));
        nav.Children().Append(BuildNavItem(PageId::Symbols,
            tok::kGlyphSymbols, L"系统符号"));
        nav.Children().Append(BuildNavItem(PageId::Diagnostics,
            tok::kGlyphAbout, L"诊断与文件"));
        Grid::SetRow(nav, 1);
        layout.Children().Append(nav);

        Border footer;
        footer.Margin(Thickness{ 10, 0, 10, 12 });
        footer.Padding(Thickness{ 11, 10, 11, 10 });
        footer.Background(tok::Brush(tok::kCardInset));
        footer.BorderBrush(tok::Brush(tok::kBorder));
        footer.BorderThickness(Thickness{ 1, 1, 1, 1 });
        footer.CornerRadius(CornerRadius{
            tok::kRadiusMd, tok::kRadiusMd, tok::kRadiusMd, tok::kRadiusMd });
        StackPanel footerBody;
        footerBody.Spacing(7);
        footerBody.Children().Append(tok::Text(L"运行环境", tok::kFontMicro,
            tok::kTextMuted, tok::kWeightSemiBold));
        Grid backendSummary;
        backendSummary.ColumnDefinitions().Append(ColumnDefinition());
        backendSummary.ColumnDefinitions().GetAt(0).Width(GridLength{ 1, GridUnitType::Star });
        backendSummary.ColumnDefinitions().Append(ColumnDefinition());
        backendSummary.ColumnDefinitions().GetAt(1).Width(GridLength{ 1, GridUnitType::Auto });
        backendSummary.Children().Append(tok::Text(L"后端", tok::kFontCaption,
            tok::kTextSecondary));
        navBackendText_ = tok::Text(L"载入中", tok::kFontCaption,
            tok::kGold, tok::kWeightMedium);
        navBackendText_.HorizontalAlignment(HorizontalAlignment::Right);
        Grid::SetColumn(navBackendText_, 1);
        backendSummary.Children().Append(navBackendText_);
        footerBody.Children().Append(backendSummary);

        Grid symbolSummary;
        symbolSummary.ColumnDefinitions().Append(ColumnDefinition());
        symbolSummary.ColumnDefinitions().GetAt(0).Width(
            GridLength{ 1, GridUnitType::Star });
        symbolSummary.ColumnDefinitions().Append(ColumnDefinition());
        symbolSummary.ColumnDefinitions().GetAt(1).Width(
            GridLength{ 1, GridUnitType::Auto });
        symbolSummary.Children().Append(tok::Text(L"符号", tok::kFontCaption,
            tok::kTextSecondary));
        navSymbolText_ = tok::Text(L"准备中", tok::kFontCaption,
            tok::kGold, tok::kWeightMedium);
        navSymbolText_.HorizontalAlignment(HorizontalAlignment::Right);
        navSymbolText_.MaxWidth(112);
        navSymbolText_.TextTrimming(TextTrimming::CharacterEllipsis);
        Grid::SetColumn(navSymbolText_, 1);
        symbolSummary.Children().Append(navSymbolText_);
        footerBody.Children().Append(symbolSummary);

        Grid targetSummary;
        targetSummary.ColumnDefinitions().Append(ColumnDefinition());
        targetSummary.ColumnDefinitions().GetAt(0).Width(GridLength{ 1, GridUnitType::Star });
        targetSummary.ColumnDefinitions().Append(ColumnDefinition());
        targetSummary.ColumnDefinitions().GetAt(1).Width(GridLength{ 1, GridUnitType::Auto });
        targetSummary.Children().Append(tok::Text(L"目标", tok::kFontCaption,
            tok::kTextSecondary));
        navTargetText_ = tok::Text(L"未选择", tok::kFontCaption,
            tok::kTextMuted, tok::kWeightMedium);
        navTargetText_.HorizontalAlignment(HorizontalAlignment::Right);
        navTargetText_.MaxWidth(96);
        navTargetText_.TextTrimming(TextTrimming::CharacterEllipsis);
        Grid::SetColumn(navTargetText_, 1);
        targetSummary.Children().Append(navTargetText_);
        footerBody.Children().Append(targetSummary);
        footer.Child(footerBody);
        Grid::SetRow(footer, 2);
        layout.Children().Append(footer);

        sidebar.Child(layout);
        return sidebar;
    }

    Button BuildNavItem(const PageId page, const hstring& glyph,
        const hstring& label)
    {
        Button item;
        item.Tag(box_value(static_cast<int32_t>(page)));
        item.Height(tok::kNavItemHeight);
        item.Padding(Thickness{ 9, 0, 10, 0 });
        item.HorizontalContentAlignment(HorizontalAlignment::Stretch);
        item.CornerRadius(CornerRadius{
            tok::kRadiusMd, tok::kRadiusMd, tok::kRadiusMd, tok::kRadiusMd });
        item.BorderThickness(Thickness{ 1, 1, 1, 1 });
        item.BorderBrush(tok::Brush(tok::Rgb(0, 0, 0, 0)));
        item.Background(tok::Brush(tok::Rgb(0, 0, 0, 0)));

        Grid content;
        ColumnDefinition markerColumn;
        markerColumn.Width(GridLength{ 3 });
        content.ColumnDefinitions().Append(markerColumn);
        ColumnDefinition iconColumn;
        iconColumn.Width(GridLength{ 31 });
        content.ColumnDefinitions().Append(iconColumn);
        ColumnDefinition textColumn;
        textColumn.Width(GridLength{ 1, GridUnitType::Star });
        content.ColumnDefinitions().Append(textColumn);
        ColumnDefinition countColumn;
        countColumn.Width(GridLength{ 1, GridUnitType::Auto });
        content.ColumnDefinitions().Append(countColumn);

        Border marker;
        marker.Width(3);
        marker.Height(17);
        marker.CornerRadius(CornerRadius{ 1.5, 1.5, 1.5, 1.5 });
        marker.Background(tok::Brush(tok::kAccent));
        marker.Visibility(Visibility::Collapsed);
        marker.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetColumn(marker, 0);
        content.Children().Append(marker);
        navMarkers_[static_cast<size_t>(page)] = marker;

        FontIcon icon = tok::Icon(glyph, 15, tok::kTextMuted);
        icon.HorizontalAlignment(HorizontalAlignment::Center);
        Grid::SetColumn(icon, 1);
        content.Children().Append(icon);

        TextBlock caption = tok::Text(label, tok::kFontBody,
            tok::kTextSecondary, tok::kWeightMedium);
        caption.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetColumn(caption, 2);
        content.Children().Append(caption);
        navTexts_[static_cast<size_t>(page)] = caption;
        navIcons_[static_cast<size_t>(page)] = icon;

        if (page == PageId::Debuggers)
        {
            navDebuggerBadge_ = tok::Pill(L"0", tok::kTextMuted,
                tok::kSunken, tok::kFontMicro - 1);
            Grid::SetColumn(navDebuggerBadge_, 3);
            content.Children().Append(navDebuggerBadge_);
        }

        item.Content(content);
        winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
            item, label);
        item.Click({ this, &Shell::OnNavClicked });
        tok::SetToolTip(item, label);
        navButtons_[static_cast<size_t>(page)] = item;
        return item;
    }

    Grid BuildOverviewPage()
    {
        Grid page = MakePageRoot();
        RowDefinition headerRow;
        headerRow.Height(GridLength{ 1, GridUnitType::Auto });
        page.RowDefinitions().Append(headerRow);
        RowDefinition statusRow;
        statusRow.Height(GridLength{ 1, GridUnitType::Auto });
        page.RowDefinitions().Append(statusRow);
        RowDefinition statsRow;
        statsRow.Height(GridLength{ 1, GridUnitType::Auto });
        page.RowDefinitions().Append(statsRow);
        RowDefinition contentRow;
        contentRow.Height(GridLength{ 1, GridUnitType::Star });
        page.RowDefinitions().Append(contentRow);

        StackPanel headerActions;
        headerActions.Orientation(Orientation::Horizontal);
        headerActions.Spacing(7);
        Button githubButton = ui::MakeButton(L"GitHub 开源仓库",
            tok::kGlyphLink, ui::ButtonVariant::Secondary,
            tok::kFontCaption);
        githubButton.Click([](const IInspectable&,
            const RoutedEventArgs&)
        {
            OpenExternalLink(
                L"https://github.com/GG-Boy223/UnrealVTDbg-IntelVT");
        });
        tok::SetToolTip(githubButton,
            L"https://github.com/GG-Boy223/UnrealVTDbg-IntelVT");
        headerActions.Children().Append(githubButton);

        Grid header = MakePageTitle(
            L"COMMAND CENTER", L"运行总览", headerActions);
        Grid::SetRow(header, 0);
        page.Children().Append(header);

        Border statusBand;
        statusBand.Padding(Thickness{ 0, 14, 0, 15 });
        statusBand.BorderBrush(tok::Brush(tok::kBorder));
        statusBand.BorderThickness(Thickness{ 0, 0, 0, 1 });
        Grid statusLayout;
        statusLayout.ColumnSpacing(16);
        ColumnDefinition iconColumn;
        iconColumn.Width(GridLength{ 1, GridUnitType::Auto });
        statusLayout.ColumnDefinitions().Append(iconColumn);
        ColumnDefinition textColumn;
        textColumn.Width(GridLength{ 1, GridUnitType::Star });
        statusLayout.ColumnDefinitions().Append(textColumn);
        ColumnDefinition actionColumn;
        actionColumn.Width(GridLength{ 1, GridUnitType::Auto });
        statusLayout.ColumnDefinitions().Append(actionColumn);

        statusSeal_ = MakeSeal(L"启", 50);
        statusSeal_.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetColumn(statusSeal_, 0);
        statusLayout.Children().Append(statusSeal_);

        StackPanel statusText;
        statusText.Spacing(3);
        statusText.VerticalAlignment(VerticalAlignment::Center);
        statusEyebrow_ = tok::Text(L"SYSTEM STATE / 本机调试通道",
            tok::kFontMicro, tok::kGold, tok::kWeightSemiBold);
        statusEyebrow_.CharacterSpacing(55);
        statusTitle_ = tok::Text(L"正在载入后端", tok::kFontHero,
            tok::kTextPrimary, tok::kWeightSemiBold);
        statusDetail_ = tok::Text(L"等待后端接口检查",
            tok::kFontBody, tok::kTextMuted);
        statusDetail_.TextTrimming(TextTrimming::CharacterEllipsis);
        statusText.Children().Append(statusEyebrow_);
        statusText.Children().Append(statusTitle_);
        statusText.Children().Append(statusDetail_);
        Grid::SetColumn(statusText, 1);
        statusLayout.Children().Append(statusText);

        StackPanel statusActions;
        statusActions.Orientation(Orientation::Horizontal);
        statusActions.Spacing(10);
        statusActions.VerticalAlignment(VerticalAlignment::Center);
        initProgress_ = ProgressRing();
        initProgress_.Width(18);
        initProgress_.Height(18);
        initProgress_.IsActive(false);
        initProgress_.Visibility(Visibility::Collapsed);
        statusActions.Children().Append(initProgress_);
        initButton_ = ui::MakeButton(L"进入 VT 调试模式",
            tok::kGlyphPlay, ui::ButtonVariant::Primary,
            tok::kFontBody);
        initButton_.MinWidth(154);
        initButton_.MinHeight(38);
        initButton_.Click({ this, &Shell::OnInitializeClicked });
        statusActions.Children().Append(initButton_);
        Grid::SetColumn(statusActions, 2);
        statusLayout.Children().Append(statusActions);
        statusBand.Child(statusLayout);
        Grid::SetRow(statusBand, 1);
        page.Children().Append(statusBand);

        Grid stats;
        stats.ColumnSpacing(10);
        for (int index = 0; index < 5; ++index)
        {
            ColumnDefinition column;
            column.Width(GridLength{ 1, GridUnitType::Star });
            stats.ColumnDefinitions().Append(column);
        }

        stats.Children().Append(MakeOverviewTile(0,
            tok::kGlyphServer, tok::kAccent, tok::kAccentSoft,
            L"后端接口", L"载入中", L"检查 UnrealDbgDll", &backendStatText_));
        stats.Children().Append(MakeOverviewTile(1,
            tok::kGlyphSymbols, tok::kGold, tok::kGoldSoft,
            L"符号缓存", L"准备中", L"Loader 符号已并入",
            &symbolStatText_, &symbolHintText_));
        stats.Children().Append(MakeOverviewTile(2,
            tok::kGlyphProcess, tok::kInfo, tok::kInfoSoft,
            L"外部调试器", L"0", L"已登记可执行文件", &debuggerStatText_));
        stats.Children().Append(MakeOverviewTile(3,
            tok::kGlyphPin, tok::kCinnabar, tok::kCinnabarSoft,
            L"目标进程", L"未选择", L"用于防检测联动", &targetStatText_));
        stats.Children().Append(MakeOverviewTile(4,
            tok::kGlyphLog, tok::kDebug, tok::kDebugSoft,
            L"运行日志", L"0", L"当前会话语义计数", &logStatText_));
        Grid::SetRow(stats, 2);
        page.Children().Append(stats);

        Grid lower;
        lower.ColumnSpacing(12);
        ColumnDefinition listColumn;
        listColumn.Width(GridLength{ 3, GridUnitType::Star });
        lower.ColumnDefinitions().Append(listColumn);
        ColumnDefinition sideColumn;
        sideColumn.Width(GridLength{ 2, GridUnitType::Star });
        lower.ColumnDefinitions().Append(sideColumn);

        Border listPanel = MakeSurface();
        Grid listLayout;
        listLayout.RowSpacing(10);
        RowDefinition listHeaderRow;
        listHeaderRow.Height(GridLength{ 1, GridUnitType::Auto });
        listLayout.RowDefinitions().Append(listHeaderRow);
        RowDefinition listBodyRow;
        listBodyRow.Height(GridLength{ 1, GridUnitType::Star });
        listLayout.RowDefinitions().Append(listBodyRow);
        RowDefinition listFooterRow;
        listFooterRow.Height(GridLength{ 1, GridUnitType::Auto });
        listLayout.RowDefinitions().Append(listFooterRow);

        Grid listHeader;
        listHeader.ColumnDefinitions().Append(ColumnDefinition());
        listHeader.ColumnDefinitions().GetAt(0).Width(GridLength{ 1, GridUnitType::Star });
        listHeader.ColumnDefinitions().Append(ColumnDefinition());
        listHeader.ColumnDefinitions().GetAt(1).Width(GridLength{ 1, GridUnitType::Auto });
        listHeader.Children().Append(MakeSectionHeading(L"调试器快捷启动",
            L"沿用现有 DebuggerList.ini"));
        Button addQuick = MakeIconButton(tok::kGlyphAdd, L"添加调试器",
            ui::ButtonVariant::Secondary);
        addQuick.Click({ this, &Shell::OnAddDebuggerClicked });
        Grid::SetColumn(addQuick, 1);
        listHeader.Children().Append(addQuick);
        Grid::SetRow(listHeader, 0);
        listLayout.Children().Append(listHeader);

        overviewDebuggerList_ = ListView();
        ConfigureListView(overviewDebuggerList_);
        overviewDebuggerList_.SelectionChanged(
            { this, &Shell::OnOverviewDebuggerSelectionChanged });
        overviewDebuggerList_.DoubleTapped(
            { this, &Shell::OnDebuggerDoubleTapped });
        Grid::SetRow(overviewDebuggerList_, 1);
        listLayout.Children().Append(overviewDebuggerList_);

        Grid listFooter;
        listFooter.ColumnDefinitions().Append(ColumnDefinition());
        listFooter.ColumnDefinitions().GetAt(0).Width(GridLength{ 1, GridUnitType::Star });
        listFooter.ColumnDefinitions().Append(ColumnDefinition());
        listFooter.ColumnDefinitions().GetAt(1).Width(GridLength{ 1, GridUnitType::Auto });
        overviewSelectionText_ = tok::Text(L"选择一个调试器", tok::kFontCaption,
            tok::kTextMuted);
        overviewSelectionText_.TextTrimming(TextTrimming::CharacterEllipsis);
        listFooter.Children().Append(overviewSelectionText_);
        overviewStartButton_ = ui::MakeButton(L"启动所选", tok::kGlyphPlay,
            ui::ButtonVariant::Secondary, tok::kFontCaption);
        overviewStartButton_.Click(
            [this](const IInspectable&, const RoutedEventArgs&)
            {
                StartSelectedDebugger(overviewSelectedIndex_);
            });
        Grid::SetColumn(overviewStartButton_, 1);
        listFooter.Children().Append(overviewStartButton_);
        Grid::SetRow(listFooter, 2);
        listLayout.Children().Append(listFooter);
        listPanel.Child(listLayout);
        Grid::SetColumn(listPanel, 0);
        lower.Children().Append(listPanel);

        Border environment = MakeSurface();
        StackPanel environmentBody;
        environmentBody.Spacing(4);
        Grid environmentHeader;
        environmentHeader.ColumnDefinitions().Append(ColumnDefinition());
        environmentHeader.ColumnDefinitions().GetAt(0).Width(
            GridLength{ 1, GridUnitType::Star });
        environmentHeader.ColumnDefinitions().Append(ColumnDefinition());
        environmentHeader.ColumnDefinitions().GetAt(1).Width(
            GridLength{ 1, GridUnitType::Auto });
        environmentHeader.Children().Append(MakeSectionHeading(L"本机环境"));
        Button overviewTlButton = ui::MakeButton(L"防检测：未启用",
            tok::kGlyphShield, ui::ButtonVariant::Ghost, tok::kFontMicro + 0.5);
        overviewTlButton_ = overviewTlButton;
        overviewTlButton.Click([this](const IInspectable&,
            const RoutedEventArgs&)
        {
            SelectPage(PageId::Target);
        });
        tok::SetToolTip(overviewTlButton, L"打开目标与防护");
        Grid::SetColumn(overviewTlButton, 1);
        environmentHeader.Children().Append(overviewTlButton);
        environmentBody.Children().Append(environmentHeader);
        environmentBody.Children().Append(MakeDivider(tok::kBorder));
        environmentBody.Children().Append(ui::MakePropertyRow(
            L"系统", L"读取中", false, &systemOsText_));
        environmentBody.Children().Append(ui::MakePropertyRow(
            L"内核", L"读取中", true, &systemKernelText_));
        environmentBody.Children().Append(ui::MakePropertyRow(
            L"处理器", L"读取中", false, &systemCpuText_));
        environmentBody.Children().Append(ui::MakePropertyRow(
            L"内存", L"读取中", false, &systemMemoryText_));
        environment.Child(environmentBody);
        Grid::SetColumn(environment, 1);
        lower.Children().Append(environment);

        const StackPanel buttonContent =
            overviewTlButton.Content().try_as<StackPanel>();
        if (buttonContent != nullptr)
        {
            for (const auto& child : buttonContent.Children())
            {
                TextBlock label = child.try_as<TextBlock>();
                if (label != nullptr)
                {
                    overviewTlText_ = label;
                    break;
                }
            }
        }

        Grid::SetRow(lower, 3);
        page.Children().Append(lower);
        return page;
    }

    FrameworkElement MakeOverviewTile(const int column,
        const hstring& glyph, const Color accent, const Color accentSoft,
        const hstring& label, const hstring& value, const hstring& hint,
        TextBlock* valueOut, TextBlock* hintOut = nullptr)
    {
        UIElement tile = ui::MakeStatTile(glyph, accent, accentSoft,
            label, value, hint, valueOut, hintOut);
        FrameworkElement element = tile.as<FrameworkElement>();
        Grid::SetColumn(element, column);
        return element;
    }

    Grid BuildDebuggerPage()
    {
        Grid page = MakePageRoot();
        RowDefinition headerRow;
        headerRow.Height(GridLength{ 1, GridUnitType::Auto });
        page.RowDefinitions().Append(headerRow);
        RowDefinition toolbarRow;
        toolbarRow.Height(GridLength{ 1, GridUnitType::Auto });
        page.RowDefinitions().Append(toolbarRow);
        RowDefinition listRow;
        listRow.Height(GridLength{ 1, GridUnitType::Star });
        page.RowDefinitions().Append(listRow);
        RowDefinition footerRow;
        footerRow.Height(GridLength{ 1, GridUnitType::Auto });
        page.RowDefinitions().Append(footerRow);

        StackPanel headerActions;
        headerActions.Orientation(Orientation::Horizontal);
        headerActions.Spacing(7);
        Button add = ui::MakeButton(L"添加", tok::kGlyphAdd,
            ui::ButtonVariant::Primary, tok::kFontCaption);
        add.Click({ this, &Shell::OnAddDebuggerClicked });
        Button remove = ui::MakeButton(L"移除", tok::kGlyphRemove,
            ui::ButtonVariant::Danger, tok::kFontCaption);
        remove.Click({ this, &Shell::OnDeleteDebuggerClicked });
        deleteDebuggerButton_ = remove;
        Button start = ui::MakeButton(L"启动", tok::kGlyphPlay,
            ui::ButtonVariant::Secondary, tok::kFontCaption);
        start.Click([this](const IInspectable&, const RoutedEventArgs&)
        {
            StartSelectedDebugger(debuggerSelectedIndex_);
        });
        startDebuggerButton_ = start;
        headerActions.Children().Append(add);
        headerActions.Children().Append(remove);
        headerActions.Children().Append(start);
        Grid header = MakePageTitle(L"DEBUGGER REGISTRY",
            L"外部调试器", headerActions);
        Grid::SetRow(header, 0);
        page.Children().Append(header);

        Grid toolbar;
        toolbar.Padding(Thickness{ 12, 9, 12, 9 });
        toolbar.Background(tok::Brush(tok::kCardInset));
        toolbar.BorderBrush(tok::Brush(tok::kBorder));
        toolbar.BorderThickness(Thickness{ 1, 1, 1, 1 });
        toolbar.CornerRadius(CornerRadius{
            tok::kRadiusMd, tok::kRadiusMd, tok::kRadiusMd, tok::kRadiusMd });
        toolbar.ColumnDefinitions().Append(ColumnDefinition());
        toolbar.ColumnDefinitions().GetAt(0).Width(GridLength{ 1, GridUnitType::Star });
        toolbar.ColumnDefinitions().Append(ColumnDefinition());
        toolbar.ColumnDefinitions().GetAt(1).Width(GridLength{ 1, GridUnitType::Auto });
        StackPanel toolbarText;
        toolbarText.Spacing(2);
        toolbarText.Children().Append(tok::Text(L"列表与旧版 Delphi 前端共用",
            tok::kFontCaption, tok::kTextSecondary, tok::kWeightMedium));
        toolbarText.Children().Append(tok::Text(L"DebuggerList.ini / 配置",
            tok::kFontMicro, tok::kTextMuted));
        toolbar.Children().Append(toolbarText);
        debuggerCountText_ = tok::Text(L"0 项", tok::kFontSubtitle,
            tok::kGold, tok::kWeightSemiBold);
        debuggerCountText_.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetColumn(debuggerCountText_, 1);
        toolbar.Children().Append(debuggerCountText_);
        Grid::SetRow(toolbar, 1);
        page.Children().Append(toolbar);

        Border listSurface = MakeSurface(tok::kRadiusLg, 6);
        listSurface.Background(tok::Brush(tok::kWindowBottom));
        debuggerList_ = ListView();
        ConfigureListView(debuggerList_);
        debuggerList_.SelectionChanged(
            { this, &Shell::OnDebuggerSelectionChanged });
        debuggerList_.DoubleTapped(
            { this, &Shell::OnDebuggerDoubleTapped });
        listSurface.Child(debuggerList_);
        Grid::SetRow(listSurface, 2);
        page.Children().Append(listSurface);

        Grid footer;
        footer.ColumnDefinitions().Append(ColumnDefinition());
        footer.ColumnDefinitions().GetAt(0).Width(GridLength{ 1, GridUnitType::Star });
        footer.ColumnDefinitions().Append(ColumnDefinition());
        footer.ColumnDefinitions().GetAt(1).Width(GridLength{ 1, GridUnitType::Auto });
        selectedDebuggerPathText_ = tok::Mono(L"未选择调试器", tok::kFontMicro + 0.5,
            tok::kTextMuted);
        selectedDebuggerPathText_.TextTrimming(TextTrimming::CharacterEllipsis);
        footer.Children().Append(selectedDebuggerPathText_);
        Button openSelected = ui::MakeButton(L"打开位置", tok::kGlyphFolder,
            ui::ButtonVariant::Ghost, tok::kFontCaption);
        openSelected.Click([this](const IInspectable&, const RoutedEventArgs&)
        {
            if (debuggerSelectedIndex_ < controller_.Debuggers().size())
            {
                OpenExplorer(controller_.Debuggers()[debuggerSelectedIndex_].path,
                    true);
            }
        });
        Grid::SetColumn(openSelected, 1);
        footer.Children().Append(openSelected);
        Grid::SetRow(footer, 3);
        page.Children().Append(footer);
        return page;
    }

    Grid BuildTargetPage()
    {
        Grid page = MakePageRoot();
        RowDefinition headerRow;
        headerRow.Height(GridLength{ 1, GridUnitType::Auto });
        page.RowDefinitions().Append(headerRow);
        RowDefinition targetRow;
        targetRow.Height(GridLength{ 1, GridUnitType::Auto });
        page.RowDefinitions().Append(targetRow);
        RowDefinition contentRow;
        contentRow.Height(GridLength{ 1, GridUnitType::Star });
        page.RowDefinitions().Append(contentRow);

        Grid header = MakePageTitle(L"TARGET & COUNTERMEASURES",
            L"目标与防护");
        Grid::SetRow(header, 0);
        page.Children().Append(header);

        Border targetBand = MakeSurface(tok::kRadiusLg, 16);
        targetBand.Background(tok::Brush(tok::kCardInset));
        Grid targetLayout;
        targetLayout.ColumnSpacing(14);
        ColumnDefinition targetIconColumn;
        targetIconColumn.Width(GridLength{ 1, GridUnitType::Auto });
        targetLayout.ColumnDefinitions().Append(targetIconColumn);
        ColumnDefinition targetTextColumn;
        targetTextColumn.Width(GridLength{ 1, GridUnitType::Star });
        targetLayout.ColumnDefinitions().Append(targetTextColumn);
        ColumnDefinition targetActionColumn;
        targetActionColumn.Width(GridLength{ 1, GridUnitType::Auto });
        targetLayout.ColumnDefinitions().Append(targetActionColumn);

        Border targetSeal = MakeSeal(L"靶", 48);
        targetSeal.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetColumn(targetSeal, 0);
        targetLayout.Children().Append(targetSeal);

        StackPanel targetText;
        targetText.Spacing(3);
        targetText.VerticalAlignment(VerticalAlignment::Center);
        targetNameText_ = tok::Text(L"尚未选择目标进程", tok::kFontSubtitle + 2,
            tok::kTextPrimary, tok::kWeightSemiBold);
        targetPidText_ = tok::Text(L"选择前台游戏进程后，防检测开关才会参与运行",
            tok::kFontCaption, tok::kTextMuted);
        targetPathText_ = tok::Mono(L"—", tok::kFontMicro, tok::kTextSecondary);
        targetPathText_.TextTrimming(TextTrimming::CharacterEllipsis);
        targetText.Children().Append(targetNameText_);
        targetText.Children().Append(targetPidText_);
        targetText.Children().Append(targetPathText_);
        Grid::SetColumn(targetText, 1);
        targetLayout.Children().Append(targetText);

        StackPanel targetActions;
        targetActions.Orientation(Orientation::Horizontal);
        targetActions.Spacing(8);
        targetActions.VerticalAlignment(VerticalAlignment::Center);
        captureTargetButton_ = ui::MakeButton(L"捕获前台进程",
            tok::kGlyphPin, ui::ButtonVariant::Primary, tok::kFontCaption);
        captureTargetButton_.Click(
            [this](const IInspectable&, const RoutedEventArgs&)
            {
                BeginTargetCapture();
            });
        Button clear = MakeIconButton(tok::kGlyphRemove, L"清除目标",
            ui::ButtonVariant::Secondary);
        clear.Click([this](const IInspectable&, const RoutedEventArgs&)
        {
            controller_.ClearTarget();
            RefreshTarget();
        });
        targetActions.Children().Append(captureTargetButton_);
        targetActions.Children().Append(clear);
        Grid::SetColumn(targetActions, 2);
        targetLayout.Children().Append(targetActions);
        targetBand.Child(targetLayout);
        Grid::SetRow(targetBand, 1);
        page.Children().Append(targetBand);

        Grid content;
        content.ColumnSpacing(12);
        ColumnDefinition tlColumn;
        tlColumn.Width(GridLength{ 3, GridUnitType::Star });
        content.ColumnDefinitions().Append(tlColumn);
        ColumnDefinition actionColumn;
        actionColumn.Width(GridLength{ 2, GridUnitType::Star });
        content.ColumnDefinitions().Append(actionColumn);

        Border tlSurface = MakeSurface(tok::kRadiusLg, 14);
        StackPanel tlBody;
        tlBody.Spacing(8);
        tlBody.Children().Append(MakeSectionHeading(L"防检测策略",
            L"修改后立即写入 Config.ini"));
        tlBody.Children().Append(MakeDivider(tok::kBorder));

        ToggleSwitch tlEnable{ nullptr };
        tlBody.Children().Append(ui::MakeToggleRow(tok::kGlyphShield,
            L"启用防检测联动", L"仅对本机目标会话生效",
            tok::kAccent, &tlEnable));
        tlEnableToggle_ = tlEnable;
        tlEnable.Toggled({ this, &Shell::OnTlToggleChanged });

        ToggleSwitch tlTick{ nullptr };
        tlBody.Children().Append(ui::MakeToggleRow(tok::kGlyphTimer,
            L"GetTickCount 偏差处理", L"沿用现有 TL 检测流程",
            tok::kGold, &tlTick));
        tlTickToggle_ = tlTick;
        tlTick.Toggled({ this, &Shell::OnTlToggleChanged });

        ToggleSwitch tlBlock{ nullptr };
        tlBody.Children().Append(ui::MakeToggleRow(tok::kGlyphPause,
            L"阻塞线程恢复", L"向驱动提交 ResumeThread 阻塞请求",
            tok::kCinnabar, &tlBlock));
        tlBlockToggle_ = tlBlock;
        tlBlock.Toggled({ this, &Shell::OnTlToggleChanged });
        tlSurface.Child(tlBody);
        Grid::SetColumn(tlSurface, 0);
        content.Children().Append(tlSurface);

        StackPanel side;
        side.Spacing(12);
        Border session = MakeSurface(tok::kRadiusLg, 14);
        StackPanel sessionBody;
        sessionBody.Spacing(8);
        sessionBody.Children().Append(MakeSectionHeading(L"会话动作"));
        sessionBody.Children().Append(MakeDivider(tok::kBorder));
        sessionBody.Children().Append(ui::MakePropertyRow(
            L"VT 状态", L"等待初始化", false, &targetTlStateText_));
        sessionBody.Children().Append(ui::MakePropertyRow(
            L"防检测", L"未启用", false, &targetGuardStateText_));
        sessionBody.Children().Append(MakeDivider(tok::kBorder));
        Button blockNow = ui::MakeButton(L"立即阻塞目标线程恢复",
            tok::kGlyphPause, ui::ButtonVariant::Danger, tok::kFontCaption);
        blockNow.Click([this](const IInspectable&, const RoutedEventArgs&)
        {
            controller_.BlockTargetResumeThread();
            RefreshAll();
        });
        blockResumeButton_ = blockNow;
        sessionBody.Children().Append(blockNow);
        session.Child(sessionBody);
        side.Children().Append(session);

        Border note = MakeSurface(tok::kRadiusLg, 14);
        StackPanel noteBody;
        noteBody.Spacing(5);
        noteBody.Children().Append(tok::Text(L"选择说明", tok::kFontCaption,
            tok::kGold, tok::kWeightSemiBold));
        noteBody.Children().Append(tok::TextWrapped(
            L"点击捕获后 3 秒内切换到目标窗口，调试台会读取当前前台 PID 与映像路径。",
            tok::kFontCaption, tok::kTextMuted));
        note.Child(noteBody);
        side.Children().Append(note);
        Grid::SetColumn(side, 1);
        content.Children().Append(side);

        Grid::SetRow(content, 2);
        page.Children().Append(content);
        return page;
    }

    Border BuildSymbolPanel()
    {
        Border surface = MakeSurface(tok::kRadiusLg, 14);
        StackPanel body;
        body.Spacing(8);

        Grid header;
        header.ColumnDefinitions().Append(ColumnDefinition());
        header.ColumnDefinitions().GetAt(0).Width(
            GridLength{ 1, GridUnitType::Star });
        header.ColumnDefinitions().Append(ColumnDefinition());
        header.ColumnDefinitions().GetAt(1).Width(
            GridLength{ 1, GridUnitType::Auto });
        header.Children().Append(MakeSectionHeading(L"符号缓存",
            L"Loader 符号准备已并入当前窗口"));

        symbolPrepareButton_ = ui::MakeButton(L"重新检查",
            tok::kGlyphDownload, ui::ButtonVariant::Secondary,
            tok::kFontCaption);
        symbolPrepareButton_.Click(
            { this, &Shell::OnPrepareSymbolsClicked });
        tok::SetToolTip(symbolPrepareButton_, L"校验并准备系统 PDB");
        Grid::SetColumn(symbolPrepareButton_, 1);
        header.Children().Append(symbolPrepareButton_);
        body.Children().Append(header);

        Grid stateRow;
        stateRow.ColumnDefinitions().Append(ColumnDefinition());
        stateRow.ColumnDefinitions().GetAt(0).Width(
            GridLength{ 1, GridUnitType::Star });
        stateRow.ColumnDefinitions().Append(ColumnDefinition());
        stateRow.ColumnDefinitions().GetAt(1).Width(
            GridLength{ 1, GridUnitType::Auto });
        symbolStatusText_ = tok::Text(L"等待校验", tok::kFontCaption,
            tok::kGold, tok::kWeightMedium);
        symbolStatusText_.TextTrimming(TextTrimming::CharacterEllipsis);
        stateRow.Children().Append(symbolStatusText_);
        symbolProgressText_ = tok::Text(L"0%", tok::kFontCaption,
            tok::kTextMuted);
        Grid::SetColumn(symbolProgressText_, 1);
        stateRow.Children().Append(symbolProgressText_);
        body.Children().Append(stateRow);

        symbolProgressBar_ = ProgressBar();
        symbolProgressBar_.Minimum(0);
        symbolProgressBar_.Maximum(100);
        symbolProgressBar_.Value(0);
        symbolProgressBar_.Height(4);
        symbolProgressBar_.Foreground(tok::Brush(tok::kGold));
        symbolProgressBar_.Background(tok::Brush(tok::kSunken));
        body.Children().Append(symbolProgressBar_);

        symbolModulesList_ = StackPanel();
        symbolModulesList_.Spacing(0);
        body.Children().Append(symbolModulesList_);

        symbolErrorText_ = tok::TextWrapped(L"", tok::kFontMicro + 0.5,
            tok::kDanger);
        symbolErrorText_.Visibility(Visibility::Collapsed);
        symbolErrorText_.IsTextSelectionEnabled(true);
        body.Children().Append(symbolErrorText_);

        surface.Child(body);
        return surface;
    }

    Grid BuildDiagnosticsPage()
    {
        Grid page = MakePageRoot();
        RowDefinition headerRow;
        headerRow.Height(GridLength{ 1, GridUnitType::Auto });
        page.RowDefinitions().Append(headerRow);
        RowDefinition bodyRow;
        bodyRow.Height(GridLength{ 1, GridUnitType::Star });
        page.RowDefinitions().Append(bodyRow);

        StackPanel actions;
        actions.Orientation(Orientation::Horizontal);
        actions.Spacing(7);
        Button refresh = ui::MakeButton(L"重新检查", tok::kGlyphRefresh,
            ui::ButtonVariant::Secondary, tok::kFontCaption);
        refresh.Click({ this, &Shell::OnRefreshClicked });
        Button log = ui::MakeButton(L"打开日志", tok::kGlyphLog,
            ui::ButtonVariant::Ghost, tok::kFontCaption);
        log.Click([this](const IInspectable&, const RoutedEventArgs&)
        {
            OpenExplorer(unrealdbg_native::JoinPath(
                controller_.ApplicationDirectory(), L"Log"));
        });
        actions.Children().Append(refresh);
        actions.Children().Append(log);
        Grid header = MakePageTitle(L"DIAGNOSTICS", L"诊断与文件", actions);
        Grid::SetRow(header, 0);
        page.Children().Append(header);

        Grid body;
        body.ColumnSpacing(12);
        ColumnDefinition filesColumn;
        filesColumn.Width(GridLength{ 3, GridUnitType::Star });
        body.ColumnDefinitions().Append(filesColumn);
        ColumnDefinition detailColumn;
        detailColumn.Width(GridLength{ 2, GridUnitType::Star });
        body.ColumnDefinitions().Append(detailColumn);

        Border filesSurface = MakeSurface(tok::kRadiusLg, 14);
        Grid filesLayout;
        filesLayout.RowSpacing(10);
        RowDefinition filesHeaderRow;
        filesHeaderRow.Height(GridLength{ 1, GridUnitType::Auto });
        filesLayout.RowDefinitions().Append(filesHeaderRow);
        RowDefinition filesBodyRow;
        filesBodyRow.Height(GridLength{ 1, GridUnitType::Star });
        filesLayout.RowDefinitions().Append(filesBodyRow);
        filesLayout.Children().Append(MakeSectionHeading(L"运行文件",
            L"仅检查存在性，不加载驱动"));
        diagnosticsList_ = StackPanel();
        diagnosticsList_.Spacing(2);
        ScrollViewer filesScroll;
        filesScroll.Content(diagnosticsList_);
        filesScroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        filesScroll.HorizontalScrollMode(ScrollMode::Disabled);
        Grid::SetRow(filesScroll, 1);
        filesLayout.Children().Append(filesScroll);
        filesSurface.Child(filesLayout);
        Grid::SetColumn(filesSurface, 0);
        body.Children().Append(filesSurface);

        ScrollViewer detailScroll;
        StackPanel detail;
        detail.Spacing(12);
        detailScroll.Content(detail);
        detailScroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        detailScroll.HorizontalScrollMode(ScrollMode::Disabled);

        detail.Children().Append(BuildSymbolPanel());

        Border environment = MakeSurface(tok::kRadiusLg, 14);
        StackPanel environmentBody;
        environmentBody.Spacing(5);
        environmentBody.Children().Append(MakeSectionHeading(L"环境状态"));
        environmentBody.Children().Append(MakeDivider(tok::kBorder));
        environmentBody.Children().Append(ui::MakePropertyRow(
            L"系统", L"—", false, &diagOsText_));
        environmentBody.Children().Append(ui::MakePropertyRow(
            L"内核", L"—", true, &diagKernelText_));
        environmentBody.Children().Append(ui::MakePropertyRow(
            L"架构", L"—", false, &diagArchText_));
        environmentBody.Children().Append(ui::MakePropertyRow(
            L"权限", L"—", false, &diagElevationText_));
        environmentBody.Children().Append(ui::MakePropertyRow(
            L"后端", L"—", false, &diagBackendText_));
        environment.Child(environmentBody);
        detail.Children().Append(environment);

        Border paths = MakeSurface(tok::kRadiusLg, 14);
        StackPanel pathsBody;
        pathsBody.Spacing(6);
        pathsBody.Children().Append(MakeSectionHeading(L"路径"));
        pathsBody.Children().Append(MakeDivider(tok::kBorder));
        diagRuntimePathText_ = tok::Mono(H(controller_.ApplicationDirectory()),
            tok::kFontMicro, tok::kTextSecondary);
        diagRuntimePathText_.IsTextSelectionEnabled(true);
        diagRuntimePathText_.TextWrapping(TextWrapping::Wrap);
        pathsBody.Children().Append(tok::Text(L"运行目录", tok::kFontMicro,
            tok::kTextMuted));
        pathsBody.Children().Append(diagRuntimePathText_);
        diagBackendPathText_ = tok::Mono(L"—", tok::kFontMicro,
            tok::kTextSecondary);
        diagBackendPathText_.IsTextSelectionEnabled(true);
        diagBackendPathText_.TextWrapping(TextWrapping::Wrap);
        pathsBody.Children().Append(tok::Text(L"后端模块", tok::kFontMicro,
            tok::kTextMuted));
        pathsBody.Children().Append(diagBackendPathText_);
        paths.Child(pathsBody);
        detail.Children().Append(paths);

        Grid::SetColumn(detailScroll, 1);
        body.Children().Append(detailScroll);
        Grid::SetRow(body, 1);
        page.Children().Append(body);
        return page;
    }

    Grid BuildHookPage()
    {
        hookUiInitialized_ = false;
        Grid page = MakePageRoot();
        RowDefinition headerRow;
        headerRow.Height(GridLength{ 1, GridUnitType::Auto });
        page.RowDefinitions().Append(headerRow);
        RowDefinition bodyRow;
        bodyRow.Height(GridLength{ 1, GridUnitType::Star });
        page.RowDefinitions().Append(bodyRow);

        StackPanel actions;
        actions.Orientation(Orientation::Horizontal);
        actions.Spacing(7);
        hookSummaryText_ = tok::Text(L"0 / 0", tok::kFontCaption,
            tok::kTextSecondary, tok::kWeightSemiBold);
        hookSummaryText_.VerticalAlignment(VerticalAlignment::Center);
        actions.Children().Append(hookSummaryText_);
        Button enableAll = ui::MakeButton(L"全部启用", tok::kGlyphCheck,
            ui::ButtonVariant::Ghost, tok::kFontCaption);
        Button disableAll = ui::MakeButton(L"全部关闭", tok::kGlyphRemove,
            ui::ButtonVariant::Ghost, tok::kFontCaption);
        enableAll.Click([this](const IInspectable&, const RoutedEventArgs&)
        {
            SetAllSupportedHooks(true);
        });
        disableAll.Click([this](const IInspectable&, const RoutedEventArgs&)
        {
            SetAllSupportedHooks(false);
        });
        hookActionButtons_ = { enableAll, disableAll };
        actions.Children().Append(enableAll);
        actions.Children().Append(disableAll);

        Grid header = MakePageTitle(L"DRIVER HOOKS", L"驱动 Hook 选择", actions);
        Grid::SetRow(header, 0);
        page.Children().Append(header);

        ScrollViewer scroll;
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
        scroll.HorizontalScrollMode(ScrollMode::Disabled);
        StackPanel content;
        content.Spacing(12);

        hookLockText_ = tok::Text(
            L"仅在进入 VT 前可编辑；点击“进入 VT 调试模式”后自动锁定。",
            tok::kFontCaption, tok::kGold, tok::kWeightMedium);
        hookLockText_.TextWrapping(TextWrapping::Wrap);
        content.Children().Append(hookLockText_);

        hookToggles_.clear();
        const std::array<const wchar_t*, 3> categories = {
            L"调试对象与事件",
            L"句柄与反检测",
            L"窗口枚举防护"
        };
        const std::array<const wchar_t*, 3> categoryHints = {
            L"建立虚拟调试会话、进程/线程与异常事件链。",
            L"处理调试端口、句柄查询和系统信息暴露。",
            L"处理 win32k 窗口句柄、类名与枚举查询。"
        };

        for (int category = 0; category < 3; ++category)
        {
            Border surface = MakeSurface(tok::kRadiusLg, 14);
            StackPanel section;
            section.Spacing(0);
            section.Children().Append(MakeSectionHeading(
                categories[static_cast<size_t>(category)],
                categoryHints[static_cast<size_t>(category)]));
            section.Children().Append(MakeDivider(tok::kBorder));

            for (size_t index = 0; index < kHookDescriptors.size(); ++index)
            {
                const HookUiDescriptor& hook = kHookDescriptors[index];
                if (hook.category != category ||
                    (controller_.System().windows11 && !hook.windows11))
                {
                    continue;
                }

                Grid row;
                row.Padding(Thickness{ 0, 8, 0, 8 });
                row.ColumnSpacing(14);
                ColumnDefinition textColumn;
                textColumn.Width(GridLength{ 1, GridUnitType::Star });
                row.ColumnDefinitions().Append(textColumn);
                ColumnDefinition switchColumn;
                switchColumn.Width(GridLength{ 1, GridUnitType::Auto });
                row.ColumnDefinitions().Append(switchColumn);

                StackPanel labels;
                labels.Spacing(2);
                TextBlock symbol = tok::Mono(hook.symbol,
                    tok::kFontCaption, tok::kTextPrimary);
                symbol.IsTextSelectionEnabled(true);
                TextBlock description = tok::Text(hook.description,
                    tok::kFontMicro, tok::kTextMuted);
                description.TextWrapping(TextWrapping::Wrap);
                labels.Children().Append(symbol);
                labels.Children().Append(description);
                row.Children().Append(labels);

                ToggleSwitch toggle;
                toggle.MinWidth(48);
                toggle.OffContent(box_value(hstring(L"")));
                toggle.OnContent(box_value(hstring(L"")));
                toggle.IsOn((controller_.HookSelectionMask() & hook.mask) != 0);
                toggle.IsEnabled(controller_.HookConfigurationEditable());
                toggle.Tag(box_value(static_cast<int32_t>(index)));
                toggle.Toggled({ this, &Shell::OnHookToggleChanged });
                winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
                    toggle, H(L"安装 " + std::wstring(hook.symbol)));
                tok::SetToolTip(toggle, hook.description);
                Grid::SetColumn(toggle, 1);
                row.Children().Append(toggle);
                hookToggles_.push_back(toggle);

                section.Children().Append(row);
                if (index + 1 < kHookDescriptors.size())
                {
                    section.Children().Append(MakeDivider(tok::kBorder));
                }
            }
            surface.Child(section);
            content.Children().Append(surface);
        }

        scroll.Content(content);
        Grid::SetRow(scroll, 1);
        page.Children().Append(scroll);
        RefreshHooks();
        return page;
    }

    Grid BuildLogsPage()
    {
        Grid page = MakePageRoot();
        RowDefinition headerRow;
        headerRow.Height(GridLength{ 1, GridUnitType::Auto });
        page.RowDefinitions().Append(headerRow);
        RowDefinition bodyRow;
        bodyRow.Height(GridLength{ 1, GridUnitType::Star });
        page.RowDefinitions().Append(bodyRow);

        StackPanel actions;
        actions.Orientation(Orientation::Horizontal);
        actions.Spacing(7);
        externalLogRefreshButton_ = ui::MakeButton(L"立即刷新",
            tok::kGlyphRefresh, ui::ButtonVariant::Secondary,
            tok::kFontCaption);
        externalLogRefreshButton_.Click([this](const IInspectable&,
            const RoutedEventArgs&)
        {
            RefreshLogView(true);
        });
        Button openFolder = ui::MakeButton(L"打开目录", tok::kGlyphFolder,
            ui::ButtonVariant::Ghost, tok::kFontCaption);
        openFolder.Click([this](const IInspectable&, const RoutedEventArgs&)
        {
            if (externalLogIndex_ == 0)
            {
                OpenExplorer(unrealdbg_native::JoinPath(
                    controller_.ApplicationDirectory(), L"Log"));
            }
            else if (externalLogIndex_ > 0 &&
                static_cast<size_t>(externalLogIndex_ - 1) <
                    externalLogFiles_.size())
            {
                const auto& file = externalLogFiles_[
                    static_cast<size_t>(externalLogIndex_ - 1)];
                OpenExplorer(std::filesystem::path(file.path)
                    .parent_path().wstring());
            }
        });
        actions.Children().Append(externalLogRefreshButton_);
        actions.Children().Append(openFolder);
        Grid header = MakePageTitle(L"LOG CENTER", L"日志中心",
            actions);
        Grid::SetRow(header, 0);
        page.Children().Append(header);

        Border surface = MakeSurface(tok::kRadiusLg, 14);
        Grid layout;
        layout.RowSpacing(9);
        RowDefinition selectorRow;
        selectorRow.Height(GridLength{ 1, GridUnitType::Auto });
        layout.RowDefinitions().Append(selectorRow);
        RowDefinition pathRow;
        pathRow.Height(GridLength{ 1, GridUnitType::Auto });
        layout.RowDefinitions().Append(pathRow);
        RowDefinition textRow;
        textRow.Height(GridLength{ 1, GridUnitType::Star });
        layout.RowDefinitions().Append(textRow);

        Grid selector;
        selector.ColumnSpacing(10);
        ColumnDefinition comboColumn;
        comboColumn.Width(GridLength{ 230 });
        selector.ColumnDefinitions().Append(comboColumn);
        ColumnDefinition statusColumn;
        statusColumn.Width(GridLength{ 1, GridUnitType::Star });
        selector.ColumnDefinitions().Append(statusColumn);
        ColumnDefinition autoColumn;
        autoColumn.Width(GridLength{ 1, GridUnitType::Auto });
        selector.ColumnDefinitions().Append(autoColumn);

        externalLogCombo_ = ComboBox();
        externalLogCombo_.PlaceholderText(L"选择日志");
        ComboBoxItem internalItem;
        internalItem.Content(box_value(hstring(L"界面运行日志 · 当前会话")));
        externalLogCombo_.Items().Append(internalItem);
        externalLogFiles_ = controller_.ExternalLogs();
        for (const auto& file : externalLogFiles_)
        {
            ComboBoxItem item;
            item.Content(box_value(H(file.name + (file.currentSystem
                ? L"  · 当前系统" : L""))));
            item.Tag(box_value(H(file.path)));
            externalLogCombo_.Items().Append(item);
        }
        externalLogCombo_.SelectionChanged({ this,
            &Shell::OnExternalLogSelectionChanged });
        const wchar_t* launchCommand = ::GetCommandLineW();
        if (launchCommand != nullptr)
        {
            int requested = -1;
            if (std::wcsstr(launchCommand, L"--ui-log=internal") != nullptr)
            {
                requested = 0;
            }
            else if (std::wcsstr(launchCommand, L"--ui-log=vtload") != nullptr)
            {
                requested = 3;
            }
            else if (std::wcsstr(launchCommand, L"--ui-log=hook64") != nullptr)
            {
                requested = 4;
            }
            else if (std::wcsstr(launchCommand, L"--ui-log=hook") != nullptr)
            {
                requested = 5;
            }
            if (requested >= 0 &&
                requested < static_cast<int>(externalLogCombo_.Items().Size()))
            {
                suppressExternalLogSelection_ = true;
                externalLogIndex_ = requested;
                externalLogCombo_.SelectedIndex(requested);
                suppressExternalLogSelection_ = false;
            }
        }
        selector.Children().Append(externalLogCombo_);

        externalLogStatusText_ = tok::Text(L"等待选择日志",
            tok::kFontCaption, tok::kTextSecondary);
        externalLogStatusText_.TextTrimming(TextTrimming::CharacterEllipsis);
        externalLogStatusText_.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetColumn(externalLogStatusText_, 1);
        selector.Children().Append(externalLogStatusText_);

        externalLogAutoRefresh_ = ToggleSwitch();
        externalLogAutoRefresh_.IsOn(true);
        externalLogAutoRefresh_.Toggled([this](const IInspectable&,
            const RoutedEventArgs&)
        {
            UpdatePollingInterval();
        });
        externalLogAutoRefresh_.OffContent(box_value(hstring(L"实时刷新")));
        externalLogAutoRefresh_.OnContent(box_value(hstring(L"实时刷新")));
        externalLogAutoRefresh_.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetColumn(externalLogAutoRefresh_, 2);
        selector.Children().Append(externalLogAutoRefresh_);
        layout.Children().Append(selector);

        externalLogPathText_ = tok::Mono(L"—", tok::kFontMicro,
            tok::kTextMuted);
        externalLogPathText_.IsTextSelectionEnabled(true);
        externalLogPathText_.TextTrimming(TextTrimming::CharacterEllipsis);
        Grid::SetRow(externalLogPathText_, 1);
        layout.Children().Append(externalLogPathText_);

        externalLogText_ = TextBox();
        externalLogText_.AcceptsReturn(true);
        externalLogText_.IsReadOnly(true);
        externalLogText_.IsSpellCheckEnabled(false);
        externalLogText_.TextWrapping(TextWrapping::NoWrap);
        externalLogText_.FontFamily(FontFamily(L"Cascadia Mono"));
        externalLogText_.FontSize(tok::kFontMicro);
        externalLogText_.Foreground(tok::Brush(tok::kTextSecondary));
        externalLogText_.Background(tok::Brush(tok::kSunken));
        externalLogText_.BorderBrush(tok::Brush(tok::kBorder));
        externalLogText_.BorderThickness(Thickness{ 1, 1, 1, 1 });
        externalLogText_.Padding(Thickness{ 10, 8, 10, 8 });
        ScrollViewer::SetHorizontalScrollBarVisibility(
            externalLogText_, ScrollBarVisibility::Auto);
        ScrollViewer::SetVerticalScrollBarVisibility(
            externalLogText_, ScrollBarVisibility::Auto);
        Grid::SetRow(externalLogText_, 2);
        layout.Children().Append(externalLogText_);

        surface.Child(layout);
        Grid::SetRow(surface, 1);
        page.Children().Append(surface);
        return page;
    }

    Grid BuildSystemPage()
    {
        Grid page = MakePageRoot();
        RowDefinition headerRow;
        headerRow.Height(GridLength{ 1, GridUnitType::Auto });
        page.RowDefinitions().Append(headerRow);
        RowDefinition bodyRow;
        bodyRow.Height(GridLength{ 1, GridUnitType::Star });
        page.RowDefinitions().Append(bodyRow);

        StackPanel actions;
        actions.Orientation(Orientation::Horizontal);
        actions.Spacing(7);
        systemPageRefreshButton_ = ui::MakeButton(L"刷新系统信息",
            tok::kGlyphRefresh, ui::ButtonVariant::Secondary,
            tok::kFontCaption);
        systemPageRefreshButton_.Click([this](const IInspectable&,
            const RoutedEventArgs&)
        {
            controller_.Refresh();
            systemPageDirty_ = true;
            RefreshSystemPage();
        });
        actions.Children().Append(systemPageRefreshButton_);
        Grid header = MakePageTitle(L"SYSTEM PROFILE", L"系统详细信息",
            actions);
        Grid::SetRow(header, 0);
        page.Children().Append(header);

        ScrollViewer scroll;
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
        scroll.HorizontalScrollMode(ScrollMode::Disabled);
        systemDetailsBody_ = StackPanel();
        systemDetailsBody_.Spacing(12);
        scroll.Content(systemDetailsBody_);
        Grid::SetRow(scroll, 1);
        page.Children().Append(scroll);
        return page;
    }

    Grid BuildSymbolsPage()
    {
        Grid page = MakePageRoot();
        RowDefinition headerRow;
        headerRow.Height(GridLength{ 1, GridUnitType::Auto });
        page.RowDefinitions().Append(headerRow);
        RowDefinition bodyRow;
        bodyRow.Height(GridLength{ 1, GridUnitType::Star });
        page.RowDefinitions().Append(bodyRow);

        StackPanel actions;
        actions.Orientation(Orientation::Horizontal);
        actions.Spacing(7);
        chooseSymbolPdbButton_ = ui::MakeButton(L"选择 PDB",
            tok::kGlyphFolder, ui::ButtonVariant::Ghost,
            tok::kFontCaption);
        chooseSymbolPdbButton_.Click([this](const IInspectable&,
            const RoutedEventArgs&)
        {
            OnChooseSymbolPdbClicked();
        });
        resetSymbolPdbButton_ = ui::MakeButton(L"系统 PDB",
            tok::kGlyphServer, ui::ButtonVariant::Ghost,
            tok::kFontCaption);
        resetSymbolPdbButton_.Click([this](const IInspectable&,
            const RoutedEventArgs&)
        {
            customSymbolPdbPath_.clear();
            symbolAnalysisConfigurationDirty_ = true;
            symbolAnalysisStartPending_ = false;
            RefreshOffsetAnalysis(true);
        });
        initializeOffsetsButton_ = ui::MakeButton(L"初始化",
            tok::kGlyphPlay, ui::ButtonVariant::Primary,
            tok::kFontCaption);
        initializeOffsetsButton_.Click([this](const IInspectable&,
            const RoutedEventArgs&)
        {
            StartSymbolAnalysis();
        });
        actions.Children().Append(chooseSymbolPdbButton_);
        actions.Children().Append(resetSymbolPdbButton_);
        actions.Children().Append(initializeOffsetsButton_);
        Grid header = MakePageTitle(L"SYSTEM SYMBOLS", L"系统符号与函数地址",
            actions);
        Grid::SetRow(header, 0);
        page.Children().Append(header);

        Border surface = MakeSurface(tok::kRadiusLg, 14);
        Grid layout;
        layout.RowSpacing(8);
        RowDefinition hintRow;
        hintRow.Height(GridLength{ 1, GridUnitType::Auto });
        layout.RowDefinitions().Append(hintRow);
        RowDefinition summaryRow;
        summaryRow.Height(GridLength{ 1, GridUnitType::Auto });
        layout.RowDefinitions().Append(summaryRow);
        RowDefinition pathRow;
        pathRow.Height(GridLength{ 1, GridUnitType::Auto });
        layout.RowDefinitions().Append(pathRow);
        RowDefinition filterRow;
        filterRow.Height(GridLength{ 1, GridUnitType::Auto });
        layout.RowDefinitions().Append(filterRow);
        RowDefinition tableRow;
        tableRow.Height(GridLength{ 1, GridUnitType::Star });
        layout.RowDefinitions().Append(tableRow);

        StackPanel heading;
        heading.Spacing(2);
        heading.Children().Append(MakeSectionHeading(L"PDB 符号库",
            L"符号来源为 C:\\Symbols 或自定义 PDB"));
        symbolAnalysisHint_ = tok::Text(
            L"地址仅对已加载模块计算；未匹配模块只显示 RVA。",
            tok::kFontMicro, tok::kTextMuted);
        symbolAnalysisHint_.TextWrapping(TextWrapping::Wrap);
        symbolAnalysisHint_.IsTextSelectionEnabled(true);
        heading.Children().Append(symbolAnalysisHint_);
        layout.Children().Append(heading);

        offsetSummaryText_ = tok::Text(L"等待符号准备",
            tok::kFontCaption, tok::kGold, tok::kWeightSemiBold);
        Grid::SetRow(offsetSummaryText_, 1);
        layout.Children().Append(offsetSummaryText_);

        StackPanel paths;
        paths.Spacing(1);
        offsetRuntimePdbText_ = tok::Mono(L"运行 PDB：—",
            tok::kFontMicro, tok::kTextMuted);
        offsetModuleBaseText_ = tok::Mono(L"内核基址：—",
            tok::kFontMicro, tok::kTextMuted);
        offsetRuntimePdbText_.TextTrimming(TextTrimming::CharacterEllipsis);
        offsetModuleBaseText_.TextTrimming(TextTrimming::CharacterEllipsis);
        paths.Children().Append(offsetRuntimePdbText_);
        paths.Children().Append(offsetModuleBaseText_);
        Grid::SetRow(paths, 2);
        layout.Children().Append(paths);

        StackPanel filters;
        filters.Spacing(7);

        Grid searchRow;
        searchRow.ColumnSpacing(9);
        ColumnDefinition searchColumn;
        searchColumn.Width(GridLength{ 1, GridUnitType::Star });
        searchRow.ColumnDefinitions().Append(searchColumn);
        ColumnDefinition pdbColumn;
        pdbColumn.Width(GridLength{ 220 });
        searchRow.ColumnDefinitions().Append(pdbColumn);

        symbolSearchBox_ = TextBox();
        symbolSearchBox_.PlaceholderText(
            L"搜索模块名、符号、函数或用途…");
        symbolSearchBox_.Text(H(symbolSearchText_));
        symbolSearchBox_.TextChanged({ this, &Shell::OnSymbolSearchChanged });
        searchRow.Children().Append(symbolSearchBox_);

        offsetPdbFilter_ = ComboBox();
        offsetPdbFilter_.PlaceholderText(L"PDB 筛选");
        ComboBoxItem allPdbItem;
        allPdbItem.Content(box_value(hstring(L"全部 PDB")));
        offsetPdbFilter_.Items().Append(allPdbItem);
        offsetPdbFilter_.SelectedIndex(0);
        offsetPdbFilter_.SelectionChanged({ this,
            &Shell::OnSymbolPdbFilterChanged });
        Grid::SetColumn(offsetPdbFilter_, 1);
        searchRow.Children().Append(offsetPdbFilter_);
        filters.Children().Append(searchRow);

        Grid optionsRow;
        optionsRow.ColumnSpacing(9);
        ColumnDefinition kindColumn;
        kindColumn.Width(GridLength{ 135 });
        optionsRow.ColumnDefinitions().Append(kindColumn);
        ColumnDefinition usedColumn;
        usedColumn.Width(GridLength{ 1, GridUnitType::Auto });
        optionsRow.ColumnDefinitions().Append(usedColumn);
        ColumnDefinition internalColumn;
        internalColumn.Width(GridLength{ 1, GridUnitType::Auto });
        optionsRow.ColumnDefinitions().Append(internalColumn);
        ColumnDefinition resultColumn;
        resultColumn.Width(GridLength{ 1, GridUnitType::Star });
        optionsRow.ColumnDefinitions().Append(resultColumn);

        offsetKindFilter_ = ComboBox();
        for (const wchar_t* label : {
            L"全部类型", L"结构字段", L"函数", L"变量", L"其他" })
        {
            ComboBoxItem item;
            item.Content(box_value(hstring(label)));
            offsetKindFilter_.Items().Append(item);
        }
        offsetKindFilter_.SelectedIndex(0);
        offsetKindFilter_.SelectionChanged({ this,
            &Shell::OnSymbolKindFilterChanged });
        optionsRow.Children().Append(offsetKindFilter_);

        symbolUsedOnlyToggle_ = ToggleSwitch();
        symbolUsedOnlyToggle_.OffContent(
            box_value(hstring(L"仅项目使用")));
        symbolUsedOnlyToggle_.OnContent(
            box_value(hstring(L"仅项目使用")));
        symbolUsedOnlyToggle_.IsOn(symbolUsedOnlyFilter_);
        symbolUsedOnlyToggle_.Toggled({ this,
            &Shell::OnSymbolUsedOnlyToggled });
        Grid::SetColumn(symbolUsedOnlyToggle_, 1);
        optionsRow.Children().Append(symbolUsedOnlyToggle_);

        symbolInternalToggle_ = ToggleSwitch();
        symbolInternalToggle_.OffContent(
            box_value(hstring(L"内部符号")));
        symbolInternalToggle_.OnContent(
            box_value(hstring(L"内部符号")));
        symbolInternalToggle_.IsOn(symbolShowInternal_);
        symbolInternalToggle_.Toggled({ this,
            &Shell::OnSymbolInternalToggled });
        Grid::SetColumn(symbolInternalToggle_, 2);
        optionsRow.Children().Append(symbolInternalToggle_);

        symbolResultText_ = tok::Text(L"", tok::kFontMicro,
            tok::kTextMuted);
        symbolResultText_.VerticalAlignment(VerticalAlignment::Center);
        symbolResultText_.HorizontalAlignment(HorizontalAlignment::Right);
        Grid::SetColumn(symbolResultText_, 3);
        optionsRow.Children().Append(symbolResultText_);
        filters.Children().Append(optionsRow);
        Grid::SetRow(filters, 3);
        layout.Children().Append(filters);

        ScrollViewer tableScroll;
        tableScroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Auto);
        tableScroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        StackPanel table;
        table.MinWidth(1080);
        table.Spacing(0);

        Grid tableHeader;
        tableHeader.Padding(Thickness{ 0, 6, 0, 6 });
        tableHeader.ColumnSpacing(10);
        const std::array<double, 6> tableWidths = {
            1.1, 2.4, 0.8, 0.95, 1.3, 2.1
        };
        const std::array<const wchar_t*, 6> tableLabels = {
            L"PDB / 模块", L"符号", L"类型", L"RVA / 偏移",
            L"运行时地址", L"用途"
        };
        for (size_t index = 0; index < tableWidths.size(); ++index)
        {
            ColumnDefinition column;
            column.Width(GridLength{ tableWidths[index],
                GridUnitType::Star });
            tableHeader.ColumnDefinitions().Append(column);
            TextBlock label = tok::Text(tableLabels[index],
                tok::kFontMicro, tok::kTextMuted, tok::kWeightSemiBold);
            Grid::SetColumn(label, static_cast<int>(index));
            tableHeader.Children().Append(label);
        }
        table.Children().Append(tableHeader);
        table.Children().Append(MakeDivider(tok::kBorder));

        offsetEmptyText_ = tok::Text(L"", tok::kFontCaption,
            tok::kTextMuted);
        offsetEmptyText_.Margin(Thickness{ 0, 16, 0, 16 });
        offsetEmptyText_.Visibility(Visibility::Collapsed);
        table.Children().Append(offsetEmptyText_);

        offsetRepeater_ = ItemsRepeater();
        offsetRepeater_.HorizontalAlignment(HorizontalAlignment::Stretch);
        offsetRepeater_.VerticalCacheLength(1.0);
        StackLayout symbolRowsLayout;
        symbolRowsLayout.Spacing(0);
        offsetRepeater_.Layout(symbolRowsLayout);
        symbolRowFactory_ =
            winrt::make_self<SymbolRowElementFactory>(this);
        offsetRepeater_.ItemTemplate(
            symbolRowFactory_.as<IInspectable>());
        table.Children().Append(offsetRepeater_);
        tableScroll.Content(table);
        Grid::SetRow(tableScroll, 4);
        layout.Children().Append(tableScroll);

        surface.Child(layout);
        Grid::SetRow(surface, 1);
        page.Children().Append(surface);
        return page;
    }

    void RefreshAll()
    {
        systemPageDirty_ = true;
        symbolPageDirty_ = true;
        RefreshSymbols();
        RefreshStatus();
        RefreshSystem();
        RefreshHooks();
        RefreshSystemPage();
        RefreshOffsetAnalysis();
        RefreshDebuggerLists();
        RefreshTarget();
        RefreshTl();
        RefreshDiagnostics();
        RefreshLogView(false);
    }

    void RefreshStatus()
    {
        const unrealdbg_native::BackendState state = controller_.State();
        Color color = tok::kGold;
        hstring badge = L"准备中";
        hstring title = L"正在检查后端";
        hstring detail = L"等待 UnrealDbgDll 接口检查";
        hstring button = L"进入 VT 调试模式";
        bool buttonEnabled = false;
        bool progress = false;
        hstring navState = L"检查中";
        const unrealdbg_native::SymbolPreparationState symbolState =
            controller_.Symbols().State();

        switch (state)
        {
        case unrealdbg_native::BackendState::Missing:
            color = tok::kDanger;
            badge = L"后端缺失";
            title = L"后端未连接";
            detail = L"未找到 UnrealDbgDll.dll 或必要导出";
            button = L"重新检查";
            buttonEnabled = false;
            navState = L"缺失";
            break;
        case unrealdbg_native::BackendState::Ready:
            navState = L"就绪";
            if (symbolState == unrealdbg_native::SymbolPreparationState::Preparing)
            {
                color = tok::kAccent;
                badge = L"符号准备";
                title = L"正在准备调试符号";
                detail = H(controller_.Symbols().Summary());
                button = L"等待符号准备";
                buttonEnabled = false;
                progress = true;
            }
            else if (symbolState == unrealdbg_native::SymbolPreparationState::Failed)
            {
                color = tok::kDanger;
                badge = L"符号失败";
                title = L"符号缓存未就绪";
                detail = H(controller_.Symbols().LastError());
                button = L"重新准备符号";
                buttonEnabled = true;
            }
            else if (symbolState == unrealdbg_native::SymbolPreparationState::Ready)
            {
                color = tok::kGold;
                badge = L"已就绪";
                title = L"等待进入 VT 调试模式";
                detail = L"后端接口与符号缓存已就绪；驱动尚未加载";
                button = L"进入 VT 调试模式";
                buttonEnabled = true;
            }
            else
            {
                color = tok::kGold;
                badge = L"待校验";
                title = L"等待符号缓存校验";
                detail = L"Loader 符号准备即将开始";
                button = L"准备符号";
                buttonEnabled = true;
            }
            break;
        case unrealdbg_native::BackendState::Initializing:
            color = tok::kAccent;
            badge = L"初始化中";
            title = L"正在建立 VT 调试通道";
            detail = L"驱动加载、权限检查与符号握手进行中";
            button = L"正在初始化";
            progress = true;
            navState = L"初始化";
            break;
        case unrealdbg_native::BackendState::Initialized:
            color = tok::kSuccess;
            badge = L"运行中";
            title = L"VT 调试模式已就绪";
            detail = L"可启动外部调试器并应用目标防检测策略";
            button = L"已进入 VT";
            navState = L"运行中";
            break;
        case unrealdbg_native::BackendState::Failed:
            color = tok::kDanger;
            badge = L"初始化失败";
            title = L"未能进入 VT 调试模式";
            detail = H(controller_.LastError());
            button = L"重新尝试";
            buttonEnabled = controller_.BackendLoaded();
            navState = L"失败";
            break;
        }

        if (titleStatusPill_ != nullptr)
        {
            titleStatusPill_.Background(tok::Brush(tok::kSunken));
            titleStatusPill_.BorderBrush(tok::Brush(tok::kBorder));
            titleStatusText_.Text(badge);
            titleStatusDot_.Background(tok::Brush(color));
        }
        if (statusTitle_ != nullptr)
        {
            statusTitle_.Text(title);
            statusTitle_.Foreground(tok::Brush(color));
            statusDetail_.Text(detail);
            statusSeal_.Background(tok::Brush(state == unrealdbg_native::BackendState::Initialized
                ? tok::kAccentSoft : tok::kCinnabarSoft));
            statusSeal_.BorderBrush(tok::Brush(color));
            TextBlock sealText = statusSeal_.Child().try_as<TextBlock>();
            if (sealText != nullptr)
            {
                sealText.Foreground(tok::Brush(color));
            }
            SetButtonCaption(initButton_, button);
            SetButtonEnabled(initButton_, buttonEnabled);
            initProgress_.IsActive(progress);
            initProgress_.Visibility(progress
                ? Visibility::Visible : Visibility::Collapsed);
        }
        if (navBackendText_ != nullptr)
        {
            navBackendText_.Text(navState);
            navBackendText_.Foreground(tok::Brush(color));
        }
        if (backendStatText_ != nullptr)
        {
            static const wchar_t* labels[] = {
                L"缺失", L"已连接", L"初始化中", L"已连接", L"失败" };
            backendStatText_.Text(labels[static_cast<int>(state)]);
            backendStatText_.Foreground(tok::Brush(color));
        }
        if (targetTlStateText_ != nullptr)
        {
            targetTlStateText_.Text(state == unrealdbg_native::BackendState::Initialized
                ? L"已进入 VT" : L"等待初始化");
        }
        UpdateActionAvailability();
    }

    void RefreshSystem()
    {
        const auto& system = controller_.System();
        const hstring os = H(system.osName);
        const hstring kernel = H(system.kernelVersion);
        const hstring cpu = H(system.cpuName);
        const hstring memory = H(system.memoryText);
        const hstring arch = H(system.architecture);
        const hstring elevation = system.elevated ? L"管理员" : L"标准权限";

        const auto set = [](const TextBlock& block, const hstring& value)
        {
            if (block != nullptr)
            {
                block.Text(value);
                tok::SetToolTip(block, value);
            }
        };
        set(systemOsText_, os);
        set(systemKernelText_, kernel);
        set(systemCpuText_, cpu);
        set(systemMemoryText_, memory);
        set(diagOsText_, os);
        set(diagKernelText_, kernel);
        set(diagArchText_, arch);
        set(diagElevationText_, elevation);
        if (targetPathText_ != nullptr && controller_.Target().path.empty())
        {
            targetPathText_.Text(L"—");
        }
    }

    UIElement MakeDebuggerRow(const unrealdbg_native::DebuggerEntry& entry)
    {
        Grid row;
        row.ColumnSpacing(10);
        ColumnDefinition iconColumn;
        iconColumn.Width(GridLength{ 34 });
        row.ColumnDefinitions().Append(iconColumn);
        ColumnDefinition textColumn;
        textColumn.Width(GridLength{ 1, GridUnitType::Star });
        row.ColumnDefinitions().Append(textColumn);
        ColumnDefinition badgeColumn;
        badgeColumn.Width(GridLength{ 1, GridUnitType::Auto });
        row.ColumnDefinitions().Append(badgeColumn);

        row.Children().Append(MakeExecutableIcon(H(entry.path), 30));

        StackPanel text;
        text.Spacing(2);
        text.VerticalAlignment(VerticalAlignment::Center);
        text.Children().Append(tok::Text(H(entry.name), tok::kFontBody,
            tok::kTextPrimary, tok::kWeightMedium));
        TextBlock path = tok::Mono(H(entry.path), tok::kFontMicro,
            tok::kTextMuted);
        path.TextTrimming(TextTrimming::CharacterEllipsis);
        tok::SetToolTip(path, H(entry.path));
        text.Children().Append(path);
        Grid::SetColumn(text, 1);
        row.Children().Append(text);

        Border badge = tok::Pill(L"EXE", tok::kTextSecondary,
            tok::kSunken, tok::kFontMicro - 1);
        badge.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetColumn(badge, 2);
        row.Children().Append(badge);
        return row;
    }

    void AddEmptyDebuggerRow(ListView& list, const wchar_t* title,
        const wchar_t* detail)
    {
        if (list == nullptr)
        {
            return;
        }
        ListViewItem item;
        item.IsHitTestVisible(false);
        item.Padding(Thickness{ 0, 0, 0, 0 });
        item.Background(tok::Brush(tok::Rgb(0, 0, 0, 0)));
        Border empty = tok::Surface(tok::kCardInset, tok::kBorder,
            tok::kRadiusMd, 14);
        empty.Margin(Thickness{ 4, 4, 4, 4 });
        StackPanel body;
        body.Spacing(4);
        body.Children().Append(tok::Text(title, tok::kFontBody,
            tok::kTextSecondary, tok::kWeightMedium));
        body.Children().Append(tok::Text(detail, tok::kFontCaption,
            tok::kTextMuted));
        empty.Child(body);
        item.Content(empty);
        list.Items().Append(item);
    }

    void FillDebuggerList(ListView& list, const size_t limit)
    {
        if (list == nullptr)
        {
            return;
        }
        list.Items().Clear();
        const auto& entries = controller_.Debuggers();
        if (entries.empty())
        {
            AddEmptyDebuggerRow(list, L"尚未添加调试器",
                L"通过“添加”选择 x64dbg、x32dbg 或其他调试器");
            return;
        }

        const size_t count = (std::min)(entries.size(), limit);
        for (size_t index = 0; index < count; ++index)
        {
            ListViewItem item;
            item.Content(MakeDebuggerRow(entries[index]));
            item.Tag(box_value(H(entries[index].path)));
            item.Padding(Thickness{ 0, 0, 0, 0 });
            item.Margin(Thickness{ 0, 0, 0, 0 });
            item.MinHeight(55);
            item.MinWidth(0);
            item.HorizontalContentAlignment(HorizontalAlignment::Stretch);
            item.VerticalContentAlignment(VerticalAlignment::Center);
            item.Background(tok::Brush(tok::Rgb(0, 0, 0, 0)));
            item.BorderBrush(tok::Brush(tok::kBorder));
            item.BorderThickness(Thickness{ 0, 0, 0, 1 });
            item.CornerRadius(CornerRadius{ 0, 0, 0, 0 });
            list.Items().Append(item);
        }
    }

    void RefreshDebuggerLists()
    {
        const size_t count = controller_.Debuggers().size();
        FillDebuggerList(debuggerList_, count);
        FillDebuggerList(overviewDebuggerList_, 4);
        overviewSelectedIndex_ = -1;
        debuggerSelectedIndex_ = -1;
        if (debuggerCountText_ != nullptr)
        {
            debuggerCountText_.Text(H(std::to_wstring(count) + L" 项"));
        }
        if (debuggerStatText_ != nullptr)
        {
            debuggerStatText_.Text(H(std::to_wstring(count)));
        }
        if (navDebuggerBadge_ != nullptr)
        {
            TextBlock badgeText = navDebuggerBadge_.Child().try_as<TextBlock>();
            if (badgeText != nullptr)
            {
                badgeText.Text(H(std::to_wstring(count)));
            }
        }
        if (overviewSelectionText_ != nullptr)
        {
            overviewSelectionText_.Text(count == 0 ? L"没有可用调试器" : L"选择列表项");
        }
        if (selectedDebuggerPathText_ != nullptr)
        {
            selectedDebuggerPathText_.Text(L"未选择调试器");
        }
        UpdateActionAvailability();
    }

    void RefreshTarget()
    {
        const auto& target = controller_.Target();
        if (targetNameText_ == nullptr)
        {
            return;
        }
        if (target.processId == 0)
        {
            targetNameText_.Text(L"尚未选择目标进程");
            targetNameText_.Foreground(tok::Brush(tok::kTextPrimary));
            targetPidText_.Text(L"点击捕获后 3 秒内切换到目标窗口");
            targetPathText_.Text(L"—");
            targetStatText_.Text(L"未选择");
            navTargetText_.Text(L"未选择");
        }
        else
        {
            targetNameText_.Text(H(target.name));
            targetNameText_.Foreground(tok::Brush(tok::kAccent));
            targetPidText_.Text(H(L"PID " + std::to_wstring(target.processId) +
                L" · 目标已锁定"));
            targetPathText_.Text(target.path.empty() ? L"映像路径不可读" : H(target.path));
            targetStatText_.Text(H(target.name));
            navTargetText_.Text(H(target.name));
        }
        targetStatText_.TextTrimming(TextTrimming::CharacterEllipsis);
        navTargetText_.TextTrimming(TextTrimming::CharacterEllipsis);
        UpdateActionAvailability();
    }

    void RefreshTl()
    {
        suppressTlEvents_ = true;
        if (tlEnableToggle_ != nullptr)
        {
            tlEnableToggle_.IsOn(controller_.TlEnabled());
            tlTickToggle_.IsOn(controller_.TlGetTickCount());
            tlBlockToggle_.IsOn(controller_.TlBlockResumeThread());
            const bool enabled = controller_.TlEnabled();
            tlTickToggle_.IsEnabled(enabled);
            tlBlockToggle_.IsEnabled(enabled);
            tlTickToggle_.Opacity(enabled ? 1.0 : 0.48);
            tlBlockToggle_.Opacity(enabled ? 1.0 : 0.48);
        }
        suppressTlEvents_ = false;

        if (overviewTlText_ != nullptr)
        {
            if (!controller_.TlEnabled())
            {
                SetButtonCaption(overviewTlButton_, L"防检测：未启用");
                overviewTlText_.Foreground(tok::Brush(tok::kTextMuted));
            }
            else
            {
                std::wstring state = L"已启用";
                if (controller_.TlGetTickCount())
                {
                    state += L" · Tick";
                }
                if (controller_.TlBlockResumeThread())
                {
                    state += L" · Resume";
                }
                SetButtonCaption(overviewTlButton_,
                    H(L"防检测：" + state));
                overviewTlText_.Foreground(tok::Brush(tok::kAccent));
            }
        }
        if (targetGuardStateText_ != nullptr)
        {
            targetGuardStateText_.Text(controller_.TlEnabled()
                ? L"已启用" : L"未启用");
        }
        UpdateActionAvailability();
    }

    void RefreshSymbols()
    {
        auto& symbols = controller_.Symbols();
        const auto state = symbols.State();
        const int progress = symbols.Progress();
        const std::wstring summary = symbols.Summary();
        const std::wstring failure = symbols.LastError();

        hstring value = L"待校验";
        hstring compact = L"等待";
        Color color = tok::kGold;
        switch (state)
        {
        case unrealdbg_native::SymbolPreparationState::Preparing:
            value = L"准备中";
            compact = H(L"准备 " + std::to_wstring(progress) + L"%");
            color = tok::kAccent;
            break;
        case unrealdbg_native::SymbolPreparationState::Ready:
            value = L"已就绪";
            compact = L"3/3";
            color = tok::kSuccess;
            break;
        case unrealdbg_native::SymbolPreparationState::Failed:
            value = L"失败";
            compact = L"失败";
            color = tok::kDanger;
            break;
        case unrealdbg_native::SymbolPreparationState::Idle:
        default:
            break;
        }

        if (symbolStatText_ != nullptr)
        {
            symbolStatText_.Text(value);
            symbolStatText_.Foreground(tok::Brush(color));
        }
        if (symbolHintText_ != nullptr)
        {
            symbolHintText_.Text(H(summary));
            tok::SetToolTip(symbolHintText_, H(summary));
        }
        if (navSymbolText_ != nullptr)
        {
            navSymbolText_.Text(compact);
            navSymbolText_.Foreground(tok::Brush(color));
            tok::SetToolTip(navSymbolText_, H(summary));
        }
        if (symbolStatusText_ != nullptr)
        {
            symbolStatusText_.Text(H(summary));
            symbolStatusText_.Foreground(tok::Brush(color));
        }
        if (symbolProgressText_ != nullptr)
        {
            symbolProgressText_.Text(H(std::to_wstring(progress) + L"%"));
        }
        if (symbolProgressBar_ != nullptr)
        {
            symbolProgressBar_.Value(progress);
            symbolProgressBar_.Foreground(tok::Brush(color));
            symbolProgressBar_.IsIndeterminate(
                state == unrealdbg_native::SymbolPreparationState::Preparing &&
                progress == 0);
        }
        if (symbolPrepareButton_ != nullptr)
        {
            const bool preparing =
                state == unrealdbg_native::SymbolPreparationState::Preparing;
            SetButtonEnabled(symbolPrepareButton_, !preparing);
            SetButtonCaption(symbolPrepareButton_,
                preparing ? L"正在准备" :
                state == unrealdbg_native::SymbolPreparationState::Failed
                    ? L"重试准备" : L"重新检查");
        }
        if (symbolErrorText_ != nullptr)
        {
            const bool failed =
                state == unrealdbg_native::SymbolPreparationState::Failed;
            symbolErrorText_.Visibility(failed
                ? Visibility::Visible : Visibility::Collapsed);
            if (failed)
            {
                symbolErrorText_.Text(failure.empty() ? L"符号准备失败" : H(failure));
            }
        }

        if (symbolModulesList_ != nullptr)
        {
            symbolModulesList_.Children().Clear();
            for (const auto& module : symbols.Modules())
            {
                Color moduleColor = tok::kTextMuted;
                hstring moduleState = L"等待";
                switch (module.state)
                {
                case unrealdbg_native::SymbolModuleState::Checking:
                    moduleColor = tok::kGold;
                    moduleState = L"校验中";
                    break;
                case unrealdbg_native::SymbolModuleState::Downloading:
                    moduleColor = tok::kAccent;
                    moduleState = H(std::to_wstring(module.progress) + L"%");
                    break;
                case unrealdbg_native::SymbolModuleState::Cached:
                    moduleColor = tok::kSuccess;
                    moduleState = L"缓存可用";
                    break;
                case unrealdbg_native::SymbolModuleState::Downloaded:
                    moduleColor = tok::kSuccess;
                    moduleState = L"已下载";
                    break;
                case unrealdbg_native::SymbolModuleState::Failed:
                    moduleColor = tok::kDanger;
                    moduleState = L"失败";
                    break;
                case unrealdbg_native::SymbolModuleState::Pending:
                default:
                    break;
                }

                hstring byteInfo = L"等待开始";
                if (module.state == unrealdbg_native::SymbolModuleState::Cached)
                {
                    byteInfo = H(ByteCountText(module.totalBytes) + L" · 本地缓存");
                }
                else if (module.state == unrealdbg_native::SymbolModuleState::Downloaded)
                {
                    byteInfo = H(ByteCountText(module.totalBytes) + L" · 已下载");
                }
                else if (module.totalBytes != 0)
                {
                    byteInfo = H(ByteCountText(module.downloadedBytes) +
                        L" / " + ByteCountText(module.totalBytes));
                }
                else if (module.downloadedBytes != 0)
                {
                    byteInfo = H(L"已下载 " +
                        ByteCountText(module.downloadedBytes) +
                        L" · 总大小未知");
                }

                Grid row;
                row.ColumnSpacing(8);
                row.Padding(Thickness{ 0, 5, 0, 5 });
                ColumnDefinition dotColumn;
                dotColumn.Width(GridLength{ 8 });
                row.ColumnDefinitions().Append(dotColumn);
                ColumnDefinition textColumn;
                textColumn.Width(GridLength{ 1, GridUnitType::Star });
                row.ColumnDefinitions().Append(textColumn);
                ColumnDefinition stateColumn;
                stateColumn.Width(GridLength{ 1, GridUnitType::Auto });
                row.ColumnDefinitions().Append(stateColumn);

                Border dot = tok::Dot(moduleColor, 7);
                dot.VerticalAlignment(VerticalAlignment::Center);
                row.Children().Append(dot);

                StackPanel text;
                text.Spacing(1);
                TextBlock name = tok::Text(H(module.moduleName),
                    tok::kFontCaption, tok::kTextPrimary, tok::kWeightMedium);
                TextBlock detail = tok::Text(H(module.detail),
                    tok::kFontMicro, tok::kTextMuted);
                detail.TextTrimming(TextTrimming::CharacterEllipsis);
                if (!module.pdbPath.empty())
                {
                    tok::SetToolTip(detail, H(module.pdbPath));
                }
                text.Children().Append(name);
                text.Children().Append(detail);
                ProgressBar moduleProgress;
                moduleProgress.Minimum(0);
                moduleProgress.Maximum(100);
                moduleProgress.Value(module.progress);
                moduleProgress.Height(3);
                moduleProgress.Margin(Thickness{ 0, 4, 0, 2 });
                moduleProgress.Foreground(tok::Brush(moduleColor));
                moduleProgress.Background(tok::Brush(tok::kSunken));
                moduleProgress.IsIndeterminate(
                    module.state ==
                        unrealdbg_native::SymbolModuleState::Downloading &&
                    module.totalBytes == 0);
                text.Children().Append(moduleProgress);
                TextBlock bytes = tok::Mono(byteInfo,
                    tok::kFontMicro, tok::kTextMuted);
                bytes.TextTrimming(TextTrimming::CharacterEllipsis);
                text.Children().Append(bytes);
                Grid::SetColumn(text, 1);
                row.Children().Append(text);

                TextBlock status = tok::Text(moduleState, tok::kFontMicro,
                    moduleColor, tok::kWeightMedium);
                status.VerticalAlignment(VerticalAlignment::Center);
                Grid::SetColumn(status, 2);
                row.Children().Append(status);
                symbolModulesList_.Children().Append(row);
            }
        }

        lastSymbolState_ = state;
        lastSymbolProgress_ = progress;
    }

    bool HookSupportedByCurrentSystem(const HookUiDescriptor& hook) const
    {
        return !controller_.System().windows11 || hook.windows11;
    }

    void RefreshHooks()
    {
        if (hookSummaryText_ == nullptr)
        {
            return;
        }

        const bool editable = controller_.HookConfigurationEditable();
        const std::uint64_t mask = controller_.HookSelectionMask();
        if (hookUiInitialized_ && mask == lastHookUiMask_ &&
            editable == lastHookUiEditable_)
        {
            return;
        }
        size_t supported = 0;
        size_t selected = 0;
        suppressHookEvents_ = true;
        for (const ToggleSwitch& toggle : hookToggles_)
        {
            if (toggle == nullptr)
            {
                continue;
            }
            const size_t index = static_cast<size_t>(
                unbox_value<int32_t>(toggle.Tag()));
            if (index >= kHookDescriptors.size())
            {
                continue;
            }
            const HookUiDescriptor& hook = kHookDescriptors[index];
            if (!HookSupportedByCurrentSystem(hook))
            {
                continue;
            }
            ++supported;
            const bool enabled = (mask & hook.mask) != 0;
            if (enabled)
            {
                ++selected;
            }
            toggle.IsOn(enabled);
            toggle.IsEnabled(editable);
            tok::SetToolTip(toggle, editable
                ? L"切换此 Hook；进入 VT 前有效"
                : L"已进入 VT 初始化流程，Hook 选择已锁定");
        }
        suppressHookEvents_ = false;

        hookSummaryText_.Text(H(std::to_wstring(selected) + L" / " +
            std::to_wstring(supported) + L" 已启用"));
        hookSummaryText_.Foreground(tok::Brush(editable
            ? tok::kAccent : tok::kTextMuted));
        hookLockText_.Text(editable
            ? L"仅在进入 VT 前可编辑；初始化开始后本页开关自动锁定。"
            : L"Hook 选择已锁定。本次 VT 会话中不能安装或取消驱动 Hook。");
        hookLockText_.Foreground(tok::Brush(editable
            ? tok::kGold : tok::kTextMuted));
        for (const Button& button : hookActionButtons_)
        {
            SetButtonEnabled(button, editable);
        }
        hookUiInitialized_ = true;
        lastHookUiMask_ = mask;
        lastHookUiEditable_ = editable;
    }

    void SetAllSupportedHooks(const bool enabled)
    {
        if (!controller_.HookConfigurationEditable())
        {
            RefreshHooks();
            return;
        }
        std::uint64_t mask = controller_.HookSelectionMask();
        for (const HookUiDescriptor& hook : kHookDescriptors)
        {
            if (!HookSupportedByCurrentSystem(hook))
            {
                continue;
            }
            if (enabled)
            {
                mask |= hook.mask;
            }
            else
            {
                mask &= ~hook.mask;
            }
        }
        if (!controller_.SetHookSelectionMask(mask))
        {
            unrealdbg_native::WriteLog(unrealdbg_native::LogLevel::Error,
                L"更新驱动 Hook 选择失败");
        }
        RefreshHooks();
    }

    void OnHookToggleChanged(const IInspectable& sender,
        const RoutedEventArgs&)
    {
        if (suppressHookEvents_)
        {
            return;
        }
        const ToggleSwitch toggle = sender.try_as<ToggleSwitch>();
        if (toggle == nullptr)
        {
            return;
        }
        const size_t index = static_cast<size_t>(
            unbox_value<int32_t>(toggle.Tag()));
        if (index >= kHookDescriptors.size() ||
            !HookSupportedByCurrentSystem(kHookDescriptors[index]))
        {
            RefreshHooks();
            return;
        }

        std::uint64_t mask = controller_.HookSelectionMask();
        if (toggle.IsOn())
        {
            mask |= kHookDescriptors[index].mask;
        }
        else
        {
            mask &= ~kHookDescriptors[index].mask;
        }
        if (!controller_.SetHookSelectionMask(mask))
        {
            unrealdbg_native::WriteLog(unrealdbg_native::LogLevel::Error,
                L"Hook 开关修改未生效");
        }
        RefreshHooks();
    }

    void RefreshLogView(const bool force)
    {
        if (externalLogCombo_ == nullptr || externalLogText_ == nullptr)
        {
            return;
        }

        const ULONGLONG now = ::GetTickCount64();
        if (force || externalLogFiles_.empty() ||
            now - lastExternalLogSourceRefreshTick_ >= 2000)
        {
            externalLogFiles_ = controller_.ExternalLogs();
            lastExternalLogSourceRefreshTick_ = now;
        }
        if (externalLogIndex_ < 0)
        {
            int preferred = 0;
            for (size_t index = 0; index < externalLogFiles_.size(); ++index)
            {
                if (externalLogFiles_[index].currentSystem &&
                    externalLogFiles_[index].available)
                {
                    preferred = static_cast<int>(index) + 1;
                    break;
                }
                if (externalLogFiles_[index].available)
                {
                    preferred = static_cast<int>(index) + 1;
                }
            }
            suppressExternalLogSelection_ = true;
            externalLogIndex_ = preferred;
            externalLogCombo_.SelectedIndex(preferred);
            suppressExternalLogSelection_ = false;
        }

        if (externalLogIndex_ >= 0 &&
            externalLogCombo_.SelectedIndex() != externalLogIndex_)
        {
            suppressExternalLogSelection_ = true;
            externalLogCombo_.SelectedIndex(externalLogIndex_);
            suppressExternalLogSelection_ = false;
        }

        if (externalLogIndex_ < 0 ||
            externalLogIndex_ > static_cast<int>(externalLogFiles_.size()))
        {
            externalLogStatusText_.Text(L"没有可选择的日志文件");
            externalLogText_.Text(L"");
            return;
        }

        if (currentPage_ != PageId::Logs && !force)
        {
            return;
        }

        if (externalLogIndex_ == 0)
        {
            const std::uint32_t count = unrealdbg_native::LogCount();
            if (!force && count == lastInternalLogCount_)
            {
                return;
            }
            lastInternalLogCount_ = count;

            std::wstring text;
            text.reserve(logHistory_.size() * 96);
            for (const auto& record : logHistory_)
            {
                text += record.text;
            }
            if (text.empty())
            {
                text = L"当前会话暂无日志。";
            }
            externalLogText_.Text(H(text));
            externalLogPathText_.Text(H(unrealdbg_native::JoinPath(
                unrealdbg_native::JoinPath(
                    controller_.ApplicationDirectory(), L"Log"),
                L"WinUI.log")));
            externalLogStatusText_.Text(H(L"当前会话 · " +
                std::to_wstring(logHistory_.size()) + L" 条"));
            externalLogStatusText_.Foreground(tok::Brush(tok::kSuccess));
            return;
        }

        const size_t externalIndex =
            static_cast<size_t>(externalLogIndex_ - 1);
        const auto& file = externalLogFiles_[
            externalIndex];
        externalLogPathText_.Text(H(file.path));

        WIN32_FILE_ATTRIBUTE_DATA attributes{};
        const bool present = ::GetFileAttributesExW(file.path.c_str(),
            GetFileExInfoStandard, &attributes) != FALSE;
        const std::uint64_t stamp = present
            ? (static_cast<std::uint64_t>(attributes.ftLastWriteTime.dwHighDateTime) << 32) |
                attributes.ftLastWriteTime.dwLowDateTime
            : 0;
        const std::uint64_t size = present
            ? (static_cast<std::uint64_t>(attributes.nFileSizeHigh) << 32) |
                attributes.nFileSizeLow
            : 0;
        if (!force && stamp == lastExternalLogStamp_ &&
            size == lastExternalLogSize_)
        {
            return;
        }
        lastExternalLogStamp_ = stamp;
        lastExternalLogSize_ = size;

        if (!present)
        {
            externalLogText_.Text(L"日志尚未生成。\r\n" +
                std::wstring(file.path));
            externalLogStatusText_.Text(L"尚未生成");
            externalLogStatusText_.Foreground(tok::Brush(tok::kTextMuted));
            return;
        }

        std::wstring text;
        std::wstring error;
        const bool read = controller_.ReadLogTail(
            file.path, 512u * 1024u, text, error);
        externalLogText_.Text(read ? H(text) : H(error));
        externalLogStatusText_.Text(H((read ? L"实时读取 · " : L"读取失败 · ") +
            ByteCountText(size)));
        externalLogStatusText_.Foreground(tok::Brush(read
            ? tok::kSuccess : tok::kDanger));
    }

    void OnExternalLogSelectionChanged(const IInspectable&,
        const SelectionChangedEventArgs&)
    {
        if (suppressExternalLogSelection_ || externalLogCombo_ == nullptr)
        {
            return;
        }
        externalLogIndex_ = externalLogCombo_.SelectedIndex();
        lastExternalLogStamp_ = 0;
        lastExternalLogSize_ = 0;
        RefreshLogView(true);
    }

    void RefreshSystemPage()
    {
        if (systemDetailsBody_ == nullptr || !systemPageDirty_)
        {
            return;
        }
        systemDetailsBody_.Children().Clear();

        std::vector<std::wstring> groups;
        for (const auto& item : controller_.SystemDetails())
        {
            if (std::find(groups.begin(), groups.end(), item.group) ==
                groups.end())
            {
                groups.push_back(item.group);
            }
        }

        for (const std::wstring& group : groups)
        {
            Border surface = MakeSurface(tok::kRadiusLg, 14);
            StackPanel body;
            body.Spacing(0);
            body.Children().Append(MakeSectionHeading(H(group)));
            body.Children().Append(MakeDivider(tok::kBorder));
            for (const auto& item : controller_.SystemDetails())
            {
                if (item.group != group)
                {
                    continue;
                }

                Grid row;
                row.Padding(Thickness{ 0, 7, 0, 7 });
                ColumnDefinition labelColumn;
                labelColumn.Width(GridLength{ 138 });
                row.ColumnDefinitions().Append(labelColumn);
                ColumnDefinition valueColumn;
                valueColumn.Width(GridLength{ 1, GridUnitType::Star });
                row.ColumnDefinitions().Append(valueColumn);
                TextBlock label = tok::Text(H(item.label + L"："),
                    tok::kFontCaption,
                    tok::kTextSecondary, tok::kWeightMedium);
                row.Children().Append(label);
                TextBlock value = tok::Mono(H(item.value),
                    tok::kFontMicro, tok::kTextPrimary);
                value.Margin(Thickness{ 14, 0, 0, 0 });
                value.TextWrapping(TextWrapping::Wrap);
                value.IsTextSelectionEnabled(true);
                Grid::SetColumn(value, 1);
                row.Children().Append(value);
                body.Children().Append(row);
                body.Children().Append(MakeDivider(tok::kBorder));
            }
            surface.Child(body);
            systemDetailsBody_.Children().Append(surface);
        }
        systemPageDirty_ = false;
    }

    void StartSymbolAnalysis()
    {
        auto& symbols = controller_.Symbols();
        if (symbols.AnalysisState() ==
            unrealdbg_native::SymbolAnalysisState::Analyzing)
        {
            return;
        }
        if (customSymbolPdbPath_.empty() && symbols.IsPreparing())
        {
            symbolAnalysisStartPending_ = true;
            RefreshOffsetAnalysis(true);
            return;
        }
        symbolAnalysisStartPending_ = false;
        symbolAnalysisConfigurationDirty_ = false;
        symbolEntriesCache_.reset();
        lastAnalysisState_ = unrealdbg_native::SymbolAnalysisState::Idle;
        symbolPageDirty_ = true;
        symbols.AnalyzeSystemSymbolsAsync(customSymbolPdbPath_);
        RefreshOffsetAnalysis(true);
    }

    void StartPendingSymbolAnalysis()
    {
        if (!symbolAnalysisStartPending_)
        {
            return;
        }
        const auto preparationState = controller_.Symbols().State();
        if (preparationState ==
            unrealdbg_native::SymbolPreparationState::Ready)
        {
            StartSymbolAnalysis();
        }
        else if (preparationState ==
            unrealdbg_native::SymbolPreparationState::Failed)
        {
            symbolAnalysisStartPending_ = false;
            RefreshOffsetAnalysis(true);
        }
    }

    void OnChooseSymbolPdbClicked()
    {
        std::wstring path;
        if (!PickPdb(hwnd_, path))
        {
            return;
        }
        customSymbolPdbPath_ = std::move(path);
        symbolAnalysisConfigurationDirty_ = true;
        symbolAnalysisStartPending_ = false;
        RefreshOffsetAnalysis(true);
    }

    void OnSymbolSearchChanged(const IInspectable& sender,
        const TextChangedEventArgs&)
    {
        const TextBox box = sender.try_as<TextBox>();
        if (box == nullptr)
        {
            return;
        }
        symbolSearchText_ = box.Text().c_str();
        symbolSearchDirty_ = true;
        symbolSearchDeadline_ = ::GetTickCount64() + 250ULL;
        UpdatePollingInterval();
    }

    void OnSymbolPdbFilterChanged(const IInspectable&,
        const SelectionChangedEventArgs&)
    {
        if (suppressSymbolFilterEvents_)
        {
            return;
        }
        symbolPageDirty_ = true;
        RefreshOffsetAnalysis(true);
    }

    void OnSymbolUsedOnlyToggled(const IInspectable& sender,
        const RoutedEventArgs&)
    {
        if (suppressSymbolFilterEvents_)
        {
            return;
        }
        const ToggleSwitch toggle = sender.try_as<ToggleSwitch>();
        symbolUsedOnlyFilter_ = toggle != nullptr && toggle.IsOn();
        symbolPageDirty_ = true;
        RefreshOffsetAnalysis(true);
    }

    void OnSymbolKindFilterChanged(const IInspectable&,
        const SelectionChangedEventArgs&)
    {
        if (suppressSymbolFilterEvents_)
        {
            return;
        }
        symbolPageDirty_ = true;
        RefreshOffsetAnalysis(true);
    }

    void OnSymbolInternalToggled(const IInspectable& sender,
        const RoutedEventArgs&)
    {
        if (suppressSymbolFilterEvents_)
        {
            return;
        }
        const ToggleSwitch toggle = sender.try_as<ToggleSwitch>();
        symbolShowInternal_ = toggle != nullptr && toggle.IsOn();
        symbolPageDirty_ = true;
        RefreshOffsetAnalysis(true);
    }

    void PopulateSymbolRow(const Grid& row,
        const unrealdbg_native::SystemSymbolEntry& symbolEntry)
    {
        TextBlock module{ nullptr };
        TextBlock symbol{ nullptr };
        TextBlock kind{ nullptr };
        TextBlock rva{ nullptr };
        TextBlock address{ nullptr };
        TextBlock usage{ nullptr };
        if (row.Children().Size() == 7)
        {
            module = row.Children().GetAt(0).try_as<TextBlock>();
            symbol = row.Children().GetAt(1).try_as<TextBlock>();
            kind = row.Children().GetAt(2).try_as<TextBlock>();
            rva = row.Children().GetAt(3).try_as<TextBlock>();
            address = row.Children().GetAt(4).try_as<TextBlock>();
            usage = row.Children().GetAt(5).try_as<TextBlock>();
        }
        if (module == nullptr || symbol == nullptr || kind == nullptr ||
            rva == nullptr || address == nullptr || usage == nullptr)
        {
            row.Children().Clear();

            module = tok::Mono(L"", tok::kFontMicro, tok::kTextMuted);
            module.TextTrimming(TextTrimming::CharacterEllipsis);
            row.Children().Append(module);

            symbol = tok::Mono(L"", tok::kFontMicro, tok::kTextPrimary);
            symbol.IsTextSelectionEnabled(true);
            symbol.TextTrimming(TextTrimming::CharacterEllipsis);
            Grid::SetColumn(symbol, 1);
            row.Children().Append(symbol);

            kind = tok::Text(L"", tok::kFontMicro, tok::kTextSecondary);
            Grid::SetColumn(kind, 2);
            row.Children().Append(kind);

            rva = tok::Mono(L"", tok::kFontMicro, tok::kTextSecondary);
            Grid::SetColumn(rva, 3);
            row.Children().Append(rva);

            address = tok::Mono(L"", tok::kFontMicro, tok::kTextMuted);
            Grid::SetColumn(address, 4);
            row.Children().Append(address);

            usage = tok::Text(L"", tok::kFontMicro, tok::kTextMuted);
            usage.TextTrimming(TextTrimming::CharacterEllipsis);
            Grid::SetColumn(usage, 5);
            row.Children().Append(usage);

            Border divider;
            divider.Height(1);
            divider.Background(tok::Brush(tok::kBorder));
            divider.VerticalAlignment(VerticalAlignment::Bottom);
            divider.IsHitTestVisible(false);
            Grid::SetColumnSpan(divider, 6);
            row.Children().Append(divider);
        }

        const auto displayName = symbolModuleDisplayNames_.find(
            symbolEntry.moduleName);
        module.Text(displayName == symbolModuleDisplayNames_.end()
            ? H(ShortPdbName(symbolEntry.moduleName))
            : H(displayName->second));
        symbol.Text(H(symbolEntry.symbolName));
        kind.Text(H(symbolEntry.kind));
        kind.Foreground(tok::Brush(symbolEntry.offset
            ? tok::kAccentText : tok::kTextSecondary));
        rva.Text(H(HexAddress(symbolEntry.rva)));
        address.Text(symbolEntry.addressValid
            ? H(HexAddress(symbolEntry.address)) : hstring(L"—"));
        address.Foreground(tok::Brush(symbolEntry.addressValid
            ? tok::kSuccess : tok::kTextMuted));
        usage.Text(H(symbolEntry.description));
        usage.Foreground(tok::Brush(symbolEntry.usedByProject
            ? tok::kAccentText : tok::kTextMuted));
        const hstring description = H(symbolEntry.description);
        const auto tooltip = ToolTipService::GetToolTip(usage);
        if (tooltip != nullptr)
        {
            ToolTip tip = tooltip.try_as<ToolTip>();
            if (tip != nullptr)
            {
                tip.Content(box_value(description));
            }
            else
            {
                tok::SetToolTip(usage, description);
            }
        }
        else
        {
            tok::SetToolTip(usage, description);
        }
    }

    Grid MakeSymbolRow(const unrealdbg_native::SystemSymbolEntry& symbolEntry)
    {
        Grid row;
        row.Padding(Thickness{ 0, 7, 0, 8 });
        row.ColumnSpacing(10);
        const std::array<double, 6> widths = {
            1.1, 2.4, 0.8, 0.95, 1.3, 2.1
        };
        for (const double width : widths)
        {
            ColumnDefinition column;
            column.Width(GridLength{ width, GridUnitType::Star });
            row.ColumnDefinitions().Append(column);
        }
        PopulateSymbolRow(row, symbolEntry);
        return row;
    }

    void CancelSymbolRowRendering()
    {
        ++symbolRenderGeneration_;
        symbolRenderQueue_.clear();
        symbolRenderIndex_ = 0;
        symbolRenderingComplete_ = true;
        if (offsetRepeater_ != nullptr)
        {
            offsetRepeater_.ItemsSource(nullptr);
        }
        symbolItemsSource_ = nullptr;
        if (symbolRowFactory_ != nullptr)
        {
            symbolRowFactory_->Reset();
        }
    }

    void SetSymbolRowSource(const size_t count)
    {
        if (offsetRepeater_ == nullptr)
        {
            return;
        }
        std::vector<IInspectable> items;
        items.reserve(count);
        const std::uint64_t generation =
            static_cast<std::uint32_t>(symbolRenderGeneration_);
        for (size_t index = 0; index < count; ++index)
        {
            items.push_back(box_value(
                (generation << 32) |
                static_cast<std::uint64_t>(index)));
        }
        symbolItemsSource_ =
            winrt::single_threaded_vector<IInspectable>(std::move(items));
        offsetRepeater_.ItemsSource(symbolItemsSource_);
        symbolRenderIndex_ = count;
        symbolRenderingComplete_ = true;
    }

    void RefreshOffsetAnalysis(const bool force = false)
    {
        if (offsetRepeater_ == nullptr || offsetSummaryText_ == nullptr ||
            offsetEmptyText_ == nullptr)
        {
            return;
        }
        auto& symbols = controller_.Symbols();
        const auto state = symbols.AnalysisState();
        const bool analysisCompleted = state != lastAnalysisState_ &&
            state == unrealdbg_native::SymbolAnalysisState::Ready;
        if (!force && !symbolPageDirty_ && !analysisCompleted)
        {
            return;
        }
        const bool analyzing =
            state == unrealdbg_native::SymbolAnalysisState::Analyzing;
        const bool waitingForPreparation =
            symbolAnalysisStartPending_;
        SetButtonEnabled(initializeOffsetsButton_,
            !analyzing && !waitingForPreparation);
        SetButtonEnabled(chooseSymbolPdbButton_, !analyzing);
        SetButtonEnabled(resetSymbolPdbButton_, !analyzing);
        SetButtonCaption(initializeOffsetsButton_,
            waitingForPreparation ? L"等待符号准备" :
            analyzing ? L"正在初始化" :
            state == unrealdbg_native::SymbolAnalysisState::Failed
                ? L"重试初始化" :
            state == unrealdbg_native::SymbolAnalysisState::Ready &&
                !symbolAnalysisConfigurationDirty_
                ? L"重新初始化" : L"初始化");

        hstring summary = L"等待系统符号准备";
        Color color = tok::kGold;
        switch (state)
        {
        case unrealdbg_native::SymbolAnalysisState::Analyzing:
            summary = customSymbolPdbPath_.empty()
                ? L"正在递归解析 C:\\Symbols 下的全部 PDB…"
                : L"正在解析自定义 PDB…";
            color = tok::kAccent;
            break;
        case unrealdbg_native::SymbolAnalysisState::Ready:
            if (symbolAnalysisConfigurationDirty_)
            {
                summary = L"PDB 配置已更改，等待初始化";
                color = tok::kGold;
            }
            else
            {
                summary = H(symbols.AnalysisSummary());
                color = symbols.AnalysisError().empty()
                    ? tok::kSuccess : tok::kGold;
            }
            break;
        case unrealdbg_native::SymbolAnalysisState::Failed:
            summary = H(symbols.AnalysisError());
            color = tok::kDanger;
            break;
        case unrealdbg_native::SymbolAnalysisState::Idle:
        default:
            summary = waitingForPreparation
                ? L"等待系统符号准备" : L"尚未初始化";
            break;
        }
        offsetSummaryText_.Text(summary);
        offsetSummaryText_.Foreground(tok::Brush(color));
        const std::wstring analysisError = symbols.AnalysisError();
        const std::wstring hintText =
            symbolAnalysisConfigurationDirty_
                ? customSymbolPdbPath_.empty()
                    ? L"系统 PDB · 等待初始化"
                    : L"自定义 PDB · 等待初始化：" + customSymbolPdbPath_
                : state == unrealdbg_native::SymbolAnalysisState::Ready &&
                !analysisError.empty()
                ? L"部分 PDB 未能解析：" + analysisError
                : customSymbolPdbPath_.empty()
                    ? L"默认解析 C:\\Symbols 下全部 PDB；可切换单 PDB、搜索或加载自定义 PDB。"
                    : L"当前使用自定义 PDB：" + customSymbolPdbPath_;
        symbolAnalysisHint_.Text(H(hintText));
        symbolAnalysisHint_.Foreground(tok::Brush(
            state == unrealdbg_native::SymbolAnalysisState::Ready &&
                !analysisError.empty()
                ? tok::kGold : tok::kTextMuted));

        const std::wstring runtimePdb = symbols.AnalysisRuntimePdb();
        const std::wstring moduleBase = symbols.AnalysisModuleBase();
        offsetRuntimePdbText_.Text(runtimePdb.empty()
            ? L"解析来源：—" : H(L"解析来源：" + runtimePdb));
        offsetModuleBaseText_.Text(moduleBase.empty()
            ? L"内核基址：—" : H(L"内核基址：" + moduleBase));
        tok::SetToolTip(offsetRuntimePdbText_,
            runtimePdb.empty() ? L"尚未解析" : H(runtimePdb));
        tok::SetToolTip(offsetModuleBaseText_,
            moduleBase.empty() ? L"尚未解析" : H(moduleBase));

        if (analysisCompleted)
        {
            symbolEntriesCache_ = symbols.SystemSymbols();
        }
        if (state != lastAnalysisState_ &&
            state == unrealdbg_native::SymbolAnalysisState::Ready)
        {
            symbolModuleFilterNames_.clear();
        }
        if (state != unrealdbg_native::SymbolAnalysisState::Ready)
        {
            symbolEntriesCache_.reset();
        }
        if (!force && !symbolPageDirty_ && !analysisCompleted)
        {
            return;
        }
        lastAnalysisState_ = state;
        CancelSymbolRowRendering();
        offsetEmptyText_.Visibility(Visibility::Collapsed);
        if (state != unrealdbg_native::SymbolAnalysisState::Ready)
        {
            offsetEmptyText_.Text(
                state == unrealdbg_native::SymbolAnalysisState::Analyzing
                    ? L"正在读取全部 PDB 的符号与函数地址…"
                    : waitingForPreparation
                        ? L"等待系统符号准备完成…"
                        : state == unrealdbg_native::SymbolAnalysisState::Failed
                            ? L"初始化失败，请检查错误后重试。"
                            : L"尚未初始化。点击“初始化”获取全部符号偏移。");
            offsetEmptyText_.Visibility(Visibility::Visible);
            symbolResultText_.Text(L"");
            symbolPageDirty_ = false;
            return;
        }
        if (symbolEntriesCache_ == nullptr)
        {
            offsetEmptyText_.Text(L"符号结果为空。");
            offsetEmptyText_.Visibility(Visibility::Visible);
            symbolResultText_.Text(L"");
            symbolPageDirty_ = false;
            return;
        }

        if (analysisCompleted || symbolModuleFilterNames_.empty())
        {
            symbolModuleFilterNames_.clear();
            for (const auto& entry : *symbolEntriesCache_)
            {
                if (std::find(symbolModuleFilterNames_.begin(),
                    symbolModuleFilterNames_.end(), entry.moduleName) ==
                    symbolModuleFilterNames_.end())
                {
                    symbolModuleFilterNames_.push_back(entry.moduleName);
                }
            }
            std::sort(symbolModuleFilterNames_.begin(),
                symbolModuleFilterNames_.end());
            symbolModuleDisplayNames_.clear();
            symbolModuleDisplayNames_.reserve(
                symbolModuleFilterNames_.size());
            for (const auto& moduleName : symbolModuleFilterNames_)
            {
                symbolModuleDisplayNames_.emplace(
                    moduleName, ShortPdbName(moduleName));
            }
        }
        if (offsetPdbFilter_ == nullptr ||
            offsetPdbFilter_.Items().Size() !=
                symbolModuleFilterNames_.size() + 1)
        {
            std::wstring previous;
            if (offsetPdbFilter_ != nullptr &&
                offsetPdbFilter_.SelectedIndex() > 0)
            {
                const int previousIndex = offsetPdbFilter_.SelectedIndex() - 1;
                if (previousIndex < static_cast<int>(
                    symbolModuleFilterNames_.size()))
                {
                    previous = symbolModuleFilterNames_[
                        static_cast<size_t>(previousIndex)];
                }
            }
            suppressSymbolFilterEvents_ = true;
            offsetPdbFilter_.Items().Clear();
            ComboBoxItem allItem;
            allItem.Content(box_value(hstring(L"全部 PDB")));
            offsetPdbFilter_.Items().Append(allItem);
            int selection = 0;
            for (size_t index = 0;
                index < symbolModuleFilterNames_.size(); ++index)
            {
                ComboBoxItem item;
                item.Content(box_value(H(ShortPdbName(
                    symbolModuleFilterNames_[index]))));
                offsetPdbFilter_.Items().Append(item);
                if (symbolModuleFilterNames_[index] == previous)
                {
                    selection = static_cast<int>(index) + 1;
                }
            }
            offsetPdbFilter_.SelectedIndex(selection);
            suppressSymbolFilterEvents_ = false;
        }

        std::wstring selectedModule;
        if (offsetPdbFilter_.SelectedIndex() > 0)
        {
            const size_t index = static_cast<size_t>(
                offsetPdbFilter_.SelectedIndex() - 1);
            if (index < symbolModuleFilterNames_.size())
            {
                selectedModule = symbolModuleFilterNames_[index];
            }
        }
        const std::wstring search = LowercaseText(symbolSearchText_);
        const int kindFilter = offsetKindFilter_ == nullptr
            ? 0 : offsetKindFilter_.SelectedIndex();
        constexpr size_t kDisplayLimit = 2000;
        symbolRenderQueue_.reserve(kDisplayLimit);
        size_t matched = 0;
        size_t displayed = 0;
        for (const auto& symbolEntry : *symbolEntriesCache_)
        {
            if (!symbolShowInternal_ && symbolEntry.internalSymbol)
            {
                continue;
            }
            if (!selectedModule.empty() &&
                symbolEntry.moduleName != selectedModule)
            {
                continue;
            }
            const bool kindMatches =
                kindFilter <= 0 ||
                (kindFilter == 1 && symbolEntry.kind == L"结构字段") ||
                (kindFilter == 2 && symbolEntry.kind == L"函数") ||
                (kindFilter == 3 && symbolEntry.kind == L"变量") ||
                (kindFilter == 4 && symbolEntry.kind == L"符号");
            if (!kindMatches)
            {
                continue;
            }
            if (symbolUsedOnlyFilter_ && !symbolEntry.usedByProject)
            {
                continue;
            }
            if (!search.empty())
            {
                if (!ContainsInsensitive(
                        symbolEntry.moduleName, search) &&
                    !ContainsInsensitive(
                        symbolEntry.symbolName, search) &&
                    !ContainsInsensitive(
                        symbolEntry.description, search))
                {
                    continue;
                }
            }
            ++matched;
            if (displayed >= kDisplayLimit)
            {
                continue;
            }
            ++displayed;
            symbolRenderQueue_.push_back(&symbolEntry);
        }
        if (matched == 0)
        {
            offsetEmptyText_.Text(L"没有匹配的符号。");
            offsetEmptyText_.Visibility(Visibility::Visible);
        }
        else
        {
            SetSymbolRowSource(displayed);
        }
        symbolPageDirty_ = false;
        symbolResultText_.Text(H(L"匹配 " + std::to_wstring(matched) +
            L" · 显示 " + std::to_wstring(displayed)));
        tok::SetToolTip(symbolResultText_, displayed < matched
            ? L"结果过多，仅显示前 2000 条；请输入关键字缩小范围"
            : L"全部匹配结果已显示");
    }

    void RefreshDiagnostics()
    {
        if (diagnosticsList_ == nullptr)
        {
            return;
        }
        diagnosticsList_.Children().Clear();
        for (const auto& file : controller_.RuntimeFiles())
        {
            diagnosticsList_.Children().Append(ui::MakeCheckRow(
                H(file.name), H(file.path), file.present, nullptr));
        }
        if (diagBackendText_ != nullptr)
        {
            switch (controller_.State())
            {
            case unrealdbg_native::BackendState::Missing:
                diagBackendText_.Text(L"缺失");
                break;
            case unrealdbg_native::BackendState::Ready:
                diagBackendText_.Text(L"已连接 / 等待初始化");
                break;
            case unrealdbg_native::BackendState::Initializing:
                diagBackendText_.Text(L"初始化中");
                break;
            case unrealdbg_native::BackendState::Initialized:
                diagBackendText_.Text(L"已进入 VT");
                break;
            case unrealdbg_native::BackendState::Failed:
                diagBackendText_.Text(L"初始化失败");
                break;
            }
        }
        if (diagRuntimePathText_ != nullptr)
        {
            diagRuntimePathText_.Text(H(controller_.ApplicationDirectory()));
        }
        if (diagBackendPathText_ != nullptr)
        {
            diagBackendPathText_.Text(controller_.BackendPath().empty()
                ? L"UnrealDbgDll.dll 未找到" : H(controller_.BackendPath()));
        }
    }

    void UpdateActionAvailability()
    {
        const auto& entries = controller_.Debuggers();
        const bool haveEntries = !entries.empty();
        const bool initialized = controller_.IsInitialized();
        SetButtonEnabled(startDebuggerButton_,
            initialized && haveEntries && debuggerSelectedIndex_ >= 0);
        SetButtonEnabled(deleteDebuggerButton_,
            haveEntries && debuggerSelectedIndex_ >= 0);
        SetButtonEnabled(overviewStartButton_,
            initialized && haveEntries && overviewSelectedIndex_ >= 0);
        SetButtonEnabled(blockResumeButton_,
            controller_.Target().processId != 0 &&
            controller_.TlEnabled() && controller_.TlBlockResumeThread());
        SetButtonEnabled(captureTargetButton_, captureDeadline_ == 0);
    }

    void ApplyNavSelectionState(const PageId page, const bool selected)
    {
        const int index = static_cast<int>(page);
        if (index < 0 || index >= kPageCount)
        {
            return;
        }
        Border marker = navMarkers_[static_cast<size_t>(index)];
        TextBlock text = navTexts_[static_cast<size_t>(index)];
        FontIcon icon = navIcons_[static_cast<size_t>(index)];
        Button item = navButtons_[static_cast<size_t>(index)];
        if (marker != nullptr)
        {
            marker.Visibility(selected
                ? Visibility::Visible : Visibility::Collapsed);
        }
        if (text != nullptr)
        {
            text.Foreground(tok::Brush(selected
                ? tok::kTextPrimary : tok::kTextSecondary));
            text.FontWeight(selected ? tok::kWeightSemiBold : tok::kWeightMedium);
        }
        if (icon != nullptr)
        {
            icon.Foreground(tok::Brush(selected
                ? tok::kAccent : tok::kTextMuted));
        }
        if (item != nullptr)
        {
            item.Background(tok::Brush(selected
                ? tok::kCard : tok::Rgb(0, 0, 0, 0)));
            item.BorderBrush(tok::Brush(selected
                ? tok::kBorderStrong : tok::Rgb(0, 0, 0, 0)));
        }
    }

    void RefreshNavSelection(
        const PageId previous, const PageId selected)
    {
        if (!navSelectionInitialized_)
        {
            for (int index = 0; index < kPageCount; ++index)
            {
                ApplyNavSelectionState(static_cast<PageId>(index),
                    index == static_cast<int>(selected));
            }
            navSelectionInitialized_ = true;
            return;
        }
        if (previous != selected)
        {
            ApplyNavSelectionState(previous, false);
            ApplyNavSelectionState(selected, true);
        }
    }

    void SelectPage(const PageId page)
    {
        const PageId previous = currentPage_;
        currentPage_ = page;
        if (!navSelectionInitialized_)
        {
            for (int index = 0; index < kPageCount; ++index)
            {
                if (pages_[static_cast<size_t>(index)] != nullptr)
                {
                    pages_[static_cast<size_t>(index)].Visibility(
                        index == static_cast<int>(page)
                        ? Visibility::Visible : Visibility::Collapsed);
                }
            }
        }
        else if (previous != page)
        {
            if (pages_[static_cast<size_t>(previous)] != nullptr)
            {
                pages_[static_cast<size_t>(previous)].Visibility(
                    Visibility::Collapsed);
            }
            if (pages_[static_cast<size_t>(page)] != nullptr)
            {
                pages_[static_cast<size_t>(page)].Visibility(
                    Visibility::Visible);
            }
        }
        RefreshNavSelection(previous, page);
        if (page == PageId::Hooks)
        {
            RefreshHooks();
        }
        else if (page == PageId::Logs)
        {
            RefreshLogView(false);
        }
        else if (page == PageId::System)
        {
            RefreshSystemPage();
        }
        else if (page == PageId::Symbols)
        {
            RefreshOffsetAnalysis();
        }
        UpdatePollingInterval();
    }

    void AppendPendingLogs()
    {
        std::vector<unrealdbg_native::LogRecord> records;
        unrealdbg_native::DrainLogs(records);
        if (!records.empty())
        {
            logHistory_.insert(logHistory_.end(),
                records.begin(), records.end());
            constexpr size_t kMaximumHistory = 3000;
            if (logHistory_.size() > kMaximumHistory)
            {
                logHistory_.erase(logHistory_.begin(),
                    logHistory_.begin() +
                        static_cast<std::ptrdiff_t>(
                            logHistory_.size() - kMaximumHistory));
            }
        }
        if (logStatText_ != nullptr)
        {
            logStatText_.Text(H(std::to_wstring(unrealdbg_native::LogCount())));
        }
    }

    void UpdatePollingInterval()
    {
        if (timer_ == nullptr)
        {
            return;
        }
        const bool busy =
            pendingInitialization_ ||
            symbolAnalysisStartPending_ ||
            captureDeadline_ != 0 ||
            symbolSearchDirty_ ||
            controller_.State() ==
                unrealdbg_native::BackendState::Initializing ||
            controller_.Symbols().IsPreparing() ||
            controller_.Symbols().AnalysisState() ==
                unrealdbg_native::SymbolAnalysisState::Analyzing;
        const int interval = busy
            ? 100
            : (currentPage_ == PageId::Logs &&
                externalLogAutoRefresh_ != nullptr &&
                externalLogAutoRefresh_.IsOn()
                ? 250 : 500);
        if (interval == currentPollingIntervalMs_)
        {
            return;
        }
        currentPollingIntervalMs_ = interval;
        timer_.Interval(std::chrono::milliseconds(interval));
    }

    void OnTimer(const IInspectable&, const IInspectable&)
    {
        if (captureDeadline_ != 0)
        {
            const ULONGLONG now = ::GetTickCount64();
            if (now >= captureDeadline_)
            {
                captureDeadline_ = 0;
                controller_.CaptureForegroundTarget(hwnd_);
                RefreshTarget();
                SetButtonCaption(captureTargetButton_, L"捕获前台进程");
            }
            else
            {
                const ULONGLONG seconds = (captureDeadline_ - now + 999) / 1000;
                SetButtonCaption(captureTargetButton_,
                    H(std::to_wstring(seconds) + L" 秒后捕获…"));
            }
        }

        const unrealdbg_native::BackendState state = controller_.State();
        bool refreshStatus = false;
        if (state != lastState_)
        {
            lastState_ = state;
            refreshStatus = true;
        }

        const auto symbolState = controller_.Symbols().State();
        const int symbolProgress = controller_.Symbols().Progress();
        if (symbolState != lastSymbolState_ ||
            symbolProgress != lastSymbolProgress_)
        {
            RefreshSymbols();
            refreshStatus = true;
            StartPendingSymbolAnalysis();
        }

        const auto analysisState = controller_.Symbols().AnalysisState();
        if (analysisState != lastAnalysisState_)
        {
            symbolPageDirty_ = true;
            RefreshOffsetAnalysis();
        }

        if (pendingInitialization_)
        {
            if (symbolState == unrealdbg_native::SymbolPreparationState::Ready)
            {
                pendingInitialization_ = false;
                if (state == unrealdbg_native::BackendState::Ready ||
                    state == unrealdbg_native::BackendState::Failed)
                {
                    controller_.BeginInitialize();
                    RefreshHooks();
                }
                refreshStatus = true;
            }
            else if (symbolState == unrealdbg_native::SymbolPreparationState::Failed)
            {
                pendingInitialization_ = false;
                unrealdbg_native::WriteLog(unrealdbg_native::LogLevel::Error,
                    L"符号准备失败，已取消进入 VT 调试模式；请检查后重试");
                refreshStatus = true;
            }
        }

        if (refreshStatus)
        {
            RefreshStatus();
            RefreshDiagnostics();
            UpdateActionAvailability();
            RefreshHooks();
        }

        if (currentPage_ == PageId::Logs &&
            externalLogAutoRefresh_ != nullptr &&
            externalLogAutoRefresh_.IsOn())
        {
            const ULONGLONG now = ::GetTickCount64();
            if (now - lastExternalLogRefreshTick_ >= 1000)
            {
                lastExternalLogRefreshTick_ = now;
                RefreshLogView(false);
            }
        }
        if (currentPage_ == PageId::Symbols && symbolSearchDirty_ &&
            ::GetTickCount64() >= symbolSearchDeadline_)
        {
            symbolSearchDirty_ = false;
            symbolPageDirty_ = true;
            RefreshOffsetAnalysis(true);
        }

        AppendPendingLogs();
        if (controller_.System().clockText != lastClock_)
        {
            RefreshSystem();
            lastClock_ = controller_.System().clockText;
        }
        UpdatePollingInterval();
    }

    void OnClosed(const IInspectable&, const WindowEventArgs&)
    {
        if (timer_ != nullptr)
        {
            timer_.Stop();
        }
        if (smokeTimer_ != nullptr)
        {
            smokeTimer_.Stop();
        }
        if (themeTestTimer_ != nullptr)
        {
            themeTestTimer_.Stop();
        }
        if (symbolTestTimer_ != nullptr)
        {
            symbolTestTimer_.Stop();
        }
        if (navigationTestTimer_ != nullptr)
        {
            navigationTestTimer_.Stop();
        }
        controller_.Shutdown();
    }

    void OnNavClicked(const IInspectable& sender, const RoutedEventArgs&)
    {
        const Button button = sender.try_as<Button>();
        if (button == nullptr)
        {
            return;
        }
        SelectPage(static_cast<PageId>(unbox_value<int32_t>(button.Tag())));
    }

    void OnRefreshClicked(const IInspectable&, const RoutedEventArgs&)
    {
        controller_.Refresh();
        RefreshAll();
    }

    void OnInitializeClicked(const IInspectable&, const RoutedEventArgs&)
    {
        if (controller_.State() == unrealdbg_native::BackendState::Missing)
        {
            controller_.Refresh();
            RefreshAll();
            return;
        }

        if (controller_.State() == unrealdbg_native::BackendState::Initializing ||
            controller_.State() == unrealdbg_native::BackendState::Initialized)
        {
            return;
        }

        const auto symbolState = controller_.Symbols().State();
        if (symbolState == unrealdbg_native::SymbolPreparationState::Ready)
        {
            controller_.BeginInitialize();
            RefreshHooks();
        }
        else
        {
            pendingInitialization_ = true;
            if (symbolState == unrealdbg_native::SymbolPreparationState::Idle ||
                symbolState == unrealdbg_native::SymbolPreparationState::Failed)
            {
                controller_.Symbols().PrepareAsync();
            }
            unrealdbg_native::WriteLog(unrealdbg_native::LogLevel::Info,
                L"进入 VT 前等待 Loader 符号缓存准备完成");
        }
        RefreshSymbols();
        RefreshStatus();
    }

    void OnPrepareSymbolsClicked(const IInspectable&,
        const RoutedEventArgs&)
    {
        if (!controller_.Symbols().IsPreparing())
        {
            controller_.Symbols().PrepareAsync();
        }
        RefreshSymbols();
        RefreshStatus();
    }

    void OnAddDebuggerClicked(const IInspectable&, const RoutedEventArgs&)
    {
        std::wstring path;
        if (!PickExecutable(hwnd_, path))
        {
            return;
        }
        controller_.AddDebugger(path);
        RefreshDebuggerLists();
    }

    void OnDeleteDebuggerClicked(const IInspectable&, const RoutedEventArgs&)
    {
        if (debuggerSelectedIndex_ >= 0 &&
            controller_.DeleteDebugger(static_cast<size_t>(debuggerSelectedIndex_)))
        {
            RefreshDebuggerLists();
        }
    }

    void OnDebuggerSelectionChanged(const IInspectable& sender,
        const SelectionChangedEventArgs&)
    {
        const ListView list = sender.try_as<ListView>();
        if (list == nullptr)
        {
            return;
        }
        debuggerSelectedIndex_ = list.SelectedIndex();
        if (debuggerSelectedIndex_ >= 0 &&
            debuggerSelectedIndex_ < static_cast<int>(controller_.Debuggers().size()))
        {
            selectedDebuggerPathText_.Text(
                H(controller_.Debuggers()[debuggerSelectedIndex_].path));
        }
        else
        {
            selectedDebuggerPathText_.Text(L"未选择调试器");
        }
        UpdateActionAvailability();
    }

    void OnOverviewDebuggerSelectionChanged(const IInspectable& sender,
        const SelectionChangedEventArgs&)
    {
        const ListView list = sender.try_as<ListView>();
        if (list == nullptr)
        {
            return;
        }
        overviewSelectedIndex_ = list.SelectedIndex();
        if (overviewSelectedIndex_ >= 0 &&
            overviewSelectedIndex_ < static_cast<int>(controller_.Debuggers().size()))
        {
            overviewSelectionText_.Text(
                H(controller_.Debuggers()[overviewSelectedIndex_].path));
        }
        else
        {
            overviewSelectionText_.Text(L"选择一个调试器");
        }
        UpdateActionAvailability();
    }

    void OnDebuggerDoubleTapped(const IInspectable& sender,
        const winrt::Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs&)
    {
        const ListView list = sender.try_as<ListView>();
        if (list != nullptr && list.SelectedIndex() >= 0)
        {
            StartSelectedDebugger(list.SelectedIndex());
        }
    }

    void StartSelectedDebugger(const int index)
    {
        if (index < 0)
        {
            return;
        }
        controller_.StartDebugger(static_cast<size_t>(index));
        AppendPendingLogs();
    }

    void OnTlToggleChanged(const IInspectable& sender,
        const RoutedEventArgs&)
    {
        if (suppressTlEvents_)
        {
            return;
        }
        const ToggleSwitch toggle = sender.try_as<ToggleSwitch>();
        if (toggle == nullptr)
        {
            return;
        }
        if (toggle == tlEnableToggle_)
        {
            controller_.SetTlEnabled(toggle.IsOn());
        }
        else if (toggle == tlTickToggle_)
        {
            controller_.SetTlGetTickCount(toggle.IsOn());
        }
        else if (toggle == tlBlockToggle_)
        {
            controller_.SetTlBlockResumeThread(toggle.IsOn());
        }
        RefreshTl();
    }

    void BeginTargetCapture()
    {
        captureDeadline_ = ::GetTickCount64() + 3000;
        SetButtonCaption(captureTargetButton_, L"3 秒后捕获…");
        SetButtonEnabled(captureTargetButton_, false);
        unrealdbg_native::WriteLog(unrealdbg_native::LogLevel::Info,
            L"目标捕获已开始，请在 3 秒内切换到目标窗口");
        AppendPendingLogs();
    }

    void ApplyInitialWindowSize()
    {
        const auto native = window_.as<::IWindowNative>();
        HWND handle{};
        if (FAILED(native->get_WindowHandle(&handle)) || handle == nullptr)
        {
            return;
        }
        hwnd_ = handle;
        RECT work{};
        ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        const int workWidth = work.right - work.left;
        const int workHeight = work.bottom - work.top;
        const int width = (std::min)(
            static_cast<int>(tok::kWindowDefaultWidth), workWidth - 56);
        const int height = (std::min)(
            static_cast<int>(tok::kWindowDefaultHeight), workHeight - 56);
        ::SetWindowPos(handle, nullptr,
            work.left + (workWidth - width) / 2,
            work.top + (workHeight - height) / 2,
            width, height, SWP_NOZORDER);
    }

    void RefreshThemeButton()
    {
        if (themeButton_ == nullptr || themeButtonIcon_ == nullptr)
        {
            return;
        }
        themeButtonIcon_.Glyph(darkTheme_
            ? tok::kGlyphSun : tok::kGlyphMoon);
        themeButtonIcon_.Foreground(tok::Brush(tok::kTextSecondary));
        tok::SetToolTip(themeButton_,
            darkTheme_ ? L"切换到明亮主题" : L"切换到暗黑主题");
    }

    void ApplyWindowTheme()
    {
        RefreshThemeButton();
        try
        {
            auto titleBar = window_.AppWindow().TitleBar();
            titleBar.ButtonBackgroundColor(tok::kTitleBar);
            titleBar.ButtonForegroundColor(tok::kTextSecondary);
            titleBar.ButtonHoverBackgroundColor(tok::kCardHover);
            titleBar.ButtonHoverForegroundColor(tok::kTextPrimary);
            titleBar.ButtonPressedBackgroundColor(tok::kSunken);
            titleBar.ButtonPressedForegroundColor(tok::kTextPrimary);
            titleBar.ButtonInactiveBackgroundColor(tok::kTitleBar);
            titleBar.ButtonInactiveForegroundColor(tok::kTextMuted);
        }
        catch (const hresult_error&)
        {
            // Caption theming is optional; the WinUI content remains valid.
        }
    }

    void ToggleTheme(const bool persist = true)
    {
        const bool nextDark = !darkTheme_;
        winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread()
            .TryEnqueue([this, nextDark, persist]()
            {
                if (nextDark == darkTheme_)
                {
                    return;
                }
                darkTheme_ = nextDark;
                if (persist)
                {
                    controller_.SetDarkTheme(darkTheme_);
                }
                tok::SetTheme(darkTheme_);
                RebuildForTheme();
            });
    }

    void RebuildForTheme()
    {
        const PageId page = currentPage_;

        BuildLayout();
        window_.Content(root_);
        window_.ExtendsContentIntoTitleBar(true);
        window_.SetTitleBar(titleBar_);
        ApplyWindowTheme();

        RefreshAll();
        AppendPendingLogs();
        SelectPage(page);
    }

    Window window_{ nullptr };
    unrealdbg_native::Runtime controller_;
    bool smokeTest_{};
    HWND hwnd_{};
    Grid root_{ nullptr };
    Grid titleBar_{ nullptr };
    Grid pageHost_{ nullptr };
    std::vector<unrealdbg_native::LogRecord> logHistory_;
    std::array<Grid, kPageCount> pages_;
    std::array<Button, kPageCount> navButtons_;
    std::array<Border, kPageCount> navMarkers_;
    std::array<TextBlock, kPageCount> navTexts_;
    std::array<FontIcon, kPageCount> navIcons_;
    PageId currentPage_{ PageId::Overview };
    bool navSelectionInitialized_{};

    Grid overviewPage_{ nullptr };
    Grid debuggerPage_{ nullptr };
    Grid targetPage_{ nullptr };
    Grid hooksPage_{ nullptr };
    Grid logsPage_{ nullptr };
    Grid systemPage_{ nullptr };
    Grid symbolsPage_{ nullptr };
    Grid diagnosticsPage_{ nullptr };

    Border titleStatusPill_{ nullptr };
    Border titleStatusDot_{ nullptr };
    TextBlock titleStatusText_{ nullptr };
    Button themeButton_{ nullptr };
    FontIcon themeButtonIcon_{ nullptr };
    TextBlock navBackendText_{ nullptr };
    TextBlock navSymbolText_{ nullptr };
    TextBlock navTargetText_{ nullptr };
    Border navDebuggerBadge_{ nullptr };

    Border statusSeal_{ nullptr };
    TextBlock statusEyebrow_{ nullptr };
    TextBlock statusTitle_{ nullptr };
    TextBlock statusDetail_{ nullptr };
    Button initButton_{ nullptr };
    ProgressRing initProgress_{ nullptr };

    TextBlock backendStatText_{ nullptr };
    TextBlock symbolStatText_{ nullptr };
    TextBlock symbolHintText_{ nullptr };
    TextBlock debuggerStatText_{ nullptr };
    TextBlock targetStatText_{ nullptr };
    TextBlock logStatText_{ nullptr };
    TextBlock systemOsText_{ nullptr };
    TextBlock systemKernelText_{ nullptr };
    TextBlock systemCpuText_{ nullptr };
    TextBlock systemMemoryText_{ nullptr };
    TextBlock overviewTlText_{ nullptr };
    Button overviewTlButton_{ nullptr };
    TextBlock overviewSelectionText_{ nullptr };
    Button overviewStartButton_{ nullptr };
    ListView overviewDebuggerList_{ nullptr };

    ListView debuggerList_{ nullptr };
    TextBlock debuggerCountText_{ nullptr };
    TextBlock selectedDebuggerPathText_{ nullptr };
    Button deleteDebuggerButton_{ nullptr };
    Button startDebuggerButton_{ nullptr };

    TextBlock targetNameText_{ nullptr };
    TextBlock targetPidText_{ nullptr };
    TextBlock targetPathText_{ nullptr };
    Button captureTargetButton_{ nullptr };
    ToggleSwitch tlEnableToggle_{ nullptr };
    ToggleSwitch tlTickToggle_{ nullptr };
    ToggleSwitch tlBlockToggle_{ nullptr };
    TextBlock targetTlStateText_{ nullptr };
    TextBlock targetGuardStateText_{ nullptr };
    Button blockResumeButton_{ nullptr };

    StackPanel diagnosticsList_{ nullptr };
    TextBlock diagOsText_{ nullptr };
    TextBlock diagKernelText_{ nullptr };
    TextBlock diagArchText_{ nullptr };
    TextBlock diagElevationText_{ nullptr };
    TextBlock diagBackendText_{ nullptr };
    TextBlock diagRuntimePathText_{ nullptr };
    TextBlock diagBackendPathText_{ nullptr };
    TextBlock symbolStatusText_{ nullptr };
    TextBlock symbolProgressText_{ nullptr };
    ProgressBar symbolProgressBar_{ nullptr };
    StackPanel symbolModulesList_{ nullptr };
    TextBlock symbolErrorText_{ nullptr };
    Button symbolPrepareButton_{ nullptr };

    TextBlock hookSummaryText_{ nullptr };
    TextBlock hookLockText_{ nullptr };
    std::vector<ToggleSwitch> hookToggles_;
    std::vector<Button> hookActionButtons_;
    std::uint64_t lastHookUiMask_{};
    bool lastHookUiEditable_{};
    bool hookUiInitialized_{};

    ComboBox externalLogCombo_{ nullptr };
    TextBox externalLogText_{ nullptr };
    ToggleSwitch externalLogAutoRefresh_{ nullptr };
    TextBlock externalLogStatusText_{ nullptr };
    TextBlock externalLogPathText_{ nullptr };
    Button externalLogRefreshButton_{ nullptr };
    std::vector<unrealdbg_native::ExternalLogFile> externalLogFiles_;
    int externalLogIndex_{ -1 };
    std::uint64_t lastExternalLogStamp_{};
    std::uint64_t lastExternalLogSize_{};
    std::uint32_t lastInternalLogCount_{};
    ULONGLONG lastExternalLogSourceRefreshTick_{};
    ULONGLONG lastExternalLogRefreshTick_{};

    StackPanel systemDetailsBody_{ nullptr };
    Button systemPageRefreshButton_{ nullptr };

    Button initializeOffsetsButton_{ nullptr };
    Button chooseSymbolPdbButton_{ nullptr };
    Button resetSymbolPdbButton_{ nullptr };
    TextBlock symbolAnalysisHint_{ nullptr };
    TextBlock offsetSummaryText_{ nullptr };
    TextBlock offsetRuntimePdbText_{ nullptr };
    TextBlock offsetModuleBaseText_{ nullptr };
    TextBox symbolSearchBox_{ nullptr };
    ComboBox offsetPdbFilter_{ nullptr };
    ComboBox offsetKindFilter_{ nullptr };
    ToggleSwitch symbolUsedOnlyToggle_{ nullptr };
    ToggleSwitch symbolInternalToggle_{ nullptr };
    TextBlock symbolResultText_{ nullptr };
    TextBlock offsetEmptyText_{ nullptr };
    ItemsRepeater offsetRepeater_{ nullptr };
    winrt::Windows::Foundation::Collections::
        IVector<IInspectable> symbolItemsSource_{ nullptr };
    winrt::com_ptr<SymbolRowElementFactory> symbolRowFactory_{ nullptr };
    unrealdbg_native::SystemSymbolSnapshot symbolEntriesCache_;
    std::vector<std::wstring> symbolModuleFilterNames_;
    std::unordered_map<std::wstring, std::wstring> symbolModuleDisplayNames_;
    std::vector<const unrealdbg_native::SystemSymbolEntry*> symbolRenderQueue_;
    size_t symbolRenderIndex_{};
    std::uint64_t symbolRenderGeneration_{};
    bool symbolRenderingComplete_{ true };
    std::wstring customSymbolPdbPath_;
    std::wstring symbolSearchText_;
    bool symbolUsedOnlyFilter_{};
    bool symbolShowInternal_{};
    bool suppressSymbolFilterEvents_{};
    bool symbolSearchDirty_{};
    bool symbolPageDirty_{ true };
    bool symbolAnalysisConfigurationDirty_{};
    bool symbolAnalysisStartPending_{};
    bool systemPageDirty_{ true };
    ULONGLONG symbolSearchDeadline_{};
    unrealdbg_native::SymbolAnalysisState lastAnalysisState_{
        unrealdbg_native::SymbolAnalysisState::Idle };

    DispatcherTimer timer_{ nullptr };
    DispatcherTimer smokeTimer_{ nullptr };
    DispatcherTimer themeTestTimer_{ nullptr };
    DispatcherTimer symbolTestTimer_{ nullptr };
    DispatcherTimer navigationTestTimer_{ nullptr };
    unrealdbg_native::BackendState lastState_{
        unrealdbg_native::BackendState::Missing };
    std::wstring lastClock_;
    unrealdbg_native::SymbolPreparationState lastSymbolState_{
        unrealdbg_native::SymbolPreparationState::Idle };
    int lastSymbolProgress_{ -1 };
    int currentPollingIntervalMs_{ 500 };
    int overviewSelectedIndex_{ -1 };
    int debuggerSelectedIndex_{ -1 };
    bool pendingInitialization_{};
    bool darkTheme_{ false };
    ULONGLONG captureDeadline_{};
    ULONGLONG symbolTestDeadline_{};
    bool suppressTlEvents_{};
    bool suppressHookEvents_{};
    bool suppressExternalLogSelection_{};
};

namespace
{
    struct WinUiApp : ApplicationT<
        WinUiApp, Microsoft::UI::Xaml::Markup::IXamlMetadataProvider>
    {
        Microsoft::UI::Xaml::Markup::IXamlType GetXamlType(
            const winrt::Windows::UI::Xaml::Interop::TypeName& type)
        {
            return metadataProvider_.GetXamlType(type);
        }

        Microsoft::UI::Xaml::Markup::IXamlType GetXamlType(const hstring& fullName)
        {
            return metadataProvider_.GetXamlType(fullName);
        }

        winrt::com_array<Microsoft::UI::Xaml::Markup::XmlnsDefinition>
        GetXmlnsDefinitions()
        {
            return metadataProvider_.GetXmlnsDefinitions();
        }

        void OnUnhandledException(const IInspectable&,
            const UnhandledExceptionEventArgs& args)
        {
            const std::wstring message = L"[界面异常] " +
                std::wstring(args.Message().c_str());
            unrealdbg_native::WriteLog(
                unrealdbg_native::LogLevel::Error, message);
            ::OutputDebugStringW((message + L"\n").c_str());
        }

        void OnLaunched(const LaunchActivatedEventArgs&)
        {
            StartupTrace(L"[app] OnLaunched");
            UnhandledException({ this, &WinUiApp::OnUnhandledException });
            try
            {
                StartupTrace(L"[app] merging XamlControlsResources");
                Resources().MergedDictionaries().Append(
                    Microsoft::UI::Xaml::Controls::XamlControlsResources());
                StartupTrace(L"[app] resources ready");
                const wchar_t* commandLine = ::GetCommandLineW();
                const bool smoke = commandLine != nullptr &&
                    std::wcsstr(commandLine, L"--ui-smoke-test") != nullptr;
                shell_ = std::make_unique<Shell>(
                    unrealdbg_native::GetApplicationDirectory(), smoke);
                StartupTrace(L"[app] shell constructed");
                shell_->Launch();
                StartupTrace(L"[app] shell launched");
            }
            catch (const hresult_error& error)
            {
                const std::wstring message = L"[界面异常] 启动失败：" +
                    std::wstring(error.message().c_str());
                StartupTrace(L"[app] hresult=0x" +
                    std::to_wstring(static_cast<uint32_t>(error.code().value)) +
                    L" " + message);
                unrealdbg_native::WriteLog(
                    unrealdbg_native::LogLevel::Error, message);
                ::OutputDebugStringW((message + L"\n").c_str());
                g_smokeExitCode = 1;
            }
            catch (...)
            {
                StartupTrace(L"[app] unknown exception");
                g_smokeExitCode = 1;
            }
        }

        Microsoft::UI::Xaml::XamlTypeInfo::
            XamlControlsXamlMetaDataProvider metadataProvider_;
        std::unique_ptr<Shell> shell_;
    };
}

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    std::wstring runtimeDirectory;
    try
    {
        runtimeDirectory = unrealdbg_native::GetApplicationDirectory();
    }
    catch (...)
    {
    }
    if (!runtimeDirectory.empty())
    {
        ::SetCurrentDirectoryW(runtimeDirectory.c_str());
    }
    try
    {
        unrealdbg_native::ConfigureLogStore(runtimeDirectory);
    }
    catch (...)
    {
    }

    StartupTrace(L"[boot] wWinMain");
    const HRESULT initialized = ::CoInitializeEx(
        nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized))
    {
        return 1;
    }

    const wchar_t* commandLine = ::GetCommandLineW();
    const bool smokeTest = commandLine != nullptr &&
        std::wcsstr(commandLine, L"--ui-smoke-test") != nullptr;

    try
    {
        StartupTrace(L"[boot] bootstrap begin");
        const PACKAGE_VERSION minVersion{};
        winrt::check_hresult(::MddBootstrapInitialize2(
            0x00020005, nullptr, minVersion,
            MddBootstrapInitializeOptions_None));
        StartupTrace(L"[boot] bootstrap ready");
    }
    catch (const hresult_error& error)
    {
        StartupTrace(L"[boot] bootstrap failed hresult=0x" +
            std::to_wstring(static_cast<uint32_t>(error.code().value)) +
            L" message=" + std::wstring(error.message().c_str()));
        if (!smokeTest)
        {
            ::MessageBoxW(nullptr,
                L"Windows App Runtime 2.5.1 引导失败。\n\n"
                L"请安装匹配的 x64 运行时后重试，日志目录中会保留诊断信息。",
                L"F3 VT 调试台", MB_OK | MB_ICONERROR);
        }
        ::CoUninitialize();
        return 1;
    }

    Application::Start([](auto&&)
    {
        StartupTrace(L"[boot] Application::Start callback");
        make<WinUiApp>();
    });

    StartupTrace(L"[boot] Application loop exited");
    ::MddBootstrapShutdown();
    ::CoUninitialize();
    return smokeTest ? g_smokeExitCode : 0;
}
