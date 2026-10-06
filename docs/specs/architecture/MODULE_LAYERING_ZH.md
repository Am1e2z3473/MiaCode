---
lifecycle: working
---

# 模块分层与解耦方向

本文只定后续方向和约束，不规定实施步骤。阶段一（方案 A + 偏好注入）已落地，当前分层、
库表、注入点与检查方式见 [模块分层（当前）](MODULE_LAYERING_CURRENT_ZH.md)。

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
| 改名 | 不改名。文件、类型、函数只搬位置不改名字，改名以后另行决定。 |
| 仓库 | 保持单仓、多个 CMake 目标，不拆仓。 |

## 阶段二（方案 B 的剩余部分）

阶段一稳定后再做：

- 从 Session 拆出 `workspace`（ChartWorkspace、文件服务、AnalysisService）和 `playback`
  （PlaybackCoordinator 及其端口），作为可被其他端复用的库。
- Session 只保留桌面装配根的职责，去掉 friend（含 `LatencySandboxController` 与各运行时宿主），
  `SessionMembers.inc` 按宿主拆分。
- 新库沿用阶段一的约束：一个目录只属于一个库、以 `src` 为根的 include、依赖方向由
  `scripts/governance/module_layering.py` 检查，并为受影响的平台组合补充 boundary spec。

## 不变量

- 运行时行为、日志事件名和字段、文件格式、偏好 JSON 的键和写入时机、资源路径、QML URI 全部保持不变。
- 构建入口不变：`MiaCode` 和 `MiaCodeLauncher` 的目标名、构建脚本、打包脚本和打包产物都不变。
- 不新增平台构建。
