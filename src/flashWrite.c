/* FreeMS2 - the open source engine management system
 *
 * Copyright 2008, 2009 Sean Keys, Fred Cooke
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


/**	@file flashWrite.c
 *
 * @brief Flash manipulation functions
 *
 * This file contains all functions that operate directly or indirectly and
 * only on flash memory. They are used for erasing data from and reprogramming
 * data to the embedded flash non-volatile storage area.
 *
 * @author Sean Keys, Fred Cooke
 */


#define FLASHWRITE_C
#include "inc/FreeMS2.h"
#include "inc/utils.h"
#include "inc/flashWrite.h"
#include "inc/flashBurn.h"
#include "inc/commsISRs.h"
#include "inc/commsCore.h"
#include <string.h>


/** @brief 擦除 Flash 内存的一个扇区
 *
 * 擦除 Flash 中的一个 1KB 扇区。在要擦除的起始扇区写入 0xFFFF（无论写入什么值，
 * 擦除后都是 0xFFFF），注册 Flash 扇区擦除命令（0x40）并调用 StackBurner()。
 * 如果尝试擦除受保护的扇区，FSTAT 寄存器中会出现 PVIOL 错误。
 * 
 * @details 为什么需要这个方法：
 * Flash 写入前必须先擦除（Flash 只能从 1 变为 0，不能从 0 变为 1）。
 * 擦除操作以扇区为单位（1KB），用于：
 * - 实时调参：修改查找表前需要擦除对应扇区
 * - 固件更新：更新固件前需要擦除 Flash 区域
 * - 配置修改：修改配置参数前需要擦除
 * 
 * 擦除过程：
 * 1. 验证地址对齐：地址必须是扇区大小的倍数
 * 2. 切换到目标 Flash 页面
 * 3. 清除错误标志（PVIOL、ACCERR）
 * 4. 写入虚拟数据到扇区起始地址（触发擦除）
 * 5. 设置擦除命令（SECTOR_ERASE）
 * 6. 调用 StackBurner() 执行擦除（汇编函数，处理页面切换）
 * 
 * @warning 这将擦除从 flashAddr 开始的整个 1KB 块，该扇区内的所有数据都将丢失
 * 
 * @param PPage Flash 页面，扇区所在的页面
 * @param flashAddr 扇区的起始地址（必须是扇区对齐的）
 * 
 * @return 错误代码：0 表示成功，非零表示失败
 *         - addressNotSectorAligned: 地址不是扇区对齐的
 *         - PVIOL: 保护违反错误（尝试擦除受保护扇区）
 *         - ACCERR: 访问错误
 *
 * @author Sean Keys
 * 
 * @todo TODO 添加对 accerr 和 pviol 错误位的返回
 * @todo TODO 验证擦除是否成功，这是必要的还是由硬件处理？
 */
unsigned short eraseSector(unsigned char PPage, unsigned short *flashAddr){

	if (((unsigned short)flashAddr % flashSectorSize) != 0){
		return addressNotSectorAligned;
	}
	unsigned char currentPage = PPAGE;
	PPAGE = PPage;
	FSTAT = (PVIOL|ACCERR); /* clear any errors */
	(*flashAddr) = 0xFFFF;     /* Dummy data to first word of page to be erased it will write FFFF regardless with the erase command*/
	PPAGE = currentPage;
	FCMD = SECTOR_ERASE;            /* set the flash command register mode to ERASE */
	StackBurner();   //PPAGE loaded into Register B, PPAGE is set with Reg B in StackBurn asm file
	//TODO add return for accerr and pviol error bits

	// @todo TODO verify the erase, is this necessary or is it taken care of by the hardware??
	return 0;
}


/** @brief 将内存块写入 Flash
 *
 * 将 RAM 中的数据块写入 Flash 内存。块大小必须小于 1024 字节，或者是 1024 的精确倍数。
 * 如果小于 1024，目标地址应在单个 Flash 扇区内；如果是 1024 的倍数，目标地址应扇区对齐。
 * 
 * @details 为什么需要这个方法：
 * ECU 需要将修改后的数据（查找表、配置参数）从 RAM 写入 Flash 以持久保存。
 * Flash 写入的特殊要求：
 * - 必须先擦除再写入（Flash 只能从 1 变为 0）
 * - 写入以扇区为单位（1KB）
 * - 如果只修改扇区的一部分，需要先读取整个扇区，修改后再写回
 * 
 * 写入策略：
 * 由于 RAM 版本可能在任意位置，我们需要基于 Flash 位置进行定位。
 * 首先确保数据不跨越扇区边界，然后找到要烧录的扇区地址。
 * 还需要确定是否有 2 或 3 个内存块需要复制到缓冲区，存在三种情况：
 * 
 * 情况 1：| 来自 Flash | 来自 RAM | 来自 Flash |
 *         （需要保留扇区前后的数据）
 * 
 * 情况 2：| 来自 Flash | 来自 RAM |
 *         （需要保留扇区前的数据）
 * 
 * 情况 3：| 来自 RAM | 来自 Flash |
 *         （需要保留扇区后的数据）
 * 
 * 写入过程：
 * 1. 验证块大小和地址对齐
 * 2. 如果需要，擦除目标扇区
 * 3. 将需要保留的 Flash 数据复制到缓冲区
 * 4. 将 RAM 数据复制到缓冲区的正确位置
 * 5. 将整个缓冲区写入 Flash 扇区
 * 
 * @warning 每次写入限制为 63KB（显然）
 * 
 * @param details 包含要读取的 RAM 地址和页面、要烧录到的 Flash 地址和页面、要读取的大小
 * @param buffer 指向至少 1024 字节长的 RAM 块的指针，用于允许独立烧录小块数据
 * 
 * @return 错误代码：0 表示成功，非零表示失败
 *         - sizeOfBlockToBurnIsZero: 要烧录的块大小为 0
 *         - 其他错误代码（地址对齐、扇区边界等）
 *
 * @author Fred Cooke
 */
unsigned short writeBlock(blockDetails* details, void* buffer){
	unsigned char sectors;
	unsigned char RAMPage;
	/* FlashPage is always the one provided and is just used as is. */
	unsigned short* RAMAddress;
	unsigned short* FlashAddress;

	/* Check that the size isn't zero... */
	if(details->size == 0){
		return sizeOfBlockToBurnIsZero;
	}else if(details->size < 1024){
		unsigned short chunkFlashAddress = (unsigned short)details->FlashAddress;
		/* Get the offset from the start of the sector */
		unsigned short offset = chunkFlashAddress % flashSectorSize;

		/* Check for flash sector boundary crossing */
		if((offset + details->size) > 1024){
			return smallBlockCrossesSectorBoundary;
		}

		/* Configure the final burn variables */
		sectors = 1; /* By definition if we are in this code there is only one */
		RAMPage = 0x00; /// this is bogus FIXME TODO @todo /* The buffer is always in linear RAM region */
		RAMAddress = buffer; /* Save the buffer start address */
		FlashAddress = (unsigned short*)(chunkFlashAddress - offset); /* Get the start of the flash sector */

		/* Possibly three parts to copy to the buffer, copy only what is required */

		/* Save the PPAGE value and set the flash page */
		unsigned char oldFlashPage = PPAGE;
		PPAGE = details->FlashPage;

		/* If the chunk doesn't start at the beginning of the sector, copy the first area from flash */
		if(offset != 0){
			memcpy(buffer, FlashAddress, offset);
			buffer += offset;
		}

		/* Copy the middle section up regardless */
//		unsigned char oldRAMPage = RPAGE;
//		RPAGE = details->RAMPage;
		memcpy(buffer, details->RAMAddress, details->size);
		buffer += details->size;
//		RPAGE = oldRAMPage;

		/* If the chunk doesn't end at the end of the sector, copy the last are from flash */
		if((offset + details->size) < 1024){
			void* chunkFlashEndAddress = details->FlashAddress + details->size;
			memcpy(buffer, chunkFlashEndAddress, (1024 - (offset + details->size)));
		}

		/* Restore the PPAGE value back */
		PPAGE = oldFlashPage;
	} else {
		/* If not smaller than 1024, check size is product of sector size */
		if((details->size % flashSectorSize) != 0){
			return sizeNotMultipleOfSectorSize;
		}

		/* Set the variables to what they would have been before */
		sectors = details->size / flashSectorSize;
		RAMPage = details->RAMPage;
		RAMAddress = (unsigned short*)details->RAMAddress;
		FlashAddress = (unsigned short*)details->FlashAddress;
	}

	unsigned char i;
	for(i=0;i<sectors;i++){
		unsigned short errorID = writeSector(RAMPage, RAMAddress, details->FlashPage, FlashAddress);
		if(errorID != 0){
			return errorID;
		}
		/* Incrementing a pointer is done by blocks the size of the type, hence 512 per sector here */
		RAMAddress += flashSectorSizeInWords;
		FlashAddress += flashSectorSizeInWords;
	}
	// @todo TODO verify the write? necessary??
	return 0;
}


/** @brief 将内存扇区写入 Flash 扇区
 *
 * 使用 writeWord 将一个 1KB 块从源地址（RAM）逐字写入 Flash 目标地址。
 * 提供起始内存地址和目标 Flash 地址，每次成功写入一个字后，两个地址都会递增 1 个字，
 * 直到整个 1024 字节扇区写入完成。在写入之前，调用 eraseSector 确保目标区域为空。
 * 
 * @details 为什么需要这个方法：
 * Flash 写入必须以扇区为单位，且必须先擦除再写入。此函数：
 * 1. 验证目标地址是扇区对齐的
 * 2. 验证目标地址在 Flash 区域内（不在 RAM 或寄存器区域）
 * 3. 擦除目标扇区（确保可以写入）
 * 4. 逐字写入整个扇区（512 个字 = 1024 字节）
 * 
 * 写入过程：
 * - 切换到目标 Flash 页面
 * - 循环 512 次，每次写入一个字（2 字节）
 * - 使用 writeWord() 执行实际的 Flash 写入操作
 * - 递增源地址和目标地址
 * 
 * @param RPage RAM 页面，RAMSourceAddress 所在的页面
 * @param RAMSourceAddress 源数据的地址（RAM 中）
 * @param PPage Flash 页面，flashDestinationAddress 所在的页面
 * @param flashDestinationAddress Flash 中要写入数据的目标地址（必须是扇区对齐的）
 * 
 * @return 错误代码：0 表示成功，非零表示失败
 *         - addressNotSectorAligned: 地址不是扇区对齐的
 *         - addressNotFlashRegion: 地址不在 Flash 区域内
 *         - writeWord() 返回的错误代码
 *
 * @author Sean Keys
 * 
 * @todo TODO 决定是否需要禁用中断，因为我们在手动设置 Flash/RAM 页面
 */
unsigned short writeSector(unsigned char RPage, unsigned short* RAMSourceAddress, unsigned char PPage , unsigned short* flashDestinationAddress){

	if(((unsigned short)flashDestinationAddress % flashSectorSize) != 0){
			return addressNotSectorAligned;
	}

	if(((unsigned short)flashDestinationAddress) < 0x4000){
		return addressNotFlashRegion;
	}

	/// @todo TODO Decide if we need to disable interrupts since we are manually setting Flash/RAM pages.
	eraseSector((unsigned char)PPage, (unsigned short*)flashDestinationAddress);  /* First Erase our destination block */

	unsigned short wordCount = flashSectorSizeInWords;

	/* Save pages */
//	unsigned char currentRPage = RPAGE;
	unsigned char currentPPage = PPAGE;

	/* Switch pages */
//	RPAGE = RPage;
	PPAGE = PPage;

	while (wordCount > 0)
	{
    	unsigned short sourceData = *RAMSourceAddress; /*Convert the RAMAddr to data(dereference) */
    	unsigned short errorID = writeWord(flashDestinationAddress, sourceData);
        if(errorID != 0){
			return errorID;
		}
		RAMSourceAddress++;
		flashDestinationAddress++;
	 	wordCount--; /* Decrement our word counter */
	}

	/* Restore pages */
//	RPAGE = currentRPage;
	PPAGE = currentPPage;
	// @todo TODO verify the write? necessary??
	return 0;
}


/** @brief Flash 编程命令 - 写入一个字到 Flash
 *
 * 将一个 16 位字写入空的（0xFFFF）Flash 地址。如果尝试写入包含数据（不是 0xFFFF）的地址，
 * FSTAT 寄存器中会记录错误。嵌入式算法的工作原理是：像写入任何其他可写地址一样写入所需的
 * Flash 地址，然后在 FCMD 寄存器中注册编程命令（0x20），其余由 StackBurner() 处理。
 * 
 * @details 为什么需要这个方法：
 * Flash 写入是底层操作，必须以字（16 位）为单位进行。此函数：
 * 1. 验证地址是字对齐的（地址必须是偶数）
 * 2. 清除错误标志（PVIOL、ACCERR）
 * 3. 将数据写入 Flash 地址（触发写入序列）
 * 4. 设置编程命令（PROGRAM）
 * 5. 调用 StackBurner() 执行实际的 Flash 写入（汇编函数，处理页面切换和时序）
 * 
 * Flash 写入要求：
 * - 目标地址必须是 0xFFFF（已擦除状态）
 * - 地址必须是字对齐的（偶数地址）
 * - 地址不能受保护（否则会触发 PVIOL 错误）
 * - 必须在正确的 Flash 页面中
 * 
 * 写入过程：
 * 1. 验证地址对齐
 * 2. 切换到目标 Flash 页面
 * 3. 清除错误标志
 * 4. 写入数据到 Flash 地址（这会触发写入序列）
 * 5. 设置编程命令
 * 6. 调用 StackBurner() 执行写入
 * 7. 恢复原始页面
 * 
 * @warning 确保目标地址不受保护，否则会在 FSTAT 中标记错误
 * 
 * @param flashDestination 要写入数据的目标 Flash 地址（必须是字对齐的）
 * @param data 要写入的数据（16 位无符号整数）
 * 
 * @return 错误代码：0 表示成功，非零表示失败
 *         - addressNotWordAligned: 地址不是字对齐的
 *         - PVIOL: 保护违反错误（尝试写入受保护地址）
 *         - ACCERR: 访问错误（目标地址不是 0xFFFF）
 *
 * @author Sean Keys
 */
unsigned short writeWord(unsigned short* flashDestination, unsigned short data){
	if((unsigned short)flashDestination & 0x0001){
		return addressNotWordAligned;
	}

	FSTAT=(ACCERR | PVIOL);
	*flashDestination = data;
	FCMD = WORD_PROGRAM;        //Load Flash Command Register With Word_Program mask
    StackBurner();

	// @todo TODO verify the write? necessary??
    return 0;
}
