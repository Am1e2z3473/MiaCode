# Source layout

Shared development rules live in [AGENTS.md](../AGENTS.md).
Task guides are maintained in `.agents/skills/`.
Current ownership contracts live in `docs/specs/ui/CURRENT_ARCHITECTURE_ZH.md`.

| Directory | Responsibility |
| --- | --- |
| `app/ui/` | Product frontend: QML, models, theme, locale and native window |
| `app/services/` | Workspace, shared application services and typed ports |
| `app/runtime/` | Session assembly and domain runtime hosts |
| `core/chart/` | Document, parser and transforms |
| `core/scene/` | GPU-independent frame/layer math |
| `core/video/` | Shared preview/video render settings |
| `editor/` | Text policy, completion and bookmark syntax |
| `audio/` | Audio backends and SFX runtime |
| `preview/runtime/` | Preview runtime, assets and export sessions |
| `preview/quick_scene/` | Shared Qt Quick/QSG chart rendering |
| `timeline/` | Timeline model and Quick surface |
| `tools/` | Domain tools, export, analysis and specs |
| `devtools/` | Command-line diagnostics: chart dump, muri dump, audio probe and offset batch |
| `common/` | Shared configuration, resource paths and logging |
| `extensions/` | Retained manifest/schema contract; no active extension host |
| `wrapper/` | Windows launcher |

Keep sources with their domain owner. Reuse existing QML controls and application services;
do not create parallel document, playback or resource-lookup authorities.

Bootstrap is the GUI entry; Session assembles runtime hosts.
QSG export supports D3D11/QRhi and OpenGL sessions and an export worker;
see `docs/specs/preview/CURRENT_RENDER_EXPORT_CONTRACT_ZH.md`.

Executable contracts stay under `tools/`; registration and source groups live in
`cmake/devtools/`. See `docs/tests/SPEC_CATALOG.md` for targeted validation.
