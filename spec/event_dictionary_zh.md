> [English](event_dictionary.md) | [简体中文](event_dictionary_zh.md)

# AxiomTrace 事件字典规范

## 1. 目的

事件字典是从 `(module_id, event_id)` 到人类可读元数据的权威映射。它存在于主机（构建机器/PC）上，绝不会以文本形式烧录到目标设备中。

## 2. YAML Schema

```yaml
version: "1.0"
dictionary:
  <module_id_hex>:
    name: "MODULE_NAME"
    description: "人类可读的模块描述"
    events:
      <event_id_hex>:
        level: "INFO|WARN|ERROR|FAULT|DEBUG"
        name: "EVENT_NAME"
        text: "带 {type} 占位符的人类可读模板"
        args:
          - name: "arg0_name"
            type: "u8|i8|u16|i16|u32|i32|f32|ts|bytes"
          - name: "arg1_name"
            type: "..."
```

## 3. 占位符语法

`text` 字段使用 `{name:type}` 占位符。支持的类型：

| 占位符       | 有效载荷类型标签 | 描述                   |
|--------------|------------------|------------------------|
| `{name:u8}`  | 0x01             | 无符号 8 位            |
| `{name:i8}`  | 0x02             | 有符号 8 位            |
| `{name:u16}` | 0x03             | 无符号 16 位           |
| `{name:i16}` | 0x04             | 有符号 16 位           |
| `{name:u32}` | 0x05             | 无符号 32 位           |
| `{name:i32}` | 0x06             | 有符号 32 位           |
| `{name:f32}` | 0x07             | IEEE-754 浮点数        |
| `{name:ts}`  | 0x08             | 压缩时间戳             |
| `{name:bytes}`| 0x09            | 字节数组               |

## 4. 代码生成 (Code Generation)

`codegen` 工具读取 YAML 字典并输出：

1. `axiom_events_generated.h` — 将 `MODULE_EVENT_NAME` 映射到 `(module_id, event_id)` 的 C 宏。
2. `axiom_modules_generated.h` — 模块 ID 枚举。
3. `dictionary.json` — 供解码器使用的紧凑型 JSON 文件。

## 5. 校验规则

- 每个 `(module_id, event_id)` 必须唯一。
- `level` 必须与字典条目匹配；固件编码器不强制执行此操作，但校验器会在不匹配时发出警告。
- `args` 的类型序列必须与编码后的有效载荷字段顺序一致。
- `text` 中的占位符必须精确引用声明的 `args`。
