/* FreeMS2 - the open source engine management system
 *
 * Copyright 2008, 2009, 2010 Fred Cooke, Philip L Johnson
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


/**	@file utils.c
 *
 * @brief Utility functions only
 *
 * General purpose utility functions that are used in various places throughout
 * the code base. Functions should only be placed here if they are not strongly
 * related to any other set of functionality.
 *
 * @author Fred Cooke
 */


#define UTILS_C
#include "inc/FreeMS2.h"
#include "inc/commsISRs.h"
#include "inc/utils.h"
#include <string.h>


/** @brief Add two unsigned shorts safely
 *
 * This will either return short max or the sum of the two arguments.
 *
 * @author Fred Cooke
 *
 * @param addend1
 * @param addend2
 */
unsigned short safeAdd(unsigned short addend1, unsigned short addend2){
	// 检查加法是否会溢出：如果 (最大值 - 加数1) > 加数2，则不会溢出
	if((SHORTMAX - addend1) > addend2){
		return addend1 + addend2;  // 安全，返回正常和
	}else{
		return SHORTMAX;  // 溢出，返回最大值
	}
}


/** @brief Add two unsigned shorts safely
 *
 * This will either return short max or the sum of the two arguments.
 *
 * @author Fred Cooke
 *
 * @param addend1
 * @param addend2
 */
unsigned short safeTrim(unsigned short addend1, signed short addend2){
	// 处理负数修正（减法）
	if(addend2 < 0){
		// 检查下溢：如果被减数大于减数的绝对值，则不会下溢
		if(addend1 > -addend2){
			return addend1 + addend2;  // 安全，返回差值（注意 addend2 是负数）
		}else{
			return 0;  // 下溢，返回最小值 0
		}
	}else if(addend2 > 0){
		// 处理正数修正（加法）
		// 检查溢出：如果修正值小于 (最大值 - 被加数)，则不会溢出
		if(addend2 < (SHORTMAX - addend1)){
			return addend1 + addend2;  // 安全，返回和
		}else{
			return SHORTMAX;  // 溢出，返回最大值
		}
	}else{
		// 修正值为 0，直接返回原值
		return addend1;
	}
}


/** @brief Scale without overflow
 *
 * Takes a base value and a scaler where 0x8000/32768 means 100%, 0 means 0%
 * and 0xFFFF/65535 means 200%, and returns the baseValue multiplied, in effect, by the
 * resulting percentage figure.
 *
 * @author Fred Cooke
 *
 * @param baseValue
 * @param scaler
 */
unsigned short safeScale(unsigned short baseValue, unsigned short scaler){
	/* 执行缩放计算：baseValue * scaler / 0x8000
	 * scaler: 0x8000 = 100%, 0 = 0%, 0xFFFF = 200%
	 * 使用 32 位中间结果避免溢出 */
	unsigned short scaled = ((unsigned long)baseValue * scaler) / SHORTHALF;

	/* 溢出检测：
	 * 如果缩放因子大于 100% (SHORTHALF)，则缩放后的值必须大于原值
	 * 如果缩放因子小于 100%，则不可能溢出
	 * 如果缩放因子大于 100% 但缩放后的值小于原值，说明发生了溢出 */
	if((scaler > SHORTHALF) && (baseValue > scaled)){
		return SHORTMAX;  // 溢出，返回最大值
	}else{
		return scaled;  // 正常，返回缩放后的值
	}
}


/** @brief Setup tune switching
 *
 * Place the correct set of tables in RAM based on a boolean parameter
 *
 * @todo TODO change parameter style to be a pointer to a register and a mask?
 *
 * @author Fred Cooke
 *
 * @param bool which set of data to enable.
 */
//void setupPagedRAM(unsigned char bool){
//	if(bool){
//		currentFuelRPage = RPAGE_FUEL_ONE;
//		currentTimeRPage = RPAGE_TIME_ONE;
//		currentTuneRPage = RPAGE_TUNE_ONE;
//	}else{
//		currentFuelRPage = RPAGE_FUEL_TWO;
//		currentTimeRPage = RPAGE_TIME_TWO;
//		currentTuneRPage = RPAGE_TUNE_TWO;
//	}
//
////	RPAGE = currentTuneRPage;
//}


/** @brief Reset key state
 *
 * Reset all important variables to their non-running state.
 *
 * @todo TODO bring this up to date and/or find a better way to do it.
 *
 * @author Fred Cooke
 */
void resetToNonRunningState(){
	/* 将 RPM 重置为零 */
	RPM0 = 0;  // 重置 RPM 缓冲区 0
	RPM1 = 0;  // 重置 RPM 缓冲区 1

	/* 确保转速表读取最低可能值
	 * 将发动机周期设置为 1 RPM 对应的周期值（最大值） */
	engineCyclePeriod = ticksPerCycleAtOneRPM;

	/* 清除所有同步标志，设置为丢失同步状态 */
	//coreStatusA &= CLEAR_RPM_VALID;  // 已注释：清除 RPM 有效标志
	coreStatusA &= CLEAR_PRIMARY_SYNC;  // 清除主同步标志
	//coreStatusA &= CLEAR_SECONDARY_SYNC;  // 已注释：清除次同步标志

	// TODO 这里还需要重置更多内容，但只重置关键内容
}


/** @brief Demonstrate PWM
 *
 * Demonstrate basic PWM module usage by setting duty to scaled ADC inputs.
 *
 * @author Fred Cooke
 */
void adjustPWM(){
	// 将原始 ADC 值缩放到占空比（右移 2 位，相当于除以 4）
	// 通道 0: 将 ADC 通道 0 的值转换为 PWM 通道 0 的占空比
	PWMDTY0 = ATD0DR0 >> 2; // 缩放原始 ADC 到占空比
	// 通道 1: 将 ADC 通道 1 的值转换为 PWM 通道 1 的占空比
	PWMDTY1 = ATD0DR1 >> 2; // 缩放原始 ADC 到占空比
	// 通道 2: 将 ADC 通道 2 的值转换为 PWM 通道 2 的占空比
	PWMDTY2 = ATD0DR2 >> 2; // 缩放原始 ADC 到占空比
	// 通道 3: 将 ADC 通道 3 的值转换为 PWM 通道 3 的占空比
	PWMDTY3 = ATD0DR3 >> 2; // 缩放原始 ADC 到占空比
	// 通道 4: 将 ADC 通道 4 的值转换为 PWM 通道 4 的占空比
	PWMDTY4 = ATD0DR4 >> 2; // 缩放原始 ADC 到占空比
	// 通道 5: 将 ADC 通道 5 的值转换为 PWM 通道 5 的占空比
	PWMDTY5 = ATD0DR5 >> 2; // 缩放原始 ADC 到占空比
}


/** @brief Read ADCs one at a time
 *
 * Read ADCs into the correct bank one at a time by name.
 *
 * @author Fred Cooke
 *
 * @param Arrays a pointer to an ADCArray struct to store ADC values in.
 */
void sampleEachADC(ADCArray *Arrays){
	/* 读取 ATD0 模块的所有 ADC 通道
	 * 按名称逐个读取，确保数据一致性 */
	Arrays->IAT = ATD0DR0;  // 通道 0: 进气温度 (IAT)
	Arrays->CHT = ATD0DR1;  // 通道 1: 缸盖温度 (CHT)
	Arrays->TPS = ATD0DR2;  // 通道 2: 节气门位置 (TPS)
	Arrays->EGO = ATD0DR3;  // 通道 3: 氧传感器 (EGO)
	Arrays->MAP = ATD0DR4;  // 通道 4: 歧管压力 (MAP)
	Arrays->AAP = ATD0DR5;  // 通道 5: 大气压力 (AAP)
	Arrays->BRV = ATD0DR6;  // 通道 6: 电池电压 (BRV)
	Arrays->MAT = ATD0DR7;  // 通道 7: 歧管温度 (MAT)
}


/** @brief Read ADCs in a loop
 *
 * Read ADCs into the correct bank in a loop using pointers.
 *
 * @author Fred Cooke
 *
 * @param Arrays a pointer to an ADCArray struct to store ADC values in.
 */
void sampleLoopADC(ADCArray *Arrays){
	// 获取 ADC 数组的地址
	unsigned short addr = (unsigned short)Arrays;

	//sendUS(addr);  // 已注释：调试输出
	unsigned char loop;
	/* 地址计算说明：
	 * (ADCArrays 结构体地址 + 缓冲区偏移(0 或结构体长度的一半) + 
	 *  特定 ADC 偏移(循环计数器 * 4) + 元素偏移(0 或 2)) =
	 * (ARRAY 块地址 + 循环计数器 * 2) */

	// 循环读取 8 个通道（每次读取 2 字节，共 16 字节）
	for(loop=0;loop<16;loop += 2){
		/* 读取第一个块（ATD0）
		 * 使用指针直接访问：将 ATD0 寄存器值复制到数组 */
		DVUSP(addr + loop) = DVUSP(ATD0_BASE + loop);
	}
}


/* @brief Read ADCs with memcpy()
 *
 * Read ADCs into the correct bank using two fixed calls to memcpy()
 *
 * @author Fred Cooke
 *
 * @param Arrays a pointer to an ADCArray struct to store ADC values in.
 *
 * @warning this will corrupt your comms if you use it... don't use it
 * @bug this will corrupt your comms if you use it... don't use it
 *
void sampleBlockADC(ADCArray *Arrays){
	memcpy(Arrays, (void*)ATD0_BASE, 16);
	memcpy(Arrays+16, (void*)ATD1_BASE, 16);
}*/


/** @brief Sleep for X milli seconds
 *
 * Run in a nested loop repeatedly for X milli seconds.
 *
 * @author Fred Cooke
 *
 * @param ms the number of milli seconds to kill
 */
void sleep(unsigned short ms){
	unsigned short j, k;
	// 外层循环：毫秒数
	for(j=0;j<ms;j++){
		// 内层循环：每个毫秒的延迟循环（约 5714 次，根据时钟频率调整）
		for(k=0;k<5714;k++){
			// 空循环，消耗 CPU 时间
		}
	}
}


/** @brief Sleep for X micro seconds
 *
 * Run in a nested loop repeatedly for X micro seconds.
 *
 * @note Very approximate...
 *
 * @author Fred Cooke
 *
 * @param us the number of micro seconds to kill
 */
void sleepMicro(unsigned short us){
	unsigned short j, k;
	// 外层循环：微秒数
	for(j=0;j<us;j++){
		// 内层循环：每个微秒的延迟循环（约 6 次，非常近似）
		for(k=0;k<6;k++){
			// 空循环，消耗 CPU 时间
		}
	}
}


/** @brief Simple checksum
 *
 * Generate a simple additive checksum for a block of data.
 *
 * @author Fred Cooke
 *
 * @param block a pointer to a memory region to checksum.
 * @param length how large the memory region to checksum is.
 *
 * @return a simple additive checksum.
 */
unsigned char checksum(unsigned char *block, unsigned short length){
	unsigned char sum = 0;  // 初始化校验和为 0
	// 遍历数据块，累加所有字节
	while (length-- > 0){
		sum += *block++;  // 累加当前字节，然后指针递增
	}
	return sum;  // 返回简单累加校验和
}


/** @brief Homebrew strcpy()
 *
 * strcpy() wouldn't compile for me for some reason so I wrote my own.
 *
 * @author Fred Cooke
 *
 * @param dest where to copy the null terminated string to.
 * @param source where to copy the null terminated string from.
 *
 * @return the length of the string copied.
 */
unsigned short stringCopy(unsigned char* dest, unsigned char* source){
	short length = -1;  // 初始化为 -1，因为会在复制前递增
	// 使用 do-while 循环，确保至少执行一次（即使源字符串为空）
	do {
		*dest++ = *source++;  // 复制当前字符，两个指针都递增
		length++;  // 长度递增
	} while(*(source-1) != 0);  // 检查刚复制的字符是否为字符串结束符 '\0'
	return (unsigned short) length;  // 返回复制的字符串长度（不包括结束符）
}

/**
 * @returns a one based index of the failure point
 *
 * @note this will return a positive result with bad data in the last position of a maximum sized block
 */
unsigned short compare(unsigned char* original, unsigned char* toCheck, unsigned short length){
	unsigned short i;
	// 逐字节比较两个内存块
	for(i=0;i<length;i++){
		// 如果发现不匹配的字节
		if(original[i] != toCheck[i]){
			return i + 1; // 返回基于 1 的索引（失败位置），0 表示成功
		}
	}
	return 0;  // 所有字节都匹配，返回 0 表示成功
}
