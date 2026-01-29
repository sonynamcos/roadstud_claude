/*
 * rf_switch_override.c
 * CKRF2179MM26 RF Switch Control for Sub-1GHz
 *
 * Truth Table (from schematic):
 * DIO_6 (VC1)  DIO_5 (VC2)  TX    RX
 * Low          High         ON    OFF
 * High         Low          OFF   ON
 */

#include <stdint.h>
#include <stdbool.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/rf/RF.h>
#include <ti/devices/DeviceFamily.h>
#include <ti/sysbios/knl/Clock.h>
#include DeviceFamily_constructPath(driverlib/ioc.h)
#include DeviceFamily_constructPath(driverlib/rf_common_cmd.h)
#include DeviceFamily_constructPath(driverlib/rf_prop_cmd.h)
#include DeviceFamily_constructPath(driverlib/rf_ieee_cmd.h)

#include "ti_drivers_config.h"

/* ===== RF Switch Control ===== */

// TX 모드: DIO_6=Low, DIO_5=High (진리표 기준)
static inline void rf_switch_set_tx(void)
{
    GPIO_write(CONFIG_RF_SW_DIO6, 0);  // Low
    GPIO_write(CONFIG_RF_SW_DIO5, 1);  // High
}

// RX 모드: DIO_6=High, DIO_5=Low (진리표 기준)
static inline void rf_switch_set_rx(void)
{
    GPIO_write(CONFIG_RF_SW_DIO6, 1);  // High
    GPIO_write(CONFIG_RF_SW_DIO5, 0);  // Low
}

// 모든 경로 OFF
static inline void rf_switch_off(void)
{
    GPIO_write(CONFIG_RF_SW_DIO6, 0);
    GPIO_write(CONFIG_RF_SW_DIO5, 0);
}

/* ===== Safety Timer: Auto-return to RX ===== */
static Clock_Struct swReturnClk;
static bool swClkInit = false;

static void swReturnCb(UArg a0)
{
    (void)a0;
    rf_switch_set_rx();
}

static void swArmReturnMs(uint32_t ms)
{
    if (!swClkInit)
    {
        Clock_Params p;
        Clock_Params_init(&p);
        p.period = 0;
        p.startFlag = false;
        Clock_construct(&swReturnClk, swReturnCb, 1, &p);
        swClkInit = true;
    }

    uint32_t ticks = (ms * 1000) / Clock_tickPeriod;
    if (ticks < 1) ticks = 1;

    Clock_stop(Clock_handle(&swReturnClk));
    Clock_setTimeout(Clock_handle(&swReturnClk), ticks);
    Clock_start(Clock_handle(&swReturnClk));
}

/* ===== TX Command Detection ===== */
static inline bool isTxCommand(uint16_t cmdNo)
{
    switch (cmdNo)
    {
        case CMD_IEEE_TX:
        case CMD_PROP_TX:
        case CMD_PROP_TX_ADV:
        case CMD_PROP_CS:
            return true;
        default:
            return false;
    }
}

/* ===== RF Driver Callback ===== */
void rfDriverCallback(RF_Handle client, RF_GlobalEvent events, void *arg)
{
    (void)client;

    // 초기화 시 RX 모드로 시작
    if (events & RF_GlobalEventInit)
    {
        rf_switch_set_rx();
    }

    // Radio Setup 시 RX 모드
    if (events & RF_GlobalEventRadioSetup)
    {
        rf_switch_set_rx();
    }

    // Command 시작 시 TX/RX 판별
    if (events & RF_GlobalEventCmdStart)
    {
        RF_Op *pOp = (RF_Op *)arg;
        if (pOp != NULL && isTxCommand(pOp->commandNo))
        {
            rf_switch_set_tx();
            swArmReturnMs(8);  // 안전장치: 8ms 후 RX 복귀
        }
        else
        {
            rf_switch_set_rx();
        }
    }

    // Command 종료 시 RX 모드로 복귀
    if (events & RF_GlobalEventCmdStop)
    {
        rf_switch_set_rx();
    }

    // Power Down 시 OFF
    if (events & RF_GlobalEventRadioPowerDown)
    {
        rf_switch_off();
    }
}

// Alias for compatibility
void rfDriverCallbackAntennaSwitching(RF_Handle client, RF_GlobalEvent events, void *arg)
{
    rfDriverCallback(client, events, arg);
}
