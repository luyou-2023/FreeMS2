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


/** @file staticInit.c
 *
 * @brief Static initialisation of non-zero variables
 *
 * This file contains static initialisations for fields that require a non-zero
 * initial value after reset. Zero fields are taken care of by GCC and doing
 * this here means less init time and init code, both good things. Variables
 * initialised here are placed together by the compiler in flash and copied up
 * to RAM as a linear block before the main method runs. This is significantly
 * more efficient than doing them one-by-one in an init routine.
 *
 * @author Fred Cooke
 */


#include "inc/FreeMS2.h"


/** @brief 转速表周期（静态初始化）
 *
 * 转速表输出周期，初始化为最大值（65535），表示最低 RPM（发动机未运行）。
 * 当发动机未运行时，转速表应显示最低值，避免显示错误的转速。
 * 
 * @details 为什么需要非零初始值：
 * - 65535 表示最大周期 = 最低 RPM
 * - 确保发动机未运行时转速表显示正确
 * - 避免未初始化变量导致的随机值
 */
unsigned short tachoPeriod = 65535;	/* 启动时的最低 RPM */

/** @brief 主脉宽（静态初始化，测试值）
 *
 * 主喷油脉宽，用于测试和调试。当前设置为 10（单位取决于系统配置）。
 * 这是临时测试值，实际值应该从燃油计算函数中获取。
 */
unsigned short masterPulseWidth = 10;  // 测试值

/** @brief 参考点火后的总角度（静态初始化，测试值）
 *
 * 从参考点（如 TDC）到点火事件的总角度，用于测试和调试。
 * 当前设置为 540 度（1.5 圈），这是临时测试值。
 */
unsigned short totalAngleAfterReferenceIgnition = 540;  // 测试值

/** @brief 参考喷油后的总角度（静态初始化，测试值）
 *
 * 从参考点（如 TDC）到喷油事件的总角度，用于测试和调试。
 * 当前设置为 180 度（0.5 圈），这是临时测试值。
 */
unsigned short totalAngleAfterReferenceInjection = 180;  // 测试值

	/* Setup the pointers to the registers for fueling use, this does NOT work if done in global.c, I still don't know why. */
//	injectorMainTimeRegisters[0] = TC2_ADDR;
//	injectorMainTimeRegisters[1] = TC3_ADDR;
//	injectorMainTimeRegisters[2] = TC4_ADDR;
//	injectorMainTimeRegisters[3] = TC5_ADDR;
//	injectorMainTimeRegisters[4] = TC6_ADDR;
//	injectorMainTimeRegisters[5] = TC7_ADDR;
//	injectorMainControlRegisters[0] = TCTL2_ADDR;
//	injectorMainControlRegisters[1] = TCTL2_ADDR;
//	injectorMainControlRegisters[2] = TCTL1_ADDR;
//	injectorMainControlRegisters[3] = TCTL1_ADDR;
//	injectorMainControlRegisters[4] = TCTL1_ADDR;
//	injectorMainControlRegisters[5] = TCTL1_ADDR;

	// TODO perhaps read from the ds1302 once at start up and init the values or different ones with the actual time and date then update them in RTI

/** @brief 发动机循环周期（静态初始化）
 *
 * 发动机完整循环（720 度）的周期，以定时器计数为单位。
 * 初始化为 1 RPM 对应的周期值（最大值），表示发动机未运行。
 * 
 * @details 为什么需要非零初始值：
 * - ticksPerCycleAtOneRPM 是 1 RPM 对应的周期值（非常大的值）
 * - 确保发动机未运行时周期值正确（表示最低 RPM）
 * - 避免未初始化变量导致的随机值
 * - 用于 RPM 计算：RPM = ticksPerCycleAtOneRPM / engineCyclePeriod
 * 
 * 设置使得当发动机未运行时转速表读取低值
 */
unsigned long engineCyclePeriod = ticksPerCycleAtOneRPM;
