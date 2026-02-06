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


/**	@file FixedConfig1.c
 * @ingroup dataInitialisers
 *
 * @brief First fixed config block
 *
 * This file contains the definition of the first fixed configuration block.
 * The declaration can be found in the global constants header file.
 *
 * Please ensure that all variables added here have good default values.
 *
 * @author Fred Cooke
 */


#include "inc/FreeMS2.h"


/// @todo TODO for coreSettingsA masks See definitions in FreeMS2.h OR is it in structs.h ???
/// @todo TODO divide fixedConfig1 into useful chunks
/// @todo TODO create presets sensor values struct
/// @todo TODO create engine setup struct
/// @todo TODO create ranges struct
/// @todo TODO add userTextField1 to the dictionary/address lookup


/** @brief 固定配置1 - 第一组固定配置数据
 *
 * 此结构体包含第一组固定配置数据，这些数据在运行时通常不会改变。
 * 包括发动机参数、通信设置、转速表设置等。
 */
const volatile fixedConfig1 fixedConfigs1 FIXEDCONF1 = {

		{  // engineSettings - 发动机设置结构体
		typicalCylinderSize,    	/* perCylinderVolume - 每缸容积：单个气缸的容积（单位：cc或ml） */
		stoichiometricAFROctane,	/* stoichiometricAFR - 理论空燃比：用于汽油的理论空燃比（通常为14.7:1） */
		typicalInjectorSize,    	/* injectorFlow - 喷油器流量：主喷油器的流量（单位：cc/min或g/min） */
		densityOfOctane,        	/* densityOfFuelAtSTP - 标准状态下燃油密度：用于计算燃油质量 */
		/* new and old.... */
		575,                  	/* capacityOfAirPerCombustionEvent - 每次燃烧事件的空气容量：用于计算空气流量 */
		192,                  	/* perPrimaryInjectorChannelFlowRate - 每个主喷油器通道的流量：主喷油器的流量 */
		192,                  	/* perSecondaryInjectorChannelFlowRate - 每个次喷油器通道的流量：分级喷油器的流量 */
		6,                    	/* ports - 端口数：喷油器端口数量（通常等于气缸数） */
		6,                    	/* coils - 线圈数：点火线圈数量 */
		6,                    	/* combustionEventsPerEngineCycle - 每个发动机循环的燃烧事件数：四冲程发动机通常等于气缸数 */
		2,                    	/* revolutionsPerEngineCycle - 每个发动机循环的转数：四冲程发动机为2转 */
		24,                   	/* primaryTeeth - 主齿数：曲轴位置传感器上的齿数（不包括缺齿） */
		0                    	/* missingTeeth - 缺齿数：曲轴位置传感器上缺失的齿数（例如36-1缺齿轮为1） */
		},

		{  // commsSettings - 通信设置结构体
		divisorFor115200bps,  	/* baudDivisor - 波特率分频器：用于设置串口通信波特率（115200 bps） */
		1                    	/* networkAddress - 网络地址：用于多设备通信时的设备地址 */
		},

		{  // tachoSettings - 转速表设置结构体
		tachoTickFactor4at50, 	/* tachoTickFactor - 转速表节拍因子：用于计算转速表输出频率 */
		tachoTotalFactor4at50	/* tachoTotalFactor - 转速表总因子：转速表输出的总修正因子 */
		},

		0x07F0,                 	/* coreSettingsA - 核心设置A：位标志字段，包含各种系统设置（如STAGED_ON、PRIMARY_POLARITY等） */

		{"Place your personal notes about whatever you like in here! Don't hesitate to tell us a story about something interesting. Do keep in mind though that when you upload your settings file to the forum this message WILL be visible to all and sundry, so don't be putting too many personal details, bank account numbers, passwords, PIN numbers, license plates, national insurance numbers, IRD numbers, social security numbers, phone numbers, email addresses, love stories and other private information in this field. In fact it is probably best if you keep the information stored here purely related to the vehicle that this system is installed on and relevant to the state of tune and configuration of settings. Lastly, please remember that this field WILL be shrinking in length from it's currently large size to something more reasonable in future. I would like to attempt to keep it at least thirty two characters long though, so writing that much is a non issue, but more won't be possible later!!"}  /* userTextField1 - 用户文本字段1：用于存储用户自定义的注释和说明信息，可以记录车辆信息、调校状态等 */
};
