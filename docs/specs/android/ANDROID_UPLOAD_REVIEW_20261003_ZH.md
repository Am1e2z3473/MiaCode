# Android 开发快照上传审阅

日期：2026-10-03。分支：`Miacode_Mobile`。来源提交：`c190bb2c138cf61032dd7ac97ec41027da4bb40d`。

本次上传独立 Android 产品工程的当前开发快照。P5 目标继续执行，完整功能和 UI 相似度尚未验收。

## 审阅范围

- CMake 产品入口改为 Android，最低 API 31、ARM64；Windows 宿主只用于验证。
- `src/android` 装配实际 v2 编辑器、预览、时间轴、导出页、设置页和文件请求服务。
- Android Activity / SAF 桥负责输入副本、私有编辑会话与导出文件发布；编码器使用设备上的 MediaCodec / MediaMuxer。
- 预览拖拽布局、弹出层缩放、下拉首项可达性、输入法提示、QSG 背景源矩形及共享导出暂停图层。
- Android 构建、APK 对齐审计、实际 UI 测试与 QML 依赖清单工具。
- 相对 v2 来源，移除 Mobile 初始独立目录未携带的桌面打包、CI、第三方桌面运行库和历史资料。保留项目许可证、素材许可证、鸣谢及第三方声明。

上传的是 Mobile 工作目录；v2 工作目录和分支未被修改。已核对暂存区，构建产物、Qt / SDK 安装、签名密钥、测试媒体、导出文件、本机会话交接及本机模拟器快捷脚本均不进入提交。

## 已完成的上传前验证

所有验证均在 `build-devtools` 中执行，Release，最多 4 个构建任务，一次一个构建。

| 项目 | 结果 | 本地证据 |
| --- | --- | --- |
| 宿主完整目标图构建 | 通过 | `host/upload-review-build-20261003.log` |
| AndroidFoundationSpec / ExportDestinationSpec / MobileExportAudioSpec | 3 项通过 | `host/upload-review-ctest-20261003.log` |
| 实际 v2 ExportSession 页面回归 | 通过；设置到运行时的联动、10–15 秒选区试听及 WAV 导出 | `host/upload-export-ui-97b5d723a69c496687300ddf8b5cd83a` |
| 回归 WAV 文件解析 | 48kHz / 双声道 / 16bit / 312000 帧，6.5 秒 | 上述目录的 `verification.json` 与 `chart-ui.wav` |
| Android Release 构建和测试签名验证 | 通过 | `android-arm64-6.11.1/upload-review-build-20261003.log` |
| APK 原生库与 ZIP 对齐 | 92 个 ARM64 库 ELF LOAD 16KB 通过；`zipalign -c -P 16 4` 通过 | `android-arm64-6.11.1/upload-review-apk-audit-20261003.json` |
| 已使用的 QRC 资源完整性 | 引用文件存在且进入 Git 暂存区 | `upload-review-source-20261003.json` |
| 新增及修改文本、签名材料和大文件核查 | UTF-8 检查、定向凭据检查通过；无签名密钥、APK 或超过 50MiB 的工作树文件进入提交 | `upload-review-source-20261003.json` |
| 暂存差异格式 | `git diff --cached --check` 通过 | Git 原生命令 |

APK：`build-devtools/android-arm64-6.11.1/MiaCodeMobile-arm64-test.apk`。

SHA256：`e577a8c1d04efe4660944ccb9fd26902c0dd98f982079436fc728a0a4c16542e`。

Android 实际单次 SAF 视频和 WAV 的既有验证详见 `ANDROID_P5_WORKLOG_ZH.md` 的 2026-10-03 记录。本次宿主验证不替代 Android 编码验证或真机验收。

## 仍待完成

- v2 批量导出接口仍待接入 Android 队列；本次未提交已完成批量导出的声明。
- 封面编辑与预设、批量封面、片头曲绘、完整预览设置与音效、编码质量和体积预设仍需制作或补齐。
- SAF 发布失败或取消时保留私有完整文件；提供方可能留下不完整的输出，恢复和清理体验需要后续完善。
- 锁屏、系统回收、权限撤回、真机性能及真实 16KB 设备行为仍待验证。
- 手机和平板各比例矩阵，以及相同内容的 v2 / Android 截图相似度至少 90% 的验收尚未完成。
- 当前 APK 使用开发测试签名，正式发布签名和发布验收另行完成。

上述事项保留在 P5 目标范围内，本次上传仅用于保存可继续开发的源码状态。
