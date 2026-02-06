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


/** @brief 安全地相加两个无符号短整型
 *
 * 执行两个无符号短整型的加法运算，防止溢出。如果结果会溢出，则返回最大值。
 * 
 * @details 为什么需要这个方法：
 * ECU 中经常需要进行脉宽、时间等值的累加计算。如果直接使用普通加法，
 * 溢出会导致结果回绕到小值，造成喷油时间错误等严重问题。此函数确保即使
 * 溢出也返回一个合理的最大值，避免系统异常。
 *
 * @param addend1 第一个加数（无符号短整型，0-65535）
 * @param addend2 第二个加数（无符号短整型，0-65535）
 * 
 * @return 如果不会溢出，返回 addend1 + addend2；如果会溢出，返回 SHORTMAX (65535)
 *
 * @author Fred Cooke
 */
unsigned short safeAdd(unsigned short addend1, unsigned short addend2){
	// 检查加法是否会溢出：如果 (最大值 - 加数1) > 加数2，则不会溢出
	if((SHORTMAX - addend1) > addend2){
		return addend1 + addend2;  // 安全，返回正常和
	}else{
		return SHORTMAX;  // 溢出，返回最大值
	}
}


/** @brief 安全地应用有符号修正值到无符号值
 *
 * 将一个有符号的修正值（可以是正数或负数）应用到无符号基值上，防止溢出和下溢。
 * 用于应用各种修正（如瞬态燃油修正、温度修正等），这些修正可能是正值（增加）或负值（减少）。
 * 
 * @details 为什么需要这个方法：
 * ECU 计算中经常需要应用修正值，例如：
 * - 瞬态燃油修正（TFC）：可能是正值（加速时增加燃油）或负值（减速时减少燃油）
 * - 温度修正：根据发动机温度调整燃油量
 * 直接使用普通加减法可能导致下溢（结果小于0）或溢出（结果超过最大值），
 * 此函数确保结果始终在有效范围内。
 *
 * @param addend1 基值（无符号短整型，0-65535），例如基础脉宽
 * @param addend2 修正值（有符号短整型，-32768 到 32767），正值表示增加，负值表示减少
 * 
 * @return 如果修正后不会溢出/下溢，返回 addend1 + addend2；
 *         如果会下溢（结果 < 0），返回 0；
 *         如果会溢出（结果 > 65535），返回 SHORTMAX (65535)
 *
 * @author Fred Cooke
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


/** @brief 安全地按百分比缩放值，防止溢出
 *
 * 将基值按百分比缩放，缩放因子使用定点数表示（0x8000 = 100%）。
 * 使用 32 位中间结果进行计算以避免溢出，并检测最终结果是否溢出。
 * 
 * @details 为什么需要这个方法：
 * ECU 中经常需要按百分比调整值，例如：
 * - 发动机温度修正（ETE）：按百分比调整燃油量（如 120% = 增加 20%）
 * - 每缸修正（PCFC）：按百分比微调各缸燃油量
 * - Lambda 修正：按百分比调整目标空燃比
 * 直接使用普通乘法可能导致中间结果溢出，此函数使用 32 位中间结果
 * 并检测最终溢出，确保结果正确。
 *
 * @param baseValue 要缩放的基值（无符号短整型，0-65535），例如基础脉宽
 * @param scaler 缩放因子（无符号短整型，0-65535），使用定点数表示：
 *               - 0x0000 (0) = 0%
 *               - 0x8000 (32768) = 100%
 *               - 0xFFFF (65535) = 200%
 * 
 * @return 如果不会溢出，返回 baseValue * (scaler / 0x8000)；
 *         如果会溢出，返回 SHORTMAX (65535)
 *
 * @author Fred Cooke
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


/** @brief 重置系统到非运行状态
 *
 * 将所有重要的运行变量重置为发动机停止时的安全状态。
 * 在发动机停止、同步丢失或系统复位时调用，确保系统处于已知的安全状态。
 * 
 * @details 为什么需要这个方法：
 * 当发动机停止或失去同步时，系统必须重置所有运行相关的变量，否则：
 * - 旧的 RPM 值可能导致错误的计算
 * - 同步标志可能导致系统认为仍在运行
 * - 旧的周期值可能导致调度错误
 * 此函数确保系统在重新启动时从干净的状态开始。
 *
 * @return 无返回值
 *
 * @author Fred Cooke
 * 
 * @todo TODO 更新此函数，添加更多需要重置的变量，或找到更好的实现方式
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


/** @brief 调整 PWM 输出占空比
 *
 * 根据 ADC 输入值设置 PWM 通道的占空比，用于控制各种执行器。
 * 当前实现将 ADC 值右移 2 位（除以 4）作为占空比，这是一个简单的演示实现。
 * 
 * @details 为什么需要这个方法：
 * ECU 需要控制各种执行器，例如：
 * - 怠速控制阀（IAC）：通过 PWM 控制开度
 * - 燃油泵：通过 PWM 控制转速
 * - 其他需要模拟量控制的设备
 * ADC 值（0-1023）需要转换为 PWM 占空比（0-255），此函数执行此转换。
 * 
 * @note 当前实现是演示性的，实际应用中可能需要更复杂的映射关系。
 *
 * @return 无返回值
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


/** @brief 逐个读取所有 ADC 通道
 *
 * 按名称逐个读取 ATD0 模块的所有 8 个 ADC 通道，将原始 ADC 值存储到结构体中。
 * 这是最直观和可读的 ADC 采样方法，每个通道都有明确的名称。
 * 
 * @details 为什么需要这个方法：
 * ECU 需要定期采样各种传感器：
 * - 温度传感器（IAT, CHT, MAT）
 * - 压力传感器（MAP, AAP）
 * - 位置传感器（TPS）
 * - 氧传感器（EGO）
 * - 电压传感器（BRV）
 * 这些传感器连接到不同的 ADC 通道，需要统一采样并存储到结构体中供后续处理。
 * 此方法按名称读取，代码清晰易懂，但执行时间稍长。
 *
 * @param Arrays 指向 ADCArray 结构体的指针，用于存储所有 ADC 通道的原始值
 * 
 * @return 无返回值，ADC 值直接写入 Arrays 指向的结构体
 *
 * @author Fred Cooke
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


/** @brief 使用循环和指针批量读取 ADC 通道
 *
 * 使用循环和指针算术批量读取所有 ADC 通道，比逐个读取更高效。
 * 通过计算地址偏移直接访问 ADC 寄存器，减少代码大小和执行时间。
 * 
 * @details 为什么需要这个方法：
 * 相比 sampleEachADC()，此方法使用循环和指针，具有以下优势：
 * - 代码更紧凑（循环 vs 8 行独立赋值）
 * - 执行时间更短（循环开销小于多次独立访问）
 * - 易于扩展（增加通道只需修改循环次数）
 * 适用于对性能要求较高的场景，例如在中断服务程序中采样。
 *
 * @param Arrays 指向 ADCArray 结构体的指针，用于存储所有 ADC 通道的原始值
 * 
 * @return 无返回值，ADC 值直接写入 Arrays 指向的结构体
 *
 * @author Fred Cooke
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


/** @brief 延迟指定的毫秒数
 *
 * 通过嵌套循环实现毫秒级延迟，用于需要精确时间控制的场景。
 * 延迟时间基于系统时钟频率计算，在 80MHz 系统时钟下，内层循环约 5714 次对应 1 毫秒。
 * 
 * @details 为什么需要这个方法：
 * ECU 中有时需要精确的延迟，例如：
 * - 初始化时的硬件稳定时间
 * - 通信协议中的时序要求
 * - 调试时的观察窗口
 * 由于嵌入式系统通常不使用操作系统提供的 sleep()，需要自己实现。
 * 此方法使用忙等待（busy-wait），会占用 CPU，但延迟精确。
 * 
 * @note 延迟时间是近似的，实际时间取决于系统时钟频率和编译器优化
 *
 * @param ms 要延迟的毫秒数（无符号短整型，0-65535）
 * 
 * @return 无返回值
 *
 * @author Fred Cooke
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


/** @brief 延迟指定的微秒数
 *
 * 通过嵌套循环实现微秒级延迟，用于需要非常精确时间控制的场景。
 * 延迟时间非常近似，内层循环约 6 次对应 1 微秒（在 80MHz 系统时钟下）。
 * 
 * @details 为什么需要这个方法：
 * ECU 中有时需要微秒级延迟，例如：
 * - 硬件接口的时序要求（如 SPI、I2C）
 * - 精确的脉冲生成
 * - 调试时的精细观察
 * 相比毫秒级延迟，微秒级延迟需要更精确的循环计数。
 * 
 * @note 延迟时间非常近似，实际时间取决于系统时钟频率和编译器优化
 *
 * @param us 要延迟的微秒数（无符号短整型，0-65535）
 * 
 * @return 无返回值
 *
 * @author Fred Cooke
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


/** @brief 计算数据块的简单累加校验和
 *
 * 对指定长度的内存块计算累加校验和，用于数据完整性验证。
 * 校验和是所有字节的简单累加，溢出部分被丢弃（只保留低 8 位）。
 * 
 * @details 为什么需要这个方法：
 * ECU 通信协议中需要验证数据完整性，防止：
 * - 传输错误（噪声、干扰）
 * - 数据损坏（内存错误）
 * - 恶意篡改（安全考虑）
 * 虽然累加校验和不是最强的校验方法，但计算简单快速，适合实时系统。
 * 接收方可以重新计算校验和并与接收到的校验和比较，如果不匹配则丢弃数据包。
 *
 * @param block 指向要计算校验和的内存区域的指针
 * @param length 要计算校验和的内存区域的大小（字节数）
 * 
 * @return 8 位校验和值（0-255），是所有字节的累加和（溢出部分丢弃）
 *
 * @author Fred Cooke
 */
unsigned char checksum(unsigned char *block, unsigned short length){
	unsigned char sum = 0;  // 初始化校验和为 0
	// 遍历数据块，累加所有字节
	while (length-- > 0){
		sum += *block++;  // 累加当前字节，然后指针递增
	}
	return sum;  // 返回简单累加校验和
}


/** @brief 自定义字符串复制函数
 *
 * 复制以 null 结尾的字符串从源地址到目标地址，并返回复制的字符串长度。
 * 这是标准库 strcpy() 的自定义实现，因为某些原因标准库函数无法编译。
 * 
 * @details 为什么需要这个方法：
 * ECU 中有时需要处理字符串，例如：
 * - 版本信息字符串
 * - 调试信息输出
 * - 配置参数名称
 * 由于嵌入式环境的限制，标准库可能不可用或有问题，需要自己实现。
 * 此函数还返回字符串长度，比标准 strcpy() 更实用。
 *
 * @param dest 目标缓冲区指针，用于存储复制的字符串（必须足够大）
 * @param source 源字符串指针，要复制的以 null 结尾的字符串
 * 
 * @return 复制的字符串长度（不包括 null 结束符），如果源字符串为空则返回 0
 *
 * @author Fred Cooke
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

/** @brief 比较两个内存块是否相同
 *
 * 逐字节比较两个内存块的内容，如果发现差异则返回差异位置。
 * 用于验证数据完整性，例如验证 Flash 写入是否成功。
 * 
 * @details 为什么需要这个方法：
 * ECU 中经常需要验证数据是否正确，例如：
 * - Flash 写入验证：写入后读取并比较，确保写入成功
 * - 配置验证：验证从外部加载的配置是否正确
 * - 通信数据验证：验证接收到的数据是否与发送的一致
 * 标准库的 memcmp() 只返回是否相同，此函数返回差异位置，更便于调试和错误处理。
 *
 * @param original 原始数据块的指针，作为比较的基准
 * @param toCheck 要检查的数据块的指针，与原始数据比较
 * @param length 要比较的数据块大小（字节数）
 * 
 * @return 如果两个数据块完全相同，返回 0；
 *         如果发现差异，返回基于 1 的索引（1 = 第一个字节不同，2 = 第二个字节不同，以此类推）
 * 
 * @note 如果数据块末尾有坏数据，且前面都匹配，会返回一个正数结果
 *
 * @author Fred Cooke
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
