/* linux/arch/arm/mach-s5p6450/include/mach/gpio.h
 *
 * Copyright (c) 2009 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * S5P6450 - GPIO lib support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_GPIO_H
#define __ASM_ARCH_GPIO_H __FILE__

#define gpio_get_value	__gpio_get_value
#define gpio_set_value	__gpio_set_value
#define gpio_cansleep	__gpio_cansleep
#define gpio_to_irq	__gpio_to_irq

/* GPIO bank sizes */
#define S5P6450_GPIO_A_NR	(6)
#define S5P6450_GPIO_B_NR	(7)
#define S5P6450_GPIO_C_NR	(8)
#define S5P6450_GPIO_D_NR	(8)
#define S5P6450_GPIO_F_NR	(16)
#define S5P6450_GPIO_G_NR	(14)
#define S5P6450_GPIO_H_NR	(10)
#define S5P6450_GPIO_I_NR	(16)
#define S5P6450_GPIO_J_NR	(12)
#define S5P6450_GPIO_K_NR	(5)
#define S5P6450_GPIO_N_NR	(16)
#define S5P6450_GPIO_P_NR	(11)
#define S5P6450_GPIO_Q_NR	(14)
#define S5P6450_GPIO_R_NR	(15)
#define S5P6450_GPIO_S_NR	(8)


/* GPIO bank numbers */

/* CONFIG_S3C_GPIO_SPACE allows the user to select extra
 * space for debugging purposes so that any accidental
 * change from one gpio bank to another can be caught.
*/
#define S5P6450_GPIO_NEXT(__gpio) \
	((__gpio##_START) + (__gpio##_NR) + CONFIG_S3C_GPIO_SPACE + 1)

enum s5p_gpio_number {
	S5P6450_GPIO_A_START = 0,
	S5P6450_GPIO_B_START = S5P6450_GPIO_NEXT(S5P6450_GPIO_A),
	S5P6450_GPIO_C_START = S5P6450_GPIO_NEXT(S5P6450_GPIO_B),
	S5P6450_GPIO_D_START = S5P6450_GPIO_NEXT(S5P6450_GPIO_C),
	S5P6450_GPIO_F_START = S5P6450_GPIO_NEXT(S5P6450_GPIO_D),
	S5P6450_GPIO_G_START = S5P6450_GPIO_NEXT(S5P6450_GPIO_F),
	S5P6450_GPIO_H_START = S5P6450_GPIO_NEXT(S5P6450_GPIO_G),
	S5P6450_GPIO_I_START = S5P6450_GPIO_NEXT(S5P6450_GPIO_H),
	S5P6450_GPIO_J_START = S5P6450_GPIO_NEXT(S5P6450_GPIO_I),
	S5P6450_GPIO_K_START = S5P6450_GPIO_NEXT(S5P6450_GPIO_J),
	S5P6450_GPIO_N_START = S5P6450_GPIO_NEXT(S5P6450_GPIO_K),
	S5P6450_GPIO_P_START = S5P6450_GPIO_NEXT(S5P6450_GPIO_N),
	S5P6450_GPIO_Q_START = S5P6450_GPIO_NEXT(S5P6450_GPIO_P),
	S5P6450_GPIO_R_START = S5P6450_GPIO_NEXT(S5P6450_GPIO_Q),
	S5P6450_GPIO_S_START = S5P6450_GPIO_NEXT(S5P6450_GPIO_R),
};

/* S5P6450 GPIO number definitions. */
#define S5P6450_GPA(_nr)	(S5P6450_GPIO_A_START + (_nr))
#define S5P6450_GPB(_nr)	(S5P6450_GPIO_B_START + (_nr))
#define S5P6450_GPC(_nr)	(S5P6450_GPIO_C_START + (_nr))
#define S5P6450_GPD(_nr)	(S5P6450_GPIO_D_START + (_nr))
#define S5P6450_GPF(_nr)	(S5P6450_GPIO_F_START + (_nr))
#define S5P6450_GPG(_nr)	(S5P6450_GPIO_G_START + (_nr))
#define S5P6450_GPH(_nr)	(S5P6450_GPIO_H_START + (_nr))
#define S5P6450_GPI(_nr)	(S5P6450_GPIO_I_START + (_nr))
#define S5P6450_GPJ(_nr)	(S5P6450_GPIO_J_START + (_nr))
#define S5P6450_GPK(_nr)	(S5P6450_GPIO_K_START + (_nr))
#define S5P6450_GPN(_nr)	(S5P6450_GPIO_N_START + (_nr))
#define S5P6450_GPP(_nr)	(S5P6450_GPIO_P_START + (_nr))
#define S5P6450_GPQ(_nr)	(S5P6450_GPIO_Q_START + (_nr))
#define S5P6450_GPR(_nr)	(S5P6450_GPIO_R_START + (_nr))
#define S5P6450_GPS(_nr)    (S5P6450_GPIO_S_START + (_nr))

#ifdef CONFIG_MACH_SMDK6450 
#define S5P6450_BACKLIGHT_PIN   S5P6450_GPF(15)
#define S5P6450_LCD_RESET_PIN   S5P6450_GPN(5)
#define S5P6450_BACKLIGHT_PUD   1
#define S5P6450_LCD_RESET_PUD	1
#endif

/* Compatibility Define   */ 

#define S5P64XX_GPA(_nr)        (S5P6450_GPIO_A_START + (_nr))
#define S5P64XX_GPB(_nr)        (S5P6450_GPIO_B_START + (_nr))
#define S5P64XX_GPC(_nr)        (S5P6450_GPIO_C_START + (_nr))
#define S5P64XX_GPD(_nr)        (S5P6450_GPIO_D_START + (_nr))

#define S5P64XX_GPF(_nr)        (S5P6450_GPIO_F_START + (_nr))
#define S5P64XX_GPG(_nr)        (S5P6450_GPIO_G_START + (_nr))
#define S5P64XX_GPH(_nr)        (S5P6450_GPIO_H_START + (_nr))
#define S5P64XX_GPI(_nr)        (S5P6450_GPIO_I_START + (_nr))
#define S5P64XX_GPJ(_nr)        (S5P6450_GPIO_J_START + (_nr))
#define S5P64XX_GPK(_nr)        (S5P6450_GPIO_K_START + (_nr))
#define S5P64XX_GPN(_nr)        (S5P6450_GPIO_N_START + (_nr))
#define S5P64XX_GPP(_nr)        (S5P6450_GPIO_P_START + (_nr))
#define S5P64XX_GPQ(_nr)        (S5P6450_GPIO_Q_START + (_nr))
#define S5P64XX_GPR(_nr)        (S5P6450_GPIO_R_START + (_nr))
#define S5P64XX_GPS(_nr)        (S5P6450_GPIO_S_START + (_nr))


/* the end of the S5P6450 specific gpios */
#define S5P6450_GPIO_END	(S5P6450_GPS(S5P6450_GPIO_S_NR) + 1)
#define S3C_GPIO_END		S5P6450_GPIO_END

/* define the number of gpios we need to the one after the GPR() range */
#define ARCH_NR_GPIOS		(S5P6450_GPS(S5P6450_GPIO_S_NR) +	\
				 CONFIG_SAMSUNG_GPIO_EXTRA + 1)

#define VALDEZ_GPIO_TSP_RST 	S5P6450_GPB(2) //mkh : need to move this definition to board specific file somewhere.
#define VALDEZ_GPIO_TSP_IRQ 	S5P6450_GPN(7) //mkh : need to move this definition to board specific file somewhere.


#include <asm-generic/gpio.h>

#endif /* __ASM_ARCH_GPIO_H */
