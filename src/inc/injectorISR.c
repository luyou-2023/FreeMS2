/* FreeMS2 - the open source engine management system
 *
 * Copyright 2008, 2009, 2010 Fred Cooke, Jared Harvey
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


/**	@file injectorISR.c
 *
 * @brief 喷油器 ISR 共享代码
 *
 * 此代码在所有 6 个通道之间完全相同，因此我们只需要一份副本。
 * 每个宏中的 X 将被替换为适合当前使用通道的编号。
 * 
 * @details 为什么需要这个方法：
 * 喷油器控制是 ECU 最关键的实时任务之一，必须在精确的曲轴角度开启和关闭。
 * 使用输出比较功能在硬件级别实现精确的时序控制，确保：
 * - 喷油脉宽精确（微秒级精度）
 * - 喷油时序准确（相对于曲轴角度）
 * - 低延迟（硬件触发，无需软件轮询）
 * 
 * 每个通道执行以下操作：
 *
 * - 1	清除中断标志
 * - 2	记录开始时间
 * - 3	测量并记录延迟
 * - 4	检查是否刚刚开启
 *   - 4.1	将通道的脉宽复制到局部变量
 *   - 4.2	根据代码运行时间常量和延迟确定最小脉宽
 *   - 4.3	将使用的脉宽限制在最小和最大值之间
 *   - 4.4	如果使用的脉宽大于当前发动机周期的周期，标记为始终开启
 *   - 4.5	设置关闭动作
 *   - 4.6	将时间增加脉宽
 *   - 4.7	如果需要分级，要么立即开启并调度关闭，要么调度开启
 * - 5	否则它刚刚关闭
 *   - 5.1	如果分级通道仍然开启，现在关闭它
 *   - 5.2	如果（自调度标志设置）调度下次开始
 *   - 5.3	否则禁用自身
 * - 6	计算并记录代码运行时间
 * - 7	返回
 *
 * @author Fred Cooke
 */


void InjectorXISR(){
	/* 清除此通道的中断标志 */
	TFLG = injectorMainOnMasks[INJECTOR_CHANNEL_NUMBER];  // 清除对应通道的中断标志位

	/* 记录当前时间作为开始时间 */
	unsigned short TCNTStart = TCNT;  // 记录 ISR 开始时的定时器值

	/* 从 IC 寄存器记录边沿时间戳 */
	unsigned short edgeTimeStamp = *injectorMainTimeRegisters[INJECTOR_CHANNEL_NUMBER];  // 读取输出比较寄存器中的时间值

	/* 基于比较时间和开始时间计算并存储延迟 */
	injectorCodeLatencies[INJECTOR_CHANNEL_NUMBER] = TCNTStart - edgeTimeStamp;  // 计算从硬件触发到代码执行的延迟

	/* 如果上升沿触发了此中断（喷油器开启） */
	if(PTIT & injectorMainOnMasks[INJECTOR_CHANNEL_NUMBER]){ // 开启时间的处理

		/* 找出脉宽的最大值和最小值 */
		unsigned short localPulseWidth = injectorMainPulseWidthsRealtime[INJECTOR_CHANNEL_NUMBER];  // 从实时缓冲区读取脉宽
		unsigned short localMinimumPulseWidth = injectorSwitchOnCodeTime + injectorCodeLatencies[INJECTOR_CHANNEL_NUMBER];  // 最小脉宽 = 代码运行时间 + 延迟

		/** @todo TODO *也许* 不是检查最小值并增加脉宽，而是如果开始和现在+常量之间的差值大于所需脉宽，直接强制关闭 */

		/* 确保我们不低于最小脉宽 */
		if(localPulseWidth < localMinimumPulseWidth){
			localPulseWidth = localMinimumPulseWidth;  // 如果脉宽太小，使用最小脉宽
		}/* else{ 直接使用该值 } */

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

		// 存储结束时间供调度器使用
		injectorMainEndTimes[INJECTOR_CHANNEL_NUMBER] = timeStamp.timeLong + localPulseWidth;  // 结束时间 = 开始时间 + 脉宽

		/* 首先设置比较动作以关闭（否则可能在开启期间意外地对喷油器进行 PWM...） */
		*injectorMainControlRegisters[INJECTOR_CHANNEL_NUMBER] &= injectorMainGoLowMasks[INJECTOR_CHANNEL_NUMBER];  // 配置输出比较：匹配时输出低电平（关闭）

		/* 设置再次关闭的时间 */
		*injectorMainTimeRegisters[INJECTOR_CHANNEL_NUMBER] += localPulseWidth;  // 关闭时间 = 当前时间 + 脉宽

		/* 这是我们实际想要的时间点，但由于代码如此简单，它只能是一个很好的短时间 */

		/* 如果需要分级喷油，立即开启或调度对应的分级喷油器，并记住我们这样做了。 */
		if(coreStatusA & STAGED_REQUIRED){
			if(fixedConfigs1.coreSettingsA & STAGED_START){
				/* 立即开启该通道 */
				STAGEDPORT |= STAGEDXON;  // 设置分级喷油器端口位（开启）
				stagedOn |= STAGEDXON;  // 记录分级喷油器已开启
			}else{
				/* 在稍后时间调度开启 */
				/// @todo TODO 使用 PIT 调度分级开启
			}
		}
		/* 计算并存储代码运行时间 */
		injectorCodeOpenRuntimes[INJECTOR_CHANNEL_NUMBER] = TCNT - TCNTStart;  // 记录开启处理代码的运行时间
	}else{ // 关闭时间的处理
		/* 如果我们开启了分级喷油器且它仍然开启，现在关闭它。 */
		if(stagedOn & STAGEDXON){
			STAGEDPORT &= STAGEDXOFF;  // 清除分级喷油器端口位（关闭）
			stagedOn &= STAGEDXOFF;  // 清除分级喷油器开启标志
		}

		/* 设置比较动作以开启并设置下次开始时间，清除自定时器标志 */
		if(selfSetTimer & injectorMainOnMasks[INJECTOR_CHANNEL_NUMBER]){
			// 如果设置了自调度标志，调度下次开启
			*injectorMainTimeRegisters[INJECTOR_CHANNEL_NUMBER] = injectorMainStartTimesHolding[INJECTOR_CHANNEL_NUMBER];  // 设置下次开启时间
			*injectorMainControlRegisters[INJECTOR_CHANNEL_NUMBER] |= injectorMainGoHighMasks[INJECTOR_CHANNEL_NUMBER];  // 配置输出比较：匹配时输出高电平（开启）
			selfSetTimer &= injectorMainOffMasks[INJECTOR_CHANNEL_NUMBER];  // 清除自调度标志
		}else{
			// 如果从这次结束到下次开始的时间很长，禁用中断和动作（节省 CPU）
			TIE &= injectorMainOffMasks[INJECTOR_CHANNEL_NUMBER];  // 禁用该通道的中断
			*injectorMainControlRegisters[INJECTOR_CHANNEL_NUMBER] &= injectorMainDisableMasks[INJECTOR_CHANNEL_NUMBER];  // 禁用输出比较动作
		}
		/* 计算并存储代码运行时间 */
		injectorCodeCloseRuntimes[INJECTOR_CHANNEL_NUMBER] = TCNT - TCNTStart;  // 记录关闭处理代码的运行时间
	}
}
