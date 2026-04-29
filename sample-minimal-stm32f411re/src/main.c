/*
 * main.c — RTOS demo: tasks with heap tracing and ILI9341 display
 */

#include "config.h"
#include "devicetree.h"
#include "device.h"
#include "drivers/uart.h"
#include "sched.h"
#include "heap.h"
#include "trace.h"
#include <stdint.h>

DEVICE_DT_DECLARE(usart2);

static const struct device *uart;

/* ── Minimal ILI9341 SPI driver ── */
#define SPI1_BASE 0x40013000
#define SPI_CR1   (*(volatile uint32_t *)(SPI1_BASE + 0x00))
#define SPI_SR    (*(volatile uint32_t *)(SPI1_BASE + 0x08))
#define SPI_DR    (*(volatile uint32_t *)(SPI1_BASE + 0x0C))

/* ── I2S audio on SPI2 ── */
#define SPI2_BASE   0x40003800
#define I2S_CR1     (*(volatile uint32_t *)(SPI2_BASE + 0x00))
#define I2S_SR      (*(volatile uint32_t *)(SPI2_BASE + 0x08))
#define I2S_DR      (*(volatile uint32_t *)(SPI2_BASE + 0x0C))
#define I2S_CFGR    (*(volatile uint32_t *)(SPI2_BASE + 0x1C))
#define I2S_PR      (*(volatile uint32_t *)(SPI2_BASE + 0x20))
#define I2S_TXE     (1 << 1)

#define GPIOA_BSRR (*(volatile uint32_t *)0x40020018)
#define DC_PIN 3
#define GPIOA_ODR  (*(volatile uint32_t *)0x40020014)
#define DC_CMD()  do { if (GPIOA_ODR & (1 << DC_PIN)) GPIOA_BSRR = (1 << (DC_PIN + 16)); } while(0)
#define DC_DATA() do { if (!(GPIOA_ODR & (1 << DC_PIN))) GPIOA_BSRR = (1 << DC_PIN); } while(0)

static void spi_send(uint8_t b) { SPI_DR = b; }

static void lcd_cmd(uint8_t cmd)  { DC_CMD();  spi_send(cmd); }
static void lcd_data(uint8_t d)   { DC_DATA(); spi_send(d); }

static void lcd_init(void)
{
    SPI_CR1 = (1 << 6) | (1 << 2); /* SPE + MSTR */
    lcd_cmd(0x11); /* Sleep out */
    lcd_cmd(0x29); /* Display on */
}

static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    lcd_cmd(0x2A);
    lcd_data(x0 >> 8); lcd_data(x0 & 0xFF);
    lcd_data(x1 >> 8); lcd_data(x1 & 0xFF);
    lcd_cmd(0x2B);
    lcd_data(y0 >> 8); lcd_data(y0 & 0xFF);
    lcd_data(y1 >> 8); lcd_data(y1 & 0xFF);
    lcd_cmd(0x2C);
}

static void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    lcd_set_window(x, y, x + w - 1, y + h - 1);
    for (uint32_t i = 0; i < (uint32_t)w * h; i++) {
        lcd_data(color >> 8);
        lcd_data(color & 0xFF);
    }
}

/* NOP command triggers framebuffer flush in emulator */
static void lcd_vsync(void) { lcd_cmd(0x00); }

/* RGB565 color helpers */
#define RGB565(r,g,b) ((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|(((b)&0xF8)>>3))
#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     RGB565(255,0,0)
#define GREEN   RGB565(0,255,0)
#define BLUE    RGB565(0,0,255)
#define YELLOW  RGB565(255,255,0)
#define CYAN    RGB565(0,255,255)

static const struct device *uart;

static void uart_print(const char *s)
{
    while (*s) uart_poll_out(uart, *s++);
}

/* Traced heap wrappers */
static void *talloc(const char *name, unsigned size)
{
    void *p = heap_alloc(size);
    if (p) trace_alloc(name, p, size);
    return p;
}

static void tfree(void *p)
{
    if (p) trace_free(p);
    heap_free(p);
}

static void print_int(int n)
{
    if (n < 0) { uart_poll_out(uart, '-'); n = -n; }
    if (n >= 10) print_int(n / 10);
    uart_poll_out(uart, '0' + (n % 10));
}

static inline uint32_t irq_save(void) { uint32_t k; __asm volatile("mrs %0, primask\ncpsid i" : "=r"(k)); return k; }
static inline void irq_restore(uint32_t k) { __asm volatile("msr primask, %0" :: "r"(k)); }

static void task_a(void)
{
    int count = 0;
    while (1) {
        uart_print("a:");
        print_int(count);
        uart_print("\n");
        count++;
        sched_sleep_ms(1000);
    }
}

static void task_b(void)
{
    int count = 0;
    while (1) {
        uart_print("b:");
        print_int(count);
        uart_print("\n");
        count++;
        sched_sleep_ms(1000);
    }
}

/* ── Jump game constants ── */
#define SCR_W 320
#define SCR_H 240
#define GROUND_Y 200
#define PLAYER_W 16
#define PLAYER_H 20
#define PLAYER_X 40
#define OBS_W 12
#define OBS_H 30
#define GRAVITY 1
#define JUMP_VEL (-12)
#define SCROLL_SPEED 3
#define MAX_OBS 3

/* Simple PRNG */
static uint32_t rng_state = 12345;
static uint32_t rng(void) { rng_state ^= rng_state << 13; rng_state ^= rng_state >> 17; rng_state ^= rng_state << 5; return rng_state; }

/* Button input — reads GPIOB pin 0 (set by browser keyboard) */
#define GPIOB_IDR (*(volatile uint32_t *)0x40020410)
#define BTN_PIN 0
static int btn_pressed(void) { return (GPIOB_IDR >> BTN_PIN) & 1; }

/* NVIC ISER0 — enable external interrupts */
#define NVIC_ISER0 (*(volatile uint32_t *)0xE000E100)

/* EXTI registers */
#define EXTI_IMR   (*(volatile uint32_t *)0x40013C00)
#define EXTI_RTSR  (*(volatile uint32_t *)0x40013C08)
#define EXTI_PR    (*(volatile uint32_t *)0x40013C14)

/* Button → EXTI mapping: PB0=A/Jump, PB1=B, PB2=Left, PB3=Right, PB4=Up */
static const char *btn_names[] = {"A", "B", "Left", "Right", "Up", "Down"};

static void sfx_jump(void);
static void sfx_beep(void);

void exti0_handler(void) { EXTI_PR = (1 << 0); uart_print("[btn] A\n"); sfx_jump(); }
void exti1_handler(void) { EXTI_PR = (1 << 1); uart_print("[btn] B\n"); sfx_beep(); }
void exti2_handler(void) { EXTI_PR = (1 << 2); uart_print("[btn] Left\n"); }
void exti3_handler(void) { EXTI_PR = (1 << 3); uart_print("[btn] Right\n"); }
void exti4_handler(void) { EXTI_PR = (1 << 4); uart_print("[btn] Up\n"); }

static void buttons_init(void)
{
    /* Configure EXTI: rising edge trigger on lines 0-4 */
    EXTI_RTSR = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4);
    EXTI_IMR  = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4);
    /* Enable EXTI0-4 in NVIC (IRQ 6-10 → bits 6-10 of ISER0) */
    NVIC_ISER0 = (1 << 6) | (1 << 7) | (1 << 8) | (1 << 9) | (1 << 10);
}

static void draw_char(int x, int y, char c, uint16_t color)
{
    static const uint8_t font[][5] = {
        [0]={0x3E,0x51,0x49,0x45,0x3E}, [1]={0x00,0x42,0x7F,0x40,0x00},
        [2]={0x42,0x61,0x51,0x49,0x46}, [3]={0x21,0x41,0x45,0x4B,0x31},
        [4]={0x18,0x14,0x12,0x7F,0x10}, [5]={0x27,0x45,0x45,0x45,0x39},
        [6]={0x3C,0x4A,0x49,0x49,0x30}, [7]={0x01,0x71,0x09,0x05,0x03},
        [8]={0x36,0x49,0x49,0x49,0x36}, [9]={0x06,0x49,0x49,0x29,0x1E},
        [10]={0x7E,0x11,0x11,0x11,0x7E}, /* A */
        [11]={0x7F,0x49,0x49,0x49,0x36}, /* B */
        [12]={0x3E,0x41,0x41,0x41,0x22}, /* C */
        [13]={0x7F,0x41,0x41,0x22,0x1C}, /* D */
        [14]={0x7F,0x49,0x49,0x49,0x41}, /* E */
        [15]={0x3E,0x41,0x49,0x49,0x3A}, /* G */
        [16]={0x7F,0x04,0x08,0x10,0x7F}, /* M (simplified) */
        [17]={0x3E,0x41,0x41,0x41,0x3E}, /* O */
        [18]={0x7E,0x09,0x09,0x09,0x06}, /* P */
        [19]={0x7F,0x09,0x19,0x29,0x46}, /* R */
        [20]={0x26,0x49,0x49,0x49,0x32}, /* S */
        [21]={0x01,0x01,0x7F,0x01,0x01}, /* T */
        [22]={0x3F,0x40,0x40,0x40,0x3F}, /* V */
        [23]={0x00,0x41,0x7F,0x41,0x00}, /* I */
        [24]={0x7F,0x40,0x40,0x40,0x40}, /* L */
        [25]={0x7F,0x04,0x08,0x10,0x7F}, /* N */
        [26]={0x07,0x08,0x70,0x08,0x07}, /* Y */
    };
    const uint8_t *glyph = 0;
    if (c >= '0' && c <= '9') glyph = font[c - '0'];
    else {
        static const char map[] = "ABCDEGMOPRSTVILNY";
        static const int idx[] = {10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26};
        for (int i = 0; map[i]; i++)
            if (c == map[i]) { glyph = font[idx[i]]; break; }
    }
    if (!glyph) return;
    for (int col = 0; col < 5; col++)
        for (int row = 0; row < 7; row++)
            if (glyph[col] & (1 << row))
                lcd_fill_rect(x + col * 2, y + row * 2, 2, 2, color);
}

static void draw_string(int x, int y, const char *s, uint16_t color)
{
    while (*s) { draw_char(x, y, *s++, color); x += 12; }
}

static void draw_number(int x, int y, int n, uint16_t color)
{
    if (n >= 100) { draw_char(x, y, '0' + (n / 100) % 10, color); x += 12; }
    if (n >= 10)  { draw_char(x, y, '0' + (n / 10) % 10, color); x += 12; }
    draw_char(x, y, '0' + n % 10, color);
}

static int check_collision(int py, int obs_x[], int obs_gap[], int n)
{
    for (int i = 0; i < n; i++) {
        if (obs_x[i] < PLAYER_X + PLAYER_W && obs_x[i] + OBS_W > PLAYER_X) {
            int obs_top = GROUND_Y - obs_gap[i];
            if (py + PLAYER_H > obs_top)
                return 1;
        }
    }
    return 0;
}

static void task_c(void)
{
    uart_print("task_c: init\n");
    lcd_cmd(0x36);
    lcd_data(0x20);

    uart_print("game start\n");

    for (;;) { /* outer loop for restart */
    int player_y = GROUND_Y - PLAYER_H;
    int vel_y = 0;
    int on_ground = 1;
    int obs_x[MAX_OBS], obs_gap[MAX_OBS];
    int score = 0;
    int game_over = 0;

    for (int i = 0; i < MAX_OBS; i++) {
        obs_x[i] = SCR_W + 120 * i + (rng() % 80);
        obs_gap[i] = 20 + (rng() % 15);
    }

    lcd_fill_rect(0, 0, SCR_W, GROUND_Y, RGB565(30, 30, 50));
    lcd_fill_rect(0, GROUND_Y, SCR_W, SCR_H - GROUND_Y, RGB565(50, 120, 50));

    while (!game_over) {
        /* game_over block disabled for testing */

        if (btn_pressed() && on_ground) { vel_y = JUMP_VEL; on_ground = 0; }
        vel_y += GRAVITY;
        player_y += vel_y;
        if (player_y >= GROUND_Y - PLAYER_H) { player_y = GROUND_Y - PLAYER_H; vel_y = 0; on_ground = 1; }

        lcd_fill_rect(PLAYER_X, 0, PLAYER_W, GROUND_Y, RGB565(30, 30, 50));
        lcd_fill_rect(PLAYER_X, player_y, PLAYER_W, PLAYER_H, YELLOW);
        lcd_fill_rect(PLAYER_X + 10, player_y + 5, 3, 3, BLACK);

        for (int i = 0; i < MAX_OBS; i++) {
            if (obs_x[i] >= 0 && obs_x[i] < SCR_W)
                lcd_fill_rect(obs_x[i], 0, OBS_W, GROUND_Y, RGB565(30, 30, 50));
            obs_x[i] -= SCROLL_SPEED;
            if (obs_x[i] < -OBS_W) {
                obs_x[i] = SCR_W + 60 + (rng() % 120);
                obs_gap[i] = 20 + (rng() % 15);
                score++;
            }
            if (obs_x[i] >= 0 && obs_x[i] < SCR_W)
                lcd_fill_rect(obs_x[i], GROUND_Y - obs_gap[i], OBS_W, obs_gap[i], RED);
        }

        if (check_collision(player_y, obs_x, obs_gap, MAX_OBS))
            game_over = 1;

        lcd_vsync();
        sched_sleep_ms(33);
    } /* end while (!game_over) */

    /* Game over screen */
    lcd_fill_rect(30, 55, 260, 120, RGB565(15, 15, 25));
    draw_string(106, 70, "GAME OVER", WHITE);
    draw_string(118, 95, "SCORE", WHITE);
    draw_number(190, 95, score, YELLOW);
    draw_string(46, 120, "PRESS A TO PLAY AGAIN", GREEN);
    lcd_vsync();
    while (!btn_pressed()) sched_sleep_ms(33);
    while (btn_pressed()) sched_sleep_ms(33);
    } /* end for(;;) restart loop */
}

/* ── I2S audio engine ── */
static void i2s_init(void)
{
    I2S_CFGR = (1 << 11) | (1 << 10); /* I2SMOD=1, I2SE=1 */
    I2S_PR   = 0;
}

static void i2s_sample(int16_t left, int16_t right)
{
    while (!(I2S_SR & I2S_TXE)) {}
    I2S_DR = (uint16_t)left;
    while (!(I2S_SR & I2S_TXE)) {}
    I2S_DR = (uint16_t)right;
}

/* Note frequencies (Hz) for a simple chiptune scale */
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_REST 0

#define SAMPLE_RATE 22050

/* Melody: simple loop */
static const struct { uint16_t freq; uint16_t dur_ms; } melody[] = {
    {NOTE_C4, 200}, {NOTE_E4, 200}, {NOTE_G4, 200}, {NOTE_C5, 400},
    {NOTE_G4, 200}, {NOTE_E4, 200}, {NOTE_C4, 400},
    {NOTE_REST, 200},
    {NOTE_D4, 200}, {NOTE_F4, 200}, {NOTE_A4, 200}, {NOTE_B4, 400},
    {NOTE_A4, 200}, {NOTE_F4, 200}, {NOTE_D4, 400},
    {NOTE_REST, 200},
};
#define MELODY_LEN (sizeof(melody)/sizeof(melody[0]))

/* SFX state — set from ISR, consumed by audio task */
static volatile int sfx_active;
static volatile uint16_t sfx_freq;
static volatile uint16_t sfx_dur_ms;

static void sfx_jump(void) { sfx_freq = 880; sfx_dur_ms = 80; sfx_active = 1; }
static void sfx_beep(void) { sfx_freq = 1200; sfx_dur_ms = 50; sfx_active = 1; }

/* Square wave generator */
static int16_t square_sample(uint32_t phase, uint16_t freq)
{
    if (freq == 0) return 0;
    uint32_t half_period = SAMPLE_RATE / (2 * freq);
    if (half_period == 0) half_period = 1;
    return (phase % (half_period * 2)) < half_period ? 8000 : -8000;
}

static void task_audio(void)
{
    i2s_init();
    uart_print("audio: init\n");
    uint32_t phase = 0;
    int note_idx = 0;

    while (1) {
        /* Check for SFX trigger */
        if (sfx_active) {
            uint16_t f = sfx_freq;
            uint32_t samples = (uint32_t)sfx_dur_ms * SAMPLE_RATE / 1000;
            sfx_active = 0;
            for (uint32_t i = 0; i < samples; i++) {
                int16_t s = square_sample(i, f);
                i2s_sample(s, s);
            }
            phase = 0;
            continue;
        }

        /* Play current melody note */
        uint16_t freq = melody[note_idx].freq;
        uint32_t samples = (uint32_t)melody[note_idx].dur_ms * SAMPLE_RATE / 1000;
        for (uint32_t i = 0; i < samples; i++) {
            int16_t s = square_sample(phase++, freq);
            i2s_sample(s, s);
        }
        note_idx = (note_idx + 1) % MELODY_LEN;
    }
}

static void idle_task(void)
{
    while (1) {}
}

extern char _heap_start;
extern char _heap_size;

void main(void)
{
    uart = DEVICE_DT_GET(usart2);

    heap_init(&_heap_start, (size_t)&_heap_size);
    lcd_init();
    /* buttons_init(); -- temporarily disabled for debugging */

    uart_print("start\n");

    sched_create_task(task_a,    "task_a", 1);
    sched_create_task(task_b,    "task_b", 1);
    sched_create_task(task_c,    "task_c", 1);
    sched_create_task(task_audio,"audio",  1);
    sched_create_task(idle_task, "idle",   255);

    extern void systick_init(uint32_t cpu_hz, uint32_t tick_hz);
    systick_init(DT_SYSCLK_HZ, 1000);

    sched_start();
}
