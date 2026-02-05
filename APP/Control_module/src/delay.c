#include "inc/delay.h"

#include <errno.h>
#include <time.h>

/*
 * 可靠相对延时：
 * - 每次调用重新开始计时
 * - 被信号打断会自动补睡剩余时间
 * - 不使用绝对时间，不做周期累计
 */
void delay_us(uint32_t us) {
    struct timespec req;
    struct timespec rem;

    /* 转换为 timespec */
    req.tv_sec = us / 1000000;
    req.tv_nsec = (long)(us % 1000000) * 1000L;

    /* 循环睡眠，直到时间耗尽 */
    while (1) {
        int ret = clock_nanosleep(
            CLOCK_MONOTONIC,  // 单调时钟
            0,                // 相对时间
            &req,
            &rem);

        if (ret == 0) {
            /* 睡够了 */
            break;
        }

        if (ret == EINTR) {
            /* 被信号打断，继续睡剩余时间 */
            req = rem;
            continue;
        }

        /* 其他错误，直接退出 */
        break;
    }
}
