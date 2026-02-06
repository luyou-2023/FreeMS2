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


/**	@file realtimeISRs.c
 * @ingroup interruptHandlers
 *
 * @brief Real time interrupts
 *
 * This file contains real time interrupt handlers. Mainly it holds the RTI
 * handler itself, however the modulus down counter and ETC timer overflow
 * functions are here too.
 *
 * @author Fred Cooke
 */


#define REALTIMEISRS_C
#include "inc/FreeMS2.h"
#include "inc/interrupts.h"
#include "inc/commsISRs.h"


/** @brief Real Time Interrupt Handler
 *
 * Handles time keeping, including all internal clocks, and generic periodic
 * tasks that run quickly and must be done on time.
 *
 * @author Fred Cooke
 */
void RTIISR(){
	/* 清除 RTI 标志 */
	CRGFLG = 0x80;  // BIT7: RTI 标志位

	/* 记录时间戳用于代码运行时间报告 */
	unsigned short startTimeRTI = TCNT;  // 记录 ISR 开始时间

	/* 递增计数器 */
	Clocks.realTimeClockMain++;  // RTI 主计数器（每 128μs 递增一次）

	/* 此函数可以通过在最大倍数处回滚主变量来执行，而不需要额外的变量，
	 * 但我不确定那样是否更好 */

	// TODO 添加八分之一毫秒 RTC 的内容？

	/* 每 8 次 RTI 执行是一次毫秒 */
	if(Clocks.realTimeClockMain % 8 == 0){
		/* 递增毫秒计数器 */
		Clocks.realTimeClockMillis++;  // 毫秒计数器递增

		/* 递增毫秒回滚变量 */
		Clocks.millisToTenths++;  // 用于跟踪到十分之一秒的毫秒数

		/* 在这里或最好在主循环中执行所有每毫秒一次的任务 */
		Clocks.timeoutADCreadingClock++;  // ADC 读取超时时钟递增
		// 检查是否超过超时时间
		if(Clocks.timeoutADCreadingClock > fixedConfigs2.sensorSettings.readingTimeout){
			/* 设置强制读取 ADC 标志 */
			coreStatusA |= FORCE_READING;  // 设置强制读取标志，通知主循环强制采样
			Clocks.timeoutADCreadingClock = 0;  // 重置超时时钟
		}

		/* 每 100 毫秒是十分之一秒 */
		if(Clocks.millisToTenths % 100 == 0){
			/* 递增十分之一秒计数器 */
			Clocks.realTimeClockTenths++;  // 十分之一秒计数器递增

			/* 递增十分之一秒回滚变量 */
			Clocks.tenthsToSeconds++;  // 用于跟踪到秒的十分之一秒数

			/* 重置毫秒回滚变量 */
			Clocks.millisToTenths = 0;  // 重置毫秒计数器

			/* 在这里或最好在主循环中执行所有每十分之一秒一次的任务 */
			// 递减端口 H 防抖变量直到它再次为零。
			if(portHDebounce != 0){
				portHDebounce -= 1;  // 递减防抖计数器
			}

			/* 每 10 个十分之一秒是一秒 */
			if(Clocks.tenthsToSeconds % 10 == 0){
				/* 递增秒计数器 */
				Clocks.realTimeClockSeconds++;  // 秒计数器递增

				// 燃油泵闪烁器
				PORTE ^= BIT4;  // 切换 PORTE 的第 4 位（燃油泵指示）

				// 调试：切换 PORTM 的所有位
				if(PORTM){
					PORTM = 0x00;  // 如果 PORTM 非零，清零
				}else{
					PORTM = 0xFF;  // 如果 PORTM 为零，设置为全 1
				}
				/* 递增秒回滚变量 */
				Clocks.secondsToMinutes++;  // 用于跟踪到分钟的秒数

				/* 重置十分之一秒回滚变量 */
				Clocks.tenthsToSeconds = 0;  // 重置十分之一秒计数器
				/* 在这里或最好在主循环中执行所有每秒一次的任务 */

				// 由于调参器性能问题（在卧室中）临时限制日志发送
				ShouldSendLog = TRUE;  // 设置应该发送日志标志
				/* 闪烁用户 LED 作为"心跳"，让新用户知道它还活着 */
				//PORTP ^= 0x80;  // 已注释：LED 心跳指示

				/* 每 60 秒是一分钟，65535 分钟对我们来说足够了 :-) */
				if(Clocks.secondsToMinutes % 60 == 0){
					/* 递增分钟计数器 */
					Clocks.realTimeClockMinutes++;  // 分钟计数器递增

					/* 可能在这里和下面添加小时字段，但那将是过度的 */
					// TODO 添加小时 RTC？

					/* 重置秒回滚变量 */
					Clocks.secondsToMinutes = 0;  // 重置秒计数器

					/* 在这里或最好在主循环中执行所有每分钟一次的任务 */
					// TODO 在分钟 RTC 中添加内容？

					/* 如果我们做小时（我们可能不会），这里的小时 if 语句 */
				}
			}
		}
	}
	// 记录 RTI ISR 的执行时间（用于性能测量）
	RuntimeVars.RTCRuntime = TCNT - startTimeRTI;
}


/** @brief ECT overflow handler
 *
 * When the ECT free running timer hits 65535 and rolls over, this is run. Its
 * job is to extend the timer to an effective 32 bits for longer measuring much
 * longer periods with the same resolution.
 *
 * @author Fred Cooke
 */
void TimerOverflow(){
	/* 递增定时器扩展变量
	 * 当 16 位定时器溢出时，扩展 32 位时间戳的高位 */
	timerExtensionClock++;  // 定时器扩展时钟递增（32 位时间戳的高 16 位）

	/* 清除定时器溢出中断标志 */
	TFLGOF = 0x80;  // BIT7: 定时器溢出标志位
}


/** @todo TODO This could be useful in future once sleeping is implemented.
// Generic periodic interrupt (This only works from wait mode...)
void VRegAPIISR(){
	// Clear the flag needs check because writing a 1 can set this one
	//if(VREGAPICL & 0x01){ // if the flag is set...
		VREGAPICL |= 0x01; // clear it...
	//} // and not otherwise!

	// DO stuff
}
*/
