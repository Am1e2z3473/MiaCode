# Android 迁移实现与验收记录

目标目录：`D:\STUDY\Project_Work\MiaCode_dev2\MiaCode_Mobile`。

目标：完整保留 v2 功能，Android 手机和平板横屏使用，本机离线导出，APK 发布；主要界面与 v2 至少 90% 相似。本文记录实际完成范围，不把开发探针算作产品功能。

## 阶段状态（2026-10-01）

| 阶段 | 当前证据 | 仍需完成 |
|---|---|---|
| P0：平台与方案验证 | Qt 6.11.1 ARM64 APK、SAF、真实谱面 QSG → MediaCodec / AAC → MP4，16KB ELF 检查 | 片头、批量、真实设备的编码能力与长任务验证 |
| P1：工程与文件会话 | 独立源码工程、横屏清单、私有工程副本、原子恢复、异步保存快照、素材/字体导入 | 真机授权与生命周期、Android 12 / 16KB 设备兼容性验证 |
| P2：编辑与分析 | 原生 EditorPane / SourceEditor、完整谱面信息表单、名义管理、多难度、v2 高亮与输入桥、分难度撤销、整理核心、真实素材加载 | 操作菜单完整接入、异步检测和时间轴联动验收、触摸/IME 行为、保存与元数据脏状态一致性 |
| P3：时间轴与预览 | v2 PreviewPane、原生 QSG 场景、背景、六类音符统计、真实波形与 PV；已接入 v2 BottomPanel / TimelineQuickItem / 异步 AnalysisService | 完整音效、速率/跳转/音频时钟、预览参数、多比例布局及真机性能 |
| P4：作品导出 | 真实谱面选区 MP4 与独立 WAV 已在 Android 模拟器完成；复用 v2 场景、PV、音效计划和静止前导；后台允许/禁止切至 Home 的分支已验证 | 接入实际 v2 导出页；片头安卓曲绘黑块、批量队列、SAF 发布、封面图层与预设、锁屏和系统中止验证 |
| P5：交付验收 | 验收要求已明确 | 完整功能回归、90% 界面对照、性能与设备矩阵、正式发布签名、最终 APK |

## 已有验证

- 宿主基础测试：文件会话、快照保存、权限失败、恢复、UTF-8、未知字段、多难度和未完成语法。
- 编辑器实际 UI 测试：输入写回、难度隔离、切换后保留撤销历史。
- `白金ディスコ` 实际工程：正确加载 Master、等级 13、谱面文本和高亮；预览统计为 Tap 394、Hold 34、Slide 82、Touch 0、Break 36、Total 546。
- 原始 `maidata.txt`：2348 字节，SHA256 `5e6bcc786b67b764938188f59d3bbf54efcbb508b4b2385b92248d25e10926a1`。测试入口复制工程后才允许编辑和恢复写入。
- 最近验证截图：`build-devtools/host/real-chart-preview.png`。该截图仅证明已接入功能，尚未证明 90% 界面相似度。
- 更新截图：`build-devtools/host/v2-chrome-timeline-ten.png`，已显示 v2 标题栏、主菜单、工具栏、Maple Mono 编辑字体、真实波形和 10 秒处音符/轨道。基础测试与编辑器 UI 测试再次通过。
- 宿主异常：MSVC 19.36 / Qt 6.11.1 的优化版 slide 轨道遍历在该工程 10 秒处崩溃；宿主验证仅对 `PreviewTrackLayerState.cpp` 使用 `/Od` 后，标准线程渲染循环截图通过。根因尚待进一步确认。Android Clang Release 保留优化并需单独验证；此处理不构成 Android 兼容性结论。
- Android 场景测试 APK：`build-devtools/android-arm64-6.11.1/MiaCodeMobile-arm64-test.apk`，已安装模拟器。92 个原生库均为 ARM64，所有 ELF LOAD 对齐通过 16KB 检查，`zipalign -c -P 16 4` 通过。仍需真实 16KB 设备验证。
- 打包问题已定位：Qt 提供的 `libavformat.so` 原始 ELF 对齐正确，Gradle 的 NDK r27 strip 输出破坏一个 LOAD 段的文件偏移。独立 `packaging/android/build.gradle` 保留该预编译库后，92 个库的审核全部通过。
- 手机高度适配：按真实标题栏、工具栏、预览与时间轴的最小几何计算工作区缩放；保留完整 v2 面板，并让触摸命中与绘制共用同一变换。模拟器中原先因高度不足而关闭的预览场景现已显示。

## 预览区拖拽卡住修复（2026-10-01）

模拟器在拖拽预览与编辑器之间的分隔条后，应用 Qt 主线程持续占用一个核心，日志反复报告六个统计单元无法放入 3×1 Grid；Android 系统服务仍能响应。根因是整体缩放比例读取预览区的动态最小高度：统计区宽度跨过 528 时，一行/两行切换改变高度，高度改变缩放，缩放又改变宽度，造成循环。

修复让整体缩放使用与宽度无关的 `PreviewPane.stableMinimumHeight`，统计区仍按实际宽度切换一行和两行；Grid 只指定列数，由 Qt 按单元数量推导行数，避免两项绑定分别更新时出现暂时的 3×1 约束。

- Android Clang Release APK 已重新构建并安装到 `emulator-5554`。
- 使用 `白金ディスコ` 的私有副本，在实际播放时拖拽分隔条，完成两行→一行→两行切换；PV、音符和时间轴在 33 秒及 65 秒处正常推进。
- 截图：`build-devtools/android-ui/drag-fix-widest.png`、`drag-fix-narrow-again.png`；修复后的对应日志没有 Grid 容量、绑定循环和着色器编译错误。
- 宿主 Release 自动回归 `--layout-smoke` 通过：连续 18 次调整预览宽度，确实覆盖一行和两行统计布局，并检查全局缩放稳定及 Grid 容量；日志 `build-devtools/host/drag-layout-smoke.log`。实际 EditorPane 的 `--ui-smoke` 输入、难度隔离与保留撤销测试通过，日志 `drag-ui-smoke.log`。
- 实际 v2 谱面信息表单：`build-devtools/android-ui/v2-metadata-android-late.png`；全屏布局：`v2-fullscreen-android-late.png`。这些证据不等同于 90% 相似度验收。
- 最新 APK 的 92 个 ARM64 原生库通过 16KB ELF LOAD 检查，`zipalign -c -P 16 4` 返回 0。报告：`build-devtools/android-arm64-6.11.1/drag-fix-apk-audit.json`；SHA256：`39c3ecfb968b1c4493767d993b89c662ff895c197214962a94da1e5928972c8a`。真实 16KB 设备验证仍待完成。
- 基础测试新增媒体覆盖与恢复、偏移/额外字段、七个谱师名义及统一名义、`pv.mp4` 自动识别和移除后恢复等用例，已通过。读取标题/曲师暂使用当前导入音频，独立音频选择器入口仍需补齐。

## 真实谱面导出核心（2026-10-01）

已新增 Android 导出任务：快照捕获谱面、素材、渲染参数及 v2 音效时间计划；使用实际 `PreviewQuickExportSession` 渲染，再由 MediaCodec / AAC / MediaMuxer 写入 MP4。原生编码在专用线程执行，帧队列容量为 2，音频先按块解码到私有暂存文件，再按块混音为 WAV。该任务已通过命令行验证入口运行，尚未接入实际 `ExportVideoPage`，因此 P4 仍在制作中。

- 实际工程 `白金ディスコ`，选区 10–15 秒，静止前导 1.5 秒，720×720 / 30fps；最终文件时长 6.5 秒，195 帧。MediaExtractor 检查帧数、尺寸、视频时间戳和 AAC 音轨，通过后才提交最终文件。
- 初次输出存在背景取样错误。`QSGSimpleTextureNode.setSourceRect` 接收相对纹理尺寸的像素矩形，既有层把矩形提前转换为归一化坐标，导致再次归一化后只取左上角极小区域。已修正独立移动工程的外层及内圈背景取样。接口依据：[Qt 文档](https://doc.qt.io/qt-6/qsgsimpletexturenode.html#setSourceRect)。
- 修正后的真实 Android 文件：`build-devtools/android-ui/chart-export/qa-chart-fixed.mp4`。再用宿主 Qt Multimedia 从最终 MP4 解码 0、3.2、6.4 秒取帧，画面包含实际 PV、音符和轨道：`decoded-fixed/decoded-1.png`。容器检查：`fixed-container.json`，任务检查：`fixed-report.json`。宿主静态背景的实际渲染样本：`build-devtools/host/export-qa-850aa1d1454441f58a6fb4cdad9505ab/chart-clip-000097.png`。
- 真实混音 WAV：48kHz、双声道、16bit、312000 音频帧；静止前导保持静音，正文包含 BGM 和 42 次谱面音效。宿主片头任务按 v2 时间规则得到 12.833333 秒 WAV，片头音频有实际信号；这不替代 Android 片头画面验证。音频检查：`audio-verification.json`。
- 继续优化：静止前导及负时间片头复用同一 PV 帧，动态 PV 使用单个保留纹理，避免逐帧进入静态图片缓存。逐帧 PV 读取目前仍使用 MediaMetadataRetriever，模拟器导出明显慢于实时，连续解码优化及真机性能验证待完成。
- 首个导出 APK（缓存优化之前）：92 个 ARM64 库均通过 ELF LOAD 16KB 检查，`zipalign -c -P 16 4` 返回 0；SHA256 `4ffee03c8351e65ec6757027b47c6b74bbb9456c575b9ff85b6e455024af37a0`。记录仅对应该构建，后续更新 APK 需重新审核。
- 当前回归：宿主实际布局测试覆盖 18 次统计区宽度变化，通过；基础 CTest 通过；无媒体的软件渲染 UI 测试通过输入、难度隔离和撤销。一次带 PV 的 UI 测试超时，须继续检查宿主媒体/渲染生命周期，不能把超时记录算作通过。
- 原始 G 盘 `maidata.txt` 哈希再次检查保持 `5e6bcc786b67b764938188f59d3bbf54efcbb508b4b2385b92248d25e10926a1`。

后台执行使用用户设置控制的前台服务。服务包含进度和取消入口，Qt 的后台运行保持开启。模拟器验证：允许后台时，片头任务切至 Home 后仍完成 385 帧；不允许后台时，等待任务开始后切至 Home，任务报告 `Export cancelled`，最终 MP4 未提交（`background-denied-verification-2.json`）。首次测试在任务开始前过早切到 Home，未取得任务报告，属于无效测试；已保留记录，未算作通过。测试结束恢复原来的后台设置与文件所有权。通知权限、锁屏、系统中止、超时与导出途中取消仍需验证。

独立 WAV 输出已在模拟器完成，文件 `build-devtools/android-ui/chart-export/qa-chart.wav`，任务 `wav-report.json`；不要求经过视频渲染。音频混音新增 `MobileExportAudioSpec`，用独立双声道信号核对源偏移、增益、定时音效、持续音效截断、输出时长以及取消后无最终文件，已与基础 CTest 一起通过。

Android 片头的初次容器检查通过，但解码画面发现漏打包 `bg_texture.frag.qsb`；已补充 ShaderTools 构建资源，宿主 3.2 秒片头样本恢复模糊背景与曲绘（`build-devtools/host/intro-shader-qa-d7c2b740af3d4136aa8c0770ae17b542/chart-intro-000096.png`）。Android 重建后模糊背景和谱师名义恢复，但曲绘槽仍是黑色，部分区域有图像碎片。`decoded-intro-fixed/decoded-1.png` 记录这一未解决问题，不能作为片头画面通过证据。分别关闭 mipmap、关闭 jacketSlot 裁切、限制 jacket 图片 sourceSize 后，实际 MP4 解码结果仍异常；三项试验均已撤回，保留 v2 的原有图片行为。后续需诊断 Android GLES / Qt Quick 离屏图像渲染，不能把这些无效试验当作修复。

## v2 / Mobile 工作区分离（2026-10-01）

已将 v2 中早期移动原型、脚本及迁移文档移至本工程审阅。7 个跟踪文件的迁移改动保留 patch 后恢复到 v2 HEAD；v2 `git status --porcelain` 为空，`git diff --check` 通过。用户随后授权删除无用原型：23 个早期文件均有当前对应版本（11 个完全相同，12 个已被新实现/文档替代），已删除这些文件及 7 个冗余工作副本，仅保留 `docs/handoff/v2-migration-residue-20261001` 内的 patch、旧哈希清单与审阅报告。现有 Mobile 源码保持独立；Qt / SDK 仍复用 v2 的 Git 忽略目录作为本机工具链，构建脚本可接受其他安装位置。后续在 Mobile 工程内继续开发，v2 作为只读参考。

## 实际 v2 导出页与设置联动（2026-10-02）

Mobile 已装配实际 `ExportVideoPage`、`ExportSidebarPage`、`ExportSession` 和 `PreviewSettingsModel`，并保留原请求弹窗及任务进度层。四类判定效果、皮肤与效果风格、内建/自定义外框、背景缩放及暂停判定区已接入真实预览；导出任务在后台加载启动时指定的皮肤与外框，避免异步预览加载或暂停显示状态污染离屏渲染。最终私有文件复制也移到后台线程，并检查取消。

- 宿主 Release 的实际导出页 smoke 通过：设置修改进入运行时/任务快照；调用实际选区试听按钮后从区间外回到 10 秒，接近终点时播放并在 15 秒停止；由实际 `ExportSession.startExport()` 输出 48kHz、双声道、16bit、312000 帧的 WAV（5 秒正文加 1.5 秒冻结前导）。证据目录 `build-devtools/host/export-settings-qa-8d8f85b494724bc8a40134f3935c90a2` 包含日志、导出页截图及 `verification.json`。首次脚本选取 0.25 秒区间，被 v2 5 秒最短区间规则纠正，脚本已修正后重测通过；未放宽产品规则。
- `AndroidFoundationSpec` 与 `MobileExportAudioSpec` 通过。拖拽回归 `--layout-smoke` 再次完成 18 次宽度变化，日志 `build-devtools/host/layout-regression-8a10e66327cc43729bcad2723553cf7e/test.log`。
- 基于上述宿主构建的离屏静态背景样本再次输出成功：`build-devtools/host/export-snapshot-qa-4917120d8da54dc7928b680685c52eaa/samples-000097.png`。此验证在 SAF 新代码集成之前，不代替当前安卓版本的验证。

新接入的 Android 文件请求桥将 v2 的素材文件选择转换为 SAF 导入；输出文件/目录选择绑定私有路径与目标 URI。发布使用后台流式复制，并重新读取目标文件核对大小与 SHA256，校验结束前任务保持运行。失败时保留私有完整文件；文件提供方不保证事务替换，中断写入可能留下不完整的目标文件。目录输入/输出能力已接入文件请求，批量调度本身仍待实现。新增 `ExportDestinationSpec` 检查单文件与目录授权范围、路径规范化及优先级。

这一阶段的 SAF 桥、异步复制及新测试的后续验证见下一节。批量、封面、完整设置持久化/音效、片头曲绘、编码质量/文件体积选项的原生落实及比例/90% 截图验收仍未完成。

## 实际界面 SAF 导出与弹出层适配（2026-10-03）

实际 `ExportVideoPage` 的文件名、浏览按钮、选区输入及开始导出按钮已在模拟器操作验证。通过 Android 系统文件选择器选择 Downloads，保留 URI 授权并以后台流式复制发布最终文件，发布后重新读取提供方文件核对 SHA256。此验证使用完整谱面测试副本，原始 G 盘 maidata.txt 的哈希仍为 `5e6bcc786b67b764938188f59d3bbf54efcbb508b4b2385b92248d25e10926a1`。模拟器时钟仍显示 10 月 2 日，记录日期按工作区环境为 10 月 3 日。

- WAV：10–15 秒选区加 1.5 秒冻结前导，48kHz、双声道、16bit、312000 帧、6.5 秒、1248044 字节。Downloads 与应用内完整文件逐字节一致，SHA256 `1953717aaf16019bae38e5ebfdee883c4bb9e29cfb4a214246f0883c702a8e7b`；证据 `build-devtools/android-ui/saf-export-qa-20261002/verification.json` 与 `report.json`。测试 APK 为本节输入提示修正之前的构建。
- MP4：从实际界面设置 30fps 与 10–15 秒选区，最终 **1024×1024**、195 帧、6.5 秒、304 个 AAC 音频包，PV 已启用，最后视频 PTS 6466666 微秒。公有/私有文件相同，SHA256 `8b371194985521eefa233428fb8ec2e0be2da330e4a99cdf78a25e1d644b2fc0`。证据 `build-devtools/android-ui/saf-export-qa-20261003/`，实际解码 0/3.2/6.4 秒样本在 `decoded/`，已查看正文样本包含 PV、Tap/Hold 与轨道。该次 APK 哈希 `0201fcc13982bdca2e29d99ef17fdf4113dc911e8e197d048d23067b0285405d`。
- 分辨率测试初次尝试点击 720 时，首项部分滚出列表且点击未生效，输出保持 1024；没有将该次视频记为 720 通过。后续修复后，720 预设从 UI 点击切换已通过，证据 `popup-first-option-verification-20261003.json`；最终 APK 的 720 视频输出尚未在本轮重跑。
- 实际选区试听在 15 秒停止，PV 和谱面对象正常显示，截图 `saf-range-paused-20261003.png`。SAF 取消已确认从系统选择器返回应用后保留文件名，证据 `saf-picker-cancel-20261003.json`；系统 Back 在选择器子目录先返回父目录，须退出选择器根目录才能验证取消。

界面适配修正：文件名输入关闭预测/自动大写并偏好 Latin；选区输入请求数字键盘，实际数字键盘截图 `saf-numeric-ime-20261003.png`。任务进度和完成提示显示用户选定的 `Download/...`，不再显示内部暂存位置；文件名输入框本身仍显示内部路径，后续需要完整的显示/编辑投影。`AndroidMain` 将 Overlay 的逻辑尺寸与 transform 对齐工作区，解决弹出层未继承缩放造成的字号/尺寸差异；实际恢复对话框居中且遮罩覆盖完整工作区。`AppComboBox` 打开时定位到当前选择的前一项，避免第一项部分滚出视口。原始、缩放修正与列表定位修正截图分别为 `popup-scale-before-20261003.png`、`popup-scale-after-visible-20261003.png`、`popup-first-option-visible-20261003.png`。依据：[Qt Popup 的 Overlay 缩放说明](https://doc.qt.io/qt-6/qml-qtquick-controls-popup.html)。

验证：三项 CTest（AndroidFoundationSpec、ExportDestinationSpec、MobileExportAudioSpec）通过；新增目标显示路径检查与授权范围检查均通过。最终宿主实际导出页设置、选区试听与 WAV 输出回归通过，证据 `build-devtools/host/popup-final-export-ui-8675b420097643b89371045a7e84231a`。Overlay 修改后的 18 次统计区宽度变化通过，日志 `build-devtools/host/popup-scale-layout-20261003.log`。Android 与宿主均增量 Release 构建成功，无清理构建产物。

最终测试签名 APK 已安装到 emulator-5554，SHA256 `e577a8c1d04efe4660944ccb9fd26902c0dd98f982079436fc728a0a4c16542e`。92 个 ARM64 库 ELF LOAD 16KB 与 zipalign -P 16 再次通过，报告 `build-devtools/android-arm64-6.11.1/popup-final-apk-audit-20261003.json`。正式签名和真实 16KB 设备验证仍待完成。当前证据支持单次实际界面的 WAV/MP4 发布流程及弹出层修正，不能作为批量/封面或 P5 完成结论。

## UI 验收方法

在相同工程、难度、主题、预览时间和内容状态下获取 v2 与 Android 截图，按对应面板对齐。保留原始截图、对齐方式和差异图。权重：工作区结构 30%、编辑器/元数据 25%、预览与时间轴 25%、菜单/设置/导出 20%。逐项检查布局、字体、间距、颜色、图标、边框与交互状态，达到加权 90% 且无主要面板缺失才可通过。

比例矩阵：16:9、19.5:9、20:9、21:9、4:3、16:10。检查系统安全区、软键盘、全屏预览、弹窗、字体导入和手势命中区域。允许按用户屏幕比例分配面板，不能用裁剪来掩盖缺失内容。

## 产品功能验收

编辑：文本、语法高亮、多难度、整理、撤销/重做、查找替换、元数据、保存和恢复。

检测：原生解析、语法诊断、动态无理分析；快速编辑与切难度后拒绝旧结果；点击诊断定位正确。

预览：真实音符与波形、播放线、缩放、音频/PV/音效同步、拖动和跳转、编辑器跟随、全屏与生命周期恢复。

导出：真实谱面 MP4/WAV、选区/片段、片头、批量、封面背景/字体/图片/文本/谱面帧/预设；无网络条件下成功；取消和失败可恢复；后台权限按用户选择执行。

发布：API 31、ARM64、16KB、手机和平板、代表设备性能、来源与许可证、正式签名 APK。测试签名 APK 和编码探针不能替代这些验收项。
