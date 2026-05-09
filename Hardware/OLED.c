#include "stm32f10x.h"
#include "OLED_Font.h"

/* 引脚配置
 *
 * OLED SCL / SCK / 时钟线 -> PB6
 * OLED SDA / 数据线       -> PB7
 */
#define OLED_W_SCL(x)		GPIO_WriteBit(GPIOB, GPIO_Pin_6, (BitAction)(x))
#define OLED_W_SDA(x)		GPIO_WriteBit(GPIOB, GPIO_Pin_7, (BitAction)(x))

/* 引脚初始化 */
void OLED_I2C_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* 开启 GPIOB 时钟 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	
	/* 软件 I2C 使用开漏输出 */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	/* PB6 -> OLED SCL */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* PB7 -> OLED SDA */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	/* I2C 空闲状态下，SCL 和 SDA 都为高电平 */
	OLED_W_SCL(1);
	OLED_W_SDA(1);
}
