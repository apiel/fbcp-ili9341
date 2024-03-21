#pragma once

#define ROUND_TO_NEAREST_INT(x) ((int)lround((x)))
#define ROUND_TO_FLOOR_INT(x) ((int)(floor((x))))
#define ROUND_TO_CEIL_INT(x) ((int)(ceil((x))))

#define MIN(x, y) ((x) <= (y) ? (x) : (y))
#define MAX(x, y) ((x) >= (y) ? (x) : (y))

#define ABS(x) ((x) < 0 ? (-(x)) : (x))

#define SWAPU32(x, y)     \
    {                     \
        uint32_t tmp = x; \
        x = y;            \
        y = tmp;          \
    }

#ifndef ALIGN_DOWN
#define ALIGN_DOWN(ptr, alignment) (((ptr)) & ~((alignment)-1))
#endif

#ifndef ALIGN_UP
#define ALIGN_UP(ptr, alignment) (((ptr) + ((alignment)-1)) & ~((alignment)-1))
#endif

#define PRINT_FLAG_2(flag_str, flag, shift) printf(flag_str ": %x\n", (reg & flag) >> shift)

#define PRINT_FLAG(flag) PRINT_FLAG_2(#flag, flag, flag##_SHIFT)

#ifndef KERNEL_MODULE
#define cpu_relax() asm volatile("yield" ::: "memory")
#endif
