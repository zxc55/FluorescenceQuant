#ifndef DELAY_H
#define DELAY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 可靠延时（微秒）
 *        每次调用独立计时，不累计
 *        保证至少睡够指定时间
 */
void delay_us(uint32_t us);

#ifdef __cplusplus
}
#endif

#endif
