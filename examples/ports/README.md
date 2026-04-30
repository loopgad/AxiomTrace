# AxiomTrace Platform Ports Reference
# 各平台参考实现索引

本目录汇总 AxiomTrace 支持的目标平台参考实现。

**实际实现位于 `baremetal/ports/` 目录：**

```
baremetal/ports/
├── arch/           # 架构层
│   ├── cortex-m/  # ARM Cortex-M 架构
│   └── riscv/     # RISC-V 架构
├── soc/           # 芯片层
│   ├── stm32f4/   # STM32F4 系列
│   ├── nrf52/     # Nordic nRF52 系列
│   └── esp32/     # Espressif ESP32 系列
├── board/         # 板级层（可选）
└── generic/       # 通用实现（默认）
```

## 快速参考

| 平台 | 架构 | 芯片 | 路径 |
|------|------|------|------|
| Host/Linux | - | generic | `baremetal/ports/generic/` |
| STM32F4 | cortex-m | stm32f4 | `baremetal/ports/soc/stm32f4/` |
| nRF52 | cortex-m | nrf52 | `baremetal/ports/soc/nrf52/` |
| ESP32 | riscv | esp32 | `baremetal/ports/soc/esp32/` |

## CMake 配置

```cmake
# 选择目标平台
cmake -B build -DAXIOM_PLATFORM=cortex-m -DAXIOM_SOC=stm32f4

# 或使用预编译库
cmake -B build -DAXIOM_PRECOMPILED_LIB=/path/to/libaxiomtrace.a
```

## 使用预编译库

See `CMakeLists.txt` 和 `example_precompiled/` 目录。