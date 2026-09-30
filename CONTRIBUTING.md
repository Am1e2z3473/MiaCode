# Contributing to MiaCode

贡献前阅读 [README.md](README.md)、[代理与开发规则](AGENTS.md)和[文档索引](docs/INDEX.md)。
模块归属与复用入口见 [.agents/skills/miacode-dev-guide/SKILL.md](.agents/skills/miacode-dev-guide/SKILL.md)。

## 开发与验证

依赖、平台构建和打包入口见 [scripts/README.md](scripts/README.md)。
使用 Release 和适用的构建目录；开发者工具与规格通过 `MIACODE_BUILD_DEV_TOOLS=ON` 启用。

规格位于 `src/tools/<domain>/`，通过 `cmake/devtools/specs/` 的 `miacode_add_spec` 注册。
目标和契约见 [Spec 索引](docs/tests/SPEC_CATALOG.md)，按改动范围选择验证。
PR 描述说明用户可见变化、同步范围、验证证据和影响验收的限制。

## 文档与技能

共享规则维护于 `AGENTS.md`，技能维护源是 `.agents/skills/`。

```sh
python3 scripts/governance/docs_index.py --check
cmake -DMIACODE_SPEC_CATALOG_CHECK=ON -P cmake/devtools/SpecCatalog.cmake
```

文档分类与元数据见 [docs/README.md](docs/README.md)。公开规范与验收清单使用中文，
生成表格保留代码标识。构建产物、日志、dump 和本地实验资料留在忽略目录。

## 许可证与第三方内容

项目自有源码使用 MIT License。仓库整体、随仓库分发的资源、打包二进制和 release archive 当前定位为非商业使用，除非具体文件或第三方许可证另有说明。
涉及依赖、素材、字体、音频和打包内容时阅读 [LICENSE](LICENSE)、
[LICENSE_SCOPE.md](LICENSE_SCOPE.md)和[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)，核对来源与授权。
