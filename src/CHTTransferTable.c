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


/**	@file CHTTransferTable.c
 * @ingroup dataInitialisers
 *
 * @brief Coolant/Head Temperature Transfer Table
 *
 * This file exists solely to contain the Coolant/Head Temperature thermistor
 * transfer function lookup table.
 *
 * @author Fred Cooke
 */


#include "inc/FreeMS2.h"


/** @brief 缸盖温度（CHT）转换表
 *
 * 使用此表格可以快速准确地将原始ADC读数转换为开尔文温度值。
 * 此表格用于NTC（负温度系数）热敏电阻的温度转换。
 * 表格大小为1024，对应10位ADC的完整范围（0-1023）。
 *
 * @details 转换原理：
 * - 热敏电阻的阻值随温度变化呈非线性关系
 * - 通过查找表可以快速将ADC值映射到实际温度
 * - 温度值以开尔文为单位存储（例如29315表示20°C = 293.15K）
 * - CHT（Coolant/Head Temperature）用于监测发动机冷却液或缸盖温度
 *
 * @author FreeTherm
 */
const volatile unsigned short CHTTransferTable[1024] LOOKUPD = {
#include "data/thermistors/Bosch.h"  // 包含Bosch热敏电阻的转换表数据（1024个值）
};
