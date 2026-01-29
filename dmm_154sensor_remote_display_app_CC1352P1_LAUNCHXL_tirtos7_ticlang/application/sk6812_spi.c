#include <stdbool.h>
#include <ti/drivers/SPI.h>
#include "ti_drivers_config.h"
#include "sk6812_spi.h"
#include "uart_debug.h"
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <string.h>
#include <ti/drivers/dpl/ClockP.h>
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(driverlib/cpu.h)


/* 2.4MHz SPI 기준 */
#define LED_CODE_0 0x80
#define LED_CODE_1 0xF0

#define BUF_SIZE (NUM_LEDS * 32)
static SPI_Handle spiHandle; // 이 줄을 추가해서 모든 함수가 공유하게 합니다.

static SPI_Handle g_spi = NULL;
static uint8_t txBuf[BUF_SIZE];
static uint8_t g_baseHue = 0;

static uint8_t encodeBit(uint8_t is1) { return is1 ? LED_CODE_1 : LED_CODE_0; }


// static void latch_delay_us(uint32_t us)
// {
//     /* us를 tick으로 환산 (최소 1tick 보장) */
//     uint32_t tick_us = Clock_tickPeriod; // tick period in microseconds
//     uint32_t ticks = (us + tick_us - 1U) / tick_us; // ceil
//     if (ticks == 0U) ticks = 1U;
//     Task_sleep(ticks);
// }

static void latch_delay_us(uint32_t us)
{
    // us 단위를 틱 단위로 계산 (최소 1틱 보장)
    // 보통 1틱은 10us~100us 사이입니다. 1틱만 쉬어도 80us 리셋 타임은 충분합니다.
    CPUdelay(us * 48);
}

static void sleep_ms(uint32_t ms)
{
    uint32_t ticks = (ms * 1000U) / Clock_tickPeriod; // Clock_tickPeriod: us/tick인 경우가 일반적
    if (ticks == 0U) ticks = 1U;
    Task_sleep(ticks);
}

static RGBW colorWheel(uint8_t pos)
{
    RGBW c;
    c.r = c.g = c.b = c.w = 0;
    pos = (uint8_t)(255 - pos);

    if (pos < 85) {
        c.r = (uint8_t)(255 - pos * 3);
        c.g = 0;
        c.b = (uint8_t)(pos * 3);
    } else if (pos < 170) {
        pos = (uint8_t)(pos - 85);
        c.r = 0;
        c.g = (uint8_t)(pos * 3);
        c.b = (uint8_t)(255 - pos * 3);
    } else {
        pos = (uint8_t)(pos - 170);
        c.r = (uint8_t)(pos * 3);
        c.g = (uint8_t)(255 - pos * 3);
        c.b = 0;
    }
    return c;
}

static uint8_t scale8(uint8_t v, uint8_t s)
{
    return (uint8_t)(((uint16_t)v * (uint16_t)s) >> 8);
}

static void sendAllSame(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    /* SPI가 초기화 안 됐으면 무시 */
    if (g_spi == NULL) return;

    uint32_t led_idx, color_idx, j;
    int bit;// MUST be signed int; do not change to uint8_t (would infinite-loop)
    uint8_t colors[4];

    colors[0] = g; colors[1] = r; colors[2] = b; colors[3] = w;

    j = 0;
    for (led_idx = 0; led_idx < NUM_LEDS; led_idx++) {
        for (color_idx = 0; color_idx < 4; color_idx++) {
            for (bit = 7; bit >= 0; bit--) {
                txBuf[j++] = encodeBit(colors[color_idx] & (1 << bit));
            }
        }
    }

    {
        SPI_Transaction t;
        memset(&t, 0, sizeof(t));
        t.count = BUF_SIZE;
        t.txBuf = (void *)txBuf;
        t.rxBuf = NULL;

        SPI_transfer(g_spi, &t);  /* 실패해도 계속 진행 */
    }

    latch_delay_us(80);/* latch */
}

void SK6812_init(void)
{
    SPI_Params p;

    SPI_init();
    SPI_Params_init(&p);
    p.dataSize = 8;
    p.bitRate = 2400000;
    p.frameFormat = SPI_POL0_PHA0;
    p.mode = SPI_CONTROLLER;

    g_spi = SPI_open(CONFIG_SPI_0, &p);  /* SK6812: DIO_23 (외장 플래시와 공유) */
    if (g_spi == NULL)
    {
        uartPrintf("[SK6812] ERROR: SPI_open failed!\r\n");
        return;
    }

    uartPrintf("[SK6812] SPI opened OK\r\n");
    sendAllSame(0,0,0,0);
}

void SK6812_setAll(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    sendAllSame(r,g,b,w);
}

// LED별로 다른 색을 보내는 전송 함수
static void sendPerLedRainbowFlow(uint8_t brightness, uint8_t w_level, uint8_t baseHue)
{
    /* SPI가 초기화 안 됐으면 무시 */
    if (g_spi == NULL) return;

    uint32_t led_idx, color_idx, j;
    int bit;

    j = 0;
    for (led_idx = 0; led_idx < NUM_LEDS; led_idx++) {

        // LED마다 hue 오프셋을 줘서 "그라데이션" 만들기
        // (NUM_LEDS가 256을 안 나누면 이 방식이 더 정확)
        uint8_t hue = (uint8_t)(baseHue + (uint8_t)(((uint16_t)led_idx * 256) / NUM_LEDS));

        RGBW c = colorWheel(hue);

        c.r = scale8(c.r, brightness);
        c.g = scale8(c.g, brightness);
        c.b = scale8(c.b, brightness);
        c.w = w_level;

        // SK6812(SPI 방식)에서 보통 GRBW 순서로 전송
        uint8_t colors[4] = { c.g, c.r, c.b, c.w };

        for (color_idx = 0; color_idx < 4; color_idx++) {
            for (bit = 7; bit >= 0; bit--) {
                txBuf[j++] = encodeBit(colors[color_idx] & (1 << bit));
            }
        }
    }

    SPI_Transaction t;
    memset(&t, 0, sizeof(t));
    t.count = BUF_SIZE;
    t.txBuf = (void *)txBuf;
    t.rxBuf = NULL;

    SPI_transfer(g_spi, &t);  /* 실패해도 계속 진행 */

    latch_delay_us(80); /* latch */
}

/**
 * 무지개 "흐름" 1프레임 출력 (LED마다 색 다름 + baseHue 이동)
 * - brightness: 0~255
 * - w_level   : 0~255
 * - hue_step  : 프레임마다 baseHue 증가량 (2~8 추천)
 */
void SK6812_rainbowFlowStep(uint8_t brightness, uint8_t w_level, uint8_t hue_step)
{
    sendPerLedRainbowFlow(brightness, w_level, g_baseHue);
    g_baseHue = (uint8_t)(g_baseHue + hue_step);
}

// 1. 함수 선언 (프로토타입) - 파일 상단에 두면 순서 문제가 해결됩니다.
void sk6812_set_pixel(uint16_t idx, uint8_t r, uint8_t g, uint8_t b);
void sk_tx_once(void); // 이미 파일 어딘가에 정의되어 있을 테니 선언만 합니다.
// 2. 픽셀 값 세팅 함수
// txBuf가 static이면 이 파일 안에서만 쓸 수 있습니다.
// 2. 픽셀 값 세팅 함수 (수정됨)
void sk6812_set_pixel(uint16_t idx, uint8_t r, uint8_t g, uint8_t b) {
    uint32_t j = idx * 32; // LED 1개당 32바이트 (G, R, B, W 각각 8비트씩)
    
    // SK6812 RGBW 모델이므로 G, R, B, W 순서로 인코딩합니다. (W는 0으로 고정)
    uint8_t colors[4] = { g, r, b, 0 }; 

    for (int color_idx = 0; color_idx < 4; color_idx++) {
        for (int bit = 7; bit >= 0; bit--) {
            // 비트 하나하나를 SPI 바이트(0x80/0xF0)로 변환해서 txBuf에 채움
            txBuf[j++] = encodeBit(colors[color_idx] & (1 << bit));
        }
    }
}

/* -----------------------------------------------------------
 * 1. Latch 지연 함수 수정 (더 안전한 방식으로)
 * ----------------------------------------------------------- */


/* -----------------------------------------------------------
 * 2. 전송 함수 수정 (핸들 이름과 지연 방식 통일)
 * ----------------------------------------------------------- */
void sk_tx_once(void) {
    SPI_Transaction t;
    memset(&t, 0, sizeof(t));
    t.count = BUF_SIZE;
    t.txBuf = (void *)txBuf;
    t.rxBuf = NULL;

    // SK6812_init에서 열었던 g_spi를 사용합니다.
    if (g_spi != NULL) { 
        SPI_transfer(g_spi, &t); 
        // 전송 직후 LED가 데이터를 인식할 수 있게 리셋 타임 제공
        latch_delay_us(100); 
    }
}

/* -----------------------------------------------------------
 * 3. 반짝이기 함수 수정 (인자 개수 오류 해결)
 * ----------------------------------------------------------- */

void sk6812_flash_once(uint8_t r, uint8_t g, uint8_t b, uint32_t duration_ms)
{
    sk6812_set_pixel(0, r, g, b);
    sk_tx_once();

    sleep_ms(duration_ms);

    sk6812_set_pixel(0, 0, 0, 0);
    sk_tx_once();

    sleep_ms(duration_ms);
}

/* =========================================================
 * 시나리오 제어 구현
 * ========================================================= */

/* 현재 상태 */
static uint8_t g_scenario = SCENARIO_NORMAL;
static uint8_t g_laneType = LANE_YELLOW;
static uint8_t g_nodeId = 1;

/* 애니메이션 상태 */
static uint16_t g_tickCount = 0;
static uint8_t g_phase = 0;        // 비/안개: 0=차선색, 1=경고색
static uint8_t g_breathDir = 1;    // 숨쉬기 방향: 1=up, 0=down
static uint8_t g_breathVal = 0;    // 숨쉬기 밝기 (0~255)

/* 축하 효과 상태 (무지개 1초) */
static uint8_t g_celebrationTicks = 0;   // 0이면 비활성, >0이면 남은 틱
static uint8_t g_prevScenario = SCENARIO_NORMAL;  // 축하 전 시나리오

/* 차선색 가져오기 */
static RGBW getLaneColor(uint8_t laneType)
{
    RGBW c = {0, 0, 0, 0};
    switch(laneType) {
        case LANE_YELLOW:
            c.r = 255; c.g = 180; c.b = 0;
            break;
        case LANE_WHITE_IN:
        case LANE_WHITE_OUT:
        default:
            c.r = 255; c.g = 255; c.b = 255;
            break;
    }
    return c;
}

/* 숨쉬기 업데이트 (부드러운 fade) */
static void updateBreath(uint8_t speed)
{
    if(g_breathDir) {
        if(g_breathVal < 255 - speed) {
            g_breathVal += speed;
        } else {
            g_breathVal = 255;
            g_breathDir = 0;
        }
    } else {
        if(g_breathVal > speed) {
            g_breathVal -= speed;
        } else {
            g_breathVal = 0;
            g_breathDir = 1;
        }
    }
}

/* 밝기 적용 */
static void setAllScaled(RGBW c, uint8_t brightness)
{
    uint8_t r = scale8(c.r, brightness);
    uint8_t g = scale8(c.g, brightness);
    uint8_t b = scale8(c.b, brightness);
    sendAllSame(r, g, b, 0);
}

void SK6812_setScenario(uint8_t scenario)
{
    g_scenario = scenario;
    g_tickCount = 0;
    g_phase = 0;
    g_breathVal = 0;
    g_breathDir = 1;
}

void SK6812_setLaneType(uint8_t laneType)
{
    g_laneType = laneType;
}

void SK6812_setNodeId(uint8_t nodeId)
{
    g_nodeId = nodeId;
}

/* 주기적으로 호출 (20~50ms 간격 권장) */
void SK6812_scenarioTick(void)
{
    g_tickCount++;

    /* 축하 모드 처리 (무지개 1초) */
    if(g_celebrationTicks > 0)
    {
        SK6812_rainbowFlowStep(255, 0, 5);  /* 빠른 무지개 */
        g_celebrationTicks--;
        if(g_celebrationTicks == 0)
        {
            /* 축하 종료, 이전 시나리오로 복원 */
            g_scenario = g_prevScenario;
        }
        return;
    }

    RGBW laneColor = getLaneColor(g_laneType);
    RGBW warnColor;

    switch(g_scenario) {

    case SCENARIO_NORMAL:
        /* 평상시: 차선색 고정 */
        sendAllSame(laneColor.r, laneColor.g, laneColor.b, 0);
        break;

    case SCENARIO_RAIN:
        /* 비: 차선색 숨쉬기 → 빨간색 펄스 → 반복 */
        updateBreath(5);
        if(g_phase == 0) {
            setAllScaled(laneColor, g_breathVal);
            if(g_breathVal == 0 && g_breathDir == 1) {
                g_phase = 1;  // 빨간색으로 전환
            }
        } else {
            warnColor.r = 255; warnColor.g = 0; warnColor.b = 0;
            setAllScaled(warnColor, g_breathVal);
            if(g_breathVal == 0 && g_breathDir == 1) {
                g_phase = 0;  // 차선색으로 전환
            }
        }
        break;

    case SCENARIO_FOG:
        /* 안개: 차선색 숨쉬기 → 노란색 펄스 → 반복 */
        updateBreath(5);
        if(g_phase == 0) {
            setAllScaled(laneColor, g_breathVal);
            if(g_breathVal == 0 && g_breathDir == 1) {
                g_phase = 1;  // 노란색으로 전환
            }
        } else {
            warnColor.r = 255; warnColor.g = 200; warnColor.b = 0;
            setAllScaled(warnColor, g_breathVal);
            if(g_breathVal == 0 && g_breathDir == 1) {
                g_phase = 0;  // 차선색으로 전환
            }
        }
        break;

    case SCENARIO_ACCIDENT:
        /* 사고: 차선색 1초 깜빡 (50ms tick 기준 20틱 = 1초) */
        if((g_tickCount % 20) < 10) {
            sendAllSame(laneColor.r, laneColor.g, laneColor.b, 0);
        } else {
            sendAllSame(0, 0, 0, 0);
        }
        break;

    case SCENARIO_SEQUENTIAL:
        /* 순차점등: ID 기반 offset 숨쉬기 */
        {
            /* nodeId에 따른 위상 offset (0~255 범위) */
            uint16_t offset = (uint16_t)g_nodeId * 40;  // ID당 40 offset
            uint16_t phase = (g_tickCount * 3 + offset) & 0xFF;

            /* 사인파 근사: 0~127~0 패턴 */
            uint8_t brightness;
            if(phase < 128) {
                brightness = phase * 2;
            } else {
                brightness = (255 - phase) * 2;
            }

            setAllScaled(laneColor, brightness);
        }
        break;

    case SCENARIO_RAINBOW:
        /* 무지개: 기존 함수 활용 */
        SK6812_rainbowFlowStep(255, 0, 3);
        break;

    case SCENARIO_OFF:
        /* 모두 끄기 */
        sendAllSame(0, 0, 0, 0);
        break;

    default:
        /* 알 수 없는 시나리오: 꺼짐 */
        sendAllSame(0, 0, 0, 0);
        break;
    }
}

/**
 * 축하 효과 (ID 설정 성공 시 등)
 * 1초간 무지개를 보여준 후 차선색으로 복원 (blocking)
 */
void SK6812_startRainbowCelebration(void)
{
    uint8_t i;

    /* 약 3초간 무지개 애니메이션 (30ms x 100 = 3초) */
    for(i = 0; i < 100; i++)
    {
        SK6812_rainbowFlowStep(255, 0, 8);  /* 빠른 무지개 */
        sleep_ms(30);
    }

    /* 차선색으로 복원 */
    RGBW laneColor = getLaneColor(g_laneType);
    sendAllSame(laneColor.r, laneColor.g, laneColor.b, 0);
}