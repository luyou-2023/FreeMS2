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


/** @file main.c
 *
 * @brief The main function!
 *
 * The function main is traditionally an applications starting point. For us
 * it has two jobs. The first is to call init() which initialises everything
 * before any normal code runs. After that main() is simply an infinite loop
 * from which low priority non-realtime code runs. The most important units of
 * code that runs under the main loop umbrella are the injection, ignition and
 * scheduling calculations.
 *
 * @author Fred Cooke
 */


#include "inc/main.h"


/** @brief 主函数 - 应用程序的入口点和主循环
 *
 * 这是应用程序的中心，所有非 ISR 代码都从这里直接或间接调用。
 * 函数分为两个主要部分：初始化和主循环。
 * 
 * @details 为什么需要这个方法：
 * 嵌入式系统需要一个明确的入口点来启动应用程序。main() 函数负责：
 * 1. 初始化：调用 init() 设置所有硬件和软件组件
 * 2. 主循环：运行低优先级、非实时任务
 * 
 * 主循环的作用：
 * - 执行燃油和点火计算（仅在需要时）
 * - 处理通信数据包
 * - 管理双缓冲机制（确保 ISR 和主循环之间的数据一致性）
 * - 处理其他非实时任务
 * 
 * 设计原则：
 * - 保持低延迟：计算只在需要时执行，确保系统能快速响应发动机需求
 * - 非阻塞：主循环不执行长时间操作，避免影响实时性
 * - 事件驱动：基于标志位决定何时执行计算，而不是固定周期
 * 
 * 与 ISR 的关系：
 * - ISR 处理实时关键任务（发动机位置检测、喷油点火调度）
 * - 主循环处理计算密集型任务（燃油计算、通信处理）
 * - 通过双缓冲机制避免数据竞争
 *
 * @return 理论上应返回退出代码，但实际上永远不会返回（无限循环）
 * 
 * @author Fred Cooke
 * 
 * @todo TODO 考虑将此函数移到分页 Flash 中
 */
int  main(){ // TODO maybe move this to paged flash ?
	// 设置所有系统组件
	init();

	//LongNoTime.timeLong = 54;
	// 进入主循环，无限循环运行直到设备关闭或复位
	while(TRUE){
	//	unsigned short start = realTimeClockMillis;
		/* 如果 ADC 需要强制采样，现在进行采样 */
		if(coreStatusA & FORCE_READING){
			ATOMIC_START(); /* 开始原子操作块，禁用中断以确保数据一致性 */
			/* 原子操作块确保完整的一组读数被一起获取 */

			/* 检查确保在我们进入不可中断状态之前没有进行过读取 */
			if(coreStatusA & FORCE_READING){ // 我们是否仍然需要这样做 TODO ?

				// 采样所有 ADC 通道，将结果存储到记录缓冲区
				sampleEachADC(ADCArraysRecord); // TODO 仍然需要做一对循环并为这两个函数计时以获得性能数据
				//sampleLoopADC(&ADCArrays);
				// 重置到非运行状态（清除运行标志等）
				resetToNonRunningState();
				// 增加超时 ADC 读取计数器
				Counters.timeoutADCreadings++;

				/* 设置标志以指示需要计算 */
				coreStatusA |= CALC_FUEL_IGN;

				/* 清除强制读取标志 */
				coreStatusA &= CLEAR_FORCE_READING;
			}

			ATOMIC_END(); /* 结束原子操作块，重新启用中断 */
		}

		/* 如果需要，首先执行主要的燃油和点火计算 */
		if(coreStatusA & CALC_FUEL_IGN){
			ATOMIC_START(); /* 开始原子操作块 */
			/* 原子操作块确保在时间紧张时不会为下一个数据集清除标志 */

			/* 切换输入缓冲区，以便我们有一组稳定的最新数据 */
			if(ADCArrays == &ADCArrays1){
				// 如果当前使用的是缓冲区1，切换到缓冲区0
				RPM = &RPM0; // TODO 临时变量，待移除
				RPMRecord = &RPM1; // TODO 临时变量，待移除
				ADCArrays = &ADCArrays0;          // 切换到缓冲区0用于计算
				ADCArraysRecord = &ADCArrays1;    // 缓冲区1用于ISR记录新数据
				mathSampleTimeStamp = &ISRLatencyVars.mathSampleTimeStamp0; // TODO 临时变量，待移除
				mathSampleTimeStampRecord = &ISRLatencyVars.mathSampleTimeStamp1; // TODO 临时变量，待移除
			}else{
				// 如果当前使用的是缓冲区0，切换到缓冲区1
				RPM = &RPM1; // TODO 临时变量，待移除
				RPMRecord = &RPM0; // TODO 临时变量，待移除
				ADCArrays = &ADCArrays1;          // 切换到缓冲区1用于计算
				ADCArraysRecord = &ADCArrays0;    // 缓冲区0用于ISR记录新数据
				mathSampleTimeStamp = &ISRLatencyVars.mathSampleTimeStamp1; // TODO 临时变量，待移除
				mathSampleTimeStampRecord = &ISRLatencyVars.mathSampleTimeStamp0; // TODO 临时变量，待移除
			}

			/* 清除计算所需标志 */
			coreStatusA &= CLEAR_CALC_FUEL_IGN;

			ATOMIC_END(); /* 结束原子操作块 */

			/* 存储从采样时间到运行时的延迟 */
			ISRLatencyVars.mathLatency = TCNT - *mathSampleTimeStamp;
			/* 跟踪我们每秒执行多少次计算... */
			Counters.calculationsPerformed++;
			/* ...以及每次计算花费多长时间 */
			unsigned short mathStartTime = TCNT;  // 记录计算开始时间

			/* 从传感器输入和记录的齿时序生成核心变量 */
			generateCoreVars();

			// 记录核心变量生成的运行时间（以定时器计数为单位）
			RuntimeVars.genCoreVarsRuntime = TCNT - mathStartTime;
			unsigned short derivedStartTime = TCNT;  // 记录派生变量生成开始时间

			/* 根据设置从核心变量生成派生变量 */
			//generateDerivedVars();  // 当前被注释掉

			// 记录派生变量生成的运行时间
			RuntimeVars.genDerivedVarsRuntime = TCNT - derivedStartTime;
			unsigned short calcsStartTime = TCNT;  // 记录计算开始时间

			/* 执行计算 TODO 如果合理的话，可能将此移到软件中断中 */
			//calculateFuelAndIgnition();  // 当前被注释掉

			// 记录计算的运行时间
			RuntimeVars.calcsRuntime = TCNT - calcsStartTime;
			/* 记录所有数学计算的总运行时间 */
			RuntimeVars.mathTotalRuntime = TCNT - mathStartTime;

			// 计算数学计算的总和运行时间（各阶段时间之和）
			RuntimeVars.mathSumRuntime = RuntimeVars.calcsRuntime + RuntimeVars.genCoreVarsRuntime + RuntimeVars.genDerivedVarsRuntime;

			ATOMIC_START(); /* 开始原子操作块 */
			/* 原子操作块确保输出缓冲区和输出缓冲区偏移量匹配 */

			/* 切换到最新数据的缓冲区 */
			if(injectorMainPulseWidthsMath == injectorMainPulseWidths1){
				// 如果数学计算使用的是缓冲区1，切换到缓冲区0
				currentDwellMath = &currentDwell0;                    // 数学计算使用闭合角缓冲区0
				currentDwellRealtime = &currentDwell1;                 // 实时ISR使用闭合角缓冲区1
				injectorMainPulseWidthsMath = injectorMainPulseWidths0;      // 数学计算使用主喷油脉宽缓冲区0
				injectorMainPulseWidthsRealtime = injectorMainPulseWidths1;  // 实时ISR使用主喷油脉宽缓冲区1
				injectorStagedPulseWidthsMath = injectorStagedPulseWidths0;   // 数学计算使用分级喷油脉宽缓冲区0
				injectorStagedPulseWidthsRealtime = injectorStagedPulseWidths1; // 实时ISR使用分级喷油脉宽缓冲区1
			}else{
				// 如果数学计算使用的是缓冲区0，切换到缓冲区1
				currentDwellMath = &currentDwell1;                    // 数学计算使用闭合角缓冲区1
				currentDwellRealtime = &currentDwell0;                 // 实时ISR使用闭合角缓冲区0
				injectorMainPulseWidthsMath = injectorMainPulseWidths1;      // 数学计算使用主喷油脉宽缓冲区1
				injectorMainPulseWidthsRealtime = injectorMainPulseWidths0;  // 实时ISR使用主喷油脉宽缓冲区0
				injectorStagedPulseWidthsMath = injectorStagedPulseWidths1;   // 数学计算使用分级喷油脉宽缓冲区1
				injectorStagedPulseWidthsRealtime = injectorStagedPulseWidths0; // 实时ISR使用分级喷油脉宽缓冲区0
			}

			ATOMIC_END(); /* 结束原子操作块 */
		}else{
			/* 如果不需要计算，在返回重试之前稍微休眠一下 */
			// 不这样做会导致ISR锁定运行时间过长
			sleepMicro(RuntimeVars.mathTotalRuntime);
			/* 使用 0.8 ticks 作为微秒，所以它将运行得比数学计算稍长一些 */
		}


//		if(!(TXBufferInUseFlags)){
			/* 如果设置了通信数据包处理标志且 TX 缓冲区可用，处理数据！ */
			if(RXStateFlags & RX_READY_TO_PROCESS){
				/* 清除标志 */
				RXStateFlags &= RX_CLEAR_READY_TO_PROCESS;

				/* 处理传入的数据包 */
				decodePacketAndRespond();
			}//else if(lastCalcCount != Counters.calculationsPerformed){ // 替换为 true 用于全速连续流测试...

				/* send asynchronous data log if required */
//				switch (TablesB.SmallTablesB.datalogStreamType) {
//					case asyncDatalogOff:
//					{
//						break;
//					}
//					case asyncDatalogBasic:
//					{
//						/* Flag that we are transmitting! */
//						TXBufferInUseFlags |= COM_SET_SCI0_INTERFACE_ID;
//						// SCI0 only for now...
//
//						// headers including length...						*length = configuredBasicDatalogLength;
//						TXBufferCurrentPositionHandler = (unsigned char*)&TXBuffer;
//
//						/* Initialised here such that override is possible */
//						TXBufferCurrentPositionSCI0 = (unsigned char*)&TXBuffer;
//						TXBufferCurrentPositionCAN0 = (unsigned char*)&TXBuffer;
//
//						/* Set the flags : firmware, no ack, no addrs, has length */
//						*TXBufferCurrentPositionHandler = HEADER_HAS_LENGTH;
//						TXBufferCurrentPositionHandler++;
//
//						/* Set the payload ID */
//						*((unsigned short*)TXBufferCurrentPositionHandler) = responseBasicDatalog;
//						TXBufferCurrentPositionHandler += 2;
//
//						/* Set the length */
//						*((unsigned short*)TXBufferCurrentPositionHandler) = configuredBasicDatalogLength;
//						TXBufferCurrentPositionHandler += 2;
//
//						/* populate data log */
//						populateBasicDatalog();
//						finaliseAndSend(0);
//						break;
//					}
//					case asyncDatalogConfig:
//					{
//						/// TODO @todo
//						break;
//					}
//					case asyncDatalogTrigger:
//					{
//						/// TODO @todo
//						break;
//					}
//					case asyncDatalogADC:
//					{
//						/// TODO @todo
//						break;
//					}
//					case asyncDatalogCircBuf:
//					{
//						/// TODO @todo
//						break;
//					}
//					case asyncDatalogCircCAS:
//					{
//						/// TODO @todo
//						break;
//					}
//					case asyncDatalogLogic:
//					{
//						/// TODO @todo
//						break;
//					}
//				}
//				// mechanism to ensure we only send something if the data has been updated
//				lastCalcCount = Counters.calculationsPerformed;
//			}
//		}
		// 每个周期一次，用于主循环心跳（J0端口）
		//PORTJ ^= 0x01;


		// 调试代码：通过LED显示串口接收状态
		if(SCI0CR2 & SCICR2_RX_ENABLE){
			// 如果串口接收使能，设置PORTK的第2位（点亮LED）
			PORTK |= BIT2;
		}else{
			// 如果串口接收未使能，清除PORTK的第2位（熄灭LED）
			PORTK &= NBIT2;
		}

		if(SCI0CR2 & SCICR2_RX_ISR_ENABLE){
			// 如果串口接收中断使能，设置PORTK的第3位（点亮LED）
			PORTK |= BIT3;
		}else{
			// 如果串口接收中断未使能，清除PORTK的第3位（熄灭LED）
			PORTK &= NBIT3;
		}

		// PWM 实验性功能
		adjustPWM();
	}
}
