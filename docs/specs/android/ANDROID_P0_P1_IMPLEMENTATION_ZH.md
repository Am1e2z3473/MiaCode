---
lifecycle: working
---

# 安卓 P0 / P1 实施与验收记录

更新：2026-10-01。范围依据 [迁移计划](ANDROID_MIGRATION_PLAN_ZH.md)。本文随测试证据更新，代码实现、主机检查、安卓运行和实机验收分别记录。

## 已实现的基础

- `src/android` 独立应用组合；安卓构建提前进入该目标，复用 v2 `ChartWorkspace`、`SimaiDocument`、时序元数据和解析器。主机验证目标不依赖桌面 BASS／FFmpeg SDK。
- 手机页签布局、平板双栏布局；谱面正文输入、多难度切换与添加、标题编辑、撤销重做、系统软键盘输入。完整高亮、辅助编辑和诊断面板归入 P2。
- 原生 SAF 选文件、选工程目录、另存为、源文件保存和分享。目录导入保留相对结构；素材和字体复制到应用内部，可离线使用。
- UTF-8 检查、异步文件读写、外部保存回读校验、内部 `QSaveFile` 原子恢复副本。保存提交后发生的新编辑保持未保存状态。源 URI 失效不会推进保存点。
- schema 1 会话保存文档、原保存点、活动难度、素材引用和后台导出选择。启动先提示恢复，不覆盖既有恢复副本；退入后台时立即刷新恢复副本。
- ARM64／Android 12 起的 CMake、固定工具链配置、SDK 下载校验、APK 原生 ABI／ELF 对齐审计工具。构建均在 `build-devtools`。

## P0 媒体探针

入口位于“素材与验证”，实际生成 720×720、30 fps、60 帧短 MP4（AVC + AAC）、48 kHz 双声道 PCM WAV 和 PNG。视频包含片头色块和按固定时间移动的八轨测试对象；音频包含测试音和节拍脉冲。帧时间戳由帧编号计算，不使用实时播放计时器。生成后用 `MediaExtractor` 检查视频帧数、时间戳单调性和音轨存在。

报告同时枚举 v2 的 10 个尺寸 × 30／60／120 fps 编码能力组合；`advertised` 与 `exportTested` 分开，能力枚举不视作实际导出通过。导出结果可通过 SAF 保存为 ZIP。

后台选择默认关闭，离开前台取消前台探针。启用后使用独立 `:export` 进程的前台服务、限时部分唤醒锁和取消通知。Android 15+ 使用 `mediaProcessing`，较早版本使用 `dataSync`。任务使用唯一运行标识和原子报告，避免旧报告误判为新任务完成。

**探针尚未证明 v2 谱面场景、真实 PV 解码、BASS 混音／音效、字体封面图层、软件编码后备、片段和批量导出的一致性。** 这些仍是 P0 技术决策的未关闭项；生产导出任务及检查点恢复在 P4。

## 工具链和依赖决策

| 项目 | 当前选择 / 状态 | 放行条件 |
|---|---|---|
| Qt | 6.11.1，host / target 同版本，P0/P1 原型固定 | 发布前检查全部 Qt 原生库；6.8.3 包的 ELF 审计已失败并升级 |
| NDK / JDK | r27c `27.2.12479018` / JDK 17 | ARM64 编译及 APK 打包 |
| 系统范围 | minSdk 31，targetSdk 35，ARM64 | Android 12–16 手机和平板验证 |
| 媒体输出原型 | Android MediaCodec + EGL Surface + MediaMuxer | 实际文件、锁屏运行、取消及时间戳检查 |
| v2 音频 | BASS Android 尚未接入；Qt 主机目标不依赖它 | Android 运行库、mix/fx/格式插件、授权与 16 KB 证据 |
| v2 PV | QtAVPlayer Android 路径尚未接入 | FFmpeg Android 依赖、MediaCodec 解码和指定时间寻帧证据 |
| 软件编码后备 | 尚未选定 | 编码兼容、性能、包体及分发许可决定 |
| 字体 | 系统字体 + 用户导入 TTF / OTF | 真实字体导入和封面一致性；桌面字体资产许可单独确认 |
| 16 KB | 应用链接器设置 + APK 全库审计 | 全库 ELF、ZIP 对齐、16 KB 设备启动分别通过 |
| 分发 | 原型使用测试签名 APK | 正式应用 ID／版本策略与正式签名由发布阶段确认 |

Linux v2 仅用于依赖／平台边界参考。Visual Maimai Mobile 的任务流程参考仍需可核验的移动版本资料；当前手机布局按迁移计划中的任务分区实现，不将未经核验的界面描述作为事实。

## 验收清单

| 阶段 | 项目 | 证据与状态 |
|---|---|---|
| P0 | 功能对应表与回归输入 | [功能清单](ANDROID_PARITY_CHECKLIST_ZH.md)；多难度和未完成输入 fixture 已加入 |
| P0 | 手机和平板界面原型 | 主机离屏渲染 440×820 / 1280×800；不是安卓实机证据 |
| P0 | 编码器端到端原型 | 代码已实现；安卓运行结果待设备测试 |
| P0 | 真实谱面 + PV + 音效短视频 | 待现有媒体／场景引擎接入与实机验证 |
| P0 | 最大预设与软件后备选择 | 能力报告已实现；压力导出和后备决定待验证 |
| P1 | 主机基础回归 | `AndroidFoundationSpec` 覆盖多难度、异步保存竞争、权限失败、取消、无效 UTF-8、不可写恢复路径 |
| P1 | ARM64 编译、Java / APK | 查看本次实际构建记录；最终状态按结果补充 |
| P1 | 手机 / 平板打开、保存、重启恢复 | 待实机，USB 设备授权与安装测试许可未确认 |
| P1 | 撤销文件授权、存储满、应用升级 | 主机失败路径已验证；Android provider、空间耗尽和同签名升级待实机 |
| P1 | 正式 APK 分发配置 | 测试签名与正式签名区分；正式签名未生成 |

P0/P1 当前不能按完整退出条件关闭。不存在“编译通过等于阶段完成”的替代验收。

## 构建与复现

固定配置在 `scripts/build/android-toolchain.json`。Qt host 与 target 必须同版本；JDK 路径显式传入，避免本机默认 Java 11。SDK 安装脚本只复用用户已接受的许可证，不代替用户接受条款。

```powershell
# Qt Android 可用 aqt 获取；host 使用同版本 Qt 桌面安装。
# Qt 6.11 的分架构仓库使用仓库安装脚本。
.\scripts\build\provision-qt.ps1 -Version 6.11.1 -AqtArch android_arm64_v8a -ArchDir android_arm64_v8a -HostPlatform all_os -Target android

.\scripts\build\provision-android.ps1 -Python <python.exe> -AcceptedLicenseDirectory <已接受的SDK许可证目录>
.\scripts\build\build-android.ps1 -QtHostRoot <同版本Qt-host> -JdkRoot <JDK17> -Ninja <ninja.exe> -SignForTesting

# 主机基础回归，无桌面媒体依赖。
cmake -S . -B build-devtools/android-host -DMIACODE_ANDROID_HOST_PROBE=ON -DCMAKE_PREFIX_PATH=<Qt-host>
cmake --build build-devtools/android-host --config Release --parallel 4
ctest --test-dir build-devtools/android-host -C Release --output-on-failure

# 审计时还需 Android build-tools 的 zipalign -c -P 16 -v 4 <apk>。
python scripts/build/audit-android-apk.py <apk> --output <报告.json>
```

媒体探针 ZIP 应包含每次运行的 JSON、MP4、WAV 和 PNG。实机测试保留机型、系统 API、页大小、前后台选择、失败原因和输出文件检查，随后更新此表。

当前原型导入上限：谱面 16 MiB、单素材 512 MiB、工程目录合计 1 GiB、嵌套 8 层。超过限制时明确失败并保留当前工程。生产版本的流式大素材导入、配额与清理策略仍需落地。
