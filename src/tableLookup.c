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


/** @file tableLookup.c
 *
 * @brief Table access functions
 *
 * Functions for writing to and reading from all of the different table types.
 *
 * @author Fred Cooke
 */


#define TABLELOOKUP_C
#include "inc/FreeMS2.h"
#include "inc/commsISRs.h"
#include "inc/tableLookup.h"


/*&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&*/
/*&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&*/
/*&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& ******* ******* ******* &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&*/
/*&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& ******* WARNING ******* &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&*/
/*&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& ******* ******* ******* &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&*/
/*&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&*/
/*&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&*/
/*&&&&&&&&&&&&&&&&&&&&                                                               &&&&&&&&&&&&&&&&&&&*/
/*&&&&&&&&&&&&&&&&&&&& These routines rely on the fact that there are no ISRs trying &&&&&&&&&&&&&&&&&&&*/
/*&&&&&&&&&&&&&&&&&&&& to access the small tables and other live settings in the RAM &&&&&&&&&&&&&&&&&&&*/
/*&&&&&&&&&&&&&&&&&&&& window as specified by the RPAGE value. If they are then bad  &&&&&&&&&&&&&&&&&&&*/
/*&&&&&&&&&&&&&&&&&&&& values WILL be occasionally read from random parts of the big &&&&&&&&&&&&&&&&&&&*/
/*&&&&&&&&&&&&&&&&&&&& tables instead of the correct place. If that occurs it WILL   &&&&&&&&&&&&&&&&&&&*/
/*&&&&&&&&&&&&&&&&&&&& cause unpredictable and VERY hard to find bugs!!              &&&&&&&&&&&&&&&&&&&*/
/*&&&&&&&&&&&&&&&&&&&&                                                               &&&&&&&&&&&&&&&&&&&*/
/*&&&&&&&&&&&&&&&&&&&&  *******  *******  YOU HAVE BEEN WARNED!!!  *******  *******  &&&&&&&&&&&&&&&&&&&*/
/*&&&&&&&&&&&&&&&&&&&&                                                               &&&&&&&&&&&&&&&&&&&*/
/*&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&*/
/*&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&*/


/* Yet to be implemented :

unsigned char lookup8Bit3dUC(
unsigned char lookup8Bit2dUC(
signed short lookup16Bit3dSS(
signed short lookup16Bit3dSS(
signed char lookup8Bit3D( */


/** @brief 从主表格中查找值（使用双线性插值）
 *
 * 在二维主表格（如 VE 表、点火提前角表）中查找指定 RPM 和 Load 对应的值。
 * 使用双线性插值算法，如果查询点不在表格节点上，会在四个相邻节点之间插值。
 * 
 * @details 为什么需要这个方法：
 * ECU 使用查找表存储发动机调参数据，例如：
 * - VE 表（容积效率表）：根据 RPM 和 Load 查找 VE 值
 * - 点火提前角表：根据 RPM 和 Load 查找点火提前角
 * - Lambda 表：根据 RPM 和 Load 查找目标空燃比
 * 由于表格是离散的（如 19x24），而实际运行参数是连续的，需要插值来获得
 * 精确的值。双线性插值在四个相邻节点之间进行线性插值，提供平滑的过渡。
 * 
 * 插值过程：
 * 1. 在 RPM 轴上找到包含 realRPM 的区间（lowRPM, highRPM）
 * 2. 在 Load 轴上找到包含 realLoad 的区间（lowLoad, highLoad）
 * 3. 获取四个角点的值：(lowRPM, lowLoad), (lowRPM, highLoad), (highRPM, lowLoad), (highRPM, highLoad)
 * 4. 先在 Load 方向插值，得到两个值：lowRPMIntLoad, highRPMIntLoad
 * 5. 再在 RPM 方向插值，得到最终值
 * 
 * 表格大小：
 * 可以通过减少 RPMLength 和 LoadLength 来减小表格大小。
 * 当前不支持增加轴的大小。
 * 
 * 表格外部的值：
 * 如果查询点在表格外部，会返回最接近的边界值或角点值。
 * 这是合理的行为，避免返回无效值。
 * 
 * @warning 此函数依赖于轴值是从低到高排序的列表。如果不是这种情况，
 * 行为未定义，可能包括内存损坏和发动机损坏。
 * 
 * @warning 这些例程依赖于没有 ISR 尝试访问 RAM 窗口中的小表格和其他实时设置。
 * 如果有，将偶尔从大表格的随机部分读取错误值，导致不可预测且非常难找的 bug！
 *
 * @param Table 指向要读取的表格的指针（mainTable 结构体）
 * @param realRPM 当前 RPM 值，用于查找表格值（无符号短整型）
 * @param realLoad 当前 Load 值（如 MAP 或 TPS），用于查找表格值（无符号短整型）
 * @param RAMPage 存储表格的 RAM 页面（当前未使用，表格应在当前可见页面）
 * 
 * @return 指定位置的双线性插值结果（无符号短整型）
 * 
 * @note X/RPM 是水平方向，Y/Load 是垂直方向
 *
 * @author Fred Cooke
 */
unsigned short lookupPagedMainTableCellValue(mainTable* Table, unsigned short realRPM, unsigned short realLoad, unsigned char RAMPage){

	/* 保存 RPAGE 值以便恢复并切换页面。 */
//	unsigned char oldRPage = RPAGE;  // 已注释：保存旧页面
//	RPAGE = RAMPage;  // 已注释：切换到目标页面

	/* 查找 RPM 轴的边界值和索引 */
	unsigned char lowRPMIndex = 0;  // 低 RPM 索引，初始化为 0
	unsigned char highRPMIndex = Table->RPMLength - 1;  // 高 RPM 索引，初始化为最后一个
	/* 如果在循环中从未设置，低值将等于高值，将在映射的边缘 */
	unsigned short lowRPMValue = Table->RPM[0];  // 低 RPM 值，初始化为第一个值
	unsigned short highRPMValue = Table->RPM[Table->RPMLength -1];  // 高 RPM 值，初始化为最后一个值

	unsigned char RPMIndex;
	// 遍历 RPM 轴，查找包含 realRPM 的区间
	for(RPMIndex=0;RPMIndex<Table->RPMLength;RPMIndex++){
		if(Table->RPM[RPMIndex] < realRPM){
			// 当前值小于目标值，更新低边界
			lowRPMValue = Table->RPM[RPMIndex];
			lowRPMIndex = RPMIndex;
		}else if(Table->RPM[RPMIndex] > realRPM){
			// 当前值大于目标值，找到高边界，退出循环
			highRPMValue = Table->RPM[RPMIndex];
			highRPMIndex = RPMIndex;
			break;
		}else if(Table->RPM[RPMIndex] == realRPM){
			// 精确匹配，无需插值
			lowRPMValue = Table->RPM[RPMIndex];
			highRPMValue = Table->RPM[RPMIndex];
			lowRPMIndex = RPMIndex;
			highRPMIndex = RPMIndex;
			break;
		}
	}

	/* 查找 Load 轴的边界值和索引 */
	unsigned char lowLoadIndex = 0;  // 低 Load 索引，初始化为 0
	unsigned char highLoadIndex = Table->LoadLength -1;  // 高 Load 索引，初始化为最后一个
	/* 如果在循环中从未设置，低值将等于高值，将在映射的边缘 */
	unsigned short lowLoadValue = Table->Load[0];  // 低 Load 值，初始化为第一个值
	unsigned short highLoadValue = Table->Load[Table->LoadLength -1];  // 高 Load 值，初始化为最后一个值

	unsigned char LoadIndex;
	// 遍历 Load 轴，查找包含 realLoad 的区间
	for(LoadIndex=0;LoadIndex<Table->LoadLength;LoadIndex++){
		if(Table->Load[LoadIndex] < realLoad){
			// 当前值小于目标值，更新低边界
			lowLoadValue = Table->Load[LoadIndex];
			lowLoadIndex = LoadIndex;
		}else if(Table->Load[LoadIndex] > realLoad){
			// 当前值大于目标值，找到高边界，退出循环
			highLoadValue = Table->Load[LoadIndex];
			highLoadIndex = LoadIndex;
			break;
		}else if(Table->Load[LoadIndex] == realLoad){
			// 精确匹配，无需插值
			lowLoadValue = Table->Load[LoadIndex];
			highLoadValue = Table->Load[LoadIndex];
			lowLoadIndex = LoadIndex;
			highLoadIndex = LoadIndex;
			break;
		}
	}

	/* 获取围绕目标点的四个角的值
	 * 表格布局：Table[LoadLength * RPMIndex + LoadIndex] */
	unsigned short lowRPMLowLoad = Table->Table[(Table->LoadLength * lowRPMIndex) + lowLoadIndex];  // 左下角
	unsigned short lowRPMHighLoad = Table->Table[(Table->LoadLength * lowRPMIndex) + highLoadIndex];  // 左上角
	unsigned short highRPMLowLoad = Table->Table[(Table->LoadLength * highRPMIndex) + lowLoadIndex];  // 右下角
	unsigned short highRPMHighLoad = Table->Table[(Table->LoadLength * highRPMIndex) + highLoadIndex];  // 右上角

	/* 在进行数学计算之前恢复 RAM 页面 */
//	RPAGE = oldRPage;  // 已注释：恢复旧页面

	/* 通过在 Load 方向插值找到两个边值
	 * 在低 RPM 行插值：从 lowRPMLowLoad 到 lowRPMHighLoad */
	unsigned short lowRPMIntLoad = lowRPMLowLoad + (((signed long)((signed long)lowRPMHighLoad - lowRPMLowLoad) * (realLoad - lowLoadValue))/ (highLoadValue - lowLoadValue));
	/* 在高 RPM 行插值：从 highRPMLowLoad 到 highRPMHighLoad */
	unsigned short highRPMIntLoad = highRPMLowLoad + (((signed long)((signed long)highRPMHighLoad - highRPMLowLoad) * (realLoad - lowLoadValue))/ (highLoadValue - lowLoadValue));

	/* 在两个边值之间插值（RPM 方向）并返回结果
	 * 从 lowRPMIntLoad 到 highRPMIntLoad */
	return lowRPMIntLoad + (((signed long)((signed long)highRPMIntLoad - lowRPMIntLoad) * (realRPM - lowRPMValue))/ (highRPMValue - lowRPMValue));
}


/** @brief 从二维表格中查找值（使用线性插值）
 *
 * 在一维查找表中查找指定输入值对应的输出值，使用线性插值算法。
 * 用于查找单输入单输出的映射关系，如喷油器死区时间表、温度修正表等。
 * 
 * @details 为什么需要这个方法：
 * ECU 使用一维查找表存储各种修正和映射关系，例如：
 * - 喷油器死区时间表：根据电池电压查找死区时间
 * - 发动机温度修正表：根据冷却液温度查找修正百分比
 * - 温度传感器转换表：将 ADC 值转换为实际温度
 * 由于表格是离散的（16 个点），而输入值是连续的，需要插值来获得精确的值。
 * 线性插值在两个相邻节点之间进行线性插值，提供平滑的过渡。
 * 
 * 插值过程：
 * 1. 在轴数组中找到包含 Value 的区间（lowAxisValue, highAxisValue）
 * 2. 获取对应的查找值（lowLookupValue, highLookupValue）
 * 3. 如果精确匹配，直接返回对应值
 * 4. 否则进行线性插值：lowLookupValue + (比例 * 差值)
 *
 * @param Table 指向要读取的表格的指针（twoDTableUS 结构体，固定 16 个点）
 * @param Value 输入值，用于查找对应的输出值（无符号短整型）
 * 
 * @return 指定位置的线性插值结果（无符号短整型）
 *         如果输入值在表格外部，返回最接近的边界值
 *
 * @author Fred Cooke
 */
unsigned short lookupTwoDTableUS(twoDTableUS * Table, unsigned short Value){

	/* 查找边界轴索引、轴值和查找值 */
	unsigned char lowIndex = 0;  // 低索引，初始化为 0
	unsigned char highIndex = 15;  // 高索引，初始化为 15（固定长度）
	/* 如果在循环中从未设置，低值将等于高值，将在映射的边缘 */
	unsigned short lowAxisValue = Table->Axis[0];  // 低轴值，初始化为第一个值
	unsigned short highAxisValue = Table->Axis[15];  // 高轴值，初始化为最后一个值
	unsigned short lowLookupValue = Table->Values[0];  // 低查找值，初始化为第一个值
	unsigned short highLookupValue = Table->Values[15];  // 高查找值，初始化为最后一个值

	unsigned char Index;
	// 遍历轴数组，查找包含 Value 的区间
	for(Index=0;Index<16;Index++){
		if(Table->Axis[Index] < Value){
			// 当前值小于目标值，更新低边界
			lowIndex = Index;
			lowAxisValue = Table->Axis[Index];
			lowLookupValue = Table->Values[Index];
		}else if(Table->Axis[Index] > Value){
			// 当前值大于目标值，找到高边界，退出循环
			highIndex = Index;
			highAxisValue = Table->Axis[Index];
			highLookupValue = Table->Values[Index];
			break;
		}else if(Table->Axis[Index] == Value){
			return Table->Values[Index]; // 如果正好匹配，直接返回值（无需插值）
		}
	}


	/* 插值并返回值
	 * 线性插值：lowLookupValue + (比例 * 差值) */
	return lowLookupValue + (((signed long)((signed long)highLookupValue - lowLookupValue) * (Value - lowAxisValue))/ (highAxisValue - lowAxisValue));
}


/** @brief 设置表格轴的值
 *
 * 设置表格中轴单元格的值，用于通过通信接口配置表格。
 * 在设置值之前会验证索引有效性和轴值顺序（必须递增）。
 * 
 * @details 为什么需要这个方法：
 * ECU 的调参需要通过通信接口（如串口）修改查找表。在修改轴值时必须：
 * - 验证索引在有效范围内
 * - 确保轴值保持递增顺序（查找和插值算法依赖于此）
 * 如果轴值顺序被打乱，查找函数可能返回错误值，导致发动机运行异常。
 * 此函数提供安全的轴值修改接口，确保数据完整性。
 *
 * @param index 要调整的轴单元格位置（0 到 length-1）
 * @param value 要设置的轴单元格值（无符号短整型）
 * @param axis 指向要调整的轴数组的指针
 * @param length 轴数组的长度
 * @param errorBase 错误代码的基础值，用于添加错误代码偏移
 * 
 * @return 错误代码：0 表示成功，非零表示失败
 *         - errorBase + invalidAxisIndex: 索引超出范围
 *         - errorBase + invalidAxisOrder: 轴值顺序无效（不是递增的）
 *
 * @author Fred Cooke
 */
unsigned short setAxisValue(unsigned short index, unsigned short value, unsigned short axis[], unsigned short length, unsigned short errorBase){
	// 检查索引是否超出范围
	if(index >= length){
		return errorBase + invalidAxisIndex;  // 返回错误：无效的轴索引
	}else{
		// 检查轴值顺序（必须递增）
		if(index > 0){
			/* 确保值不小于前一个值 */
			if(axis[index - 1] > value){
				return errorBase + invalidAxisOrder;  // 返回错误：轴顺序无效（小于前一个值）
			}
		}
		if(index < (length -1)){
			/* 确保值不大于后一个值 */
			if(value > axis[index + 1]){
				return errorBase + invalidAxisOrder;  // 返回错误：轴顺序无效（大于后一个值）
			}
		}
	}

	/* 如果通过了所有检查，设置值 */
	axis[index] = value;  // 设置轴值
	return 0;  // 返回成功
}


/** @brief 设置主表格单元格的值
 *
 * 设置主表格（二维表格）中指定单元格的值，用于通过通信接口配置表格。
 * 在设置值之前会验证索引有效性。
 * 
 * @details 为什么需要这个方法：
 * ECU 调参时需要修改主表格（如 VE 表、点火提前角表）中的单个单元格。
 * 通过通信接口可以实时修改表格值，无需重新烧录固件。此函数提供安全的
 * 单元格修改接口，确保索引在有效范围内，防止内存越界。
 *
 * @param RPageValue 表格所在的 RAM 页面（当前未使用）
 * @param Table 指向要调整的表格的指针（mainTable 结构体）
 * @param RPMIndex 要调整的单元格的 RPM 位置（0 到 RPMLength-1）
 * @param LoadIndex 要调整的单元格的 Load 位置（0 到 LoadLength-1）
 * @param cellValue 要设置的单元格值（无符号短整型）
 * 
 * @return 错误代码：0 表示成功，非零表示失败
 *         - invalidMainTableRPMIndex: RPM 索引超出范围
 *         - invalidMainTableLoadIndex: Load 索引超出范围
 *
 * @author Fred Cooke
 */
unsigned short setPagedMainTableCellValue(unsigned char RPageValue, mainTable* Table, unsigned short RPMIndex, unsigned short LoadIndex, unsigned short cellValue){
//	unsigned char oldRPage = RPAGE;  // 已注释：保存旧页面
	unsigned short errorID = 0;  // 初始化错误 ID 为 0（成功）
//	RPAGE = RPageValue;  // 已注释：切换到目标页面
	// 检查 RPM 索引是否有效
	if(RPMIndex < Table->RPMLength){
		// 检查 Load 索引是否有效
		if(LoadIndex < Table->LoadLength){
			// 计算表格单元格位置并设置值
			// 表格布局：Table[LoadLength * RPMIndex + LoadIndex]
			Table->Table[(Table->LoadLength * RPMIndex) + LoadIndex] = cellValue;
		}else{
			errorID = invalidMainTableLoadIndex;  // 返回错误：无效的 Load 索引
		}
	}else{
		errorID = invalidMainTableRPMIndex;  // 返回错误：无效的 RPM 索引
	}
//	RPAGE = oldRPage;  // 已注释：恢复旧页面
	return errorID;  // 返回错误 ID（0 表示成功）
}


/** @brief Set an RPM axis value
 *
 * Sets the value of an RPM axis cell in a table. This is used when configuring
 * the table via a comms interface.
 *
 * @author Fred Cooke
 *
 * @param RPageValue The page of RAM that the table is in.
 * @param Table is a pointer to the table to adjust.
 * @param RPMIndex The RPM position of the cell to adjust.
 * @param RPMValue The value to set the RPM axis cell to.
 *
 * @return An error code. Zero means success, anything else is a failure.
 */
unsigned short setPagedMainTableRPMValue(unsigned char RPageValue, mainTable* Table, unsigned short RPMIndex, unsigned short RPMValue){
//	unsigned char oldRPage = RPAGE;
//	RPAGE = RPageValue;
	unsigned short errorID = setAxisValue(RPMIndex, RPMValue, Table->RPM, Table->RPMLength, errorBaseMainTableRPM);
//	RPAGE = oldRPage;
	return errorID;
}


/** @brief Set a load axis value
 *
 * Sets the value of a load axis cell in a table. This is used when configuring
 * the table via a comms interface.
 *
 * @author Fred Cooke
 *
 * @param RPageValue The page of RAM that the table is in.
 * @param Table is a pointer to the table to adjust.
 * @param LoadIndex The load position of the cell to adjust.
 * @param LoadValue The value to set the load axis cell to.
 *
 * @return An error code. Zero means success, anything else is a failure.
 */
unsigned short setPagedMainTableLoadValue(unsigned char RPageValue, mainTable* Table, unsigned short LoadIndex, unsigned short LoadValue){
//	unsigned char oldRPage = RPAGE;
//	RPAGE = RPageValue;
	unsigned short errorID = setAxisValue(LoadIndex, LoadValue, Table->Load, Table->LoadLength, errorBaseMainTableLoad);
//	RPAGE = oldRPage;
	return errorID;
}


/** @brief Set a two D table cell value
 *
 * Sets the value of a cell in a two D table. This is used when configuring the
 * table via a comms interface.
 *
 * @author Fred Cooke
 *
 * @param RPageValue The page of RAM that the table is in.
 * @param Table is a pointer to the table to adjust.
 * @param cellIndex The position of the cell to adjust.
 * @param cellValue The value to set the cell to.
 *
 * @return An error code. Zero means success, anything else is a failure.
 */
unsigned short setPagedTwoDTableCellValue(unsigned char RPageValue, twoDTableUS* Table, unsigned short cellIndex, unsigned short cellValue){
	if(cellIndex > 15){
		return invalidTwoDTableIndex;
	}else{
//		unsigned char oldRPage = RPAGE;
//		RPAGE = RPageValue;
		Table->Values[cellIndex] = cellValue;
//		RPAGE = oldRPage;
		return 0;
	}
}


/** @brief Set a two D axis value
 *
 * Sets the value of an axis cell in a table. This is used when configuring
 * the table via a comms interface.
 *
 * @author Fred Cooke
 *
 * @param RPageValue The page of RAM that the table is in.
 * @param Table is a pointer to the table to adjust.
 * @param axisIndex The position of the axis cell to adjust.
 * @param axisValue The value to set the axis cell to.
 *
 * @return An error code. Zero means success, anything else is a failure.
 */
unsigned short setPagedTwoDTableAxisValue(unsigned char RPageValue, twoDTableUS* Table, unsigned short axisIndex, unsigned short axisValue){
//	unsigned char oldRPage = RPAGE;
//	RPAGE = RPageValue;
	unsigned short errorID = setAxisValue(axisIndex, axisValue, Table->Axis, 16, errorBaseTwoDTableAxis);
//	RPAGE = oldRPage;
	return errorID;
}


/** @brief Validate a main table
 *
 * Check that the configuration of the table is valid. Assumes pages are
 * correctly set. @todo more detail here....
 *
 * @author Fred Cooke
 *
 * @param Table is a pointer to the table to be validated.
 *
 * @return An error code. Zero means success, anything else is a failure.
 */
unsigned short validateMainTable(mainTable* Table){
	/* 如果需要，扩展此函数以接受 r 和 f 页面并检查
	 * 任何主表格，而不仅仅是线性空间中刚接收的不受信任的表格 */

	// 检查 RPM 轴长度是否超出最大值
	if(Table->RPMLength > MAINTABLE_MAX_RPM_LENGTH){
		return invalidMainTableRPMLength;  // 返回错误：RPM 长度无效
	}else if(Table->LoadLength > MAINTABLE_MAX_LOAD_LENGTH){
		// 检查 Load 轴长度是否超出最大值
		return invalidMainTableLoadLength;  // 返回错误：Load 长度无效
	}else if((Table->RPMLength * Table->LoadLength) > MAINTABLE_MAX_MAIN_LENGTH){
		// 检查表格总大小是否超出最大值
		return invalidMainTableMainLength;  // 返回错误：表格总长度无效
	}else{
		/* 检查 RPM 轴的顺序（必须递增） */
		unsigned char i;
		for(i=0;i<(Table->RPMLength - 1);i++){
			// 如果前一个值大于后一个值，顺序无效
			if(Table->RPM[i] > Table->RPM[i+1]){
				return invalidMainTableRPMOrder;  // 返回错误：RPM 轴顺序无效
			}
		}
		/* 检查 Load 轴的顺序（必须递增） */
		unsigned char j;
		for(j=0;j<(Table->LoadLength - 1);j++){
			// 如果前一个值大于后一个值，顺序无效
			if(Table->Load[j] > Table->Load[j+1]){
				return invalidMainTableLoadOrder;  // 返回错误：Load 轴顺序无效
			}
		}
		/* 如果通过了所有检查，表格有效 */
		return 0;  // 返回成功
	}
}


/** @brief Validate a two D table
 *
 * Check that the order of the axis values is correct and therefore that the
 * table is valid too.
 *
 * @author Fred Cooke
 *
 * @param Table is a pointer to the table to be validated.
 *
 * @return An error code. Zero means success, anything else is a failure.
 */
unsigned short validateTwoDTable(twoDTableUS* Table){
	/* 检查轴的顺序（必须递增） */
	unsigned char i;
	for(i=0;i<(TWODTABLEUS_LENGTH - 1);i++){
		// 如果前一个值大于后一个值，顺序无效
		if(Table->Axis[i] > Table->Axis[i+1]){
			return invalidTwoDTableAxisOrder;  // 返回错误：轴顺序无效
		}
	}
	return 0;  // 返回成功
}
