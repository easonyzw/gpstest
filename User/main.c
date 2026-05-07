#include "stm32f10x.h"
#include "OLED.h"
#include "gps.h"

int main(void)
{
	OLED_Init();
	GPS_Init(9600);

	OLED_ShowString(1, 1, "GPS Init...");

	while (1)
	{
		GPS_ShowOnOLED();
	}
}
