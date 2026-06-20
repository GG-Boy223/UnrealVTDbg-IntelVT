# 文档索引

## 根目录入口

| 文档 | 说明 |
|:-----|:-----|
| `README.md` | 项目当前模块边界、x64-only 状态、构建脚本和文档入口。 |
| `Task-Status.md` | 当前任务清单、已完成事项和高价值发现。 |
| `CLAUDE.md` | 本地协作规则和工程约束。 |
| `scripts/README.md` | 脚本索引。 |

## 架构文档

| 文档 | 说明 |
|:-----|:-----|
| `docs/architecture/DbgkSys重构设计.md` | DbgkSys 重构设计记录。 |
| `docs/architecture/架构规范分析报告.md` | 架构规范分析记录。 |

## 历史归档

| 目录 | 说明 |
|:-----|:-----|
| `docs/archive/2026-06-12-refactor-reports/` | 2026-06-12 重构过程中的历史报告、最终总结、进度快照和临时任务清单。 |

## 脚本目录

| 文档 | 说明 |
|:-----|:-----|
| `scripts/README.md` | 当前根构建入口、公共 helper、legacy 脚本和模块内脚本边界。 |

## 模块内文档

| 文档 | 说明 |
|:-----|:-----|
| `Common/Ring0/Inject/ApcInject/拆分报告.md` | APC 注入模块拆分记录。 |
| `Hook/HookCallSet/Hooks/README.txt` | HookCallSet hooks 局部说明。 |
| `Common/Ring0/ia32-doc/README.md` | ia32-doc 第三方/生成资料说明。 |

## 维护规则

- 根目录只放入口文档、任务状态、构建脚本、解决方案和必须的工程入口文件。
- 历史汇报、临时总结和过期清单统一归档到 `docs/archive/`。
- 架构分析和设计记录统一放到 `docs/architecture/`。
- 脚本索引统一维护在 `scripts/README.md`。
- 模块强相关文档保留在模块目录内。
- `.claude/` 是本地工具目录且被 git 忽略；其中旧计划和临时总结不作为当前仓库文档入口。
- `reference_origin/` 只作为本地原始参考副本，不进入文档索引和提交范围。
