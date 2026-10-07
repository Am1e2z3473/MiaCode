# 开发入口

共享构建与验证约定见 [AGENTS.md](../../../../AGENTS.md)。

| 内容 | 入口 |
| --- | --- |
| 平台依赖、构建与打包脚本 | `scripts/README.md`、`scripts/build/` |
| 构建目标与目录选择 | `miacode-concurrent-build` skill |
| 命令行诊断 | `src/devtools/`、`cmake/devtools/CliTools.cmake` |
| 库定义与分层检查 | `cmake/MiaCodeModules.cmake`、`scripts/governance/module_layering.py`、`docs/specs/architecture/MODULE_LAYERING_CURRENT_ZH.md` |
| Spec 注册 | `cmake/devtools/specs/`、`cmake/devtools/MiaCodeSpecRegistry.cmake` |
| Spec 目标与契约索引 | `docs/tests/SPEC_CATALOG.md` |
| 调试开关与日志 | `docs/ops/DEBUG_INDEX.md`、`src/common/DebugLog.h` |
| 资源解析 | `src/core/video/AssetPaths.h`、`src/core/chart/ChartAssetPaths.h`、ChartMediaService |
| 翻译与偏好 | `translations/`、`src/app/services/PreferenceDocument.h` |
| 公开规范和验收清单 | `docs/INDEX.md`、`docs/README.md` |

新增 Spec 在所属领域 manifest 使用 `miacode_add_spec` 登记 owner、contract ID、类别和执行方式。
库内代码通过 LIBS 链接所属 `miacode_*` 库，SOURCES 只列 Spec 自身和 `MiaCode` 的源文件；保留所需链接边界，compile-only 规格以编译验证。
目录生成命令为 `cmake -P cmake/devtools/SpecCatalog.cmake`。

技能位于 `.agents/skills/`。
