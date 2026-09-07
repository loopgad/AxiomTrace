# AxiomTrace Platform Reference

> [English](platform_reference.md) | 简体中文（本页合并）
>
> 版本: v1.0  |  状态: 现行（中英双语合并文档）

---

## 目录

1. [概述](#概述)
2. [Port 接口定义](#port-接口定义)
3. [通用实现 (Generic)](#通用实现-generic)
4. [ARM Cortex-M 实现](#arm-cortex-m-实现)
5. [RISC-V 实现](#risc-v-实现)
6. [主机/模拟实现](#主机-模拟实现)
7. [自定义平台移植](#自定义平台移植)

---

## 概述

AxiomTrace 的平台抽象层 (Port Layer) 区分 Host generic、架构默认
provider 与消费方自定义 provider。仓库提供的 generic 和架构实现在
编译器支持时使用 weak 符号，因此板级强符号可覆盖单个默认回调；
完整自定义 Port 仍建议通过 `AXIOM_PORT_SOURCE=NONE` 显式选择。

### Port 接口设计原则

- **零依赖**: Port 实现不依赖外部库 (如 HAL)
- **Provider 边界**: 仓库默认回调可由板级强符号覆盖；完整自定义 Port 由消费方显式提供
- **向后兼容**: 保持 `axiom_port.h` 接口不变
- **上下文明确**: 时间戳与临界区接口服务于热路径；字符串、故障快照和 Flash 接口的 ISR 语义由集成方保证，Flash 擦写不得在 ISR 中执行

### 文件位置

```
baremetal/port/
  axiom_port.h                         # 公共接口定义
baremetal/ports/generic/
  axiom_port_generic.c                 # Host/default provider
baremetal/ports/arch/cortex-m/
  axiom_port_cortex_m.c                # Cortex-M provider
baremetal/ports/arch/riscv/
  axiom_port_riscv.c                   # RISC-V provider
```

---

## Port 接口定义

### axiom_port_timestamp()

获取单调递增的时间戳。

```c
extern uint32_t axiom_port_timestamp(void);
```

**返回值**: 微秒级时间戳，32位无符号整数，自然回绕

**实现要求**:
- 微秒级精度或更高（如果配置 `AXIOM_CPU_HZ`，架构 Port 才能进行周期换算）
- 自然回绕 (32位溢出)
- 热路径可安全调用；不得依赖阻塞锁

**推荐实现**:
- ARM Cortex-M: DWT_CYCCNT 寄存器
- RISC-V: mcycle CSR
- 主机: `clock_gettime(CLOCK_MONOTONIC)`

### axiom_port_critical_enter() / axiom_port_critical_exit()

临界区保护，用于保护共享数据结构的访问。

```c
extern void axiom_port_critical_enter(void);
extern void axiom_port_critical_exit(void);
```

**实现要求**:
- 必须可嵌套 (配对调用)
- 用于保护 Core 热路径共享状态；不得在其中调度或阻塞

**推荐实现**:
- ARM Cortex-M: PRIMASK 或 BASEPRI
- RISC-V: mstatus.MIE
- 主机: `pthread_mutex` 或禁用中断 (不推荐)

### axiom_port_string_out()

开发日志输出，将字符串输出到调试终端。

```c
extern void axiom_port_string_out(const char *str);
```

**实现要求**:
- 非阻塞
- 只有在具体实现明确支持时才可从 ISR 调用
- 目标设备: UART / RTT / SWO / ITM / stdout

### axiom_port_fault_hook()

故障钩子函数，在 AX_FAULT 事件触发时调用。

```c
extern void axiom_port_fault_hook(uint8_t module_id, uint16_t event_id,
                                  const uint8_t *payload, uint8_t payload_len);
```

**参数**:
- `module_id`: 模块 ID
- `event_id`: 事件 ID
- `payload`: 负载数据
- `payload_len`: 负载长度

**用途示例**:
- 触发硬件断点
- 写入调试器
- 保存故障上下文

### axiom_port_fault_snapshot()

获取寄存器快照，用于 Fault Capsule。

```c
extern uint8_t axiom_port_fault_snapshot(uint8_t *buf, uint8_t max_len);
```

**参数**:
- `buf`: 输出缓冲区
- `max_len`: 缓冲区最大长度

**返回值**: 实际写入的字节数

**Cortex-M 推荐快照**:
- R0-R3, R12, LR, PSR, PC
- CONTROL, PRIMASK, BASEPRI

### axiom_port_flash_erase() / axiom_port_flash_write() / axiom_port_flash_read()

Flash 操作函数，用于 Fault Capsule 存储。

```c
extern int axiom_port_flash_erase(uint32_t addr, uint32_t len);
extern int axiom_port_flash_write(uint32_t addr, const uint8_t *data, uint32_t len);
extern int axiom_port_flash_read(uint32_t addr, uint8_t *out, uint32_t len);
```

**返回值**: 失败返回负数；Port 可以按目标驱动约定返回成功值。当前
capsule 写入路径接受 `0` 或完整 `len` 作为成功。

**要求**:
- 非 ISR 调用
- 擦除必须按页/扇区对齐
- `AXIOM_CAPSULE_FLASH_BASE` 是回调收到的起始地址；读写偏移由 Core
  加到该地址。Core 不管理 linker section 或分区占用，集成方必须保证该
  `BASE/SIZE` 区域可擦除、不会与固件/其他数据重叠。

---

## 通用实现 (Generic)

位置: `baremetal/ports/generic/axiom_port_generic.c`

通用实现适用于主机环境和不需要特殊硬件功能的场景。

### 特性

- 在支持的 ELF 编译器上，generic 和架构默认函数为弱符号；Windows/PE 等不支持该属性的目标使用普通强符号默认值
- 时间戳默认返回 0
- 临界区为空操作
- Flash 操作返回 -1 (不支持)

### 使用场景

- 主机单元测试
- QEMU/模拟器环境
- 快速原型开发

### 编译配置

```bash
# 自动检测为 host 平台
cmake -B build -S .
```

---

## ARM Cortex-M 实现

### 推荐的 Port 实现

#### 时间戳实现 (DWT_CYCCNT)

```c
uint32_t axiom_port_timestamp(void) {
    return DWT->CYCCNT;
}
```

**注意**: 确保在初始化时启用 DWT_CYCCNT:

```c
CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
```

#### 临界区实现 (PRIMASK)

```c
void axiom_port_critical_enter(void) {
    __asm volatile ("cpsid i" : : : "memory");
}

void axiom_port_critical_exit(void) {
    __asm volatile ("cpsie i" : : : "memory");
}
```

#### 临界区实现 (BASEPRI，更精细控制)

```c
void axiom_port_critical_enter(void) {
    __asm volatile ("movs r0, #0x01\n msr basepri, r0" : : : "r0", "memory");
}

void axiom_port_critical_exit(void) {
    __asm volatile ("movs r0, #0x00\n msr basepri, r0" : : : "r0", "memory");
}
```

#### 字符串输出实现 (ITM)

```c
void axiom_port_string_out(const char *str) {
    while (*str) {
        while ((ITM->PORT[0].u32 & 1) == 0);
        ITM->PORT[0].u8 = *str++;
    }
}
```

### 常用芯片系列参考

| 芯片系列 | 时钟源 | 调试接口 | Flash 大小 |
|---------|--------|----------|-----------|
| STM32F4 | APB2 (168MHz) | SWD/JTAG | 1-2MB |
| STM32F1 | APB2 (72MHz) | SWD/JTAG | 64-512KB |
| nRF52 | HFCLK (64MHz) | SWD | 256KB-1MB |
| EFM32PG | HFRCO (40MHz) | SWD | 256KB-1MB |

### 已知问题与限制

1. **DWT_CYCCNT 不在睡眠时计数**: 如果需要跟踪睡眠时间，使用 RTCC 或其他始终运行的计数器
2. **多核心**: 在多核芯片上，每个核心有独立的 DWT，需要为每个核心单独初始化
3. **ITM 带宽限制**: ITM 最高 21MB/s，高频日志可能丢失数据

---

## RISC-V 实现

### 推荐的 Port 实现

#### 时间戳实现 (mcycle)

```c
uint32_t axiom_port_timestamp(void) {
    uint32_t cycles;
    __asm volatile ("csrr %0, mcycle" : "=r"(cycles));
    return cycles;
}
```

**注意**: 确保 mcycle 在所有特权级别可用。

#### 临界区实现 (mstatus.MIE)

```c
static uint32_t g_critical_nesting = 0;
static uint32_t g_mstatus_state = 0;
#define MSTATUS_MIE 0x8u

void axiom_port_critical_enter(void) {
    uint32_t mstatus;
    const uint32_t mie = MSTATUS_MIE;
    __asm volatile ("csrrc %0, mstatus, %1"
                    : "=r"(mstatus) : "r"(mie) : "memory");
    if (g_critical_nesting == 0) {
        g_mstatus_state = mstatus;
    }
    g_critical_nesting++;
}

void axiom_port_critical_exit(void) {
    if (g_critical_nesting > 0) {
        g_critical_nesting--;
        if (g_critical_nesting == 0) {
            if (g_mstatus_state & MSTATUS_MIE) {
                const uint32_t mie = MSTATUS_MIE;
                __asm volatile ("csrs mstatus, %0" : : "r"(mie) : "memory");
            }
        }
    }
}
```

### RISC-V 平台注意事项

1. **时间戳分辨率**: mcycle 频率等于 CPU 频率，需要根据实际频率调整
2. **调试接口**: 通常使用 JTAG 或 debug module
3. **中断控制器**: 确保正确配置 PLIC 或 CLINT

---

## 主机/模拟实现

### 主机 Port 实现示例

#### 时间戳实现

```c
#include <time.h>

uint32_t axiom_port_timestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000000 + ts.tv_nsec / 1000);
}
```

#### 临界区实现

```c
#include <pthread.h>

static pthread_mutex_t critical_mutex = PTHREAD_MUTEX_INITIALIZER;

void axiom_port_critical_enter(void) {
    pthread_mutex_lock(&critical_mutex);
}

void axiom_port_critical_exit(void) {
    pthread_mutex_unlock(&critical_mutex);
}
```

#### 字符串输出

```c
#include <stdio.h>

void axiom_port_string_out(const char *str) {
    fputs(str, stdout);
}
```

### 模拟环境

| 模拟器 | 调试接口 | 时间戳精度 |
|-------|----------|-----------|
| QEMU | GDB | 纳秒级 |
| Renode | GDB | 微秒级 |
| Renode | telnet | 微秒级 |

---

## 自定义平台移植

### 移植步骤

#### 1. 创建 Port 源文件

新增架构时，在 `baremetal/ports/arch/<name>/` 下创建
`axiom_port_<name>.c`，并在 `baremetal/ports/CMakeLists.txt` 增加明确的
`AXIOM_PLATFORM` 分支。若应用已经完整提供 Port，则将源文件放在消费方
工程中，使用 `AXIOM_PORT_SOURCE=NONE`，不要依赖 generic 默认值：

```c
#include "axiom_port.h"

/* 必须实现的函数 */
uint32_t axiom_port_timestamp(void) {
    /* 返回微秒时间戳 */
}

void axiom_port_critical_enter(void) {
    /* 禁用中断/获取锁 */
}

void axiom_port_critical_exit(void) {
    /* 启用中断/释放锁 */
}

/* 可选实现 */
void axiom_port_string_out(const char *str) {
    /* 输出字符串到调试终端 */
}
```

#### 2. 确保强符号 provider

在 GCC/Clang 环境下，确保你的实现不使用 `__attribute__((weak))`:

```c
/* 正确: 强符号实现 */
uint32_t axiom_port_timestamp(void) {
    return platform_get_timestamp();
}

/* 错误: 弱符号可能被 generic 覆盖 */
__attribute__((weak)) uint32_t axiom_port_timestamp(void) {
    return 0;
}
```

#### 3. 编译验证

```bash
# Host/custom provider 的编译检查（架构目标请同时传入 toolchain file）
cmake -S . -B build-custom -DAXIOM_PLATFORM=host \
  -DAXIOM_PORT_SOURCE=/absolute/path/axiom_port_custom.c \
  -DAXIOM_BUILD_TESTS=OFF
cmake --build build-custom
```

#### 4. 测试验证

```c
/* 验证时间戳递增 */
uint32_t t1 = axiom_port_timestamp();
uint32_t t2 = axiom_port_timestamp();
assert(t2 >= t1);  /* 应始终成立 */

/* 验证临界区可重入 */
axiom_port_critical_enter();
axiom_port_critical_enter();
axiom_port_critical_exit();
axiom_port_critical_exit();
```

### 常见问题

#### Q: 时间戳不准怎么办?

**A**: 检查时钟源配置。确保:
1. 时钟频率正确
2. 时钟在睡眠模式下继续运行
3. 没有时钟分频导致精度丢失

#### Q: 临界区死锁怎么办?

**A**: 确保:
1. 临界区不调用阻塞 API
2. 临界区不调用 AxiomTrace API (会递归获取锁)
3. 配对正确 (enter/exit 数量一致)

#### Q: Flash 操作失败怎么办?

**A**: 检查:
1. 地址是否对齐 (通常按页/扇区对齐)
2. 是否在中断上下文外调用
3. Flash 控制器是否已解锁

---

## 相关文档

- [移植指南](./porting_guide.md)
- [AxiomTrace README](../../README.md)

---

## 更新日志

| 版本 | 日期 | 说明 |
|---|---|---|
| 1.0 | 2026-04-30 | 初始版本 |
