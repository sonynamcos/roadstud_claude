/*
 * sk6812_spi.h
 *
 * SK6812 LED Driver - Boot Rainbow Only (DMM Safe)
 *
 * GPT Checklist Compliant:
 * - Frame-based limit (not time-based)
 * - No RTOS dependencies before BIOS_start
 * - Proper shutdown (OFF frame + SPI close + GPIO LOW)
 */

#ifndef SK6812_SPI_H_
#define SK6812_SPI_H_

#include <stdint.h>
#include <stdbool.h>

/* LED count */
#define NUM_LEDS 6

/* RGBW structure */
typedef struct {
    uint8_t r, g, b, w;
} RGBW;

/*
 * Boot Rainbow - Call ONCE in main() before BIOS_start()
 *
 * This function:
 * 1. Opens SPI
 * 2. Plays N frames of rainbow animation
 * 3. Sends OFF frame
 * 4. Closes SPI
 * 5. Sets DIO_23 (LED DIN) to GPIO LOW
 *
 * After this function returns, LED subsystem is completely shut down
 * and will NOT interfere with RF/BLE operations.
 *
 * @param numFrames - Number of rainbow frames to play (recommended: 30-40)
 */
extern void BootRainbow_RunOnce(uint8_t numFrames);

/*
 * Check if boot rainbow has completed
 * Returns true if LED subsystem is shut down
 */
extern bool SK6812_isShutdown(void);

#endif /* SK6812_SPI_H_ */
