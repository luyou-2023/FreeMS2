/* FreeMS2 - the open source engine management system
 *
 * Copyright 2008, 2009, 2010 Fred Cooke
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


/**	@file Simple.c
 * @ingroup interruptHandlers
 * @ingroup enginePositionRPMDecoders
 *
 * @brief Reads any signal that is once per cylinder
 *
 * This file contains the two interrupt service routines required for to build
 * cleanly. However, only the first one is used due to the simple nature of it.
 *
 * The functional ISR just blindly injects fuel for every input it receives.
 * Thus a perfectly clean input is absolutely essential at this time.
 *
 * Supported engines include:
 * B230F
 *
 * @author Fred Cooke
 *
 * @note Even though I ran my US road trip car on this exact code, I don't recommend it unless you REALLY know what you are doing!
 */


#include "inc/FreeMS2.h"
#include "inc/interrupts.h"
#include "inc/decoderInterface.h"
#include "inc/utils.h"


/** Primary RPM ISR
 *
 * Schedule events :
 * Blindly start fuel pulses for each and every input pulse.
 *
 * Sample ADCs :
 * Grab a unified set of ADC readings at one time in a consistent crank location to eliminate engine cycle dependent noise.
 * Set flag stating that New pulse, advance, etc should be calculated.
 *
 * @author Fred Cooke
 *
 * @warning These are for testing and demonstration only, not suitable for driving with just yet.
 *
 * @todo TODO make this code more general and robust such that it can be used for real simple applications
 */
void PrimaryRPMISR(){
	/* 清除此输入捕获通道的中断标志 */
	TFLG = 0x01;  // BIT0: 通道 0 中断标志位

	/* 在这里保存所有相关的可用数据 */
	unsigned short codeStartTimeStamp = TCNT;		/* 保存当前定时器计数值 */
	unsigned short edgeTimeStamp = TC0;				/* 保存边沿时间戳（输入捕获寄存器值） */
	unsigned char PTITCurrentState = PTIT;			/* 保存端口 T 的值，无论 DDRT 的状态如何 */

	// 对于沃尔沃，始终设置为已同步，因为实际上不可能失去同步
	coreStatusA |= PRIMARY_SYNC;  // 设置主同步标志

	/* 以计数单位计算延迟 */
	ISRLatencyVars.primaryInputLatency = codeStartTimeStamp - edgeTimeStamp;  // 延迟 = 代码开始时间 - 边沿时间

	// 如果是上升沿（BIT0 = 1）
	if(PTITCurrentState & 0x01){
		Counters.primaryTeethSeen++;  // 增加主齿计数

		LongTime timeStamp;  // 32 位时间戳结构

		/* 安装低字（16 位） */
		timeStamp.timeShorts[1] = edgeTimeStamp;  // 低 16 位 = 边沿时间戳
		/* 找出我们的定时器值的含义并将其放入高字（16 位） */
		if(TFLGOF && !(edgeTimeStamp & 0x8000)){ /* 参见 68hc11 参考手册 10.3.5 第 4 段了解详细信息 */
			// 如果定时器溢出且边沿时间戳的高位为 0，说明溢出发生在边沿之后
			timeStamp.timeShorts[0] = timerExtensionClock + 1;  // 高 16 位 = 扩展时钟 + 1
		}else{
			timeStamp.timeShorts[0] = timerExtensionClock;  // 高 16 位 = 扩展时钟
		}

		// 来自输入的临时数据
		primaryLeadingEdgeTimeStamp = timeStamp.timeLong;  // 当前上升沿时间戳
		timeBetweenSuccessivePrimaryPulses = primaryLeadingEdgeTimeStamp - lastPrimaryPulseTimeStamp;  // 计算连续主脉冲之间的时间
		lastPrimaryPulseTimeStamp = primaryLeadingEdgeTimeStamp;  // 更新上次脉冲时间戳

// = 60 * (1000000 / 0.8)  // 每分钟的计数 = 60秒 * (1秒的微秒数 / 每个计数的微秒数)
#define ticksPerMinute   75000000 // 这是正确的。

		// 计算 RPM：RPM = 每分钟计数 / 脉冲间隔
		*RPMRecord = (unsigned short) (ticksPerMinute / timeBetweenSuccessivePrimaryPulses);

		// TODO 在调度器使用的齿以外的齿上采样 ADC，以最小化峰值运行时间并获得干净信号
		sampleEachADC(ADCArrays);  // 采样所有 ADC 通道
		Counters.syncedADCreadings++;  // 增加同步 ADC 读取计数
		*mathSampleTimeStampRecord = TCNT;  // 记录采样时间戳

		/* 设置标志以指示需要计算 */
		coreStatusA |= CALC_FUEL_IGN;  // 设置计算燃油和点火标志

		/* 重置读取超时时钟 */
		Clocks.timeoutADCreadingClock = 0;  // 重置超时计数器

		// 如果主脉宽大于最小脉宽，调度喷油事件
		// 使用参考脉宽来决定。火花需要移到此区域外 TODO
		if(masterPulseWidth > injectorMinimumPulseWidth){
			/* 确定半个周期是否大于 short 最大值 */
			unsigned short maxAngleAfter;  // 最大提前角
			if((engineCyclePeriod >> 1) > 0xFFFF){
				// 如果半个周期超过 16 位最大值
				maxAngleAfter = 0xFFFF;  // 使用 16 位最大值
			}else{
				maxAngleAfter = (unsigned short)(engineCyclePeriod >> 1);  // 使用半个周期
			}

			/* 检查提前角，确保它小于前一个发动机周期的 1/2 且大于代码时间 */
			unsigned short advance;  // 提前角
			if(totalAngleAfterReferenceInjection > maxAngleAfter){ // 如果太大，使其为最大值
				advance = maxAngleAfter;  // 限制为最大值
			}else if(totalAngleAfterReferenceInjection < trailingEdgeSecondaryRPMInputCodeTime){ // 如果太小，使其为最小值
				advance = trailingEdgeSecondaryRPMInputCodeTime;  // 限制为最小值
			}else{ // 否则按原样使用
				advance = totalAngleAfterReferenceInjection;  // 使用计算出的提前角
			}

			// 确定长和短开始时间
			unsigned short startTime = edgeTimeStamp + advance;  // 16 位开始时间
			unsigned long startTimeLong = timeStamp.timeLong + advance;  // 32 位开始时间

			/* 确定要调度的通道 */
			unsigned char fuelChannel = 0;//(primaryPulsesPerSecondaryPulse / 2) - 1;  // 简单模式：总是通道 0

			// 确定是否需要重新调度
			unsigned char reschedule = 0;  // 重新调度标志
			// 计算时间差：开始时间 - (上次结束时间 + 关闭代码时间)
			unsigned long diff = startTimeLong - (injectorMainEndTimes[fuelChannel] + injectorSwitchOffCodeTime);
			if(diff > LONGHALF){
				// 如果时间差太大（超过 32 位的一半），需要重新调度
				reschedule = 1;
			}

			// 调度相应的通道
			if(!(*injectorMainControlRegisters[fuelChannel] & injectorMainEnableMasks[fuelChannel]) || reschedule){
				/* 如果定时器没有仍在运行，或者设置的时间太长，将其设置为在正确的时间很快再次开始 */
				*injectorMainControlRegisters[fuelChannel] |= injectorMainEnableMasks[fuelChannel];  // 启用输出比较动作
				*injectorMainTimeRegisters[fuelChannel] = startTime;  // 设置比较时间
				TIE |= injectorMainOnMasks[fuelChannel];  // 启用中断
				TFLG = injectorMainOnMasks[fuelChannel];  // 清除中断标志
			}else{
				// 定时器正在运行，设置保持值供下次使用
				injectorMainStartTimesHolding[fuelChannel] = startTime;  // 保存开始时间到保持变量
				selfSetTimer |= injectorMainOnMasks[fuelChannel]; // 设置一个位，让定时器中断知道从变量设置自己的新开始时间
			}
		}
		// 记录上升沿处理代码的运行时间
		RuntimeVars.primaryInputLeadingRuntime = TCNT - codeStartTimeStamp;
	}else{
		// 下降沿：记录下降沿处理代码的运行时间
		RuntimeVars.primaryInputTrailingRuntime = TCNT - codeStartTimeStamp;
	}
}


/** Secondary RPM ISR
 *
 * Unused in this decoder.
 */
void SecondaryRPMISR(){
	/* 清除此输入捕获通道的中断标志 */
	TFLG = 0x02;  // BIT1: 通道 1 中断标志位

	/* 在这里保存所有相关的可用数据 */
	unsigned short codeStartTimeStamp = TCNT;		/* 保存当前定时器计数值 */
	unsigned short edgeTimeStamp = TC1;				/* 保存时间戳（输入捕获寄存器值） */
	unsigned char PTITCurrentState = PTIT;			/* 保存端口 T 的值，无论 DDRT 的状态如何 */
//	unsigned short PORTS_BACurrentState = PORTS_BA;	/* 已注释：保存点火输出状态 */

	/* 以计数单位计算延迟 */
	ISRLatencyVars.secondaryInputLatency = codeStartTimeStamp - edgeTimeStamp;  // 延迟 = 代码开始时间 - 边沿时间

	/** @todo TODO 丢弃窄脉冲！测试齿宽和齿周期
	 * 宽度应该基于硬件的设置方式。即 LM1815
	 * 被调整为具有特定宽度的脉冲输出。此噪声
	 * 滤波器应该与该宽度匹配，硬件滤波器也应该匹配。
	 */

	LongTime timeStamp;  // 32 位时间戳结构

	/* 安装低字（16 位） */
	timeStamp.timeShorts[1] = edgeTimeStamp;  // 低 16 位 = 边沿时间戳
	/* 找出我们的定时器值的含义并将其放入高字（16 位） */
	if(TFLGOF && !(edgeTimeStamp & 0x8000)){ /* 参见 68hc11 参考手册 10.3.5 第 4 段了解详细信息 */
		// 如果定时器溢出且边沿时间戳的高位为 0
		timeStamp.timeShorts[0] = timerExtensionClock + 1;  // 高 16 位 = 扩展时钟 + 1
	}else{
		timeStamp.timeShorts[0] = timerExtensionClock;  // 高 16 位 = 扩展时钟
	}

	/* LM1815 可变磁阻传感器放大器允许输出在齿的中心处被拉高。
	 * 因此，我们看到的齿的开始实际上是物理齿的中心。因为
	 * 齿的形状、轮廓和间距可能不同，这是我们可以从中调度的唯一可靠边沿，
	 * 因此下降沿代码非常简单。
	 */
	// 如果是上升沿（BIT1 = 1）
	if(PTITCurrentState & 0x02){
		Counters.secondaryTeethSeen++;  // 增加次齿计数

		/* 上升沿代码
		 *
		 * 从当前上升沿减去上次下降沿
		 * 将当前上升沿记录为上次上升沿
		 *
		 * 将脉宽记录为高电平持续时间
		 */
		lengthOfSecondaryLowPulses = timeStamp.timeLong - lastSecondaryPulseTrailingTimeStamp;  // 低电平脉冲长度 = 当前时间 - 上次下降沿时间
		lastSecondaryPulseLeadingTimeStamp = timeStamp.timeLong;  // 更新上次上升沿时间戳
	}else{
		/* 下降沿代码
		 *
		 * 从当前下降沿减去上次上升沿
		 * 将当前下降沿记录为上次下降沿
		 *
		 * 将脉宽记录为低电平持续时间
		 */
//		lengthOfSecondaryHighPulses = timeStamp.timeLong - lastSecondaryPulseLeadingTimeStamp;  // 已注释：高电平脉冲长度
		lastSecondaryPulseTrailingTimeStamp = timeStamp.timeLong;  // 更新上次下降沿时间戳
	}
}
