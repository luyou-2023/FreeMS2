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


/**	@file NipponDenso.c
 * @ingroup interruptHandlers
 * @ingroup enginePositionRPMDecoders
 *
 * @brief Reads Nippon Denso 24/2 sensors
 *
 * This file contains the two interrupt service routines for handling engine
 * position and RPM signals from mainly Toyota engines using this sensor style.
 *
 * One ISR handles the 24 evenly spaced teeth and the other handles the two
 * adjacent teeth. This signal style provides enough information for wasted
 * spark ignition and semi sequential fuel injection.
 *
 * Supported engines include:
 * - 4A-GE
 * - 7A-FE
 * - 3S-GE
 * - 1UZ-FE
 * - Mazda F2T
 *
 * @author Fred Cooke
 *
 * @note Pseudo code that does not compile with zero warnings and errors MUST be commented out.
 *
 * @todo TODO make this generic for evenly spaced teeth with a pulse per revolution from the second input.
 */


#include "inc/FreeMS2.h"
#include "inc/interrupts.h"
#include "inc/decoderInterface.h"
#include "inc/utils.h"


/** @brief 主 RPM 中断服务程序（日本电装 24/2 解码器）
 *
 * 处理日本电装 24/2 传感器信号，用于丰田等发动机。主输入有 24 个均匀分布的齿，
 * 用于检测曲轴位置和计算 RPM。每两个主脉冲对应一个气缸，支持半顺序喷油和浪费火花点火。
 * 
 * @details 为什么需要这个方法：
 * 日本电装 24/2 传感器是许多丰田发动机使用的标准传感器：
 * - 主输入：24 个均匀分布的齿（每 15 度一个齿）
 * - 次输入：2 个相邻的齿（用于区分发动机循环）
 * - 提供足够的信息用于浪费火花点火和半顺序燃油喷射
 * 
 * 支持的发动机：
 * - 4A-GE, 7A-FE, 3S-GE, 1UZ-FE, Mazda F2T 等
 * 
 * 功能：
 * 1. 位置/RPM 信号解释：
 *    - 丢弃过早到达的边沿（可能失去同步）
 *    - 检查是否失去同步（脉冲到达太晚）
 *    - 比较连续边沿的时间戳并计算 RPM
 *    - 将 RPM 和位置存储到全局变量
 * 
 * 2. 事件调度：
 *    - 遍历所有事件（火花和燃油）
 *    - 调度那些在此齿之后且在下个预期齿之前的事件
 * 
 * 3. ADC 采样：
 *    - 在一致的曲轴位置一次性获取统一的 ADC 读数集
 *    - 消除发动机循环相关的噪声
 *    - 设置标志指示需要计算新的脉宽、提前角等
 * 
 * 同步检测：
 * - 每转应该有 12 个主脉冲（24 个齿 / 2）
 * - 如果 primaryPulsesPerSecondaryPulse > 12，失去同步
 * - 失去同步时清除同步标志并重置计数器
 * 
 * @warning 这些代码仅用于测试和演示，目前不适合实际驾驶使用
 * 
 * @return 无返回值
 *
 * @author Fred Cooke
 * 
 * @todo TODO 更新上述文档以反映实际情况
 * @todo TODO 完成此代码到可用标准
 */
void PrimaryRPMISR(){
	/* Clear the interrupt flag for this input compare channel */
	TFLG = 0x01;

	/* Save all relevant available data here */
	unsigned short codeStartTimeStamp = TCNT;		/* Save the current timer count */
	unsigned short edgeTimeStamp = TC0;				/* Save the edge time stamp */
	unsigned char PTITCurrentState = PTIT;			/* Save the values on port T regardless of the state of DDRT */
//	unsigned short PORTS_BACurrentState = PORTS_BA;	/* Save ignition output state */

	/* Calculate the latency in ticks */
	ISRLatencyVars.primaryInputLatency = codeStartTimeStamp - edgeTimeStamp;

	/** @todo TODO discard narrow ones! test for tooth width and tooth period
	 * the width should be based on how the hardware is setup. IE the LM1815
	 * is adjusted to have a pulse output of a particular width. This noise
	 * filter should be matched to that width as should the hardware filter.
	 */

	/* The LM1815 variable reluctance sensor amplifier allows the output to be
	 * pulled high starting at the center of a tooth. So, what we see as the
	 * start of a tooth is actually the centre of a physical tooth. Because
	 * tooth shape, profile and spacing may vary this is the only reliable edge
	 * for us to schedule from, hence the trailing edge code is very simple.
	 */
	// 如果是上升沿（BIT0 = 1）
	if(PTITCurrentState & 0x01){
		// 递增曲轴脉冲计数 TODO 这需要包装在齿周期和宽度检查中
		primaryPulsesPerSecondaryPulse++;  // 增加主脉冲计数（24/2 模式：每转 24 个齿）

		// 计算粗略 RPM（当变量正确使用时这将是错误的）
		*RPMRecord = ticksPerCycleAtOneRPMx2 / engineCyclePeriod; /* 0.8us 计数, 150mil = 2 x 60 秒, 乘以 RPM 比例因子 2 */
		// RPM = (每分钟计数 / 2) / 发动机周期

		// 在第二个触发器到达且周期正确之前不运行（非常临时）
		if(!(coreStatusA & PRIMARY_SYNC)){
			primaryTeethDroppedFromLackOfSync++;  // 增加因缺少同步而丢弃的齿计数
			return;  // 未同步，退出
		}

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

		// 来自输入的临时数据
		primaryLeadingEdgeTimeStamp = timeStamp.timeLong;  // 当前上升沿时间戳
		timeBetweenSuccessivePrimaryPulses = lastPrimaryPulseTimeStamp - primaryLeadingEdgeTimeStamp;  // 计算连续主脉冲之间的时间
		lastPrimaryPulseTimeStamp = primaryLeadingEdgeTimeStamp;  // 更新上次脉冲时间戳
//		timeBetweenSuccessivePrimaryPulsesBuffer = (timeBetweenSuccessivePrimaryPulses >> 1) + (timeBetweenSuccessivePrimaryPulsesBuffer >> 1);  // 已注释：缓冲计算

		// TODO 使调度要么从启动时固定且范围有限，要么如果实用的话在运行时调度，以允许燃油和点火的任意提前和延迟。

		/* 通过计数过高检查同步丢失
		 * 24/2 模式：每转应该有 12 个主脉冲（24 个齿 / 2） */
		if(primaryPulsesPerSecondaryPulse > 12){
			/* 递增丢失同步计数 */
			Counters.crankSyncLosses++;  // 增加曲轴同步丢失计数

			/* 清除同步状态 */
			coreStatusA &= CLEAR_PRIMARY_SYNC;  // 清除主同步标志

			/* 重置齿计数 */
			primaryPulsesPerSecondaryPulse = 0;  // 重置主脉冲计数

			/* 在我们做坏事之前离开这里 */
			return;  // 失去同步，退出
		}

		// CAUTION came to me lying in bed half asleep idea :

		// TODO move tooth selection to the calc loop in main such that this routine just iterates through an array of events and schedules those that are destined for this tooth.

		// if ign enabled
			// iterate through ignition first, schedule all of those
			// iterate through dwell next, schedule all of those
		// if fuel enabled
			// iterate through main fuel next, schedule all of those
			// if staging enabled and required
				// iterate through staged fuel last,

		// TODO should make for a clean compact scheduling implementation. the fuel code doesn't care when/how it has started in the past, and hopefully ign will be the same.

		// 这将在未来通过数组和每齿检查来完成
		// 每两个主脉冲执行一次（24/2 模式：每转 12 次）
		if((primaryPulsesPerSecondaryPulse % 2) == 0){

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
				unsigned short startTime = primaryLeadingEdgeTimeStamp + advance;  // 16 位开始时间（注意：这里可能有溢出问题）
				unsigned long startTimeLong = timeStamp.timeLong + advance;  // 32 位开始时间

				/* 确定要调度的通道
				 * 24/2 模式：每两个主脉冲对应一个气缸
				 * primaryPulsesPerSecondaryPulse: 2,4,6,8,10,12 对应通道 0,1,2,3,4,5 */
				unsigned char fuelChannel = (primaryPulsesPerSecondaryPulse / 2) - 1;  // 燃油通道 = (脉冲数 / 2) - 1
				unsigned char ignitionChannel = (primaryPulsesPerSecondaryPulse / 2) - 1;  // 点火通道 = (脉冲数 / 2) - 1

				// 检查通道号是否有效（最大 5，对应 6 个通道）
				if(fuelChannel > 5 || ignitionChannel > 5){
//					send("bad fuel : ");  // 已注释：调试输出
	//				sendUC(fuelChannel);  // 已注释
		//			send("bad  ign : ");  // 已注释
			//		sendUC(ignitionChannel);  // 已注释
					return;  // 通道号无效，退出
				}

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

				// TODO advance/retard/dwell numbers all need range checking etc done. some of this should be done in the calculator section, and some here. currently none is done at all and for that reason, this will not work in a real system yet, if it works at all.
				// as do array indexs here and in the ISRs...


				// TODO implement mechanism for dropping a cylinder in event of over queueing or spark cut/round robin
				// important as ignition sequence disrupted when this occurs as it stands.

				// TODO check queue length checks to ensure we dont count up to somewhere we can never count down from. This could be causing the hanging long phenomina

				// DWELL
/*
				// If dwell is not currently enabled, set it all up
				if(!(PITCE & DWELL_ENABLE)){
					// Schedule Dwell event (do this first because it comes earliest.
					// set the channel to fire
					nextDwellChannel = ignitionChannel;

					// set the time
					PITLD0 = advance;
					//				PITLD0 = ignitionAdvances[ignitionChannel] - *currentDwellRealtime; BAD for various reasons!

					// clear the flags first as they apparently become set any old time whether enabled or not.
					PITTF |= DWELL_ENABLE;

					// turn on the ints
					PITINTE |= DWELL_ENABLE;

					// clear the flags first as they apparently become set any old time whether enabled or not.
					PITTF |= DWELL_ENABLE;

					// enable channels
					PITCE |= DWELL_ENABLE;
				}else if(dwellQueueLength == 0){
					// load time offset such that next period is correct
					PITLD0 = (advance - PITCNT0);

					// increment queue length
					dwellQueueLength++;
				}else if(dwellQueueLength > fixedConfigs1.engineSettings.combustionEventsPerEngineCycle){ //TODO sensible figures here for array index OOBE
					// do nothing, or increment a counter or something similar.
				}else{
					unsigned short sumOfDwells = PITLD0;
					// add up the prequeued time periods

					// queue = 1 pitld is all
					// queue = 2 one from 0 index of array AND pitld

					unsigned char index = 0;
					while(index < (dwellQueueLength -1)){
						sumOfDwells += queuedDwellOffsets[index];
						index++;
					}
					//				for(index = 0;index < (dwellQueueLength -1);index++){ // is this right?
					//					sumOfDwells += queuedDwellOffsets[index];
					//				}

					// store time offset in appropriate array location
					queuedDwellOffsets[dwellQueueLength - 1] = advance - (PITCNT0 + sumOfDwells);

					// increment queue length from one or more
					dwellQueueLength++;
				}

				// IGNITION experimental stuff

				// If ignition is not currently enabled, set it all up
				if(!(PITCE & IGNITION_ENABLE)){
					// Schedule Ignition event (do this first because it comes earliest.
					// set the channel to fire
					nextIgnitionChannel = ignitionChannel;

					// figure out the time to set the delay reg to
					PITLD1 = advance + injectorMainPulseWidthsRealtime[fuelChannel];
					//				PITLD1 = ignitionAdvances[ignitionChannel + outputBankIgnitionOffset];

					// clear the flags first as they apparently become set any old time whether enabled or not.
					PITTF |= IGNITION_ENABLE;

					// turn on the ints
					PITINTE |= IGNITION_ENABLE;

					// clear the flags first as they apparently become set any old time whether enabled or not.
					PITTF |= IGNITION_ENABLE;

					// enable channels
					PITCE |= IGNITION_ENABLE;
				}else if(ignitionQueueLength == 0){
					// load timer register
					PITLD1 = ((advance + injectorMainPulseWidthsRealtime[fuelChannel]) - PITCNT1);

					// increment to 1
					ignitionQueueLength++;
				}else if(ignitionQueueLength > fixedConfigs1.engineSettings.combustionEventsPerEngineCycle){ //TODO sensible figures here for array index OOBE
					// do nothing, or increment a counter or something similar.
				}else{
					unsigned short sumOfIgnitions = PITLD1;
					// add up the prequeued time periods

					// queue = 1 pitld is all
					// queue = 2 one from 0 index of array AND pitld


					unsigned char index = 0;
					while(index < (ignitionQueueLength - 1)){
						sumOfIgnitions += queuedIgnitionOffsets[index];
						index++;
					}
					//	for(index = 0;index < (ignitionQueueLength -1);index++){ // is this right?
					// 		sumOfIgnitions += queuedIgnitionOffsets[index];
					//	}

					// store time offset in appropriate array location
					queuedIgnitionOffsets[ignitionQueueLength - 1] = advance - (PITCNT1 + sumOfIgnitions);

					// increment from 1 or more
					ignitionQueueLength++;
				}*/
			}
		}
		// 记录上升沿处理代码的运行时间
		RuntimeVars.primaryInputLeadingRuntime = TCNT - codeStartTimeStamp;
	}else{
		// 下降沿：记录下降沿处理代码的运行时间
		RuntimeVars.primaryInputTrailingRuntime = TCNT - codeStartTimeStamp;
	}

	Counters.primaryTeethSeen++;  // 增加主齿计数
	// 找出 RPM 和准确的 TDC 参考

	// 如果你说得快，听起来不多：
	// 基于火花切断、燃油切断、时序变量、状态变量和配置变量调度燃油和点火
}


/** @brief 次 RPM 中断服务程序（日本电装 24/2 解码器）
 *
 * 处理次 RPM 输入（2 个相邻齿），用于区分发动机循环和确定相位。
 * 每次收到次 RPM 脉冲时，重置主脉冲计数器并确认同步。
 * 
 * @details 为什么需要这个方法：
 * 24/2 传感器中的"2"表示次输入有 2 个相邻齿，用于：
 * - 区分两个 360 度循环（四冲程发动机的 720 度循环）
 * - 确定发动机相位（哪个气缸处于压缩冲程）
 * - 重置主脉冲计数器（每转重置一次）
 * - 计算发动机周期（用于 RPM 计算）
 * 
 * 工作原理：
 * - 检测次 RPM 输入的上升沿
 * - 重置 primaryPulsesPerSecondaryPulse = 0（每转重置）
 * - 验证主脉冲计数是否正确（应该是 12 个）
 * - 计算发动机周期（2 × 次脉冲间隔）
 * - 设置同步标志（PRIMARY_SYNC）
 * 
 * 同步验证：
 * - 如果主脉冲计数不是 12，且已同步，则失去同步
 * - 失去同步时清除同步标志并增加错误计数
 * 
 * @return 无返回值
 *
 * @author Fred Cooke
 * 
 * @todo TODO 更新此文档
 * @todo TODO 完成此代码到可用标准
 */
void SecondaryRPMISR(){
	/* Clear the interrupt flag for this input compare channel */
	TFLG = 0x02;

	/* Save all relevant available data here */
	unsigned short codeStartTimeStamp = TCNT;		/* Save the current timer count */
	unsigned short edgeTimeStamp = TC1;				/* Save the timestamp */
	unsigned char PTITCurrentState = PTIT;			/* Save the values on port T regardless of the state of DDRT */
//	unsigned short PORTS_BACurrentState = PORTS_BA;	/* Save ignition output state */

	/* Calculate the latency in ticks */
	ISRLatencyVars.secondaryInputLatency = codeStartTimeStamp - edgeTimeStamp;

	/** @todo TODO discard narrow ones! test for tooth width and tooth period
	 * the width should be based on how the hardware is setup. IE the LM1815
	 * is adjusted to have a pulse output of a particular width. This noise
	 * filter should be matched to that width as should the hardware filter.
	 */

	/* The LM1815 variable reluctance sensor amplifier allows the output to be
	 * pulled high starting at the center of a tooth. So, what we see as the
	 * start of a tooth is actually the centre of a physical tooth. Because
	 * tooth shape, profile and spacing may vary this is the only reliable edge
	 * for us to schedule from, hence the trailing edge code is very simple.
	 */
	// 如果是上升沿（BIT1 = 1）
	if(PTITCurrentState & 0x02){
// 这段代码这样写是因为有好的理由吗？
//		primaryPulsesPerSecondaryPulseBuffer = primaryPulsesPerSecondaryPulse;  // 已注释：缓冲主脉冲计数
		primaryPulsesPerSecondaryPulse = 0;  // 重置主脉冲计数（每转重置一次）

		// 如果我们没有得到正确数量的脉冲，丢弃同步并重新开始
		// 注意：此时 primaryPulsesPerSecondaryPulse 已经是 0，所以这个检查可能有问题
		if((primaryPulsesPerSecondaryPulse != 12) && (coreStatusA & PRIMARY_SYNC)){
			coreStatusA &= CLEAR_PRIMARY_SYNC;  // 清除主同步标志
			Counters.crankSyncLosses++;  // 增加曲轴同步丢失计数
		}

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

		// 获取我们实际想要的数据
		engineCyclePeriod = 2 * (timeStamp.timeLong - lastSecondaryOddTimeStamp); // 保存发动机周期（24/2 模式：每转 2 个次脉冲）
		lastSecondaryOddTimeStamp = timeStamp.timeLong; // 保存此时间戳供下次使用

		// 因为这是我们唯一的参考，每次我们收到这个脉冲，我们知道我们在哪里（到目前为止的简单模式）
		coreStatusA |= PRIMARY_SYNC;  // 设置主同步标志（每转同步一次）
		RuntimeVars.secondaryInputLeadingRuntime = TCNT - codeStartTimeStamp;  // 记录上升沿处理运行时间
	}else{
		// 下降沿：记录下降沿处理运行时间
		RuntimeVars.secondaryInputTrailingRuntime = TCNT - codeStartTimeStamp;
	}

	Counters.secondaryTeethSeen++;  // 增加次齿计数
	// 找出相位/发动机周期参考，显示我们在哪个气缸组

	/* 如果标志不在开始时清除，则中断在运行时会被重新调度，因此不能在 ISR 结束时完成 */
}
