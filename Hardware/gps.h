#ifndef __GPS_H
#define __GPS_H

#include "stm32f10x.h"
#include <stdint.h>

/*
接线：
GPS_TXD -> STM32 PA10 / USART1_RX
GPS_RXD -> STM32 PA9  / USART1_TX，可接可不接
GPS_GND -> STM32 GND
GPS_VCC -> 按模块要求接 3.3V 或 5V
*/

#define GPS_RX_BUFFER_LEN       128

#define GPS_FIELD_TIME_LEN      16
#define GPS_FIELD_LAT_LEN       16
#define GPS_FIELD_NS_LEN        4
#define GPS_FIELD_LON_LEN       16
#define GPS_FIELD_EW_LEN        4
#define GPS_FIELD_SPEED_LEN     12
#define GPS_FIELD_DATE_LEN      12

typedef struct
{
	volatile uint8_t frame_ready;              // 收到完整 RMC 帧
	uint8_t parsed;                            // 已解析过数据
	uint8_t valid;                             // 1=定位有效，0=未定位

	char raw[GPS_RX_BUFFER_LEN];               // 原始 RMC 数据

	char utc[GPS_FIELD_TIME_LEN];              // UTC 时间，例如 123519.00
	char latitude[GPS_FIELD_LAT_LEN];          // 纬度，原始格式 ddmm.mmmm
	char ns[GPS_FIELD_NS_LEN];                 // N/S
	char longitude[GPS_FIELD_LON_LEN];         // 经度，原始格式 dddmm.mmmm
	char ew[GPS_FIELD_EW_LEN];                 // E/W
	char speed[GPS_FIELD_SPEED_LEN];           // 速度，单位 knots
	char date[GPS_FIELD_DATE_LEN];             // 日期，ddmmyy
} GPS_Data_t;

extern GPS_Data_t GPS_Data;

void GPS_Init(uint32_t baudrate);
void GPS_Clear(void);
uint8_t GPS_Parse(void);
void GPS_ShowOnOLED(void);

#endif
