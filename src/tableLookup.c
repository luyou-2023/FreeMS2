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


/** @brief Main table read function
 *
 * Looks up a value from a main table using interpolation.
 *
 * The process :
 *
 * Take a table with two movable axis sets and two axis lengths,
 * loop to find which pairs of axis values and indexs we are between,
 * interpolate two pairs down to two values,
 * interpolate two values down to one value.
 *
 * Table size :
 *
 * To reduce the table size from 19x24 to something smaller, simply
 * reduce the RPMLength and LoadLength fields to lower values.
 * Increasing the size of either axis is not currently possible.
 *
 * Values outside the table :
 *
 * Given that the axis lists are in order, a data point outside
 * the table will give the value adjacent to it, and one outside
 * one of the four corners will give the corner value. This is a
 * clean and reasonable behaviour in my opinion.
 *
 * Reminder : X/RPM is horizontal, Y/Load is vertical
 *
 * @warning This function relies on the axis values being a sorted
 * list from low to high. If this is not the case behaviour is
 * undefined and could include memory corruption and engine damage.
 *
 * @author Fred Cooke
 *
 * @param Table is a pointer to the table to read from.
 * @param realRPM is the current RPM for which a table value is required.
 * @param realLoad is the current load for which a table value is required.
 * @param RAMPage is the RAM page that the table is stored in.
 *
 * @return The interpolated value for the location specified.
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


/** @brief Two D table read function
 *
 * Looks up a value from a two D table using interpolation.
 *
 * @author Fred Cooke
 *
 * @param Table is a pointer to the table to read from.
 * @param Value is the position value used to lookup the return value.
 *
 * @return the interpolated value for the position specified
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


/** @brief Set an axis value
 *
 * Sets the value of an axis cell in a table. This is used when configuring
 * a table via a communication interface.
 *
 * @author Fred Cooke
 *
 * @param index The position of the axis cell to adjust.
 * @param value The value to set the axis cell to.
 * @param axis A pointer to the axis array to adjust.
 * @param length The length of the axis array.
 * @param errorBase The base value to add error code offsets to.
 *
 * @return An error code. Zero means success, anything else is a failure.
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


/** @brief Set a main table cell value
 *
 * Sets the value of a cell in a main table. This is used when configuring a
 * table via a communication interface.
 *
 * @author Fred Cooke
 *
 * @param RPageValue The page of RAM that the table is in.
 * @param Table A pointer to the table to adjust.
 * @param RPMIndex The RPM position of the cell to adjust.
 * @param LoadIndex The load position of the cell to adjust.
 * @param cellValue The value to set the cell to.
 *
 * @return An error code. Zero means success, anything else is a failure.
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
