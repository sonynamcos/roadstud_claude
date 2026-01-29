#ifndef SK6812_SPI_H_
#define SK6812_SPI_H_

#include <stdint.h>

#define NUM_LEDS 6

typedef struct {
    uint8_t r, g, b, w;
} RGBW;

void SK6812_init(void);
void SK6812_setAll(uint8_t r, uint8_t g, uint8_t b, uint8_t w);

/* 무지개: 내부에 baseHue 유지 */
void SK6812_rainbowStep(uint8_t brightness, uint8_t w_level);
void SK6812_rainbowFlowStep(uint8_t brightness, uint8_t w_level, uint8_t hue_step);
void sk6812_flash_once(uint8_t r, uint8_t g, uint8_t b, uint32_t duration_ms);
// sk6812_spi.h 파일 뒷부분에 추가
void sk6812_flash_once(uint8_t r, uint8_t g, uint8_t b, uint32_t duration_ms);
void sk6812_set_pixel(uint16_t idx, uint8_t r, uint8_t g, uint8_t b);

#endif
