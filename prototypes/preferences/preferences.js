const { translations, shortcuts, aboutInfo } = window.preferenceData;
const storageKey = 'miacode.preferences.prototype';
const defaults = {
  languageToken: 'zh_CN', efficientSidebarEnabled: false,
  themeModeToken: 'dark', lightThemeToken: 'light', darkThemeToken: 'dark',
  previewOnLeft: false, blurMaterialsEnabled: true,
  enabled: false, imagePath: '', opacity: .35, panelAlpha: 230, blur: 0,
  sizeMode: 'cover', position: 'center',
  editorFontSize: 12, editorLineSpacing: 1, editorInputHandlingMode: 0,
  editorAutoWrap: true, editorAutoCompletion: true, editorScrollPastEnd: true,
  editorSelectionBeatDisplay: true, editorOverviewRulerEnabled: true,
  videoDecodePrefersSoftware: false, canvasFrameRateMode: 0,
  stageMediaFrameRateMode: 0, timelineFrameRateMode: 0, checkEnabled: true,
  shortcutBindings: {}, lastCheckText: ''
};
let saved = {};
try { saved = JSON.parse(localStorage.getItem(storageKey) || '{}'); } catch { /* Use defaults when browser storage is unavailable. */ }
const state = { ...defaults, ...saved };
const dialog = document.querySelector('#preferences');
const tabs = document.querySelector('#tabs');
const panels = document.querySelector('#panels');
const pages = [
  ['interface', 'qml.interface'], ['background', 'dialog.preferences.background_group'],
  ['editor', 'dialog.preferences.editor_group'], ['performance', 'dialog.preferences.performance_group'],
  ['shortcuts', 'dialog.preferences.shortcuts_group'], ['updates', 'dialog.preferences.updates_group'],
  ['about', 'action.about']
];
let activePage = 'interface';
let capturingId = '';
let imageUrl = '';
let updatePending = false;
let updateOutcome = '';

function t(key) {
  return translations[state.languageToken]?.[key] || translations.zh_CN[key] || key;
}
function el(tag, attributes = {}, children = []) {
  const node = document.createElement(tag);
  for (const [key, value] of Object.entries(attributes)) {
    if (key === 'text') node.textContent = value;
    else if (key === 'class') node.className = value;
    else node.setAttribute(key, value);
  }
  node.append(...children);
  return node;
}
function save() {
  const { imagePath, ...persisted } = state;
  try { localStorage.setItem(storageKey, JSON.stringify(persisted)); } catch { /* Controls remain usable without browser storage. */ }
}
function set(key, value) {
  state[key] = value;
  save();
}
function button(label, onClick, attributes = {}) {
  const node = el('button', { type: 'button', text: label, ...attributes });
  node.addEventListener('click', onClick);
  return node;
}
function select(key, labelKey, options) {
  const input = el('select', { id: key, name: key });
  for (const [value, label] of options) {
    const option = el('option', { value: String(value), text: label });
    option.selected = state[key] === value;
    input.append(option);
  }
  input.addEventListener('change', () => {
    set(key, options[input.selectedIndex][0]);
    if (key === 'languageToken') render();
    if (['themeModeToken', 'lightThemeToken', 'darkThemeToken'].includes(key)) {
      applyTheme();
      render();
    }
    if (['sizeMode', 'position'].includes(key)) applyBackground();
  });
  return el('div', { class: 'row' }, [el('label', { for: key, text: t(labelKey) }), input]);
}
function toggle(key, labelKey) {
  const input = el('input', { type: 'checkbox', class: 'toggle', id: key, name: key, role: 'switch' });
  input.checked = state[key];
  input.addEventListener('change', () => {
    set(key, input.checked);
    if (key === 'enabled') applyBackground();
    if (key === 'blurMaterialsEnabled') dialog.style.backdropFilter = input.checked ? 'blur(24px)' : 'none';
  });
  return el('label', { class: 'row toggle-row', for: key }, [input, el('span', { text: t(labelKey) })]);
}
function slider(key, labelKey, min, max, step, format, scale = 1) {
  const input = el('input', { type: 'range', id: key, name: key, min, max, step, value: state[key] / scale });
  const output = el('output', { for: key });
  function update() {
    output.textContent = format(Number(input.value));
    input.style.setProperty('--fill', `${(input.value - min) / (max - min) * 100}%`);
    input.setAttribute('aria-valuetext', output.textContent);
  }
  input.addEventListener('input', () => {
    set(key, scale === 1 ? Number(input.value) : Math.round(input.value * scale));
    update();
    if (['opacity', 'panelAlpha', 'blur'].includes(key)) applyBackground();
  });
  update();
  return el('div', { class: 'row' }, [el('label', { for: key, text: t(labelKey) }), el('div', { class: 'slider-control' }, [input, output])]);
}
const translatedOptions = (prefix, values) => values.map(value => [value, t(prefix + value)]);
const groupLabels = {
  appearance: ['外观', 'Appearance', '外観'], workspace: ['工作区', 'Workspace', 'ワークスペース'],
  image: ['背景图片', 'Background image', '背景画像'], presentation: ['显示效果', 'Presentation', '表示効果'],
  text: ['文字排版', 'Typography', '文字組み'], input: ['输入', 'Input', '入力'],
  display: ['编辑辅助', 'Editing aids', '編集補助'], video: ['视频', 'Video', '動画'],
  rendering: ['渲染', 'Rendering', '描画'], transform: ['谱面变换', 'Chart transforms', '譜面変換'],
  playback: ['播放', 'Playback', '再生'], timeline: ['时间轴', 'Timeline', 'タイムライン'],
  editor: ['编辑器', 'Editor', 'エディター']
};
function settingsGroup(name, fields) {
  const index = state.languageToken === 'en_US' ? 1 : state.languageToken === 'ja_JP' ? 2 : 0;
  const title = groupLabels[name][index];
  return el('section', { class: 'settings-group', 'aria-label': title }, [el('h3', { text: title }), ...fields]);
}
function settingsGroups(...groups) {
  return el('div', { class: 'settings-groups' }, groups.flat());
}
function layoutOptions() {
  const group = el('div', { class: 'layout-options', role: 'radiogroup', 'aria-label': t('dialog.preferences.interface_layout') });
  for (const value of [false, true]) {
    const label = t('dialog.preferences.layout.' + (value ? 'preview_editor' : 'editor_preview'));
    const miniature = el('div', { class: 'miniature', 'aria-hidden': 'true' }, [
      el('div', { class: 'mini-title' }),
      el('div', { class: 'mini-body' }, [
        el('div', { class: 'mini-sidebar' }),
        el('div', { class: 'mini-panes' }, [
          el('div', { class: 'mini-editor' }, [el('pre', { text: '(120)\n{4}1,2,3,4,\n5h[4:1],\n6,7,8,' }), el('div', { class: 'mini-timeline' })]),
          el('div', { class: 'mini-preview' }, [el('img', { src: '../../src/app/ui/resources/icons/circle-play.svg', alt: '' }), el('div', { class: 'mini-transport' })])
        ])
      ])
    ]);
    group.append(button('', () => { set('previewOnLeft', value); renderPanel(); }, {
      class: 'layout-card', role: 'radio', 'aria-checked': String(state.previewOnLeft === value),
      'aria-label': label, 'data-value': String(value)
    }));
    group.lastChild.append(miniature, el('span', { text: label }));
  }
  group.addEventListener('keydown', event => {
    if (!['ArrowLeft', 'ArrowRight'].includes(event.key)) return;
    event.preventDefault();
    set('previewOnLeft', !state.previewOnLeft);
    renderPanel();
    panels.querySelector('[aria-checked="true"]').focus();
  });
  return el('div', { class: 'row layout-row' }, [el('span', { class: 'field-label', text: t('dialog.preferences.interface_layout') }), group]);
}
function interfacePage() {
  const palettes = translatedOptions('dialog.preferences.theme.', ['dark', 'light', 'legacy', 'legacy_light']);
  const appearance = [
    select('themeModeToken', 'dialog.preferences.theme.mode', translatedOptions('dialog.preferences.theme.', ['system', 'light', 'dark']))
  ];
  if (state.themeModeToken !== 'dark') appearance.push(select('lightThemeToken', 'dialog.preferences.theme.light_palette', palettes));
  if (state.themeModeToken !== 'light') appearance.push(select('darkThemeToken', 'dialog.preferences.theme.dark_palette', palettes));
  appearance.push(toggle('blurMaterialsEnabled', 'dialog.preferences.blur_materials'));
  return [settingsGroups(
    [settingsGroup('appearance', appearance)],
    [settingsGroup('workspace', [
      select('languageToken', 'dialog.preferences.language', [['system', t('dialog.preferences.language.system')], ['zh_CN', '简体中文'], ['en_US', 'English'], ['ja_JP', '日本語']]),
      select('efficientSidebarEnabled', 'dialog.preferences.sidebar_mode', [[false, t('dialog.preferences.sidebar_mode.standard')], [true, t('dialog.preferences.sidebar_mode.efficient')]]),
      layoutOptions()
    ])]
  )];
}
function backgroundPage() {
  const clear = button(t('dialog.preferences.background.clear'), () => {
    if (imageUrl) URL.revokeObjectURL(imageUrl);
    imageUrl = '';
    set('imagePath', '');
    document.querySelector('#image-picker').value = '';
    applyBackground();
    renderPanel();
  });
  clear.disabled = !state.imagePath;
  return [settingsGroups([settingsGroup('image', [
    toggle('enabled', 'qml.enable_application_background'),
    el('div', { class: 'row file-row' }, [
      el('span', { class: 'file-name', text: state.imagePath || t('qml.no_background_image_selected'), title: state.imagePath }),
      button(t('cover.choose_image'), () => document.querySelector('#image-picker').click()), clear
    ]),
    slider('opacity', 'qml.image_opacity', .1, .8, .01, value => Math.round(value * 100) + '%')
  ])], [settingsGroup('presentation', [
    select('sizeMode', 'qml.scale_mode', translatedOptions('dialog.preferences.background.scale.', ['cover', 'contain', 'stretch', 'center', 'repeat'])),
    select('position', 'dialog.preferences.background.position', translatedOptions('dialog.preferences.background.position.', ['center', 'left', 'right', 'top', 'bottom', 'left_top', 'right_top', 'left_bottom', 'right_bottom'])),
    slider('panelAlpha', 'qml.background_mask_opacity', 0, 1, .01, value => Math.round(value * 100) + '%', 255),
    slider('blur', 'qml.blur_radius', 0, 64, 1, value => String(value))
  ])])];
}
function editorPage() {
  return [settingsGroups([
    settingsGroup('text', [
      slider('editorFontSize', 'dialog.preferences.editor_font_size', 8, 28, 1, value => value + ' pt'),
      select('editorLineSpacing', 'dialog.preferences.editor_line_spacing', [1, 1.5, 2, 3, 5].map(value => [value, String(value)])),
      toggle('editorAutoWrap', 'preferences.editor_auto_wrap')
    ])
  ], [
    settingsGroup('input', [
      select('editorInputHandlingMode', 'preferences.input_handling', [[0, t('preferences.correct_full_width_only')], [1, t('preferences.block_input_methods_and_correct_full_width')], [2, t('preferences.leave_input_unchanged')]]),
      toggle('editorAutoCompletion', 'preferences.auto_completion')
    ]),
    settingsGroup('display', [
      toggle('editorScrollPastEnd', 'preferences.editor_scroll_past_end'),
      toggle('editorSelectionBeatDisplay', 'preferences.editor_selection_beat_display'),
      toggle('editorOverviewRulerEnabled', 'preferences.editor_overview_ruler')
    ])
  ])];
}
function performancePage() {
  const rates = [[0, t('dialog.render_settings.preview.canvas_frame_rate.display')], [1, '30 FPS'], [2, '60 FPS']];
  return [settingsGroups([settingsGroup('video', [
    select('videoDecodePrefersSoftware', 'qml.video_decoding', [[false, t('qml.hardware_decoding')], [true, t('qml.software_decoding')]]),
    select('stageMediaFrameRateMode', 'qml.pv_frame_rate', rates)
  ])], [settingsGroup('rendering', [
    select('canvasFrameRateMode', 'qml.canvas_frame_rate', rates),
    select('timelineFrameRateMode', 'qml.timeline_frame_rate', rates)
  ])])];
}
function shortcutsPage() {
  const lists = Object.fromEntries(['transform', 'preview', 'timeline', 'editor'].map(name => [name, el('div', { class: 'shortcut-list' })]));
  for (const action of shortcuts) {
    const binding = state.shortcutBindings[action.id] ?? action.default;
    const capture = button('', () => {
      capturingId = action.id;
      renderPanel();
      panels.querySelector(`[data-shortcut="${action.id}"]`).focus();
    }, { class: `shortcut-capture${capturingId === action.id ? ' recording' : ''}${binding !== action.default ? ' modified' : ''}`, 'data-shortcut': action.id });
    capture.append(el('span', { text: t(action.label_key) }), el('kbd', { text: capturingId === action.id ? t('qml.recording') : binding }));
    const reset = button(t('qml.restore_defaults'), () => { delete state.shortcutBindings[action.id]; save(); renderPanel(); });
    reset.disabled = binding === action.default;
    lists[action.id.split('.')[0]].append(el('div', { class: 'shortcut-row' }, [capture, reset]));
  }
  return [settingsGroups(
    [settingsGroup('transform', [lists.transform])],
    [settingsGroup('playback', [lists.preview]), settingsGroup('timeline', [lists.timeline]), settingsGroup('editor', [lists.editor])]
  ), button(t('qml.restore_all_defaults'), () => { set('shortcutBindings', {}); capturingId = ''; renderPanel(); }, { class: 'reset-all' })];
}
function updatesPage() {
  const check = button(t('dialog.preferences.update.check_now'), () => {
    updatePending = true;
    updateOutcome = '';
    renderPanel();
    // The frontend prototype presents a local result and never contacts the release service.
    setTimeout(() => {
      updatePending = false;
      updateOutcome = t('dialog.preferences.update.up_to_date');
      set('lastCheckText', new Intl.DateTimeFormat(state.languageToken === 'system' ? undefined : state.languageToken.replace('_', '-'), { dateStyle: 'short', timeStyle: 'short' }).format(new Date()));
      if (activePage === 'updates') renderPanel();
    }, 600);
  });
  check.disabled = updatePending;
  const fields = [toggle('checkEnabled', 'dialog.preferences.update.auto_check')];
  if (state.lastCheckText) fields.push(el('div', { class: 'update-status', text: t('dialog.preferences.update.last_check').replace('%1', state.lastCheckText) }));
  fields.push(el('div', { class: 'update-actions' }, [check, el('span', { class: 'update-status', role: 'status', text: updateOutcome })]));
  return fields;
}
function renderPanel() {
  const scrollTop = panels.scrollTop;
  document.querySelector('#page-title').textContent = t(pages.find(([id]) => id === activePage)[1]);
  const factories = { interface: interfacePage, background: backgroundPage, editor: editorPage, performance: performancePage, shortcuts: shortcutsPage, updates: updatesPage, about: aboutPage };
  panels.replaceChildren(el('section', { class: 'page', role: 'tabpanel', id: `panel-${activePage}`, 'aria-labelledby': `tab-${activePage}` }, factories[activePage]()));
  panels.scrollTop = scrollTop;
}
function choosePage(id, focus = false) {
  capturingId = '';
  activePage = id;
  for (const tab of tabs.querySelectorAll('[role="tab"]')) {
    const selected = tab.id === `tab-${id}`;
    tab.setAttribute('aria-selected', String(selected));
    tab.tabIndex = selected ? 0 : -1;
  }
  panels.scrollTop = 0;
  renderPanel();
  if (focus) document.querySelector(`#tab-${id}`).focus();
}
function render() {
  document.documentElement.lang = state.languageToken.replace('_', '-');
  document.title = t('dialog.preferences.title');
  document.querySelector('#close-preferences').setAttribute('aria-label', t('action.close'));
  document.querySelector('#close-preferences').title = t('action.close');
  document.querySelector('#open-preferences').textContent = t('dialog.preferences.title');
  const pageTabs = pages.map(([id, key]) => {
    const tab = button('', () => choosePage(id), { id: `tab-${id}`, role: 'tab', 'aria-controls': `panel-${id}`, 'aria-selected': String(id === activePage), tabindex: id === activePage ? '0' : '-1' });
    tab.append(el('span', { text: t(key) }));
    return tab;
  });
  const aboutTab = pageTabs.pop();
  tabs.replaceChildren(...pageTabs, el('div', { class: 'sidebar-bottom' }, [
    aboutTab, el('div', { class: 'sidebar-version', text: '版本号：' + aboutInfo.version })
  ]));
  renderPanel();
}
function aboutPage() {
  return [
    el('div', { class: 'about-brand' }, [
      el('img', { src: '../../resources/icons/app.png', alt: '', width: '48', height: '48' }),
      el('div', {}, [el('h3', { class: 'about-name', text: 'MiaCode' }), el('div', { class: 'about-version', text: 'v' + aboutInfo.version })])
    ]),
    el('dl', { class: 'about-details' }, [
      el('dt', { text: t('about.platform') }), el('dd', { text: aboutInfo.platform }),
      el('dt', { text: t('about.build_type') }), el('dd', { text: aboutInfo.buildType })
    ])
  ];
}
function applyTheme() {
  const dark = state.themeModeToken === 'dark' || (state.themeModeToken === 'system' && matchMedia('(prefers-color-scheme: dark)').matches);
  const palette = dark ? state.darkThemeToken : state.lightThemeToken;
  const light = palette === 'light' || palette === 'legacy_light';
  const colors = light ? {
    surface: '#f4f6f8', panel: '#ebeff3', elevated: '#ffffff', border: '#c8d2de', separator: '#c8d2de', text: '#3d4856', secondary: '#5d6b7c', accent: '#526f98', hover: '#e5eaf0', selected: '#d9e7f8', track: '#c8d2de', knob: '#7e8da0', 'checked-track': '#a3b9d3', 'checked-knob': '#ffffff'
  } : { surface: '#121314', panel: '#191a1b', elevated: '#202122', border: '#333536', separator: '#2a2b2c', text: '#e8e8e8', secondary: '#aeaeae', accent: '#297aa0', hover: '#2c2d2e', selected: '#333536', track: '#333536', knob: '#808284', 'checked-track': '#374b5a', 'checked-knob': '#d6dce2' };
  if (palette === 'legacy') Object.assign(colors, { surface: '#151a20', panel: '#1b2129', accent: '#4f8fec', selected: '#315d9e' });
  if (palette === 'legacy_light') Object.assign(colors, { surface: '#f3f3f3', panel: '#e8e8e8', accent: '#0078d4' });
  for (const [name, value] of Object.entries(colors)) document.documentElement.style.setProperty(`--${name}`, value);
  document.documentElement.style.colorScheme = light ? 'light' : 'dark';
}
function applyBackground() {
  const background = document.querySelector('.background');
  background.style.backgroundImage = state.enabled && imageUrl ? `url("${imageUrl}")` : '';
  background.style.opacity = state.opacity;
  background.style.filter = `blur(${state.blur}px)`;
  background.style.backgroundSize = { cover: 'cover', contain: 'contain', stretch: '100% 100%', center: 'auto', repeat: 'auto' }[state.sizeMode];
  background.style.backgroundRepeat = state.sizeMode === 'repeat' ? 'repeat' : 'no-repeat';
  background.style.backgroundPosition = state.position.replaceAll('_', ' ');
  dialog.style.background = state.enabled && imageUrl ? `color-mix(in srgb, var(--panel) ${state.panelAlpha / 255 * 100}%, transparent)` : '';
}
tabs.addEventListener('keydown', event => {
  if (!['ArrowUp', 'ArrowDown', 'Home', 'End'].includes(event.key)) return;
  event.preventDefault();
  let index = pages.findIndex(([id]) => id === activePage);
  if (event.key === 'Home') index = 0;
  else if (event.key === 'End') index = pages.length - 1;
  else index = (index + (event.key === 'ArrowDown' ? 1 : -1) + pages.length) % pages.length;
  choosePage(pages[index][0], true);
});
dialog.addEventListener('keydown', event => {
  if (!capturingId) return;
  event.preventDefault();
  event.stopPropagation();
  if (event.key === 'Escape') { capturingId = ''; renderPanel(); return; }
  if (['Control', 'Alt', 'Shift', 'Meta'].includes(event.key)) return;
  const modifiers = [event.ctrlKey && 'Ctrl', event.altKey && 'Alt', event.shiftKey && 'Shift', event.metaKey && 'Meta'].filter(Boolean);
  const key = event.code === 'Space' ? 'Space' : event.key.length === 1 ? event.key.toUpperCase() : event.key;
  state.shortcutBindings[capturingId] = [...modifiers, key].join('+');
  capturingId = '';
  save();
  renderPanel();
});
document.querySelector('#image-picker').addEventListener('change', event => {
  const file = event.target.files[0];
  if (!file) return;
  if (imageUrl) URL.revokeObjectURL(imageUrl);
  imageUrl = URL.createObjectURL(file);
  set('imagePath', file.name);
  applyBackground();
  renderPanel();
});
document.querySelector('#close-preferences').addEventListener('click', () => dialog.close());
dialog.addEventListener('close', () => { capturingId = ''; document.querySelector('#open-preferences').hidden = false; });
document.querySelector('#open-preferences').addEventListener('click', event => { event.currentTarget.hidden = true; dialog.showModal(); });
matchMedia('(prefers-color-scheme: dark)').addEventListener('change', applyTheme);
applyTheme();
render();
dialog.showModal();
