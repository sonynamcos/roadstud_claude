#ifndef SK6812_SPI_H_
#define SK6812_SPI_H_

#include <stdint.h>

#define NUM_LEDS 6

/* 시나리오 정의 */
#define SCENARIO_NORMAL      1   // 평상시: 차선색 고정
#define SCENARIO_RAIN        2   // 비: 차선색 숨쉬기 + 빨간색 펄스
#define SCENARIO_FOG         3   // 안개: 차선색 숨쉬기 + 노란색 펄스
#define SCENARIO_ACCIDENT    4   // 사고: 차선색 1초 깜빡
#define SCENARIO_SEQUENTIAL  5   // 순차점등: ID 기반 offset 숨쉬기
#define SCENARIO_RAINBOW     6   // 무지개
#define SCENARIO_OFF         7   // 모두 끄기

/* 차선 타입 */
#define LANE_YELLOW      0   // 황색
#define LANE_WHITE_IN    1   // 흰색(안)
#define LANE_WHITE_OUT   2   // 흰색(밖)

typedef struct {
    uint8_t r, g, b, w;
} RGBW;

/* 기본 함수 */
void SK6812_init(void);
void SK6812_setAll(uint8_t r, uint8_t g, uint8_t b, uint8_t w);

/* 무지개 */
void SK6812_rainbowStep(uint8_t brightness, uint8_t w_level);
void SK6812_rainbowFlowStep(uint8_t brightness, uint8_t w_level, uint8_t hue_step);

/* 유틸 */
void sk6812_flash_once(uint8_t r, uint8_t g, uint8_t b, uint32_t duration_ms);
void sk6812_set_pixel(uint16_t idx, uint8_t r, uint8_t g, uint8_t b);

/* 시나리오 제어 */
void SK6812_setScenario(uint8_t scenario);
void SK6812_setLaneType(uint8_t laneType);
void SK6812_setNodeId(uint8_t nodeId);
void SK6812_scenarioTick(void);  // 주기적으로 호출 (20~50ms 권장)

/* 축하 효과 (ID 설정 성공 시 등) */
void SK6812_startRainbowCelebration(void);  // 1초간 무지개 후 이전 시나리오로 복원

#endif
