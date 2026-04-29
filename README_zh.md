> [English](README.md) | [简体中文](README_zh.md)

# AxiomTrace v1.0

> MCU 裸机可观测微内核 — 开发可读、量产低扰动、故障可追溯、协议可冻结、工具链可信。

---

## 快速开始（5 分钟）

```c
#include "axiomtrace.h"

int main(void) {
    axiom_init();
    AX_EVT(INFO, 0x01, 0x01, (uint16_t)3200);
    return 0;
}
```

## 核心特性

- **Event Record 本体**：固件中只有结构化二进制事件，文本/JSON 由主机工具渲染。
- **零 malloc / 零 printf / 零阻塞**：热路径 O(1)，ISR 可写。
- **Backend 可插拔**：Memory / UART / USB / RTT / SWO / CAN-FD，新增后端不修改 Core。
- **Fault Capsule**：正常运行 0 Flash 写入，故障后自动冻结 pre/post 窗口并 commit 到 Flash。
- **Profile 编译期裁剪**：`DEV` / `FIELD` / `PROD`，量产时 `AX_LOG` 和 `AX_PROBE` 自动展开为空。
- **C11 `_Generic` 类型安全**：参数类型错误在编译期捕获。

## 架构

```text
AxiomTrace/
  Frontend Plane   AX_LOG / AX_EVT / AX_PROBE / AX_FAULT / AX_KV
  Core Plane       Ring → Encode → Filter → Timestamp → CRC → Drop Stats
  Backend Plane    Memory / UART / USB / RTT / SWO / CAN-FD / Flash Capsule
  Tool Plane       Decoder / Text Render / JSON Export / Golden Test
```

## 版本路线

| 版本 | 目标 |
| :--- | :--- |
| v0.1-core | Core 可证明 |
| v0.2-wire | Wire Format 可冻结 |
| v0.3-frontend | Frontend 可用 |
| v0.4-backend | Backend 可插拔 |
| v0.5-probe | Probe 时序探查成熟 |
| v0.6-capsule | Fault Capsule 成熟 |
| v0.7-toolchain | Toolchain 完整 |
| v0.8-portability | 跨平台验证 |
| v0.9-rc | Release Candidate |
| v1.0-stable | 稳定发布 |

## 构建

```bash
cmake -B build -S . -DAXIOM_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Python 工具链

AxiomTrace 提供 Python 包用于在主机端解码二进制日志。

### 安装

```bash
pip install axiomtrace
```

或从源码安装:

```bash
cd tool
pip install -e .
```

### CLI 使用方法

安装后，`axiom-decoder` 命令可用:

```bash
# 解码二进制日志为文本
axiom-decoder <input.bin> -o text > output.txt

# 导出为 JSON
axiom-decoder <input.bin> -o json > output.json

# 使用字典文件进行符号化输出
axiom-decoder <input.bin> -d dictionary.json -o text

# 显示帮助
axiom-decoder --help
```

### Python API

```python
from axiomtrace_tools.decoder import decode_stream

# 解码二进制文件
with open("trace.bin", "rb") as f:
    raw = f.read()

frames = decode_stream(raw)

# 处理帧
for frame in frames:
    if 'error' in frame:
        print(f"[ERROR: {frame['error']}]")
        continue
    print(f"[{frame['seq']:4d}] [{frame['level']:5s}] mod=0x{frame['module_id']:02X} evt=0x{frame['event_id']:04X}")
```

## 目录结构

```text
baremetal/
  core/         Ring, Event, Encode, CRC, Timestamp, Filter
  frontend/     AX_LOG/EVT/PROBE/FAULT/KV macros, Profile control
  backend/      Backend contract, registry, templates
  port/         Platform abstraction (weak symbols)
  examples/     Minimal and full examples
  axiomtrace.h  Unified public header
spec/           Protocol and API specifications
tool/           Python decoder, golden tests, benchmark, scripts
tests/          Host unit tests
```

## 许可

[LICENSE](LICENSE)
