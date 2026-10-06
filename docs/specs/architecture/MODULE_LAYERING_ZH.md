---
lifecycle: working
---

# 模块分层与解耦方向

本文只定方向和约束，不规定实施步骤。阶段一完成后，把已落地的部分转为 `stable-current` 规范，删除本文已过时的内容。

## 目标

桌面、Android、Web 各端用自己的 UI 调用下层模块，每个平台只链接自己需要的库：

- Android 不带编辑器。
- Web 不带编辑器和时间轴。

## 已定决策（2026-10-06）

| 问题 | 决定 |
| --- | --- |
| 其他端的 UI 技术栈 | 统一用 Qt 6（Qt for Android / WebAssembly）。下层库最低依赖 QtCore/QtGui，不去 Qt 化。 |
| 播放协调 | 其他端要复用。最终目标是方案 B：从 Session 拆出运行时内核。 |
| 偏好 | 采用注入方式：下层只接收值，持久化由 app 的设置宿主负责。 |
| 改名 | 这次不改名。文件、类型、函数只搬位置不改名字，改名以后另行决定。 |
| 仓库 | 保持单仓、多个 CMake 目标，不拆仓。 |

## 阶段

1. **阶段一（方案 A + 偏好注入）**：拆掉依赖环，把每个模块做成独立的构建目标。现有语义保持不变。
2. **阶段二（方案 B 的剩余部分）**：阶段一稳定后再做。从 Session 拆出 `workspace`（ChartWorkspace、文件服务、AnalysisService）和 `playback`（PlaybackCoordinator 及其端口）。Session 只保留桌面装配根的职责，去掉 friend，`SessionMembers.inc` 按宿主拆分。

## 目标库（阶段一）

一个目录只能属于一个库，一个库可以包含多个目录。`tools/` 里的产品代码迁到所属库的目录，`tools/` 只保留 Spec。

| CMake 目标 | 源目录 | 主要内容 | 允许依赖 |
| --- | --- | --- | --- |
| `miacode_base` | `src/common` | 日志、调试选项、诊断、任务取消、文件戳、本地化文本、崩溃恢复、看门狗 | Qt |
| `miacode_chart` | `src/core/chart` | 文档、解析、变换、选择；音符模型（`TimelineData.h`、`MuriPadTimeEntry`）、`ChartClockCount`、`ChartAssetPaths`；slide_data 资源 | base |
| `miacode_analysis` | `src/core/analysis` | Muri 产品代码、Muri 报告类型/配置/渲染选项、分析流水线（`TimelineSlowRefresh`） | chart |
| `miacode_editor_core` | `src/editor` | 文本策略、补全、书签语法 | chart |
| `miacode_scene` | `src/core/scene`、`src/core/video` | 场景数学、Preview*Config、SFX 时间线语义、`AssetPaths`；字体资源 | analysis |
| `miacode_audio` | `src/audio` | 音频后端接口、worker、协议、设置、`QtPreviewSfxRuntime`、`WaveformCache` 与解码器接口 | scene |
| `miacode_audio_bass` | `src/audio/bass` | BASS 后端与离线解码 | audio |
| `miacode_timeline` | `src/timeline` | 时间轴模型 | analysis, audio |
| `miacode_timeline_quick` | `src/timeline/quick` | QSG 图层；QML 模块 `MiaCode.Timeline` | timeline |
| `miacode_preview_quick` | `src/preview/quick_scene`、`src/preview/runtime` | QSG 场景、PreviewRuntime、素材与纹理仓库；shader 和判定特效资源；QML 模块 `MiaCode.Preview` | scene |
| `miacode_stage_media` | `src/preview/stage_media` | PreviewStageMediaHost 和 QtAVPlayer | scene, preview_quick |
| `miacode_export` | `src/export` | 视频/封面导出、QSG/D3D11 导出会话、共享 D3D11 设备、导出音频 | 以上任意库 |
| `miacode_media_tools` | `src/media_tools` | 原 `tools/media`、`zip_export`、`net` | base |
| `MiaCode` | `src/app` | UI、services、runtime、quick_shell；延迟检测宿主移入 runtime | 任意库 |

`common` 里的领域内容按使用方归位：

- 音符模型归 chart，Muri 相关归 analysis。
- 预览配置和 SFX 语义归 scene，音频配置和波形归 audio，导出配置归 export。
- 只有 app 用的内容（片头、项目偏好、ID3、媒体导入）归 app。

## 约束

- **依赖方向**：只能依赖表中「允许依赖」列出的库，以及这些库自身的依赖（可以传递）。任何库都不能依赖 `MiaCode`。新增一个层级检查脚本，放在 `scripts/governance/`，并接入 CI。
- **include 写法**：统一写成以 `src` 为根的完整路径。每个库都 PUBLIC 导出 `src` 根，去掉现在平铺的 include 目录。
- **Qt 依赖**：每个库只链接自己用到的 Qt 模块。Qt 私有 API（QuickPrivate、MultimediaQuickPrivate）只允许出现在 `stage_media` 和 `export` 里。
- **第三方依赖**：
  - BASS 只能在 `audio_bass` 和 `export` 里用。
  - QtAVPlayer/FFmpeg 只能在 `stage_media` 和 `export` 里用。
  - ScintillaQuick 只能在 `MiaCode` 里用。
  - miniz 和 QtNetwork 只能在 `media_tools` 和 `MiaCode` 里用。
- **资源**：qrc 跟随所属的库一起移动，资源路径（例如 `:/data/slide_data.json`）保持不变。
- **QML**：QML 模块的 URI 和版本号保持不变，并删除现在分散在多处的重复类型注册。
- **偏好**：
  - 库内代码不能直接读写偏好文件，只通过值或端口拿到偏好。
  - 端口的实现和持久化逻辑放在 app 侧的设置宿主里。`PreferenceDocument` 移到 app 的服务层，不再放在 `app/ui`。
  - GUI 进程和导出 worker 进程各自在启动入口安装偏好提供者。导出 worker 现在会隐式读取 `preferences.json`，安装后的行为要和现在一致。
- **QtMultimedia**：场景数学不能依赖 QtMultimedia。视频帧改用不透明句柄传递。
- **Spec 和开发工具**：改为链接库，不再重复列出源文件。为 Web 和 Android 的库组合各加一个 boundary spec，只链接对应的库，用来证明不带 app 也能完成链接。

## 不变量

- 运行时行为、日志事件名和字段、文件格式、偏好 JSON 的键和写入时机、资源路径、QML URI 全部保持不变。
- 构建入口不变：`MiaCode` 和 `MiaCodeLauncher` 的目标名、构建脚本、打包脚本和打包产物都不变。
- 不新增平台构建。阶段一只需要保证下层库能单独构建、单独链接。
