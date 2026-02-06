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


/**	@file fuelAndIgnitionCalcs.c
 * @ingroup measurementsAndCalculations
 *
 * @brief Fuel and ignition calculations.
 *
 * This file contains all of the main fuel and ignition calculations based
 * upon the variables that we have already determined in previous stages.
 *
 * @author Fred Cooke
 */


#define FUELANDIGNITIONCALCS_C
#include "inc/FreeMS2.h"
#include "inc/utils.h"
#include "inc/commsCore.h"
#include "inc/tableLookup.h"
#include "inc/decoderInterface.h"
#include "inc/fuelAndIgnitionCalcs.h"


/** @brief 燃油和点火计算
 *
 * 使用各种主要算法计算基础脉宽，然后应用各种修正，如喷油器死区时间、
 * 瞬态燃油修正、发动机温度修正和每缸修正。燃油喷射时序也在这里确定。
 *
 * 也在这里计算点火时序和闭合角。这些也应用了几个修正。
 *
 * @todo TODO 实现所有点火相关功能并完成所有燃油喷射相关功能。
 * @todo TODO 更改配置方式，确保最常见的选项在第一个 if() 之后。
 * @todo TODO 为这些项目在固定配置块中添加实际配置选项。
 *
 * @author Fred Cooke
 * 
 * @note 当前整个函数被注释掉，待实现
 */
//void calculateFuelAndIgnition(){
//	/*&&&&&&&&&&&&& 逐步执行基本计算以获得最终脉宽 &&&&&&&&&&&&*/
//
//	if(TRUE /* 真实方法 */){
//		unsigned short airInletTemp = CoreVars->IAT; /* 除 MAF 外，所有方法都使用此值。 */
//		/* 确定空气流量数据类型 */
//		if(TRUE /* SpeedDensity 速度密度模式 */){
//			/* 在 512kPa 或约 60psi 增压和 128% VE 之前不会溢出。 */
//			DerivedVars->AirFlow = ((unsigned long)CoreVars->MAP * DerivedVars->VEMain) / oneHundredPercentVE;
//			/* 结果始终在 450 - 65535 之间。 */
//			// 空气流量 = MAP * VE / 100%
//		}else if(FALSE /*AlphaN 节气门角度模式*/){
//			DerivedVars->AirFlow = DerivedVars->VEMain; /* 实际上不是 VE，而是没有密度信息的调谐空气流量 */
//		}else if(FALSE /*MAF 质量空气流量模式*/){
//			DerivedVars->AirFlow = CoreVars->MAF; /* 只需将温度固定在适当水平以提供正确的 Lambda */
//			/// @todo TODO 找出正确的"温度"以使 MAF 正常工作！
//			airInletTemp = roomTemperature; // 293.15k 是 20c * 100 得到值，所以除以 100 得到实际数字
//		}else if(FALSE /*FixedAF 固定空气流量模式*/){ /* 从配置中固定空气流量 */
//			DerivedVars->AirFlow = fixedConfigs2.sensorPresets.presetAF;
//		}else{ /* 默认不供油并报错 */
//			DerivedVars->AirFlow = 0;
//			/* 如果有人监听，让他们知道出了问题 */
////			sendError(AIRFLOW_NOT_CONFIGURED_CODE); // 或者可能排队？
//		}
//
//
//		/* 在超过 125C 进气、1.5 Lambda 和密度如水的燃油之前不会溢出 */
//		DerivedVars->densityAndFuel = (((unsigned long)((unsigned long)airInletTemp * DerivedVars->Lambda) / stoichiometricLambda) * fixedConfigs1.engineSettings.densityOfFuelAtSTP) / densityOfFuelTotalDivisor;
//		/* 结果始终在 7500 - 60000 之间。 */
//		// 密度和燃油因子 = (进气温度 * Lambda / 理论空燃比) * 标准条件下燃油密度 / 总除数
//
//		/* 进气温度和压力的除数：
//		 * #define airInletTempDivisor 100
//		 * #define airPressureDivisor 100
//		 * 相互抵消！所有其他都被使用。 */
//
//
//		DerivedVars->BasePW = (bootFuelConst * DerivedVars->AirFlow) / DerivedVars->densityAndFuel;
//		// 基础脉宽 = (启动燃油常数 * 空气流量) / 密度和燃油因子
//	}else if(FALSE /*configured 配置模式*/){ /* 从配置中固定脉宽 */
//		DerivedVars->BasePW = fixedConfigs2.sensorPresets.presetBPW;
//	}else{ /* 默认不供油并报错 */
//		DerivedVars->BasePW = 0;
//		/* 如果有人监听，让他们知道出了问题 */
////		sendError(BPW_NOT_CONFIGURED_CODE); // 或者可能排队？
//	}
//
//	/*&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&*/
//
//
//
//
//	/*&&&&&&&&&&&&&&&&&&&&&&&&&&&& 应用所有修正 PCFC, ETE, IDT, TFC 等 &&&&&&&&&&&&&&&&&&&&&&&&&&&*/
//
//	/* 计算后应用修正 */
//	DerivedVars->EffectivePW = safeTrim(DerivedVars->BasePW, DerivedVars->TFCTotal);
//	// 应用瞬态燃油修正（TFC）：在基础脉宽上加减修正值
//	DerivedVars->EffectivePW = safeScale(DerivedVars->EffectivePW, DerivedVars->ETE);
//	// 应用发动机温度修正（ETE）：按百分比缩放有效脉宽
//
//
//	unsigned char channel; // 此变量的声明在下面的多个循环中使用。
//
//	/* "计算"各个燃油脉宽 */
//	for(channel = 0; channel < INJECTION_CHANNELS; channel++){ /// @todo TODO 使喷油器通道来自配置，而不是定义。
//		/* 添加或减去每缸燃油修正 */
//		unsigned short channelPW;
//		channelPW = safeScale(DerivedVars->EffectivePW, TablesB.SmallTablesB.perCylinderFuelTrims[channel]);
//		// 应用每缸燃油修正（PCFC）：按百分比缩放有效脉宽
//
//		/* 添加 IDT 以获得最终值并放入数组 */
//		injectorMainPulseWidthsMath[channel] = safeAdd(channelPW, DerivedVars->IDT);
//		// 最终脉宽 = 通道脉宽 + 喷油器死区时间（IDT）
//	}
//
//	/* 参考脉宽用于比较等 */
//	unsigned short refPW = safeAdd(DerivedVars->EffectivePW, DerivedVars->IDT);
//	// 参考脉宽 = 有效脉宽 + 喷油器死区时间
//	DerivedVars->RefPW = refPW;
//	/*&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&*/
//
//
//
//
//	/*&&&&&&&&&&&&&&&&& 基于 IDT 调度脉宽开始，使燃油正确定时 &&&&&&&&&&&&&&&&&&&*/
//
//	for(channel = 0;channel < INJECTION_CHANNELS;channel++){ /// @todo TODO 使喷油器通道来自配置，而不是定义。
//		//injectorMainAdvances[channel] = IDT blah blah.  // 待实现：设置每通道的提前角
//	}
//
//	/* 这将涉及使用 RPM、喷油器点火角和 IDT 来正确调度事件 */
//
//	/** @todo TODO 在完成此功能之前需要完成调度工作。 */
//
//	/*&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&*/
//
//
//
//
//	/*&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& 计算闭合角和点火角 &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&*/
//	/*&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&*/
//
//
//
//
//	/*&&&&&&&&&&&&&&& 基于闭合角和点火角调度闭合的开始和结束 &&&&&&&&&&&&&&&&*/
//	/*&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&*/
//
//
//
//
//	/*&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& 临时（和旧的）代码 &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&*/
//
//	/* "计算"每通道修正前的标称总脉宽 */
//	masterPulseWidth = refPW;//(ADCArrays->EGO << 6) + (ADCArrays->MAP >> 4);  // 已注释：旧的计算方法
//
//	/* "计算"各个燃油脉宽 */
//	for(channel = 0; channel < INJECTION_CHANNELS; channel++){
//		injectorMainPulseWidthsMath[channel] = masterPulseWidth;  // 临时：所有通道使用相同的脉宽
//	}
//
//	/// @todo TODO x 6 主脉宽, x 6 分级脉宽, x 6 分级通道标志 if(coreSettingsA & STAGED_ON){}
//
//	/* 设置分级状态开启或关闭（目前基于可更改设置） */
//	if(fixedConfigs1.coreSettingsA & STAGED_ON){
//		coreStatusA |= STAGED_REQUIRED;  // 设置需要分级喷油标志
//		/// @todo TODO 基于某种映射和/或基于复杂负荷的配置确定分级需求。
//	}else{
//		coreStatusA &= STAGED_NOT_REQUIRED;  // 清除需要分级喷油标志
//	}
//
//	// 临时点火测试
//	unsigned short intendedAdvance = ADCArrays->MAT << 6;  // 临时：使用 MAT 计算提前角（左移 6 位）
//	unsigned short intendedDwell = intendedAdvance >> 1;  // 临时：闭合角 = 提前角 / 2
//
//	short c;
//	for(c=0;c<IGNITION_CHANNELS;c++){
//		ignitionAdvances[IGNITION_CHANNELS] = intendedAdvance;  // 临时：所有通道使用相同的提前角
//	}
//	*currentDwellMath = intendedDwell;  // 设置当前闭合角
//
////	unsigned short minPeriod = ignitionMinimumDwell << 1;  // 已注释：最小周期
//	//	if(intendedDwell < ignitionMinimumDwell){  // 已注释：闭合角边界检查
////		dwellLength = ignitionMinimumDwell;
////	}else{
////		dwellLength = intendedDwell;
////	}
////	if(intendedPeriod < minPeriod){  // 已注释：周期边界检查
////		dwellPeriod = minPeriod;
////	}else{
////		dwellPeriod = intendedPeriod;
////	}
////	PITLD0 = dwellPeriod;  // 已注释：设置 PIT 定时器
//
//	/** @todo TODO 计算燃油提前角（六个） */
//	// 现在只对所有通道使用一个...
//	totalAngleAfterReferenceInjection = (ADCArrays->TPS << 6);  // 临时：使用 TPS 计算参考喷射后的总角度
//
//	/** @todo TODO 计算闭合周期（一个） */
//
//	/** @todo TODO 计算点火提前角（十二个） */
//
//	/*&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& 临时代码结束 &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&*/
//}
