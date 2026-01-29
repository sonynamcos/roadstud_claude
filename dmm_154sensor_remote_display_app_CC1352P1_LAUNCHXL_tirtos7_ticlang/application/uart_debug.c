#include "uart_debug.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// 전역 변수 (모든 파일이 이 하나를 공유하게 됨)
UART2_Handle uart2Handle = NULL;

int uartDebugInit(void) {
    UART2_Params uart2Params;
    UART2_Params_init(&uart2Params);
    uart2Params.baudRate = 115200;
    uart2Params.writeMode = UART2_Mode_BLOCKING;
    
    uart2Handle = UART2_open(CONFIG_DISPLAY_UART, &uart2Params);
    if (uart2Handle == NULL) return -1;

    uartPrint("UART Debug Initialized\r\n");
    return 0;
}

void uartPrint(const char* msg) {
    if (uart2Handle != NULL && msg != NULL) {
        UART2_write(uart2Handle, msg, strlen(msg), NULL);
    }
}

void uartPrintf(const char* format, ...) {
    if (uart2Handle != NULL && format != NULL) {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        UART2_write(uart2Handle, buffer, strlen(buffer), NULL);
    }
}

void uartPrintHex(const char* prefix, const uint8_t* data, size_t len)
{
    if (uart2Handle != NULL && data != NULL) {
        if (prefix != NULL) {
            uartPrint(prefix);
            uartPrint(": ");
        }
        
        for (size_t i = 0; i < len; i++) {
            char hex[4];
            snprintf(hex, sizeof(hex), "%02X ", data[i]);
            uartPrint(hex);
        }
        uartPrint("\r\n");
    }
}
// (나머지 uartPrintHex 등도 동일하게 .c로 이동)