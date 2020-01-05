#ifndef __UART_IF_H__
#define __UART_IF_H__

#include <ti/drivers/UART.h>
#include "ti_drivers_config.h"

//
//
//
#define UART_PRINT Report
#define DBG_PRINT  Report
#define ERR_PRINT(x) Report ("Error [%d] at line [%d] in function [%s]\n", x, __LINE__, __FUNCTION__)

//
//
//
UART_Handle InitTerm (void);
int Report (const char *pcFormat, ...);
int TrimSpace (char * pcInput);
int GetCmd (char *pcBuffer, unsigned int uiBufLen);
void Message (const char *str);
void ClearTerm (void);
char getch (void);
void putch (char ch);

#endif
