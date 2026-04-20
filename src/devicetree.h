/*
 * devicetree.h — Hand-written device tree for STM32F411RE Nucleo
 *
 * In Zephyr, this file is auto-generated from .dts files by
 * scripts/dts/gen_defines.py. The .dts describes the hardware:
 *
 *   / {
 *       soc {
 *           usart2: serial@40004400 {
 *               compatible = "st,stm32-usart";
 *               reg = <0x40004400 0x400>;
 *               clocks = <&rcc APB1 17>;
 *               pinctrl = <&gpioa 2 7>;  // PA2, AF7
 *               baudrate = <115200>;
 *               status = "okay";
 *           };
 *       };
 *   };
 *
 * gen_defines.py would parse that and emit the #defines below.
 * We skip the tooling and write them directly.
 */

#ifndef DEVICETREE_H
#define DEVICETREE_H

/* ---- Clock tree (RCC) ---- */
#define DT_RCC_BASE             0x40023800
#define DT_RCC_AHB1ENR_OFFSET  0x30
#define DT_RCC_APB1ENR_OFFSET  0x40

/* ---- GPIO ---- */
#define DT_GPIOA_BASE           0x40020000

/* ---- USART2 node ---- */
#define DT_USART2_BASE          0x40004400
#define DT_USART2_BAUDRATE      115200
#define DT_USART2_CLK_BUS       APB1    /* which bus it's on */
#define DT_USART2_CLK_BIT       17      /* bit in APB1ENR */
#define DT_USART2_TX_PORT_BASE  DT_GPIOA_BASE
#define DT_USART2_TX_PIN        2       /* PA2 */
#define DT_USART2_TX_AF         7       /* AF7 = USART2 */
#define DT_USART2_GPIO_CLK_BIT  0       /* GPIOA = bit 0 in AHB1ENR */

/* System clock (HSI default after reset) */
#define DT_SYSCLK_HZ           16000000

/* ---- Console alias ---- */
/* In Zephyr: chosen { zephyr,console = &usart2; }; */
#define DT_CONSOLE_BASE         DT_USART2_BASE
#define DT_CONSOLE_BAUDRATE     DT_USART2_BAUDRATE

#endif
