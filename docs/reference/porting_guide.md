# AxiomTrace Porting Guide / AxiomTrace 移植指南

> 适用于 AxiomTrace 1.0。公共 Port 接口位于
> `baremetal/port/axiom_port.h`；本页同时是英文 README 链接的边界摘要。

本页的核心保证是：Core、Frontend、Backend contract 和 Host generic Port
由仓库维护并通过 Host 检查；reference Port 只保证接口/编译形状；Integrator
负责真实 SoC、板卡、SDK、启动/链接、时钟、IRQ、传输和目标测量。厂商目录
因此只提供映射，不再冒充可直接烧录的硬件驱动。

## 1. 架构边界

AxiomTrace 的仓库内核按职责分为五个平面：

```text
frontend/  ->  core/  ->  backend/
                 ^          |
                 |          +--> downstream transport
                 +-- port/  +--> vendor port package
```

- `frontend/`：`AX_EVT`、`AX_LOG`、`AX_PROBE`、`AX_FAULT`、`AX_KV` 宏。
- `core/`：事件帧、编码、Ring、过滤、时间戳、诊断和 Fault Capsule。
- `backend/`：Backend 契约、Memory Backend、Deferred Backend。
- `port/`：稳定的 `axiom_port_*` 接口声明。
- `ports/`：Host 默认实现、架构实现和需要外部 SDK 的厂商包。

顶层 CMake 只负责选择架构 Port；它不猜测 SoC 或开发板，也不把厂商 SDK 混入核心库。

## 2. Port 目录约定

```text
baremetal/ports/
├── CMakeLists.txt              # host/cortex-m/riscv 选择器
├── generic/                    # Host 和默认弱符号实现
├── arch/
│   ├── cortex-m/               # Cortex-M 架构实现
│   └── riscv/                  # RISC-V 架构实现
├── stm32/                      # STM32 integration map（不编译源码）
├── nrf52/                      # nRF52 integration map（不编译源码）
└── esp32/                      # ESP-IDF integration map（不注册源码）
```

`soc/`、`board/` 不再作为空的占位目录提交。新的 SoC 或开发板适配应放在实际需要 SDK/链接配置的厂商包中，并用 README 说明依赖。

## 3. 顶层 CMake 选项

| 选项 | 可选值 | 作用 |
| --- | --- | --- |
| `AXIOM_PLATFORM` | `host`、`cortex-m`、`riscv` | 选择一个架构级 Port；默认按 `CMAKE_SYSTEM_PROCESSOR` 自动检测 |
| `AXIOM_PRESET` | `custom`、`tiny`、`prod`、`field`、`dev` | 选择资源预设 |
| `AXIOM_BUILD_TESTS` | `ON`/`OFF` | 构建 Host 测试 |
| `AXIOM_BUILD_EXAMPLES` | `ON`/`OFF` | 构建 `baremetal/examples` |
| `AXIOM_EXTERNAL_PORT` | `ON`/`OFF`（已弃用） | `AXIOM_PORT_SOURCE=NONE` 的兼容别名 |
| `AXIOM_CPU_HZ` | 非负整数 | 架构 Port 的真实 CPU 时钟；未设置时不伪造微秒时间 |
| `AXIOM_PORT_SOURCE` | 路径列表或 `NONE` | 选择自定义 Port 源；`NONE` 表示由应用完整提供 |

`AXIOM_SOC` 和 `AXIOM_BOARD` 不是顶层项目的选项。厂商 Port 自己处理 SDK、芯片和板级配置，避免“配置成功但实际仍使用 generic Port”的隐式回退。

## 4. 构建方式

Host 默认使用 generic Port。Python 工具建议使用隔离环境，避免受 PEP 668
保护的系统 Python：

```sh
python -m venv .venv
. .venv/bin/activate
python -m pip install ./tool     # 可选 YAML：python -m pip install './tool[yaml]'
```

仓库也提供锁定的 `uv` 路径：

```sh
uv sync --project tool
uv run --project tool axiom-decoder trace.bin --format raw
```

Host 默认使用 generic Port：

```sh
cmake -S . -B build -G Ninja \
  -DAXIOM_BUILD_TESTS=ON -DAXIOM_BUILD_EXAMPLES=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

交叉编译时显式指定架构和工具链：

```sh
cmake -S . -B build-cortex-m -G Ninja \
  -DAXIOM_PLATFORM=cortex-m \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/toolchain.cmake \
  -DAXIOM_BUILD_TESTS=OFF -DAXIOM_BUILD_EXAMPLES=OFF
cmake --build build-cortex-m
```

## 5. 使用厂商 Port 映射

三套厂商目录在 1.0 RC 都是 reference-only map：它们不包含或编译
寄存器/RTT 驱动，不搜索任意 SDK 路径，也不注入 CPU/ABI/板卡参数。统一的
可编译 callback skeleton 位于 `baremetal/examples/example_custom_port.c`；
把它复制进真实固件并替换所有 TODO，再使核心构建显式使用外部 Port：

```cmake
set(AXIOM_PORT_SOURCE NONE CACHE STRING "" FORCE)
add_subdirectory(path/to/AxiomTrace axiomtrace-core)
target_sources(firmware PRIVATE axiomtrace_custom_port.c)
target_link_libraries(firmware PRIVATE AxiomTrace::axiomtrace)
```

Integrator 必须自行提供芯片启动/链接、准确时钟、IRQ 临界区语义、Port
快照/Flash 策略和 Backend transport。Backend 的 normal `write()` 必须有界、
不阻塞、不分配；不可用时返回负值，让 registry 的 degradation/recovery
策略处理。发布主库只承诺 Memory 与 Deferred Backend；厂商目录没有
CMake package、硬件符号或可链接 target。

ESP32 则在消费方的 ESP-IDF component 中注册自己的源码，例如：

```cmake
idf_component_register(
    SRCS "axiomtrace_impl.c" "axiomtrace_custom_port.c"
    INCLUDE_DIRS "."
    REQUIRES driver)
```

详细的 platform 依赖只写在各目录的短 README 中；这些文档不等于真实硬件
验证。

## 6. 添加新的架构 Port

1. 在 `baremetal/ports/arch/<name>/` 添加 `axiom_port_<name>.c`。
2. 实现以下基础接口：

   ```c
   uint32_t axiom_port_timestamp(void);
   void axiom_port_critical_enter(void);
   void axiom_port_critical_exit(void);
   ```

3. 按需实现字符串输出、Fault Hook、快照和 Flash 接口；不需要的接口可以返回默认失败值。
4. 在 `baremetal/ports/CMakeLists.txt` 增加一个明确的 `AXIOM_PLATFORM` 分支，并确认源文件存在。
5. 不要为单个源文件再创建未被父级调用的 CMake 包装文件；如果应用完整
   提供 Port，使用 `AXIOM_PORT_SOURCE=NONE`（旧的 `AXIOM_EXTERNAL_PORT=ON`
   仅为兼容别名）。

时间戳必须单调递增且允许 32 位自然回绕；临界区必须成对、可嵌套或明确禁止嵌套；Port 热路径不能分配堆内存、阻塞等待或擦写 Flash。

## 7. 添加新的厂商/板级包

当适配需要 SDK 头文件、UART/RTT 驱动、启动代码或链接脚本时，在 `baremetal/ports/<vendor>/` 建立独立包：

- `CMakeLists.txt` 只声明真实存在且已在目标 SDK 中验证的源文件和依赖；
- 未验证的仓库参考代码必须从默认包目标移除，并在 README 标成 map/reference；
- `README.md` 记录 SDK 版本、必需符号、初始化顺序和链接要求；
- 包目标通过 `AxiomTrace::axiomtrace` 依赖核心库；
- 不修改核心 `CMakeLists.txt` 来猜测具体开发板；
- 在目标工具链/SDK 工程中验证，Host CTest 不冒充硬件验证。

## 8. 验证清单

- Host：`cmake --build`、`ctest --test-dir build --output-on-failure`。
- Python 工具：先 `uv sync --project tool --extra test`，再
  `uv run --project tool --extra test python -m pytest -q`；没有 uv 时在
  `.venv` 内执行 `python -m pip install './tool[test]'`。
- 单头文件：运行 `tool/scripts/amalgamate.py` 并编译单 implementation TU。
- 文档与版本：运行 `python scripts/release_checks.py`。
- 目标平台：使用真实交叉编译器检查启动、Port、链接脚本和资源预算。

最后一次修改 Port 后，应同步更新 `docs/reference/DIR_STRUCTURE.md`、README 的平台说明和 `docs/changelog/CHANGELOG*.md`。
在 1.0 RC 阶段，只有真实工具链/SDK 项目的交叉编译和时序报告才能把
Integrator 状态提升为 measured；Host CTest 不冒充硬件验证。
