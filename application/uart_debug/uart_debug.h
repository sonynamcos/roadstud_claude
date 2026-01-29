#ifndef UART_DEBUG_H
#define UART_DEBUG_H

#include <ti/drivers/UART2.h>
#include "ti_drivers_config.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 함수 선언
int uartDebugInit(void);
void uartPrint(const char* msg);
void uartPrintf(const char* format, ...);
void uartPrintHex(const char* prefix, const uint8_t* data, size_t len);

// 매크로
#define UART_LOG(msg)               uartPrint(msg "\r\n")
#define UART_LOG_INFO(msg)          uartPrint("[INFO] " msg "\r\n")
#define UART_SEPARATOR()            uartPrint("========================================\r\n")

#ifdef __cplusplus
}
#endif
#endif