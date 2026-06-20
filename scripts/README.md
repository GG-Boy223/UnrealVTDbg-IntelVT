# 脚本索引

## 根目录入口

| 脚本 | 说明 |
|:-----|:-----|
| `build_vt_driver_release_x64.bat` | 构建 `VT_Driver/VT_Driver.vcxproj` 的 Release x64。 |
| `build_dbgk_win10_release_x64.bat` | 构建 `DbgkSysWin10/DbgkSysWin10.vcxproj` 的 Release x64。 |
| `build_dbgk_win11_release_x64.bat` | 构建 `DbgkSysWin11/DbgkSysWin11.vcxproj` 的 Release x64。 |
| `build_hook_release_x64.bat` | 构建 `Hook/Hook.vcxproj` 的 Release x64。 |
| `build_loader_release_x64.bat` | 构建 `Loader/Loader.vcxproj` 的 Release x64。 |
| `build_d_encryptiondll_release_x64.bat` | 构建 `D-encryptiondll/D-encryption.sln` 的 Release x64。 |
| `build_unreal_dbgdll_release_x64.bat` | 构建 `UnrealDbgDll/UnrealDbgDll.vcxproj` 的 Release x64。 |

## 公共实现

| 脚本 | 说明 |
|:-----|:-----|
| `scripts/build/msbuild_release_x64.bat` | 根目录构建入口共用的 VS DevCmd + MSBuild Release x64 封装。 |

## Delphi 边界

| 项 | 说明 |
|:---|:-----|
| `UnrealDbg/UnrealDbg.dproj` | Delphi Win64 UI 源码，不在当前 7 个 C++/driver 构建入口内。 |
| `CardRegistration/卡密注册.dproj` | Delphi Win64 工具源码，不在当前 7 个 C++/driver 构建入口内。 |
| `D-encryption/D_encryption.dproj` | Delphi Win64 工具源码，不在当前 7 个 C++/driver 构建入口内。 |
| `SymbolTool/SymbolTool.dproj` | Delphi Win64 工具源码，不在当前 7 个 C++/driver 构建入口内。 |

当前环境未验证 Delphi/dcc64 构建；发布包内 `UnrealDbg.aes` 来自原始运行资产。

## 发布校验

| 脚本 | 说明 |
|:-----|:-----|
| `scripts/release/refresh_dist_manifest_x64.ps1` | 递归刷新 x64 发布目录 manifest，覆盖工具目录和运行资产。 |
| `scripts/release/prune_dist_x64.ps1` | 删除发布目录中的 32 位 PE 和明确 x86 工具残留。 |
| `scripts/release/validate_dist_x64.ps1` | 校验发布目录没有 32 位 PE/明显 x86 工具残留，并复核 manifest 大小与 SHA256。 |

## 历史脚本

| 脚本 | 说明 |
|:-----|:-----|
| `scripts/legacy/release.bat` | 旧发布打包脚本，依赖旧输出目录布局，当前不作为发布入口。 |
| `scripts/legacy/check_code_standard.sh` | 旧 Bash 行数检查脚本，口径已落后当前 active 编译项规则，仅保留作历史参考。 |

## 模块内脚本

| 路径 | 说明 |
|:-----|:-----|
| `Common/Ring0/ia32-doc/*.py` | ia32-doc 自带 Python 生成器，属于第三方/生成资料目录，保留在模块内。 |

## 维护规则

- 当前可执行构建入口保留在根目录，便于直接运行和历史验证记录对齐。
- 公共逻辑放到 `scripts/build/`。
- 旧脚本统一放到 `scripts/legacy/`，不得作为当前验证入口。
- 临时分析脚本和结构化输出脚本不再保留在根目录。
