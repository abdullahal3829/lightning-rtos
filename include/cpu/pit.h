/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#ifndef KERNEL_PIT_H
#define KERNEL_PIT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*pit_callback_fn)(uint64_t tick, uint64_t ms, void *context);

bool pit_initialize(void *system_table);
uint64_t pit_get_frequency_hz(void);
uint64_t pit_get_tick64(void);
uint64_t pit_get_ms(void);
void pit_wait_ms(uint64_t ms);
bool pit_is_operational(void);
bool pit_set_frequency_hz(uint64_t frequency_hz);
bool pit_register_callback(pit_callback_fn callback, void *context);

#ifdef __cplusplus
}
#endif

#endif
