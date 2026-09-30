---
name: qt-ui-design
description: 设计或审查 MiaCode 的 Qt/QML 页面、导航、交互、主题与键盘访问。
license: LicenseRef-Qt-Commercial OR BSD-3-Clause
metadata:
  author: qt-ai-skills
  version: "1.0"
  qt-version: "6.x"
  category: conceptual
---

# Qt/QML 界面设计

共享规则见 [AGENTS.md](../../../AGENTS.md)，组件入口见 `src/app/ui/components/`。

## 设计

- 根据任务和相邻页面确定操作目标、内容优先级、窗口尺寸、输入方式、语言与主题。涉及产品决策的缺失信息向用户确认。
- 沿用现有页面结构与共享控件。表单用 LabeledCombo/LabeledSlider，对话框用 AppDialog/DialogFooter，菜单用 AppMenu/AppMenuItem。
- 布局表达内容层级、操作分组和窗口缩放关系；尺寸与视觉参数从 Theme 获取。
- 操作状态覆盖可用性、进行中、完成和错误反馈；耗时任务接入 JobProgressService。
- 检查键盘焦点、快捷键、文本输入、长文案和主题切换；图标与文本共同表达关键动作。
- 布局或裁剪诊断使用 `qt-ui-layout-pitfalls` 的症状表。

## 审查

结合源码与可用渲染资料核对布局、操作路径、焦点、文字溢出、主题和命中区域。
报告具体位置、观察证据、用户影响和修改建议；实现推断与渲染观察分别标明。

本仓库版本依据 Qt UI Design 技能裁剪，许可见 [LICENSE.txt](LICENSE.txt)。
