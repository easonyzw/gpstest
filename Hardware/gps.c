#include "gps.h"
#include "OLED.h"
#include <string.h>

GPS_Data_t GPS_Data;

static char GPS_RxBuf[GPS_RX_BUFFER_LEN];
static volatile uint16_t GPS_RxIndex = 0;

/* 判断是否为 $GPRMC 或 $GNRMC */
static uint8_t GPS_IsRMC(char *buf)
{
	if (buf[0] == '$' && buf[3] == 'R' && buf[4] == 'M' && buf[5] == 'C')
	{
		return 1;
	}
	return 0;
}

/* 从 NMEA 语句中提取第 field_index 个字段
   RMC 字段：
   0: $GPRMC/$GNRMC
   1: UTC时间
   2: 状态 A/V
   3: 纬度
   4: N/S
   5: 经度
   6: E/W
   7: 速度
   8: 航向
   9: 日期
*/
static void GPS_CopyField(const char *sentence, uint8_t field_index, char *out, uint8_t out_len)
{
	uint8_t field = 0;
	const char *start = sentence;
	const char *p = sentence;
	uint16_t len;
	uint16_t i;

	if (out_len == 0)
	{
		return;
	}

	out[0] = '\0';

	while (1)
	{
		if (*p == ',' || *p == '*' || *p == '\r' || *p == '\n' || *p == '\0')
		{
			if (field == field_index)
			{
				len = p - start;

				if (len >= out_len)
				{
					len = out_len - 1;
				}

				for (i = 0; i < len; i++)
				{
					out[i] = start[i];
				}

				out[len] = '\0';
				return;
			}

			if (*p == '*' || *p == '\r' || *p == '\n' || *p == '\0')
			{
				return;
			}

			field++;
			start = p + 1;
		}

		p++;
	}
}

static void GPS_ClearParsedData(void)
{
	GPS_Data.valid = 0;

	memset(GPS_Data.utc, 0, GPS_FIELD_TIME_LEN);
	memset(GPS_Data.latitude, 0, GPS_FIELD_LAT_LEN);
	memset(GPS_Data.ns, 0, GPS_FIELD_NS_LEN);
	memset(GPS_Data.longitude, 0, GPS_FIELD_LON_LEN);
	memset(GPS_Data.ew, 0, GPS_FIELD_EW_LEN);
	memset(GPS_Data.speed, 0, GPS_FIELD_SPEED_LEN);
	memset(GPS_Data.date, 0, GPS_FIELD_DATE_LEN);
}

void GPS_Clear(void)
{
	GPS_RxIndex = 0;

	memset(GPS_RxBuf, 0, GPS_RX_BUFFER_LEN);
	memset(&GPS_Data, 0, sizeof(GPS_Data));
}

void GPS_Init(uint32_t baudrate)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	GPS_Clear();

	/* USART1 和 GPIOA 时钟 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

	/* PA9 -> USART1_TX */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* PA10 -> USART1_RX */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* USART1 配置 */
	USART_InitStructure.USART_BaudRate = baudrate;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART1, &USART_InitStructure);

	/* NVIC 配置 */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	/* 开启接收中断 */
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

	/* 使能 USART1 */
	USART_Cmd(USART1, ENABLE);
}

/* USART1 中断函数：接收 GPS NMEA 数据 */
void USART1_IRQHandler(void)
{
	uint8_t ch;
	uint16_t i;
	uint16_t copy_len;

	if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
	{
		ch = (uint8_t)USART_ReceiveData(USART1);

		/* '$' 是 NMEA 一帧数据的开始 */
		if (ch == '$')
		{
			GPS_RxIndex = 0;
		}

		if (GPS_RxIndex < GPS_RX_BUFFER_LEN - 1)
		{
			GPS_RxBuf[GPS_RxIndex++] = ch;
		}
		else
		{
			GPS_RxIndex = 0;
		}

		/* '\n' 是一帧数据结束 */
		if (ch == '\n')
		{
			GPS_RxBuf[GPS_RxIndex] = '\0';

			/* 只保存 RMC 帧，RMC 里有时间、经纬度、定位状态 */
			if (GPS_IsRMC(GPS_RxBuf))
			{
				copy_len = GPS_RxIndex;

				if (copy_len >= GPS_RX_BUFFER_LEN)
				{
					copy_len = GPS_RX_BUFFER_LEN - 1;
				}

				for (i = 0; i < copy_len; i++)
				{
					GPS_Data.raw[i] = GPS_RxBuf[i];
				}

				GPS_Data.raw[copy_len] = '\0';
				GPS_Data.frame_ready = 1;
			}

			GPS_RxIndex = 0;
			memset(GPS_RxBuf, 0, GPS_RX_BUFFER_LEN);
		}

		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
	}
}

/* 解析 GPS 数据，有新数据返回 1，没有新数据返回 0 */
uint8_t GPS_Parse(void)
{
	char sentence[GPS_RX_BUFFER_LEN];
	char status[4];

	if (GPS_Data.frame_ready == 0)
	{
		return 0;
	}

	/* 复制一份出来解析，避免中断同时改数据 */
	USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);
	memcpy(sentence, GPS_Data.raw, GPS_RX_BUFFER_LEN);
	GPS_Data.frame_ready = 0;
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

	GPS_ClearParsedData();

	GPS_CopyField(sentence, 1, GPS_Data.utc, GPS_FIELD_TIME_LEN);
	GPS_CopyField(sentence, 2, status, sizeof(status));
	GPS_CopyField(sentence, 3, GPS_Data.latitude, GPS_FIELD_LAT_LEN);
	GPS_CopyField(sentence, 4, GPS_Data.ns, GPS_FIELD_NS_LEN);
	GPS_CopyField(sentence, 5, GPS_Data.longitude, GPS_FIELD_LON_LEN);
	GPS_CopyField(sentence, 6, GPS_Data.ew, GPS_FIELD_EW_LEN);
	GPS_CopyField(sentence, 7, GPS_Data.speed, GPS_FIELD_SPEED_LEN);
	GPS_CopyField(sentence, 9, GPS_Data.date, GPS_FIELD_DATE_LEN);

	if (status[0] == 'A')
	{
		GPS_Data.valid = 1;
	}
	else
	{
		GPS_Data.valid = 0;
	}

	GPS_Data.parsed = 1;

	return 1;
}

/* OLED 固定显示一整行，自动补空格，防止旧字符残留 */
static void GPS_OLED_ShowLine(uint8_t line, const char *text)
{
	char buf[17];
	uint8_t i = 0;

	while (i < 16 && text[i] != '\0')
	{
		buf[i] = text[i];
		i++;
	}

	while (i < 16)
	{
		buf[i] = ' ';
		i++;
	}

	buf[16] = '\0';

	OLED_ShowString(line, 1, buf);
}

/* 拼接一行：prefix + value + suffix，最长 16 字符 */
static void GPS_MakeLine(char *line, const char *prefix, const char *value, const char *suffix)
{
	uint8_t pos = 0;

	while (*prefix != '\0' && pos < 16)
	{
		line[pos++] = *prefix++;
	}

	while (*value != '\0' && pos < 16)
	{
		line[pos++] = *value++;
	}

	while (*suffix != '\0' && pos < 16)
	{
		line[pos++] = *suffix++;
	}

	line[pos] = '\0';
}

/* 在 OLED 上显示 GPS 数据，main.c 里循环调用这个函数即可 */
void GPS_ShowOnOLED(void)
{
	char line[17];
	uint8_t updated;
	static uint8_t wait_showed = 0;

	updated = GPS_Parse();

	/* 还没收到 GPS 数据 */
	if (updated == 0 && GPS_Data.parsed == 0)
	{
		if (wait_showed == 0)
		{
			GPS_OLED_ShowLine(1, "GPS Waiting...");
			GPS_OLED_ShowLine(2, "PA10<-GPS TX");
			GPS_OLED_ShowLine(3, "Baud:9600");
			GPS_OLED_ShowLine(4, "Wait signal");
			wait_showed = 1;
		}

		return;
	}

	/* 没有新数据就不刷新 */
	if (updated == 0)
	{
		return;
	}

	/* 收到数据，但还没有定位成功 */
	if (GPS_Data.valid == 0)
	{
		GPS_OLED_ShowLine(1, "GPS No Fix");
		GPS_OLED_ShowLine(2, "Searching...");
		GPS_OLED_ShowLine(3, "Check antenna");
		GPS_OLED_ShowLine(4, "Open sky better");
		return;
	}

	/* 定位成功，显示经纬度 */
	GPS_OLED_ShowLine(1, "GPS Located");

	GPS_MakeLine(line, "Lat:", GPS_Data.latitude, GPS_Data.ns);
	GPS_OLED_ShowLine(2, line);

	GPS_MakeLine(line, "Lon:", GPS_Data.longitude, GPS_Data.ew);
	GPS_OLED_ShowLine(3, line);

	GPS_MakeLine(line, "UTC:", GPS_Data.utc, "");
	GPS_OLED_ShowLine(4, line);
}
