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


/**	@file coreVarsGenerator.c
 * @ingroup measurementsAndCalculations
 *
 * @brief Generate and average the core variables.
 *
 * This file contains the function that transfers the raw ADC values to actual
 * physical measurements and averages them.
 *
 * @author Fred Cooke
 */


#define COREVARSGENERATOR_C
#include "inc/FreeMS2.h"
#include "inc/commsCore.h"
#include "inc/coreVarsGenerator.h"
#include "inc/decoderInterface.h"


/** @brief Generate the core variables and average them.
 *
 * Each raw ADC value is converted to a usable measurement via a variety of
 * methods chosen at runtime by configured settings. Once in their native units
 * and therefore closer to maximal use of the available data range they are
 * all averaged.
 *
 * @todo TODO incorporate averaging code, right now its a straight copy.
 * @todo TODO change the way configuration is done and make sure the most common options are after the first if().
 * @todo TODO add actual configuration options to the fixed config blocks for these items.
 *
 * @author Fred Cooke
 */
void generateCoreVars(){
	/* 计算并获取我们将用于执行计算的基本变量 */


	/* 预计算在多个地方使用的值 */

	/* 限制 TPS ADC 读数并将其偏移到从零开始 */
	unsigned short unboundedTPSADC = ADCArrays->TPS;  // 读取未限制的TPS ADC值
	if(unboundedTPSADC > fixedConfigs2.sensorRanges.TPSMaximumADC){
		// 如果ADC值超过最大值，设置为ADC范围
		boundedTPSADC = TPSADCRange;
	}else if(unboundedTPSADC > fixedConfigs2.sensorRanges.TPSMinimumADC){ // 强制使用辅助配置... TODO 移除这个
		// 如果ADC值在最小值和最大值之间，减去最小值得到从零开始的值
		boundedTPSADC = unboundedTPSADC - fixedConfigs2.sensorRanges.TPSMinimumADC;
	}else{
		// 如果ADC值小于等于最小值，设置为0
		boundedTPSADC = 0;
	}


	/* 使用转换变量从 ADC 获取 BRV（所有安装都需要这个） */
	unsigned short localBRV;  // 局部变量存储电池参考电压
	if(TRUE){ /* 如果 BRV 已连接 */
		// 使用线性转换公式：值 = (ADC值 * 范围) / ADC分辨率 + 最小值
		// 转换为毫伏单位
		localBRV = (((unsigned long)ADCArrays->BRV * fixedConfigs2.sensorRanges.BRVRange) / ADC_DIVISIONS) + fixedConfigs2.sensorRanges.BRVMinimum;
	}else if(FALSE){ /* 配置为固定值 */
		/* 从配置设置中获取首选的 BRV 值 */
		localBRV = fixedConfigs2.sensorPresets.presetBRV;
	}else{ /* 如果配置损坏，故障保护 */
		/* 默认为正常交流发电机充电电压 14.4V */
		localBRV = runningVoltage;  // 使用运行电压常量（14400毫伏）
		/* 如果有人监听，让他们知道出了问题 */
		sendErrorIfClear(BRV_NOT_CONFIGURED_CODE);  // 发送错误代码（如果缓冲区空闲）
	}


	unsigned short localCHT;  // 局部变量存储缸盖温度
	/* 使用转换表从 ADC 获取 CHT（所有安装都需要这个） */
	if(TRUE){ /* 如果 CHT 已连接 */
		// 使用查找表直接转换ADC值到温度值（单位：K * 100）
		localCHT = CHTTransferTable[ADCArrays->CHT];
	}else if(FALSE){ /* 配置为从 ADC 读取作为线性电位器 */
		/* 以合理的方式将 ADC 读数转换为发动机温度 */
		// 线性转换：0 ADC = 0°C = 273.15K = 27315, 1023 ADC = 102.3°C = 375.45K = 37545
		localCHT = (ADCArrays->CHT * 10) + freezingPoint;
	}else if(FALSE){ /* 配置为固定值 */
		/* 从配置设置中获取首选的 CHT 值 */
		localCHT = fixedConfigs2.sensorPresets.presetCHT;
	}else{ /* 如果配置损坏，故障保护 */
		/* 默认为正常运行温度 85°C/358K */
		localCHT = runningTemperature;  // 使用运行温度常量（35800 = 358K * 100）
		/* 如果有人监听，让他们知道出了问题 */
		sendErrorIfClear(CHT_NOT_CONFIGURED_CODE);  // 发送CHT未配置错误代码
	}


	unsigned short localIAT;
	/* Get IAT from ADC using the transfer table (all installations need this) */
	if(TRUE){ /* If IAT connected  */ /* using false here causes iat to default to room temp, useful with heatsoaked OEM sensors like the Volvo's... */
		localIAT = IATTransferTable[ADCArrays->IAT];
	}else if(FALSE){ /* Configured to be read From ADC as dashpot */
		/* Transfer the ADC reading to an air temperature in a reasonable way */
		localIAT = (ADCArrays->IAT * 10) + 27315; /* 0 ADC = 0C = 273.15K = 27315, 1023 ADC = 102.3C = 375.45K = 37545 */
	}else if(FALSE){ /* Configured to be fixed value */
		/* Get the preferred IAT figure from configuration settings */
		localIAT = fixedConfigs2.sensorPresets.presetIAT;
	}else{ /* Fail safe if config is broken */
		/* Default to normal air temperature of 20C/293K */
		localIAT = roomTemperature;
		/* If anyone is listening, let them know something is wrong */
		sendErrorIfClear(IAT_NOT_CONFIGURED_CODE);
	}


	unsigned short localMAT;
	/* Determine the MAT reading for future calculations */
	if(TRUE){ /* If MAT sensor is connected */
		/* Get MAT from ADC using same transfer table as IAT (too much space to waste on having two) */
		localMAT = IATTransferTable[ADCArrays->MAT];
	}else if(FALSE){ /* Configured to be fixed value */
		/* Get the preferred MAT figure from configuration settings */
		localMAT = fixedConfigs2.sensorPresets.presetMAT;
	}else{ /* Fail safe if config is broken */
		/* If not, default to same value as IAT */
		localMAT = localIAT;
		/* If anyone is listening, let them know something is wrong */
		sendErrorIfClear(MAT_NOT_CONFIGURED_CODE);
	}


	unsigned short localMAP;  // 局部变量存储歧管绝对压力
	unsigned short localIAP;  // 局部变量存储中冷器压力
	/* 确定用于未来计算的 MAP 压力 */
	if(TRUE){ /* 如果 MAP 传感器已连接 */
		/* 使用转换变量从 ADC 获取 MAP */
		// 线性转换：值 = (ADC值 * 范围) / ADC分辨率 + 最小值
		// 单位：kPa * 100（例如：10000 = 100.00 kPa）
		localMAP = (((unsigned long)ADCArrays->MAP * fixedConfigs2.sensorRanges.MAPRange) / ADC_DIVISIONS) + fixedConfigs2.sensorRanges.MAPMinimum;
		if(TRUE){ /* 如果中冷器增压传感器已连接 */
			/* 使用相同的转换变量从 ADC 获取 IAP，因为它们都需要读取相同的范围 */
			localIAP = (((unsigned long)ADCArrays->IAP * fixedConfigs2.sensorRanges.MAPRange) / ADC_DIVISIONS) + fixedConfigs2.sensorRanges.MAPMinimum;
		}
	}else if(FALSE){ /* 配置为 MAP 模拟 TPS 信号 */
		/* 通过转换从 TPS 获取 MAP */
		// 将TPS ADC值转换为MAP值，用于Alpha-N模式
		localMAP = (((unsigned long)boundedTPSADC * TPSMAPRange) / TPSADCRange) + fixedConfigs2.sensorRanges.TPSClosedMAP;
	}else if(FALSE){ /* 配置为 ADC 上的线性电位器 */
		/* 通过转换从 ADC 获取 MAP，转换为内部 kPa 值，其中 1023ADC = 655kPa */
		// 左移6位相当于乘以64：1023 * 64 = 65472 ≈ 65500 (655.00 kPa)
		localMAP = ADCArrays->MAP << 6;
		if(TRUE){ /* 如果中冷器增压传感器已启用 */
			/* 通过转换从 ADC 获取 IAP，转换为内部 kPa 值，其中 1023ADC = 655kPa */
			localIAP = ADCArrays->IAP << 6;
		}
	}else if(FALSE){ /* 配置为从配置中获取固定 MAP */
		/* 从配置设置中获取首选的 MAP 值 */
		localMAP = fixedConfigs2.sensorPresets.presetMAP;
	}else{ /* 如果配置损坏，故障保护 */
		/* 默认为零以取消所有其他计算并有效切断燃油 */
		localMAP = 0;  // MAP为0会导致燃油计算为0，从而切断燃油供应
		/* 如果有人监听，让他们知道出了问题 */
		sendErrorIfClear(MAP_NOT_CONFIGURED_CODE); // 或者可能将其排队？
	}


	/* Determine MAF variable if required */
	unsigned short localMAF = 0; // Default to zero as it is not required for anything except main PW calcs optionally
	if(TRUE){
		localMAF = MAFTransferTable[ADCArrays->MAF];
	}

	unsigned short localAAP;
	/* Determine the Atmospheric pressure to use for future calculations */
	if(TRUE){ /* Configured for second sensor to read AAP */
		/* get AAP from ADC using separate vars to allow 115kPa sensor etc to be used */
		localAAP = (((unsigned long)ADCArrays->AAP * fixedConfigs2.sensorRanges.AAPRange) / ADC_DIVISIONS) + fixedConfigs2.sensorRanges.AAPMinimum;
	}else if(FALSE){ /* Configured for dash potentiometer on ADC */
		/* Get AAP from ADC via conversion to internal kPa figure where 1023ADC = 102.3kPa */
		localAAP = ADCArrays->AAP * 10;
	}else if(FALSE){ /* Configured for fixed AAP reading from pre start */
		/* Get the AAP reading as saved during startup */
		localAAP = bootTimeAAP; /* This is populated pre start up */
	}else if(FALSE){ /* Configured for fixed AAP from config */
		/* Get the preferred AAP figure from configuration settings */
		localAAP = fixedConfigs2.sensorPresets.presetAAP;
	}else{ /* Fail safe if config is broken */
		/* Default to sea level */
		localAAP = seaLevelKPa; /* 100kPa */
		/* If anyone is listening, let them know something is wrong */
		sendErrorIfClear(AAP_NOT_CONFIGURED_CODE); // or maybe queue it?
	}


	unsigned short localEGO;
	/* Get main Lambda reading */
	if(TRUE){ /* If WBO2-1 is connected */
		/* Get EGO from ADCs using transfer variables */
		localEGO = (((unsigned long)ADCArrays->EGO * fixedConfigs2.sensorRanges.EGORange) / ADC_DIVISIONS) + fixedConfigs2.sensorRanges.EGOMinimum;
	}else if(FALSE){ /* Configured for fixed EGO from config */
		/* Get the preferred EGO figure from configuration settings */
		localEGO = fixedConfigs2.sensorPresets.presetEGO;
	}else{ /* Default value if not connected incase other things are misconfigured */
		/* Default to stoichiometric */
		localEGO = stoichiometricLambda; /* EGO / 32768 = Lambda */
		/* If anyone is listening, let them know something is wrong */
		sendErrorIfClear(EGO_NOT_CONFIGURED_CODE); // or maybe queue it?
	}


	unsigned short localEGO2;
	/* Get second Lambda reading */
	if(TRUE){ /* If WBO2-2 is connected */
		/* Get EGO2 from ADCs using same transfer variables as EGO */
		localEGO2 = (((unsigned long)ADCArrays->EGO2 * fixedConfigs2.sensorRanges.EGORange) / ADC_DIVISIONS) + fixedConfigs2.sensorRanges.EGOMinimum;
	}else if(FALSE){ /* Configured for fixed EGO2 from config */
		/* Get the preferred EGO2 figure from configuration settings */
		localEGO2 = fixedConfigs2.sensorPresets.presetEGO2;
	}else{ /* Default value if not connected incase other things are misconfigured */
		/* Default to stoichiometric */
		localEGO2 = stoichiometricLambda;
		/* If anyone is listening, let them know something is wrong */
		sendErrorIfClear(EGO2_NOT_CONFIGURED_CODE); // or maybe queue it?
	}


	unsigned short localTPS;  // 局部变量存储节气门位置百分比
	/* 获取 TPS 百分比 */
	if(TRUE){ /* 如果 TPS 已连接 */
		/* 从 ADC 获取 TPS，不需要添加 TPS 最小值，因为我们知道根据定义它是零 */
		// 将ADC值转换为百分比：值 = (ADC值 * 最大值) / ADC范围
		// TPS_RANGE_MAX 通常是 64000，表示 100% = 64000/640
		localTPS = ((unsigned long)boundedTPSADC * TPS_RANGE_MAX) / TPSADCRange;
	}else if(FALSE){ /* 配置为 TPS 模拟 MAP 信号 */
		/* 通过转换从 MAP 获取 TPS */
		/* 将 MAP 信号限制在范围内 */
		if(localTPS > fixedConfigs2.sensorRanges.TPSOpenMAP){ /* 大于 ~95kPa */
			localTPS = TPS_RANGE_MAX; /* 64000/640 = 100% */
		}else if(localTPS < fixedConfigs2.sensorRanges.TPSClosedMAP){ /* 小于 ~30kPa */
			localTPS = 0;  // 小于关闭MAP值，设置为0%
		}else{ /* 将 MAP 范围缩放到 TPS 范围 */
			// 计算MAP值相对于关闭MAP的偏移量
			localTPS = localMAP - fixedConfigs2.sensorRanges.TPSClosedMAP;
		}
		// 从 MAP 获取 TPS，不需要添加 TPS 最小值，因为我们知道根据定义它是零
		// 将MAP范围映射到TPS百分比范围
		localTPS = ((unsigned long)localTPS * TPS_RANGE_MAX) / (fixedConfigs2.sensorRanges.TPSOpenMAP - fixedConfigs2.sensorRanges.TPSClosedMAP);
	}else if(FALSE){ /* 配置为 ADC 上的线性电位器 */
		/* 从 ADC 获取 TPS，如所示：1023 ADC = 100%，0 ADC = 0% */
		// 直接线性转换，无需校准
		localTPS = ((unsigned long)ADCArrays->TPS * TPS_RANGE_MAX) / ADC_DIVISIONS;
	}else if(FALSE){ /* 配置为从配置中获取固定 TPS */
		/* 从配置设置中获取首选的 TPS 值 */
		localTPS = fixedConfigs2.sensorPresets.presetTPS;
	}else{ /* 如果配置损坏，故障保护 */
		/* 默认为 50% 以不触发任何 WOT（全开节气门）或 CT（关闭节气门）条件 */
		localTPS = halfThrottle;  // 使用半节气门常量（32000 = 50%）
		/* 如果有人监听，让他们知道出了问题 */
		sendErrorIfClear(TPS_NOT_CONFIGURED_CODE); // 或者可能将其排队？
	}


	/* 通过锁定 ISR 一秒钟并获取齿记录数据来获取 RPM */
	// 原子操作开始
	// 复制 rpm 数据
	// 原子操作结束

	// 从记录的数据计算 RPM 和增量 RPM 以及增量增量 RPM
	CoreVars->RPM = *RPM; // 临时实现！！
	unsigned short localDRPM = 0;   // 局部变量存储RPM变化率（未实现）
	unsigned short localDDRPM = 0;  // 局部变量存储RPM变化加速度（未实现）


	/* 变量平均处理部分 */


	/* 根据配置对变量进行平均 */
	/* 严格来说，只有主要变量需要平均。之后，派生变量在某种程度上已经平均了。*/
	/* 但是，对派生变量进行一些短期平均可能也有优势，所以这是以后需要研究的事情。*/

	/// @todo TODO 在这里对生成的值进行平均

//			newVal var word        ' 来自 ADC 的值
//			smoothed var word    ' 一个很好的平滑结果
//
//			if newval > smoothed then
//			        smoothed = smoothed + (newval - smoothed)/alpha
//			else
//			        smoothed = smoothed - (smoothed - newval)/alpha
//			endif

	// 来自：http://www.tigoe.net/pcomp/code/category/code/arduinowiring/41

	// 目前只是将它们复制进去
	CoreVars->IAT = localIAT;    // 存储进气温度
	CoreVars->CHT = localCHT;    // 存储缸盖温度
	CoreVars->TPS = localTPS;    // 存储节气门位置
	CoreVars->EGO = localEGO;    // 存储主氧传感器值
	CoreVars->BRV = localBRV;    // 存储电池参考电压
	CoreVars->MAP = localMAP;    // 存储歧管绝对压力
	CoreVars->AAP = localAAP;    // 存储大气压力
	CoreVars->MAT = localMAT;    // 存储歧管温度

	CoreVars->EGO2 = localEGO2;   // 存储第二氧传感器值
	CoreVars->IAP = localIAP;    // 存储中冷器压力
	CoreVars->MAF = localMAF;    // 存储质量空气流量
	CoreVars->DRPM = localDRPM;  // 存储RPM变化率（当前为0）
	CoreVars->DDRPM = localDDRPM; // 存储RPM变化加速度（当前为0）

	// 稍后实现...
	unsigned short i;
	for(i=0;i<CORE_VARS_LENGTH;i++){ // TODO
		/* 根据配置数组对所有主要变量执行平均 */
		// 获取旧值
		// 根据配置数组值处理新旧值以产生结果
		// 将结果分配给旧值持有者
	} // TODO

	/*&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&*/
}
