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


/**	@file FixedConfig2.c
 * @ingroup dataInitialisers
 *
 * @brief Second fixed config block
 *
 * This file contains the definition of the second fixed configuration block.
 * The declaration can be found in the global constants header file.
 *
 * Please ensure that all variables added here have good default values.
 *
 * @author Fred Cooke
 */


#include "inc/FreeMS2.h"


/// @todo TODO divide fixedConfig2 into useful chunks
/// @todo TODO create engine hardware config chunk
/// @todo TODO create random stuff chunk
/// @todo TODO add the userTextField2 to the dictionary/address lookup


/** @brief 固定配置2 - 第二组固定配置数据
 *
 * 此结构体包含第二组固定配置数据，这些数据在运行时通常不会改变。
 * 包括传感器预设值、传感器范围、超时设置等。
 */
const volatile fixedConfig2 fixedConfigs2 FIXEDCONF2 = {
		{  // sensorPresets - 传感器预设值结构体：用于测试或默认值
		roomTemperature,        	/* presetIAT - 预设进气温度：用于测试或默认值（单位：开尔文×100） */
		runningTemperature,     	/* presetCHT - 预设缸盖温度：用于测试或默认值（单位：开尔文×100） */
		halfThrottle,           	/* presetTPS - 预设节气门位置：用于测试或默认值（单位：百分比×100） */
		stoichiometricLambda,   	/* presetEGO - 预设氧传感器1值：用于测试或默认值（理论空燃比） */
		runningVoltage,         	/* presetBRV - 预设电池参考电压：用于测试或默认值（单位：伏特×100） */
		idleManifoldPressure,   	/* presetMAP - 预设歧管压力：用于测试或默认值（单位：kPa×100） */
		seaLevelKPa,            	/* presetAAP - 预设大气压力：用于测试或默认值（海平面标准大气压，单位：kPa×100） */
		roomTemperature,        	/* presetMAT - 预设歧管温度：用于测试或默认值（单位：开尔文×100） */
		stoichiometricLambda,   	/* presetEGO2 - 预设氧传感器2值：用于测试或默认值（理论空燃比） */
		maxExpectedBoost,       	/* presetIAP - 预设中冷器压力：用于测试或默认值（最大预期增压，单位：kPa×100） */
		idlePulseWidth,         	/* presetBPW - 预设基础脉宽：用于测试或默认值（怠速时的喷油脉宽，单位：定时器计数） */
		idleAirFlow          		/* presetAF - 预设空气流量：用于测试或默认值（怠速时的空气流量） */
		},

		{  // sensorRanges - 传感器范围结构体：定义各传感器的ADC范围和物理范围
		offIdleMAP,         		/* TPSClosedMAP - 节气门关闭时的MAP值：用于判断节气门是否关闭（单位：kPa×100） */
		nearlyWOTMAP,       		/* TPSOpenMAP - 节气门接近全开时的MAP值：用于判断节气门是否接近全开（单位：kPa×100） */
		MPX4250AMin,        		/* MAPMinimum - MAP传感器最小ADC值：MAP传感器的最小ADC读数 */
		MPX4250ARange,      		/* MAPRange - MAP传感器ADC范围：MAP传感器的ADC读数范围 */
		MPX4100AMin,        		/* AAPMinimum - AAP传感器最小ADC值：大气压力传感器的最小ADC读数 */
		MPX4100ARange,      		/* AAPRange - AAP传感器ADC范围：大气压力传感器的ADC读数范围 */
		LC1LambdaMin,       		/* EGOMinimum - EGO传感器最小ADC值：氧传感器1的最小ADC读数 */
		LC1LambdaRange,     		/* EGORange - EGO传感器ADC范围：氧传感器1的ADC读数范围 */
		batteryVoltageMin,  		/* BRVMinimum - BRV传感器最小ADC值：电池参考电压传感器的最小ADC读数 */
		batteryVoltageRange,		/* BRVRange - BRV传感器ADC范围：电池参考电压传感器的ADC读数范围 */
		TPSDefaultMin,      		/* TPSMinimumADC - TPS传感器最小ADC值：节气门位置传感器的最小ADC读数（用于校准） */
		TPSDefaultMax       		/* TPSMaximumADC - TPS传感器最大ADC值：节气门位置传感器的最大ADC读数（用于校准） */
		},

		{  // timeoutSettings - 超时设置结构体
		500,                  	/* readingTimeout - 读取超时：ADC读取超时时间（单位：RTI周期，默认500 = 0.5秒，对应4缸发动机60 RPM） */
		                       	/* @todo TODO 新的ADC采样方法：始终异步采样ADC，如果未同步则使用异步ADC读数，否则使用同步读数。在数学计算开始时通过数组指针设置 */
		},

		{"Place your personal notes about whatever you like in here! Don't hesitate to tell us a story about something interesting. Do keep in mind though that when you upload your settings file to the forum this message WILL be visible to all and sundry, so don't be putting too many personal details, bank account numbers, passwords, PIN numbers, license plates, national insurance numbers, IRD numbers, social security numbers, phone numbers, email addresses, love stories and other private information in this field. In fact it is probably best if you keep the information stored here purely related to the vehicle that this system is installed on and relevant to the state of tune and configuration of settings. Lastly, please remember that this field WILL be shrinking in length from it's currently large size to something more reasonable in future. I would like to attempt to keep it at least thirty two characters long though, so writing that much is a non issue, but not more"}  /* userTextField2 - 用户文本字段2：用于存储用户自定义的注释和说明信息，可以记录车辆信息、调校状态等 */
};
