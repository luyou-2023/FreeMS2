/*	FreeMS2 - the open source engine management system

	Copyright 2009, 2010 Philip L Johnson, Fred Cooke

	This file is part of the FreeMS2 project.

	FreeMS2 software is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	FreeMS2 software is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with any FreeMS2 software.  If not, see http://www.gnu.org/licenses/

	We ask that if you make any changes to this file you email them upstream to
	us at admin(at)diyefi(dot)org or, even better, fork the code on github.com!

	Thank you for choosing FreeMS2 to run your engine! */


/**	@file MissingTeeth.c
 * @ingroup interruptHandlers
 * @ingroup enginePositionRPMDecoders
 *
 * @brief Missing teeth, mostly 36-1 and 60-2
 *
 * @note Pseudo code that does not compile with zero warnings and errors MUST be commented out.
 *
 * @author Philip Johnson
 */


#include "inc/FreeMS2.h"
#include "inc/interrupts.h"
#include "inc/decoderInterface.h"


/** Primary RPM ISR
 *
 * @todo TODO Docs here!
 */
void PrimaryRPMISR(void) {
	// 静态变量：用于跟踪缺齿解码状态
	static LongTime thisHighLowTime = { 0 };  // 当前高+低时间（完整周期）
	static LongTime lastHighLowTime = { 0 };  // 上次高+低时间
	static LongTime lowTime = { 0 };  // 低电平时间
	static LongTime lastPeriod = { 0 };  // 上次周期
	static LongTime lastTimeStamp = { 0 };  // 上次时间戳
	static unsigned int count = 0;  // 同步计数器

	/* 清除此输入捕获通道的中断标志 */
	TFLG = 0x01;  // BIT0: 通道 0 中断标志位

	/* 在这里保存所有相关的可用数据 */
	unsigned short codeStartTimeStamp = TCNT; /* 保存当前定时器计数值 */
	unsigned short edgeTimeStamp = TC0; /* 保存边沿时间戳（输入捕获寄存器值） */
	unsigned char PTITCurrentState = PTIT; /* 保存端口 T 的值，无论 DDRT 的状态如何 */
	//	unsigned short PORTS_BACurrentState = PORTS_BA;	/* 已注释：保存点火输出状态 */

	/* 以计数单位计算延迟 */
	ISRLatencyVars.primaryInputLatency = codeStartTimeStamp - edgeTimeStamp;  // 延迟 = 代码开始时间 - 边沿时间

	LongTime thisTimeStamp;  // 32 位时间戳结构
	/* 安装低字（16 位） */
	thisTimeStamp.timeShorts[1] = edgeTimeStamp;  // 低 16 位 = 边沿时间戳
	/* 找出我们的定时器值的含义并将其放入高字（16 位） */
	if (TFLGOF && !(edgeTimeStamp & 0x8000)) { /* 参见 68hc11 参考手册 10.3.5 第 4 段了解详细信息 */
		// 如果定时器溢出且边沿时间戳的高位为 0
		thisTimeStamp.timeShorts[0] = timerExtensionClock + 1;  // 高 16 位 = 扩展时钟 + 1
	} else {
		thisTimeStamp.timeShorts[0] = timerExtensionClock;  // 高 16 位 = 扩展时钟
	}

	/* 转换之间有多少个计数？ */
	LongTime thisPeriod;  // 当前周期
	if (thisTimeStamp.timeLong > lastTimeStamp.timeLong) {
		// 正常情况：当前时间大于上次时间
		thisPeriod.timeLong = thisTimeStamp.timeLong - lastTimeStamp.timeLong;  // 周期 = 当前时间 - 上次时间
	} else {
		// 处理溢出：当前时间小于上次时间（发生了定时器溢出）
		thisPeriod.timeLong = thisTimeStamp.timeLong + (0xFFFFFFFF
				- lastTimeStamp.timeLong);  // 周期 = 当前时间 + (最大值 - 上次时间)
	}
	lastTimeStamp.timeLong = thisTimeStamp.timeLong;  // 更新上次时间戳

	/* 根据配置设置边沿 */
	unsigned char risingEdge = PTITCurrentState & 0x01;  // BIT0: 上升沿标志

	// 如果上次周期不为 0（不是第一次）
	if (lastPeriod.timeLong != 0) {
		if (risingEdge) {
			// 上升沿：计算完整周期（高电平 + 低电平）
			thisHighLowTime.timeLong = thisPeriod.timeLong + lowTime.timeLong;  // 完整周期 = 当前周期 + 低电平时间
			// 查找缺齿
			if (count == 0 || count == 70) {
				// 同步检测：缺齿的周期应该是正常齿的 2 倍（在 1.5 到 2.5 倍之间）
				if (thisHighLowTime.timeLong > (lastHighLowTime.timeLong + (lastHighLowTime.timeLong>>1)) &&
						thisHighLowTime.timeLong < ((lastHighLowTime.timeLong<<1) + (lastHighLowTime.timeLong>>1))) {
					// 我们已同步（找到缺齿）
					count = 1;  // 重置计数器为 1
				} else {
					// 我们已失去同步
					count = 0;  // 重置计数器为 0
				}
			}else if (count == 2 || (count%2 == 0 &&
					thisHighLowTime.timeLong > (lastHighLowTime.timeLong>>1) &&
					thisHighLowTime.timeLong < (lastHighLowTime.timeLong<<1) )) {
				// 正常齿检测：周期应该在正常范围内（0.5 到 2 倍之间）
				count++;  // 递增计数器
			} else {
				// 我们已失去同步（周期异常）
				count = 0;  // 重置计数器
			}

			// 递增曲轴脉冲计数 TODO 这需要包装在齿周期和宽度检查中
			lastHighLowTime.timeLong = thisHighLowTime.timeLong;  // 更新上次完整周期
			primaryPulsesPerSecondaryPulse++;  // 递增主脉冲计数
			RuntimeVars.primaryInputLeadingRuntime = TCNT - codeStartTimeStamp;  // 记录上升沿处理运行时间
		} else {
			// 下降沿：更新低电平时间
			if (count%2 == 1) {
				// 如果计数器是奇数，递增（正常状态）
				count++;
			} else {
				// 我们已失去同步（计数器状态异常）
				count = 0;  // 重置计数器
			}
			RuntimeVars.primaryInputTrailingRuntime = TCNT - codeStartTimeStamp;  // 记录下降沿处理运行时间
			lowTime.timeLong = thisPeriod.timeLong;  // 更新低电平时间
		}
	}
	lastPeriod.timeLong = thisPeriod.timeLong;  // 更新上次周期
	Counters.primaryTeethSeen++;  // 增加主齿计数
}


/** Secondary RPM ISR
 *
 * @todo TODO Docs here!
 */
void SecondaryRPMISR(void) {
	/* 清除此输入捕获通道的中断标志 */
	TFLG = 0x02;  // BIT1: 通道 1 中断标志位

	/* 在这里保存所有相关的可用数据 */
	unsigned short codeStartTimeStamp = TCNT; /* 保存当前定时器计数值 */
	unsigned short edgeTimeStamp = TC1; /* 保存时间戳（输入捕获寄存器值） */
	unsigned char PTITCurrentState = PTIT; /* 保存端口 T 的值，无论 DDRT 的状态如何 */
	//	unsigned short PORTS_BACurrentState = PORTS_BA;	/* 已注释：保存点火输出状态 */

	/* 以计数单位计算延迟 */
	ISRLatencyVars.secondaryInputLatency = codeStartTimeStamp - edgeTimeStamp;  // 延迟 = 代码开始时间 - 边沿时间

	// TODO 丢弃窄脉冲！测试齿宽和齿周期

	/* 根据配置设置边沿 */
	unsigned char risingEdge;  // 上升沿标志
	if (fixedConfigs1.coreSettingsA & SECONDARY_POLARITY) {
		// 如果配置了反向极性，BIT1 = 1 表示上升沿
		risingEdge = PTITCurrentState & 0x02;  // BIT1: 上升沿标志
	} else {
		// 正常极性，BIT1 = 0 表示上升沿
		risingEdge = !(PTITCurrentState & 0x02);  // BIT1 取反：上升沿标志
	}

	if (risingEdge) {
		// 上升沿处理
		LongTime timeStamp;  // 32 位时间戳结构

		/* 安装低字（16 位） */
		timeStamp.timeShorts[1] = edgeTimeStamp;  // 低 16 位 = 边沿时间戳
		/* 找出我们的定时器值的含义并将其放入高字（16 位） */
		if (TFLGOF && !(edgeTimeStamp & 0x8000)) { /* 参见 68hc11 参考手册 10.3.5 第 4 段了解详细信息 */
			// 如果定时器溢出且边沿时间戳的高位为 0
			timeStamp.timeShorts[0] = timerExtensionClock + 1;  // 高 16 位 = 扩展时钟 + 1
		} else {
			timeStamp.timeShorts[0] = timerExtensionClock;  // 高 16 位 = 扩展时钟
		}

		RuntimeVars.secondaryInputLeadingRuntime = TCNT - codeStartTimeStamp;  // 记录上升沿处理运行时间
	} else {
		// 下降沿处理
		RuntimeVars.secondaryInputTrailingRuntime = TCNT - codeStartTimeStamp;  // 记录下降沿处理运行时间
	}

	Counters.secondaryTeethSeen++;  // 增加次齿计数
}
