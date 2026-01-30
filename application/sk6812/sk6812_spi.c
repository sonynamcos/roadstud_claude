/*
 * sk6812_spi.c
 *
 * SK6812 LED Driver - Boot Rainbow Only (DMM Safe)
 *
 * GPT Checklist Compliant:
 * - Called in main() BEFORE BIOS_start()
 * - Frame-based limit (not time-based)
 * - No RTOS APIs (Task_sleep, Clock, Event forbidden)
 * - Proper shutdown sequence
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <ti/drivers/SPI.h>
#include <ti/drivers/GPIO.h>
#include "ti_drivers_config.h"
#include "sk6812_spi.h"

/* Device family for CPUdelay */
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(driverlib/cpu.h)

/*
 * SPI Bit Encoding for WS2812/SK6812 @ 2.4MHz
 * 0 bit: 0x80 (short high pulse)
 * 1 bit: 0xF0 (long high pulse)
 */
#define LED_CODE_0  0x80
#define LED_CODE_1  0xF0

/* Buffer size: 4 colors (GRBW) x 8 bits x NUM_LEDS */
#define BUF_SIZE    (NUM_LEDS * 32)

/* DIO_23 = SPI MOSI = LED DIN */
#define LED_DIN_IOID    IOID_23

/* Static state */
static SPI_Handle g_spi = NULL;
static uint8_t txBuf[BUF_SIZE];
static bool g_shutdown = false;

/*
 * Minimal delay for latch time (80us minimum for SK6812)
 * Using CPUdelay - OK before BIOS_start since no other tasks exist
 */
static void latchDelay(void)
{
    /* ~100us at 48MHz: 48 cycles/us * 100us = 4800 */
    CPUdelay(4800);
}

/*
 * Short inter-frame delay (~20ms)
 * Using CPUdelay - OK before BIOS_start
 */
static void frameDelay(void)
{
    /* ~20ms at 48MHz: 48000 cycles/ms * 20ms = 960000 */
    CPUdelay(960000);
}

/*
 * Encode a single bit to SPI byte
 */
static inline uint8_t encodeBit(uint8_t bitVal)
{
    return bitVal ? LED_CODE_1 : LED_CODE_0;
}

/*
 * Color wheel for rainbow effect
 */
static RGBW colorWheel(uint8_t pos)
{
    RGBW c = {0, 0, 0, 0};
    pos = 255 - pos;

    if (pos < 85) {
        c.r = 255 - pos * 3;
        c.g = 0;
        c.b = pos * 3;
    } else if (pos < 170) {
        pos -= 85;
        c.r = 0;
        c.g = pos * 3;
        c.b = 255 - pos * 3;
    } else {
        pos -= 170;
        c.r = pos * 3;
        c.g = 255 - pos * 3;
        c.b = 0;
    }
    return c;
}

/*
 * Scale brightness (0-255)
 */
static inline uint8_t scale8(uint8_t val, uint8_t scale)
{
    return (uint8_t)(((uint16_t)val * (uint16_t)scale) >> 8);
}

/*
 * Fill buffer with rainbow gradient and transmit
 */
static void sendRainbowFrame(uint8_t baseHue, uint8_t brightness)
{
    if (g_spi == NULL) return;

    uint32_t j = 0;
    int bit;

    /* Build buffer with per-LED rainbow gradient */
    for (uint32_t led = 0; led < NUM_LEDS; led++) {
        /* Each LED gets offset hue for gradient effect */
        uint8_t hue = baseHue + (uint8_t)(((uint16_t)led * 256) / NUM_LEDS);
        RGBW c = colorWheel(hue);

        /* Apply brightness scaling */
        c.r = scale8(c.r, brightness);
        c.g = scale8(c.g, brightness);
        c.b = scale8(c.b, brightness);
        c.w = 0;

        /* SK6812 order: G, R, B, W */
        uint8_t colors[4] = {c.g, c.r, c.b, c.w};

        for (uint32_t color = 0; color < 4; color++) {
            for (bit = 7; bit >= 0; bit--) {
                txBuf[j++] = encodeBit(colors[color] & (1 << bit));
            }
        }
    }

    /* SPI transfer */
    SPI_Transaction t;
    memset(&t, 0, sizeof(t));
    t.count = BUF_SIZE;
    t.txBuf = (void *)txBuf;
    t.rxBuf = NULL;

    SPI_transfer(g_spi, &t);
    latchDelay();
}

/*
 * Send all LEDs same color
 */
static void sendAllSame(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    if (g_spi == NULL) return;

    uint32_t j = 0;
    int bit;
    uint8_t colors[4] = {g, r, b, w};  /* GRBW order */

    for (uint32_t led = 0; led < NUM_LEDS; led++) {
        for (uint32_t color = 0; color < 4; color++) {
            for (bit = 7; bit >= 0; bit--) {
                txBuf[j++] = encodeBit(colors[color] & (1 << bit));
            }
        }
    }

    SPI_Transaction t;
    memset(&t, 0, sizeof(t));
    t.count = BUF_SIZE;
    t.txBuf = (void *)txBuf;
    t.rxBuf = NULL;

    SPI_transfer(g_spi, &t);
    latchDelay();
}

/*
 * Open SPI for LED
 */
static bool openSPI(void)
{
    SPI_Params params;

    SPI_init();
    SPI_Params_init(&params);
    params.dataSize = 8;
    params.bitRate = 2400000;  /* 2.4MHz for WS2812/SK6812 timing */
    params.frameFormat = SPI_POL0_PHA0;
    params.mode = SPI_CONTROLLER;

    g_spi = SPI_open(CONFIG_SPI_0, &params);
    return (g_spi != NULL);
}

/*
 * Close SPI and set DIN to GPIO LOW
 */
static void shutdownLED(void)
{
    if (g_spi != NULL) {
        SPI_close(g_spi);
        g_spi = NULL;
    }

    /*
     * Set DIO_23 (LED DIN) to GPIO output LOW
     * This prevents floating state and ensures LED stays off
     *
     * Note: External NVS flash shares SPI but is not used at runtime
     * in this project, so GPIO LOW is safe.
     */
    GPIO_setConfig(LED_DIN_IOID, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_write(LED_DIN_IOID, 0);

    g_shutdown = true;
}

/*
 * Boot Rainbow - Main entry point
 *
 * Call this ONCE in main() BEFORE BIOS_start()
 */
void BootRainbow_RunOnce(uint8_t numFrames)
{
    uint8_t baseHue = 0;
    uint8_t hueStep = 8;  /* Speed of color rotation */
    uint8_t brightness = 128;  /* 50% brightness to reduce power */

    /* 1. Open SPI */
    if (!openSPI()) {
        /* SPI open failed - just mark as shutdown and return */
        g_shutdown = true;
        return;
    }

    /* 2. Play rainbow animation (frame-limited) */
    for (uint8_t i = 0; i < numFrames; i++) {
        sendRainbowFrame(baseHue, brightness);
        baseHue += hueStep;
        frameDelay();  /* ~20ms between frames */
    }

    /* 3. Send OFF frame (all zeros) - MUST transmit, not just buffer */
    sendAllSame(0, 0, 0, 0);

    /* 4. Shutdown: SPI close + GPIO LOW */
    shutdownLED();
}

/*
 * Check if LED subsystem is shut down
 */
bool SK6812_isShutdown(void)
{
    return g_shutdown;
}
