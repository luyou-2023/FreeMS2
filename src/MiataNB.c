/* FreeMS2 - the open source engine management system
 *
 * Copyright 2009, 2010 Fred Cooke
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


/**	@file MiataNB.c
 * @ingroup interruptHandlers
 * @ingroup enginePositionRPMDecoders
 *
 * @brief Miata from 9x to 0x
 *
 * @note Pseudo code that does not compile with zero warnings and errors MUST be commented out.
 *
 * @todo TODO This file contains SFA but Abe Mara is going to fill it up with
 * @todo TODO wonderful goodness very soon ;-)
 *
 * @author Who Ever
 */


#include "inc/FreeMS2.h"
#include "inc/interrupts.h"
#include "inc/decoderInterface.h"


/** @brief 主 RPM 中断服务程序（马自达 Miata NB 解码器）
 *
 * 处理马自达 Miata NB（1999-2005）的曲轴位置信号，用于位置检测和 RPM 计算。
 * Miata NB 使用特定的曲轴轮模式，需要特殊的解码逻辑。
 * 
 * @details 为什么需要这个方法：
 * 马自达 Miata NB（第二代 Miata）使用特定的曲轴位置传感器配置：
 * - 需要特定的解码算法来正确识别曲轴位置
 * - 支持顺序喷油和精确点火控制
 * - 适用于 1.8L BP 发动机
 * 
 * 当前状态：
 * 此函数目前是基本框架，包含：
 * - 中断标志清除
 * - 时间戳记录
 * - 延迟计算
 * - 边沿检测（根据配置的极性）
 * - 基本计数
 * 
 * 待实现功能：
 * - 位置解码逻辑（特定于 Miata NB）
 * - RPM 计算
 * - ADC 采样
 * - 事件调度
 * - 同步丢失检测
 * 
 * @return 无返回值
 *
 * @author Who Ever
 * 
 * @todo TODO 完善文档说明
 * @todo TODO 实现 Miata NB 特定的解码逻辑
 * @todo TODO 丢弃窄脉冲！测试齿宽和齿周期
 * @todo TODO 包装齿周期和宽度检查
 */
void PrimaryRPMISR(void)
{
	/* Clear the interrupt flag for this input compare channel */
	TFLG = 0x01;

	/* Save all relevant available data here */
	unsigned short codeStartTimeStamp = TCNT;		/* Save the current timer count */
	unsigned short edgeTimeStamp = TC0;				/* Save the edge time stamp */
	unsigned char PTITCurrentState = PTIT;			/* Save the values on port T regardless of the state of DDRT */
//	unsigned short PORTS_BACurrentState = PORTS_BA;	/* Save ignition output state */

	/* Calculate the latency in ticks */
	ISRLatencyVars.primaryInputLatency = codeStartTimeStamp - edgeTimeStamp;

	// TODO discard narrow ones! test for tooth width and tooth period

	/* Set up edges as per config */
	unsigned char risingEdge;
	if(fixedConfigs1.coreSettingsA & PRIMARY_POLARITY){
		risingEdge = PTITCurrentState & 0x01;
	}else{
		risingEdge = !(PTITCurrentState & 0x01);
	}

	if(risingEdge){
		// increment crank pulses TODO this needs to be wrapped in tooth period and width checking
		primaryPulsesPerSecondaryPulse++;

		LongTime timeStamp;

		/* Install the low word */
		timeStamp.timeShorts[1] = edgeTimeStamp;
		/* Find out what our timer value means and put it in the high word */
		if(TFLGOF && !(edgeTimeStamp & 0x8000)){ /* see 10.3.5 paragraph 4 of 68hc11 ref manual for details */
			timeStamp.timeShorts[0] = timerExtensionClock + 1;
		}else{
			timeStamp.timeShorts[0] = timerExtensionClock;
		}
		RuntimeVars.primaryInputLeadingRuntime = TCNT - codeStartTimeStamp;
	}else{
		RuntimeVars.primaryInputTrailingRuntime = TCNT - codeStartTimeStamp;
	}

	Counters.primaryTeethSeen++;
}


/** @brief 次 RPM 中断服务程序（马自达 Miata NB 解码器）
 *
 * 处理次 RPM 输入（通常是凸轮轴信号），用于区分发动机循环和确定相位。
 * 在马自达 Miata NB 解码器中，次 RPM 输入用于确定发动机处于哪个 360 度循环中。
 * 
 * @details 为什么需要这个方法：
 * 四冲程发动机每 720 度完成一个完整循环，但曲轴每 360 度转一圈。
 * 次 RPM 输入（凸轮轴信号）用于：
 * - 区分两个 360 度循环（区分压缩冲程和排气冲程）
 * - 确定发动机相位（哪个气缸处于压缩冲程）
 * - 支持顺序喷油（每个气缸在正确的时机喷油）
 * 
 * 当前状态：
 * 此函数目前是基本框架，包含：
 * - 中断标志清除
 * - 时间戳记录
 * - 延迟计算
 * - 边沿检测（根据配置的极性）
 * - 基本计数
 * 
 * 待实现功能：
 * - 相位检测和同步（特定于 Miata NB）
 * - 发动机周期计算
 * - 同步验证
 * 
 * @return 无返回值
 *
 * @author Who Ever
 * 
 * @todo TODO 完善文档说明
 * @todo TODO 实现 Miata NB 特定的相位检测逻辑
 * @todo TODO 丢弃窄脉冲！测试齿宽和齿周期
 */
void SecondaryRPMISR(void)
{
	/* Clear the interrupt flag for this input compare channel */
	TFLG = 0x02;

	/* Save all relevant available data here */
	unsigned short codeStartTimeStamp = TCNT;		/* Save the current timer count */
	unsigned short edgeTimeStamp = TC1;				/* Save the timestamp */
	unsigned char PTITCurrentState = PTIT;			/* Save the values on port T regardless of the state of DDRT */
//	unsigned short PORTS_BACurrentState = PORTS_BA;	/* Save ignition output state */

	/* Calculate the latency in ticks */
	ISRLatencyVars.secondaryInputLatency = codeStartTimeStamp - edgeTimeStamp;

	// TODO discard narrow ones! test for tooth width and tooth period

	/* Set up edges as per config */
	unsigned char risingEdge;
	if(fixedConfigs1.coreSettingsA & SECONDARY_POLARITY){
		risingEdge = PTITCurrentState & 0x02;
	}else{
		risingEdge = !(PTITCurrentState & 0x02);
	}

	if(risingEdge){
		LongTime timeStamp;

		/* Install the low word */
		timeStamp.timeShorts[1] = edgeTimeStamp;
		/* Find out what our timer value means and put it in the high word */
		if(TFLGOF && !(edgeTimeStamp & 0x8000)){ /* see 10.3.5 paragraph 4 of 68hc11 ref manual for details */
			timeStamp.timeShorts[0] = timerExtensionClock + 1;
		}else{
			timeStamp.timeShorts[0] = timerExtensionClock;
		}

		RuntimeVars.secondaryInputLeadingRuntime = TCNT - codeStartTimeStamp;
	}else{
		RuntimeVars.secondaryInputTrailingRuntime = TCNT - codeStartTimeStamp;
	}

	Counters.secondaryTeethSeen++;
}
