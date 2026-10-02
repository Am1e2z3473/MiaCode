# MiaCode Mobile

MiaCode v2 的独立 Android 产品工程。目标为 Android 12 / API 31 及以上的 ARM64 手机和平板，APK 发布，本机离线完成编辑、预览和作品导出。界面强制横屏，复用 v2 的 QML 组件、主题、图标和谱面核心。

## 当前状态

开发目标为 P5，当前仍在实现和验证 P2 / P3 / P4。已经接入 v2 文本编辑器、多难度、语法高亮、撤销历史、谱面整理核心、原生预览场景、时间轴、波形及异步检测。实际 v2 导出页已在 Android 模拟器上验证单次选区 MP4 / WAV 导出，经 SAF 保存到 Downloads，最终文件与应用私有完整文件的 SHA256 一致。批量导出、封面编辑与批量封面、部分预览设置和片头曲绘仍未完成，设备与后台行为也需要继续验证。

UI 验收要求为与 v2 至少 90% 相似。这个要求尚未完成截图对照验收；组件复用数量不作为相似度结论。详细进度与验收口径见 [迁移记录](docs/specs/android/ANDROID_P5_WORKLOG_ZH.md)。

## 构建

工程不依赖原 v2 的源码或构建目录。Qt、Android SDK / NDK 和 JDK 可以共用本机安装，使用脚本参数明确指定路径。所有构建和测试产物写入 `build-devtools`，Release，最多 4 个任务。

Android 构建入口为 `scripts/build/build-android.ps1`；具体参数以脚本 `param` 为准。开发测试签名不作为正式发布签名。

Windows 宿主仅用于验证同一套 Android QML、谱面核心和场景：

```powershell
cmake -S . -B build-devtools/host -G "Visual Studio 17 2022" -A x64 -DMIACODE_ANDROID_HOST_PROBE=ON -DCMAKE_PREFIX_PATH=<Qt-host-root>
cmake --build build-devtools/host --config Release --parallel 4
ctest --test-dir build-devtools/host -C Release --output-on-failure
```

在 Qt 运行库、插件和 QML 模块路径已配置时，可使用宿主 `MiaCodeAndroid.exe --fixture <maidata.txt> --storage-root <test-storage> --capture <output.png> --size 1280x720 --preview-second 10` 截图。`--fixture` 会先复制工程到测试私有目录，编辑和恢复写入不会改动输入工程。

测试使用自行提供的 simai 工程与媒体文件，测试副本和截图保存在 `build-devtools`；原始输入保持只读。测试素材、导出视频和本机签名密钥不进入 Git。

更新 QML 组件清单：`python scripts/build/mobile-qml-closure.py`。

## 文件与权限

Android 使用 SAF 选择工程、素材和输出位置；编辑会话与恢复记录保存在应用私有目录。用户决定是否允许后台导出。正式导出需要继续验证权限撤回、取消、锁屏、系统回收和批量任务恢复。

源码来源和版本记录在 `migration-origin.json`。v2 Linux 版本与 Visual Maimai Mobile 用于适配参考。许可证与素材授权记录保留在 `LICENSE`、`LICENSE_SCOPE.md` 和 `licenses`，发布前需要完成最终核对。
