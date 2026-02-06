/* FreeMS2 - the open source engine management system
 *
 * Copyright 2008, 2009 Fred Cooke
 *
 * This file is part of the FreeMS2 project.
 *
 * FreeMS2 software is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FreeMS2 software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with any FreeMS2 software.  If not, see http://www.gnu.org/licenses/
 *
 * We ask that if you make any changes to this file you email them upstream to
 * us at admin(at)diyefi(dot)org or, even better, fork the code on github.com!
 *
 * Thank you for choosing FreeMS2 to run your engine!
 */


/** @file miscISRs.c
 * @ingroup interruptHandlers
 *
 * @brief Miscellaneous Interrupt Handlers
 *
 * Various non-descript interrupt handlers that don't really fit anywhere else
 * and aren't big enough to live on their own just yet.
 *
 * @author Fred Cooke
 */


#include "inc/FreeMS2.h"
#include "inc/interrupts.h"


/** @brief Unimplemented Interrupt Handler
 *
 * Unimplemented interrupt service routine for calls  we weren't expecting.
 * Currently this simply counts bad calls like any other event type.
 *
 * @author Fred Cooke
 */
void UISR(void){
	/* 递增未实现的 ISR 执行计数器
	 * 用于处理未预期的中断调用 */
	Counters.callsToUISRs++;  // 增加未实现 ISR 调用计数
}


/** @brief Port P pins ISR
 *
 * Interrupt handler for edge events on port P pins. Not currently used.
 *
 * @author Fred Cooke
 */
void PortPISR(void){
	/* 清除所有端口 P 标志（我们一次只需要一个） */
	PIFP = ONES;  // 清除所有端口 P 中断标志位
	/* 递增未实现的 ISR 执行计数器
	 * 当前未使用，仅用于计数 */
	Counters.callsToUISRs++;  // 增加未实现 ISR 调用计数
}			/* Port P 中断服务例程 */


/** @brief Port J pins ISR
 *
 * Interrupt handler for edge events on port J pins. Not currently used.
 *
 * @author Fred Cooke
 */
void PortJISR(void){
	/* 清除所有端口 J 标志（注释说端口 H，但实际是端口 J）
	 * 我们一次只需要一个 */
	PIFJ = ONES;  // 清除所有端口 J 中断标志位
	/* 递增未实现的 ISR 执行计数器
	 * 当前未使用，仅用于计数 */
	Counters.callsToUISRs++;  // 增加未实现 ISR 调用计数
}


/** @brief IRQ/PE1 pin ISR
 *
 * Interrupt handler for edge events on the IRQ/PE1 pin. Not currently used.
 *
 * @author Fred Cooke
 */
void IRQISR(void){
	/* 清除标志
	 * 待实现：需要确定如何清除 IRQ 标志 */
	// ?? TODO  // 待实现：清除 IRQ 标志

	/* 递增未实现的 ISR 执行计数器
	 * 当前未使用，仅用于计数 */
	Counters.callsToUISRs++;  // 增加未实现 ISR 调用计数
}


/** @brief XIRQ/PE0 pin ISR
 *
 * Interrupt handler for edge events on the XIRQ/PE0 pin. Not currently used.
 *
 * @author Fred Cooke
 */
void XIRQISR(void){
	/* 清除标志
	 * 待实现：需要确定如何清除 XIRQ 标志 */
	// ?? TODO  // 待实现：清除 XIRQ 标志

	/* 递增未实现的 ISR 执行计数器
	 * 当前未使用，仅用于计数 */
	Counters.callsToUISRs++;  // 增加未实现 ISR 调用计数
}


/** @brief Low Voltage Counter
 *
 * Count how often our voltage drops lower than it should without resetting.
 *
 * @author Fred Cooke
 */
void LowVoltageISR(void){
	/* 清除标志
	 * 写入 1 到 BIT0 清除低电压中断标志 */
	VREGCTRL |= 0x01;  // BIT0: 低电压中断标志位（写入 1 清除）

	/* 递增计数器
	 * 记录电压低于正常值但未复位的次数 */
	Counters.lowVoltageConditions++;  // 增加低电压条件计数
}
