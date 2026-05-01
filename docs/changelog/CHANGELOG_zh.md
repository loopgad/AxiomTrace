> [English](CHANGELOG.md) | [简体中文](CHANGELOG_zh.md)

# 更新日志

所有 AxiomTrace 的重要变更都将记录在此文件中。

格式基于 [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)，
本项目遵循 [语义化版本 (Semantic Versioning)](https://semver.org/spec/v2.0.0.html)。

## [未发布]

### 变更
- **重大变更**：完成 v1.0 架构重构。项目从 v2.0 "文本优先、默认 Flash" 设计转向 v1.0 "以 Event Record 为唯一事实来源" 的微内核模型。
- 将所有 v2.0 占位符实现替换为 v1.0 五平面架构（语义层 / 前端层 / 核心层 / 后端层 / 工具层）。
- 重命名并重组了 `baremetal/` 目录，以匹配 v1.0 平面架构。

### 新增
- `../project/RULES.md`：强制执行开发规则，包括热路径禁令（无 malloc、无 printf、无 Flash 擦除）、丢弃摘要要求，以及强制性的 issue→design→spec→golden→impl→decoder→tests→benchmark→docs→changelog 工作流。
- `../project/PLAN.md` v1.0：冻结目标架构、发布门槛和设计哲学。
- `../project/ROUTE.md` v1.0：从 v0.1-core 到 v0.9-rc 再到 v1.0-stable 的开发阶段。
- `../../spec/api_reference.md`：`AX_LOG`、`AX_EVT`、`AX_PROBE`、`AX_FAULT`、`AX_KV` 的前端 API 参考。
- `../../spec/decoder_protocol.md`：解码器输入/输出协议和字典格式规范。

### 移除
- v2.0 `axiom_storage_t` 统一存储抽象（由 Backend Contract 替代）。
- v2.0 `AXIOM_LOG("fmt", ...)` 类 printf 运行时字符串哈希（由结构化 Event Record 宏替代）。
- v2.0 链接器段自动注册后端系统（由显式 `axiom_backend_register()` 替代）。

## [0.1.0] - 待定

### 新增
- 核心层：无锁 ISR-safe RAM Ring 缓冲区，支持编译期配置大小和策略（丢弃/覆盖）。
- 核心层：二进制 Event Record 编码器，支持 C11 `_Generic` 类型安全 payload 编码和自描述类型标签。
- 核心层：CRC-16/CCITT-FALSE，带 256 字节 ROM 查表。
- 核心层：压缩相对时间戳（delta 编码）。
- 核心层：运行时级别/模块过滤和丢弃统计，支持生成 DROP_SUMMARY。
- 前端层：支持 DEV/FIELD/PROD Profile 编译期裁剪的 `AX_LOG`、`AX_EVT`、`AX_PROBE`、`AX_FAULT`、`AX_KV` 宏。
- 后端层：统一的 `axiom_backend_t` 合约，包含 `write/ready/flush/panic_write/on_drop`。
- 后端层：Memory、UART、USB CDC、RTT、SWO/ITM、CAN-FD 后端模板。
- Fault Capsule：故障前窗口冻结、故障后窗口捕获、寄存器快照、固件哈希、Capsule CRC、Flash commit。
- 工具层：Python 解码器、文本渲染器、JSON 导出器、故障报告器、Golden 帧管理器、基准测试工具。
- 主机单元测试和 Python 回归测试。
- 单文件库生成器 (`../../tool/scripts/amalgamate.py`)。
