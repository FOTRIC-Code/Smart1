# MANIFEST — Volga.IRtekSDK_Linux 厂商交付包

本目录是 fotric.cn 交付的 IRtek Linux SDK 原始包，**用途是给用户与 AI 提供热像产品开发的
参考资料**（头文件 API 面、demo 用法范例）。绑定的 ABI 契约基线见
[`docs/irtek-uvc-sdk-abi.md`](../../docs/irtek-uvc-sdk-abi.md)。

## 来源与版本

| 项 | 值 |
|---|---|
| 交付方 | fotric.cn |
| 本地接收时间 | 2026-10-19（文件 mtime） |
| 厂商版本号 | **无**（包内无 version 文件 / changelog，UVC SDK 仅运行时可查 `uvc_get_sdk_version()`） |
| 版本标识方式 | 以下方指纹表为准；对比时按 sha256 判定同异 |

## 入库策略：文本入库，`.so` 不入库

- **入库**：头文件、demo 源码、CMake、README、LICENSE——即用户/AI 需要查阅的全部文本。
- **不入库**：全部 `.so`（`.gitignore` 已排除，本地文件保留在磁盘上，可正常跑 demo）。
- 理由：① 固件构建与运行使用的两个 `.so` 已在 `app/net_app/libs/irtek/lib/`（有 LICENSE 随附）；
  ② 本包交付时**未附带任何许可文本**，其中 `libIRtekNetSDK.so` 在现有 LICENSE 中无据可依，
  git 历史不可逆，先隔离待厂商确认；③ x86-64 宿主库与"固件组件"用途条款字面不适配。
- 厂商书面确认覆盖后，把 `.so` 加回只需删除对应 ignore 规则。

## 许可分级（LICENSE 为逐字复制，不产生新授权）

本目录的 `LICENSE` 复制自 `app/net_app/libs/irtek/LICENSE`（随固件用库交付的原文）。
其字面覆盖范围与本品类的对照：

| 内容 | LICENSE 字面覆盖 | 状态 |
|---|---|---|
| 头文件（`*/header/`） | ✅ "the headers under include/" | 可入库 |
| demo 源码 / CMake | ⚠️ 未提及（demo 天职即参考使用，属善意推断） | 入库，待确认 |
| rk3506 `libIRtekUVCSDK.so` / `libIRtekFileSDK.so` | ⚠️ 名义覆盖（名字对得上，但与随 LICENSE 交付的非同一文件，见指纹表） | 不入库，待确认 |
| rk3506 / ubuntu20 `libIRtekNetSDK.so` | ❌ 未被点名，无任何许可文本 | 不入库，待确认 |
| ubuntu20 `libIRtekUVCSDK.so` / `libIRtekFileSDK.so` | ❌ 用途条款不适用（非固件组件） | 不入库，待确认 |

## `.so` 指纹表（sha256 / 字节数）

本包（rk3506 = ARM32 hard-float，ubuntu20 = x86-64）：

| 库 | 变体 | sha256 | 大小 |
|---|---|---|---|
| libIRtekNetSDK.so | rk3506 | `23622c93…3871d7e` | 1,147,072 |
| libIRtekNetSDK.so | ubuntu20 | `437a1d69…3ffcff7` | 1,579,544 |
| libIRtekUVCSDK.so | rk3506 | `90e0a0e7…66bdd9e6` | 127,612 |
| libIRtekUVCSDK.so | ubuntu20 | `465d8ddf…51f26f08` | 284,992 |
| libIRtekFileSDK.so | rk3506 | `73c40812…d9638c2a` | 9,067,068 |
| libIRtekFileSDK.so | ubuntu20 | `4f30668f…a2459b5f` | 3,554,432 |

固件预置对照（`app/net_app/libs/irtek/lib/`，**ABI 契约锚定的是这两份**）：

| 库 | sha256 | 大小 | 与本包 rk3506 版 |
|---|---|---|---|
| libIRtekUVCSDK.so | `f885ae61…412c4d` | 127,592 | **不同文件**（差 20 字节，疑似同版本不同构建/打包，未证实） |
| libIRtekFileSDK.so | `a3fb06f6…5b0fe1e1` | 9,067,100 | **不同文件**（差 32 字节，同上） |

> ubuntu22 变体目录仅含 README 占位，无 `.so` 交付。

## 待厂商（fotric.cn）确认项

1. 本包整体（尤其 NetSDK、x86-64 库、demo 源码）是否适用所附 LICENSE；若否，请补覆盖
   整包的许可文本。
2. 本包 rk3506 版两个 `.so` 与固件预置版的版本对应关系（是否同版本的不同构建）。
3. `libIRtekNetSDK.so` 的许可与使用范围。
4. （与 ABI 契约文档「待厂商确认清单」9 项一并对接。）

## 更新流程

收到厂商新交付包时：

1. 整体替换本目录内容，核对上方指纹表并逐行更新（sha256 是唯一的版本标识）。
2. 文本变更随主库提交；`.so` 仍按 ignore 规则排除（厂商确认覆盖后另议）。
3. 若 rk3506 版 `libIRtekUVCSDK.so` / `libIRtekFileSDK.so` 指纹变化，**必须同步评估**
   `docs/irtek-uvc-sdk-abi.md` 的契约指纹是否仍然成立，并通知 `irtek_thermal` 绑定维护方。
