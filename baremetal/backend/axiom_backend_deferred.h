#ifndef AXIOM_BACKEND_DEFERRED_H
#define AXIOM_BACKEND_DEFERRED_H

#include "axiom_backend.h"
#include "axiom_ring.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Deferred Ring Configuration
 * --------------------------------------------------------------------------- */

/* 默认 deferred ring 大小，可由用户覆盖 */
#ifndef AXIOM_DEFERRED_RING_SIZE
#define AXIOM_DEFERRED_RING_SIZE 256
#endif

/* ---------------------------------------------------------------------------
 * Deferred Ring Context
 * 独立 ring，与主 ring 隔离，固定大小静态分配
 * --------------------------------------------------------------------------- */
typedef struct {
    axiom_ring_t ring;
    uint8_t buf[AXIOM_DEFERRED_RING_SIZE];
} axiom_deferred_ring_ctx_t;

/* ---------------------------------------------------------------------------
 * Deferred Backend Context
 * 级联下游 actual backend，热路径不阻塞
 * --------------------------------------------------------------------------- */
typedef struct {
    axiom_backend_t backend;           /* 可注册到 registry 的 backend */
    const axiom_backend_t *downstream; /* 下游 actual backend */
    axiom_deferred_ring_ctx_t deferred_ring;
} axiom_backend_deferred_ctx_t;

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

/**
 * @brief 初始化 deferred backend
 *
 * @param ctx deferred backend 上下文（需静态分配）
 * @param deferred_ring_ctx deferred ring 上下文（需静态分配）
 * @param downstream 下游 actual backend（deferred → actual）
 * @return int 0 成功，负值失败
 *
 * @note deferred backend 注册到 registry 后，热路径 write() 调用
 *       仅将数据写入本地 ring，立即返回，不阻塞。flush() 时
 *       才将数据级联到下游 backend。
 * @note 此函数不分配内存，所有缓冲区均为静态分配
 */
int axiom_backend_deferred_init(axiom_backend_deferred_ctx_t *ctx,
                                 axiom_deferred_ring_ctx_t *deferred_ring_ctx,
                                 const axiom_backend_t *downstream);

/**
 * @brief 获取 deferred backend 的 axiom_backend_t 实例
 *
 * @param ctx deferred backend 上下文
 * @return const axiom_backend_t* 用于注册到 registry
 *
 * @note 返回的 backend 结构需在 init 之后使用
 */
const axiom_backend_t *axiom_backend_deferred_get_backend(axiom_backend_deferred_ctx_t *ctx);

/**
 * @brief 检查 deferred ring 中缓存的数据量
 *
 * @param ctx deferred backend 上下文
 * @return uint32_t 已缓存字节数
 */
uint32_t axiom_backend_deferred_pending(const axiom_backend_deferred_ctx_t *ctx);

/**
 * @brief 重置 deferred ring（丢弃所有缓存数据）
 *
 * @param ctx deferred backend 上下文
 */
void axiom_backend_deferred_reset(axiom_backend_deferred_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_BACKEND_DEFERRED_H */