---
lifecycle: working
---

# 模块分层阶段一交付报告（方案 A + 偏好注入）

- **分支**：`refactor/module-layering-a`，基于 `dev`（`e58f528a`），已推送到远程。
- **方向文档**：[模块分层与解耦方向](../specs/architecture/MODULE_LAYERING_ZH.md)，现在只保留阶段二的内容。
- **当前规范**：[模块分层（当前）](../specs/architecture/MODULE_LAYERING_CURRENT_ZH.md)。
- **结论**：阶段一的全部工作包都已提交，命令行侧的验证全部通过，下一步是 GUI 人工验收。

实施代理在收尾时被中止，没有交出最终报告。本报告根据提交记录、文档改动和复核结果整理，
所有验证数据都在分支最新提交 `7080ac85` 上重新跑过。

## 提交

| 提交 | 内容 | 工作包 |
| --- | --- | --- |
| `729d5e3e` | docs(architecture): add module layering direction | 方向文档 |
| `92243889` | fix(devtools): restore spec builds and offscreen test runs | 范围外修复，见下 |
| `3c2822bf` | refactor(build): include project headers by src-rooted paths | W9（include） |
| `17caf0fc` | refactor(chart): move the note model into core/chart | W1 |
| `f1a2163e` | refactor(analysis): move Muri and the analysis pipeline into core/analysis | W1、W2 |
| `7bd04294` | refactor(tools): move product code out of src/tools | tools 迁出 |
| `48a13308` | refactor(common): move domain headers to their owning modules | W3 |
| `0905de95` | refactor(preferences): inject preferences into libraries through a provider port | W4B |
| `074c5858` | refactor(audio): split the BASS backend into audio/bass behind injected providers | W5 |
| `fe23b84b` | fix(specs): join the log writer before the preview rate spec exits | 范围外修复，见下 |
| `41eabe81` | refactor(scene): carry video frames as an opaque handle | W6 |
| `07b3526c` | refactor(preview): split stage media and export sessions out of the preview runtime | W7 |
| `9473d103` | refactor(app): keep runtime and services below the UI and entry layers | W10 |
| `3489bbc3` | build: split MiaCode into layered static libraries | W8、W9 |
| `f7215efc` | build(qml): provide MiaCode.Preview and MiaCode.Timeline as QML modules | W12 |
| `8e9874d9` | test(architecture): add Web and Android library boundary specs | W9（boundary） |
| `df65c057` | ci(governance): check module layering | W9（层级检查） |
| `7080ac85` | docs(architecture): document the current module layering | 文档同步 |

两个范围外修复的原因：

- **`92243889`**：要拿到改动前的基线，这些 Spec 必须先能构建，并能在 offscreen 模式下运行。
- **`fe23b84b`**：`QmlPreviewRateFeedbackSpec` 退出前没有等待日志写线程结束，导致结果时好时坏。

两项都不影响产品代码。

改动规模：506 个文件，+4055 / −3145 行，其中大部分是文件搬迁和 include 路径改写。

## 验证（在 `7080ac85` 上复核）

| 检查 | 结果 |
| --- | --- |
| Release 全量构建（`build-msvc`，Ninja Multi-Config，开启 DEV_TOOLS） | 通过，共 924 个目标 |
| `ctest -C Release`（排除 `qml_export_video_page_spec`） | 118 项中 105 项通过、13 项失败。这 13 项改动前就失败，失败的断言和基线逐条一致 |
| 基线对比 | 改动前失败 14 项；`dependency_allowlist_spec` 现在通过了，没有新增失败 |
| `qml_export_video_page_spec` 单独运行 | 失败（批量导出切换设置标签页的 4 条断言）。这个 Spec 会创建真实的 QML 窗口；它的源码和它加载的 QML 在分支上都没有改动，也不链接任何改过的库，所以判断是原有问题。没有在 `dev` 上实际运行验证 |
| `web_module_boundary_spec`、`android_module_boundary_spec` | 构建通过，ctest 也通过 |
| `miacode_simai_dump` / `miacode_muri_dump`：用 `samples/*/maidata.txt` 对比改动前后的输出 | 32 个输出文件全部逐字节一致 |
| `scripts/governance/module_layering.py` 及其测试 | 通过 |
| `docs_index.py --check`、`SpecCatalog` 检查、`docs_and_guides_test.py`、`spec_registry_test.py` | 通过 |

改动前就失败的 13 项：`qml_chart_drop_bridge_spec`、`qml_shortcut_binding_spec`、
`qml_document_lifecycle_contract_spec`、`qml_export_intro_sound_contract_spec`、
`qml_selection_range_export_contract_spec`、`qml_cover_export_contract_spec`、`qml_main_menu_spec`、
`qml_ui_theme_contract_spec`、`chart_workspace_file_service_spec`、`application_services_spec`、
`timeline_model_spec`、`pv_memory_host_contract_spec`、`debug_flag_index_spec`。

## 与方向文档不一致的地方

以[当前规范](../specs/architecture/MODULE_LAYERING_CURRENT_ZH.md)为准：

| 项目 | 方向文档 | 实际实现 |
| --- | --- | --- |
| `miacode_media_tools` 的依赖 | base | chart |
| `PreviewSharedD3D11Device`、`PvMemoryDiagnostics` | 归 export | 归 `stage_media`，export 通过依赖 `stage_media` 使用 |
| QtAVPlayer/FFmpeg | `stage_media` 和 export | 只在 `stage_media` |
| `miacode_audio` 的 Qt 依赖 | QtCore/QtGui | 另外链接 QtMultimedia |
| `IntroConfig`、`TimelineMarkerOffset` | 未指定 | 归 chart |
| include 规则 | 全部以 `src` 为根 | 例外：qmltyperegistrar 生成的代码只按文件名包含头文件，所以 `preview_quick`、`timeline_quick`、`MiaCode` 以 PRIVATE 方式加入这些头文件所在目录 |
| 新增文件 | — | `app/platform/PlatformDiagnostics.h`（GPU 与进程诊断）、`app/services/ExportPagePort.h`（导出页端口） |

## 需要你拍板

1. **`miacode_audio` 依赖 QtMultimedia。** Qt for WebAssembly 对 QtMultimedia 的支持有限。要么保留现状，Web 端的音频后端到时候再适配；要么把依赖 QtMultimedia 的部分拆到独立的库。
2. **没有安装偏好提供者时，读取返回空、写入失败。** 桌面端的三个入口（GUI、CLI 导出、导出 worker）都已安装。其他端接入时需要自己安装。要不要在 base 层提供一个默认的内存实现？
3. **`miacode_media_tools` 依赖 chart，和方向文档写的 base 不一致。** 是接受现状，还是把它用到的 chart 内容下沉？
4. **14 项原有失败的 Spec**（13 项加 `qml_export_video_page_spec`）这次没有处理。是否另开任务修复？
5. **合入 `dev` 的方式**：保留 18 个提交线性合入，还是 squash 成一个提交？

## GUI 人工验收

清单在 `docs/_private/MODULE_LAYERING_A_GUI_ACCEPTANCE_ZH.md`，本地文件，不入库。

构建产物 `build-msvc\Release\MiaCode.exe` 对应 `7080ac85`。清单覆盖以下 12 项：

- 预览 HUD 字体，包括按区域分别设置，以及重启后是否保留
- 视频导出的 HUD 字体是否走 worker 进程
- 封面导出的设置是否保留
- 时间轴的语言和波形显示
- 预览音频和设备
- PV/BG 视频播放
- 视频导出和封面导出
- 片头
- 延迟检测页
- 编辑器
- 更新检查
- 其他受影响的路径

## 后续

- **验收完成后**：GUI 验收通过、上面的问题都拍板后，再合入 `dev`。之后删除本报告和私有验收清单。
- **阶段二**：方案 B 的剩余部分，见方向文档。
