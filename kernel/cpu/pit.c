/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#include <cpu/pit.h>
#include <asm/gpioutil.h>
#include <definations.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <Uefi.h>

#define PIT_CHANNEL0_PORT 0x40U
#define PIT_COMMAND_PORT  0x43U
#define PIT_LATCH_CHANNEL0 0x00U
#define PIT_MODE_CHANNEL0_RATE_GEN 0x30U

#define PIT_INPUT_CLOCK_HZ 1193182ULL
#define PIT_CAL_STALL_US 50000ULL
#define PIT_MIN_FREQ_HZ 100000ULL
#define PIT_MAX_FREQ_HZ 20000000ULL
#define PIT_BIOS_DEFAULT_FREQ_HZ 1193182ULL
#define PIT_BIOS_LIVENESS_ITER 100000U
#define PIT_SEQ_RETRY_LIMIT 8U

static volatile uint64_t g_pit_frequency_hz = 0ULL;
static volatile bool g_pit_operational = false;
static volatile uint16_t g_pit_last_counter = 0U;
static volatile uint64_t g_pit_overflow_count = 0ULL;
static volatile uint64_t g_pit_tick_at_rebase = 0ULL;
static volatile uint64_t g_pit_ms_at_rebase = 0ULL;
static volatile uint64_t g_pit_seq = 0ULL;
static volatile pit_callback_fn g_pit_callback = NULL;
static volatile void *g_pit_callback_context = NULL;

static uint16_t pit_read_channel0(void) {
    outb(PIT_COMMAND_PORT, PIT_LATCH_CHANNEL0);
    uint8_t low = inb(PIT_CHANNEL0_PORT);
    uint8_t high = inb(PIT_CHANNEL0_PORT);
    return (uint16_t)(((uint16_t)high << 8) | low);
}

extern void pit_delay_iterations(uint32_t count);

static bool pit_program_hz(uint64_t frequency_hz) {
    uint64_t divisor;
    uint8_t lobyte;
    uint8_t hibyte;

    if (frequency_hz == 0ULL || frequency_hz > PIT_INPUT_CLOCK_HZ)
        return false;

    divisor = PIT_INPUT_CLOCK_HZ / frequency_hz;
    if (divisor == 0ULL || divisor > 0xFFFFULL)
        return false;

    lobyte = (uint8_t)(divisor & 0xFFULL);
    hibyte = (uint8_t)((divisor >> 8) & 0xFFULL);

    outb(PIT_COMMAND_PORT, PIT_MODE_CHANNEL0_RATE_GEN);
    outb(PIT_CHANNEL0_PORT, lobyte);
    outb(PIT_CHANNEL0_PORT, hibyte);
    return true;
}

static void pit_seq_write_begin(void) {
    __atomic_add_fetch(&g_pit_seq, 1ULL, __ATOMIC_ACQUIRE);
    __atomic_thread_fence(__ATOMIC_RELEASE);
}

static void pit_seq_write_end(void) {
    __atomic_thread_fence(__ATOMIC_RELEASE);
    __atomic_add_fetch(&g_pit_seq, 1ULL, __ATOMIC_RELEASE);
}

static uint64_t pit_seq_read_begin(void) {
    return __atomic_load_n(&g_pit_seq, __ATOMIC_ACQUIRE);
}

static bool pit_seq_read_retry(uint64_t start) {
    uint64_t end = __atomic_load_n(&g_pit_seq, __ATOMIC_ACQUIRE);
    return (start != end) || (start & 1ULL);
}

static uint64_t pit_accumulate_tick(void) {
    uint16_t current = pit_read_channel0();
    uint16_t last = g_pit_last_counter;
    uint64_t tick;
    pit_callback_fn cb;
    void *ctx;
    uint64_t ms;

    if (current > last)
        g_pit_overflow_count++;

    g_pit_last_counter = current;
    tick = (g_pit_overflow_count << 16) | (uint64_t)current;

    cb = g_pit_callback;
    ctx = g_pit_callback_context;
    if (cb != NULL) {
        ms = pit_get_ms();
        cb(tick, ms, ctx);
    }

    return tick;
}

bool pit_initialize(void *system_table) {
    EFI_SYSTEM_TABLE *st = NULL;
    EFI_BOOT_SERVICES *bs = NULL;
    uint16_t start;
    uint16_t stop;
    uint16_t elapsed;
    uint64_t frequency;
    uint16_t probe_first;
    uint16_t probe_second;

    if (system_table != NULL)
        st = (EFI_SYSTEM_TABLE *)system_table;

    if (firmware == UEFI) {
        if (st == NULL)
            return false;
        bs = st->BootServices;
        if (bs == NULL)
            return false;

        outb(PIT_COMMAND_PORT, PIT_LATCH_CHANNEL0);
        start = pit_read_channel0();

        bs->Stall(PIT_CAL_STALL_US);

        stop = pit_read_channel0();
        elapsed = (uint16_t)(start - stop);

        if (elapsed == 0U)
            return false;

        frequency = elapsed * 1000000ULL / PIT_CAL_STALL_US;

        if (frequency < PIT_MIN_FREQ_HZ || frequency > PIT_MAX_FREQ_HZ)
            return false;
    } else {
        frequency = PIT_BIOS_DEFAULT_FREQ_HZ;

        probe_first = pit_read_channel0();
        pit_delay_iterations(PIT_BIOS_LIVENESS_ITER);
        probe_second = pit_read_channel0();
        if (probe_second == probe_first)
            return false;
    }

    if (!pit_program_hz(frequency))
        return false;

    g_pit_frequency_hz = frequency;
    g_pit_last_counter = pit_read_channel0();
    g_pit_overflow_count = 0ULL;
    g_pit_tick_at_rebase = pit_accumulate_tick();
    g_pit_ms_at_rebase = 0ULL;
    g_pit_seq = 0ULL;
    g_pit_operational = true;
    return true;
}

uint64_t pit_get_frequency_hz(void) {
    uint64_t freq;

    if (!g_pit_operational)
        return 0ULL;

    freq = __atomic_load_n(&g_pit_frequency_hz, __ATOMIC_ACQUIRE);
    if (freq == 0ULL)
        return 0ULL;

    if (IS_BIOS()) {
        uint16_t first = pit_read_channel0();
        pit_delay_iterations(PIT_BIOS_LIVENESS_ITER);
        uint16_t second = pit_read_channel0();
        if (first == second)
            return 0ULL;
    }

    return freq;
}

uint64_t pit_get_tick64(void) {
    return pit_accumulate_tick();
}

uint64_t pit_get_ms(void) {
    uint64_t start;
    uint64_t freq;
    uint64_t base_tick;
    uint64_t base_ms;
    uint64_t tick_now;
    uint64_t elapsed_ticks;
    uint64_t result;
    unsigned int retries;

    retries = 0U;
    for (;;) {
        start = pit_seq_read_begin();
        freq = g_pit_frequency_hz;
        base_tick = g_pit_tick_at_rebase;
        base_ms = g_pit_ms_at_rebase;
        tick_now = pit_accumulate_tick();
        if (!pit_seq_read_retry(start))
            break;
        if (++retries >= PIT_SEQ_RETRY_LIMIT)
            return 0ULL;
    }

    if (freq == 0ULL)
        return 0ULL;

    elapsed_ticks = (tick_now >= base_tick) ? (tick_now - base_tick) : 0ULL;
    result = base_ms + (elapsed_ticks * 1000ULL / freq);
    return result;
}

void pit_wait_ms(uint64_t ms) {
    uint64_t start_ms;
    uint64_t now;

    if (ms == 0ULL)
        return;

    start_ms = pit_get_ms();
    for (;;) {
        now = pit_get_ms();
        if ((now - start_ms) >= ms)
            return;
    }
}

bool pit_is_operational(void) {
    return g_pit_operational;
}

bool pit_set_frequency_hz(uint64_t frequency_hz) {
    uint64_t tick_now;
    uint64_t old_freq;
    uint64_t elapsed_ms;

    if (!g_pit_operational)
        return false;

    if (frequency_hz < PIT_MIN_FREQ_HZ || frequency_hz > PIT_MAX_FREQ_HZ)
        return false;

    if (!pit_program_hz(frequency_hz))
        return false;

    pit_seq_write_begin();

    old_freq = g_pit_frequency_hz;
    tick_now = pit_accumulate_tick();

    if (old_freq != 0ULL)
        elapsed_ms = (tick_now - g_pit_tick_at_rebase) * 1000ULL / old_freq;
    else
        elapsed_ms = 0ULL;

    g_pit_ms_at_rebase = g_pit_ms_at_rebase + elapsed_ms;
    g_pit_tick_at_rebase = tick_now;
    g_pit_frequency_hz = frequency_hz;

    pit_seq_write_end();
    return true;
}

bool pit_register_callback(pit_callback_fn callback, void *context) {
    if (!g_pit_operational)
        return false;

    pit_seq_write_begin();

    g_pit_callback = callback;
    g_pit_callback_context = context;

    pit_seq_write_end();
    return true;
}