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


/**	@file init.c
 *
 * @brief Initialise the devices state
 *
 * Setup, configure and initialise all aspects of the devices state including
 * but not limited to:
 *
 * - Setup the bus clock speed
 * - Configuration based variable initialisation
 * - I/O register behaviour and initial state
 * - Configure and enable interrupts
 * - Copy tunable data up to RAM from flash
 * - Configure peripheral module behaviour
 *
 * @author Fred Cooke
 */


#define INIT_C
#include "inc/FreeMS2.h"
#include "inc/flashWrite.h"
#include "inc/interrupts.h"
#include "inc/utils.h"
#include "inc/commsISRs.h"
#include "inc/pagedLocationBuffers.h"
#include "inc/init.h"
#include "inc/decoderInterface.h"
#include <string.h>


/** @brief The main top level init
 *
 * The main init function to be called from main.c before entering the main
 * loop. This function is simply a delegator to the finer grained special
 * purpose init functions.
 *
 * @author Fred Cooke
 */
void init(){
	ATOMIC_START();         	/* 禁用所有中断，同时我们将板子配置为可用状态 */
	initPLL();              	/* 设置 PLL 并使用它 */
	initIO();               	/* TODO 使这依赖于配置。设置所有引脚和模块为低功耗无害状态 */
	initAllPagedRAM();      	/* 将表格和配置数据块从 flash 复制到分页 RAM 块以进行快速数据查找 */
	initVariables();        	/* 初始化其余的运行变量等 */
	initFlash();            	/* TODO，完成这个 */
	initECTTimer();         	/* TODO 以有组织的方式将其移到配置内部。设置定时器模块及其各个方面 */
	initSCIStuff();         	/* 设置我们将使用的 sci 模块。 */
	initConfiguration();    	/* TODO 在这里设置用户/功能/配置！ */
	initInterrupts();       	/* 仍然是最后一步，在这里重置定时器，启用中断 TODO 以有组织的方式将其移到配置内部。设置其余各个中断 */
	ATOMIC_END();           	/* 重新启用任何已配置的中断 */
}


/* used to chop out all the init stuff at compile time for hardware testing. */
//#define NO_INIT


/** @brief Set the PLL clock frequency
 *
 * Set the Phase Locked Loop to our desired frequency (80MHz) and switch to
 * using it for clock (40MHz bus speed).
 *
 * @author Fred Cooke
 */
void initPLL(){
	// 切换到基础外部 OSCCLK 以确保不使用 PLL（复位后关闭，但不确定监控程序是否在传递控制之前将其打开）
	CLKSEL &= PLLSELOFF;
	// 关闭 PLL 设备以调整其速度（复位后默认打开）
	PLLCTL &= PLLOFF;
	// 设置参考分频器：16MHz / (3 + 1) = 4MHz 总线频率
	REFDV = PLLDIVISOR;
	// 设置 PLL 倍频器：4MHz * (9 + 1) = 40MHz 总线频率
	SYNR = PLLMULTIPLIER;
	// 重新打开 PLL 设备，现在运行在 80MHz
	PLLCTL |= PLLON;

	// 等待 PLL 锁定到目标频率
	while (!(CRGFLG & PLLLOCK)){
		/* 在 PLL 环路锁定到目标频率之前什么都不做 */
		/* 目标频率由 (2 * (晶振频率 / (REFDV + 1)) * (SYNR + 1)) 给出 */
		/* 总线频率是 PLL 频率的一半，由 ((晶振频率 / (REFDV + 1)) * (SYNR + 1)) 给出 */
	}

	// 切换到 PLL 时钟用于内部总线频率
	CLKSEL = PLLSELON;
	/* 来自 MC9S12XDP512V2.pdf 第 2.4.1.1.2 节第 101 页第三段 */
	/* "这最多需要 4 个 OSCCLK 时钟周期加上 4 个 PLL 时钟周期" */
	/* "在此期间所有时钟冻结，CPU 活动停止" */
	/* 因此没有必要等待这发生，我们已经... */
}


/* Configure all the I/O to default values to keep power use down etc */
void initIO(){
	/* for now, hard code all stuff to be outputs as per Freescale documentation,	*/
	/* later what to do will be pulled from flash configuration such that all		*/
	/* things are setup at once, and not messed with thereafter. when the port		*/
	/* something uses is changed via the tuning interface, the confuration will be	*/
	/* done on the fly, and the value burned to flash such that next boot happens	*/
	/* correctly and current running devices are used in that way.					*/

	/* Turn off and on and configure all the modules in an explicit way */
	// TODO set up and turn off all modules (CAN,SCI,SPI,IIC,etc)

	/* Digital input buffers on the ATD channels are off by default, leave them this way! */
	//ATD0DIEN = ZEROS; /* You are out of your mind if you waste this on digital Inputs */
	//ATD1DIEN0 = ZEROS; /* You are out of your mind if you waste this on digital Inputs (NOT-bonded, can't use) */
	//ATD1DIEN1 = ZEROS; /* You are out of your mind if you waste this on digital Inputs */

	/* 并将它们全部配置为模拟输入 */
	//ATD0CTL0 = 0x07/* 打开 mult 时需要设置此值以导致回绕，但复位后是正确的 */
	//ATD0CTL1 = 0x07/* 触发和中断配置，目前未使用 */
	// ATD0CTL2: 0xC0 = 0b11000000
	// - BIT7 (ADPU): 1 = 打开 ADC 模块
	// - BIT6 (AFFC): 1 = 自动标志清除
	ATD0CTL2 = 0xC0;
	// ATD0CTL3: 0x40 = 0b01000000
	// - BIT6 (S8C): 1 = 序列长度 = 8 个通道
	ATD0CTL3 = 0x40;
	// ATD0CTL4: 0x73 = 0b01110011
	// - BIT7-5: 预分频器设置 ADC 时钟
	// - BIT4-0: 采样时间设置
	ATD0CTL4 = 0x73;
	// ATD0CTL5: 0xB0 = 0b10110000
	// - BIT7 (DJM): 1 = 右对齐
	// - BIT6 (SCAN): 1 = 扫描所有通道
	// - BIT5-0: 多路复用器选择（写入此寄存器会开始转换）
	ATD0CTL5 = 0xB0;

	/* 再次配置它们全部为模拟输入（重复配置以确保正确） */
	ATD0CTL4 = 0x73; /* 设置 ADC 时钟和采样周期以获得最佳精度 */
	ATD0CTL5 = 0xB0; /* 设置右对齐，多路复用并扫描所有通道。写入此值会开始转换 */

#ifndef NO_INIT
	/* 设置 PWM 组件并将其值初始化为关闭 */
	// PWME: 0x7F = 0b01111111，启用 PWM 通道 0-6（通道 7 是主板上的用户 LED）
	PWME = 0x7F;
	// PWMCLK: 选择最快的时钟源用于所有通道
	PWMCLK = ZEROS;
	// PWMPRCLK: 选择最快的预分频器用于所有通道
	PWMPRCLK = ZEROS;
	// PWMSCLA: 通道 A 缩放器设置为最快
	PWMSCLA = ZEROS;
	// PWMSCLB: 通道 B 缩放器设置为最快
	PWMSCLB = ZEROS;
	/* TODO PWM 通道级联以获得高分辨率 */
	// 在这里将通道对连接在一起（还需要启用 16 位寄存器）
	/* TODO 使用频率和初始占空比初始化 pwm 通道以供实际使用 */
	// 用于测试的初始 PWM 设置
	/* PWM 周期 */
	PWMPER0 = 0xFF; // 255 用于 ADC0 测试
	PWMPER1 = 0xFF; // 255 用于 ADC1 测试
	PWMPER2 = 0xFF; // 255 用于 ADC1 测试
	PWMPER3 = 0xFF; // 255 用于 ADC1 测试
	PWMPER4 = 0xFF; // 255 用于 ADC1 测试
	PWMPER5 = 0xFF; // 255 用于 ADC1 测试
	/* PWM 占空比 */
	// 将所有通道的占空比设置为 0（关闭输出）
	PWMDTY0 = 0;
	PWMDTY1 = 0;
	PWMDTY2 = 0;
	PWMDTY3 = 0;
	PWMDTY4 = 0;
	PWMDTY5 = 0;


	/* 初始化配置为输出的引脚状态 */
	/* 初始化为低电平，这样默认情况下所有接地的晶体管都被关闭 */
	PORTA = ZEROS; /* 串口监控引脚在 0x40，如果复位时输出电容很大可能会导致问题 */
	PORTB = ZEROS; /* 初始化其余火花输出为关闭 */
	// PORTE: 0x1F = 0b00011111
	// 当不使用时应该是 0b10011111，PE7 应该为高，PE5 和 PE6 应该为低，其余为高
	PORTE = 0x1F;
	PORTK = ZEROS; /* 初始化 PORTK 为低 */
	/* AD0PT1 如果你将这些浪费在数字输入上，你就疯了 */
	/* AD1PT1 如果你将这些浪费在数字输入上，你就疯了 */

	/* 初始化数据方向寄存器 */
	/* 根据 MC9S12XDP512V2.pdf 第 1.2.2 章末尾的注释设置为输出 */
	DDRA = ONES; /* GPIO (8 位) - 所有位设置为输出 */
	DDRB = ONES; /* GPIO (8 位) - 所有位设置为输出 */
	// DDRE: 0xFC = 0b11111100
	// PE0 和 PE1 是时钟和模式引脚，只能输入，其余是 GPIO 输出
	DDRE = 0xFC;
	// DDRK: 只有 0,1,2,3,4,5,7 位可用，不是 6（共 7 位）
	DDRK = ONES;
	DDRS = ONES; /* SCI0, SCI1, SPI0 (8 位) - 所有位设置为输出 */
	// DDRT: 0xFC = 0b11111100
	// ECT 引脚 0,1 设置为输入捕获 (IC)，2:7 设置为输出比较 (OC)
	DDRT = 0xFC;
	DDRM = ONES; /* CAN 0 - 3 (8 位) - 所有位设置为输出 */
	DDRP = ONES; /* PWM 引脚 (8 位) - 所有位设置为输出 */
	DDRJ = ONES; /* 在 112 引脚芯片上只有 0,1,6,7 引出 (4 位) */
	/* AD0DDR1 如果你将这些浪费在数字输入上，你就疯了 */
#endif
}


/** @brief Buffer lookup tables addresses
 *
 * Save pointers to the lookup tables which live in paged flash.
 *
 * @note Many thanks to Jean Bélanger for the inspiration/idea to do this!
 *
 * @author Fred Cooke
 */
void initLookupAddresses(){
	// 保存查找表的地址指针，这些表位于分页 flash 中
	// 这样做是为了避免在访问分页数据时出现警告
	IATTransferTableLocation = (void*)&IATTransferTable;    // 进气温度转换表地址
	CHTTransferTableLocation = (void*)&CHTTransferTable;    // 缸盖温度转换表地址
	MAFTransferTableLocation = (void*)&MAFTransferTable;    // 质量空气流量转换表地址
	TestTransferTableLocation = (void*)&TestTransferTable;  // 测试转换表地址
}


/** @brief Buffer fuel tables addresses
 *
 * Save pointers to the fuel tables which live in paged flash.
 *
 * @note Many thanks to Jean Bélanger for the inspiration/idea to do this!
 *
 * @author Fred Cooke
 */
void initFuelAddresses(){
	/* 在页面内设置地址以避免警告 */
	// 保存燃油表格的 Flash 地址指针，这些表格位于分页 flash 中
	VETableMainFlashLocation		= (void*)&VETableMainFlash;          // 主 VE 表 Flash 地址
	VETableSecondaryFlashLocation	= (void*)&VETableSecondaryFlash;      // 辅助 VE 表 Flash 地址
	VETableTertiaryFlashLocation	= (void*)&VETableTertiaryFlash;       // 第三 VE 表 Flash 地址
	LambdaTableFlashLocation		= (void*)&LambdaTableFlash;           // Lambda 表 Flash 地址
	VETableMainFlash2Location		= (void*)&VETableMainFlash2;         // 主 VE 表 Flash2 地址
	VETableSecondaryFlash2Location	= (void*)&VETableSecondaryFlash2;     // 辅助 VE 表 Flash2 地址
	VETableTertiaryFlash2Location	= (void*)&VETableTertiaryFlash2;      // 第三 VE 表 Flash2 地址
	LambdaTableFlash2Location		= (void*)&LambdaTableFlash2;          // Lambda 表 Flash2 地址
}


/** @brief Copy fuel tables to RAM
 *
 * Initialises the fuel tables in RAM by copying them up from flash.
 *
 * @author Fred Cooke
 */
//void initPagedRAMFuel(void){
//	/* Copy the tables from flash to RAM */
//	RPAGE = RPAGE_FUEL_ONE;
//	memcpy((void*)&TablesA,	VETableMainFlashLocation,		MAINTABLE_SIZE);
//	memcpy((void*)&TablesB,	VETableSecondaryFlashLocation,	MAINTABLE_SIZE);
//	memcpy((void*)&TablesC,	VETableTertiaryFlashLocation,	MAINTABLE_SIZE);
//	memcpy((void*)&TablesD,	LambdaTableFlashLocation,		MAINTABLE_SIZE);
//	RPAGE = RPAGE_FUEL_TWO;
//	memcpy((void*)&TablesA,	VETableMainFlash2Location,		MAINTABLE_SIZE);
//	memcpy((void*)&TablesB,	VETableSecondaryFlash2Location,	MAINTABLE_SIZE);
//	memcpy((void*)&TablesC,	VETableTertiaryFlash2Location,	MAINTABLE_SIZE);
//	memcpy((void*)&TablesD,	LambdaTableFlash2Location,		MAINTABLE_SIZE);
//}


/** @brief Buffer timing tables addresses
 *
 * Save pointers to the timing tables which live in paged flash.
 *
 * @note Many thanks to Jean Bélanger for the inspiration/idea to do this!
 *
 * @author Fred Cooke
 */
void initTimingAddresses(){
	/* Setup addresses within the page to avoid warnings */
	IgnitionAdvanceTableMainFlashLocation			= (void*)&IgnitionAdvanceTableMainFlash;
	IgnitionAdvanceTableSecondaryFlashLocation		= (void*)&IgnitionAdvanceTableSecondaryFlash;
	InjectionAdvanceTableMainFlashLocation			= (void*)&InjectionAdvanceTableMainFlash;
	InjectionAdvanceTableSecondaryFlashLocation		= (void*)&InjectionAdvanceTableSecondaryFlash;
	IgnitionAdvanceTableMainFlash2Location			= (void*)&IgnitionAdvanceTableMainFlash2;
	IgnitionAdvanceTableSecondaryFlash2Location		= (void*)&IgnitionAdvanceTableSecondaryFlash2;
	InjectionAdvanceTableMainFlash2Location			= (void*)&InjectionAdvanceTableMainFlash2;
	InjectionAdvanceTableSecondaryFlash2Location	= (void*)&InjectionAdvanceTableSecondaryFlash2;
}


/** @brief Copy timing tables to RAM
 *
 * Initialises the timing tables in RAM by copying them up from flash.
 *
 * @author Fred Cooke
 */
//void initPagedRAMTime(){
//	/* Copy the tables from flash to RAM */
//	RPAGE = RPAGE_TIME_ONE;
//	memcpy((void*)&TablesA,	IgnitionAdvanceTableMainFlashLocation,			MAINTABLE_SIZE);
//	memcpy((void*)&TablesB,	IgnitionAdvanceTableSecondaryFlashLocation,		MAINTABLE_SIZE);
//	memcpy((void*)&TablesC,	InjectionAdvanceTableMainFlashLocation,			MAINTABLE_SIZE);
//	memcpy((void*)&TablesD,	InjectionAdvanceTableSecondaryFlashLocation,	MAINTABLE_SIZE);
//	RPAGE = RPAGE_TIME_TWO;
//	memcpy((void*)&TablesA,	IgnitionAdvanceTableMainFlash2Location,			MAINTABLE_SIZE);
//	memcpy((void*)&TablesB,	IgnitionAdvanceTableSecondaryFlash2Location,	MAINTABLE_SIZE);
//	memcpy((void*)&TablesC,	InjectionAdvanceTableMainFlash2Location,		MAINTABLE_SIZE);
//	memcpy((void*)&TablesD,	InjectionAdvanceTableSecondaryFlash2Location,	MAINTABLE_SIZE);
//}


/** @brief Buffer tunable tables addresses
 *
 * Save pointers to the tunable tables which live in paged flash and their
 * sub-sections too.
 *
 * @note Many thanks to Jean Bélanger for the inspiration/idea to do this!
 *
 * @author Fred Cooke
 */
void initTunableAddresses(){
	/* Setup addresses within the page to avoid warnings */
	SmallTablesAFlashLocation 	= (void*)&SmallTablesAFlash;
	SmallTablesBFlashLocation 	= (void*)&SmallTablesBFlash;
	SmallTablesCFlashLocation 	= (void*)&SmallTablesCFlash;
	SmallTablesDFlashLocation 	= (void*)&SmallTablesDFlash;
	SmallTablesAFlash2Location	= (void*)&SmallTablesAFlash2;
	SmallTablesBFlash2Location	= (void*)&SmallTablesBFlash2;
	SmallTablesCFlash2Location	= (void*)&SmallTablesCFlash2;
	SmallTablesDFlash2Location	= (void*)&SmallTablesDFlash2;

	/* TablesA */
	dwellDesiredVersusVoltageTableLocation    = (void*)&SmallTablesAFlash.dwellDesiredVersusVoltageTable;
	dwellDesiredVersusVoltageTable2Location   = (void*)&SmallTablesAFlash2.dwellDesiredVersusVoltageTable;
	injectorDeadTimeTableLocation             = (void*)&SmallTablesAFlash.injectorDeadTimeTable;
	injectorDeadTimeTable2Location            = (void*)&SmallTablesAFlash2.injectorDeadTimeTable;
	postStartEnrichmentTableLocation          = (void*)&SmallTablesAFlash.postStartEnrichmentTable;
	postStartEnrichmentTable2Location         = (void*)&SmallTablesAFlash2.postStartEnrichmentTable;
	engineTempEnrichmentTableFixedLocation    = (void*)&SmallTablesAFlash.engineTempEnrichmentTableFixed;
	engineTempEnrichmentTableFixed2Location   = (void*)&SmallTablesAFlash2.engineTempEnrichmentTableFixed;
	primingVolumeTableLocation                = (void*)&SmallTablesAFlash.primingVolumeTable;
	primingVolumeTable2Location               = (void*)&SmallTablesAFlash2.primingVolumeTable;
	engineTempEnrichmentTablePercentLocation  = (void*)&SmallTablesAFlash.engineTempEnrichmentTablePercent;
	engineTempEnrichmentTablePercent2Location = (void*)&SmallTablesAFlash2.engineTempEnrichmentTablePercent;
	dwellMaxVersusRPMTableLocation            = (void*)&SmallTablesAFlash.dwellMaxVersusRPMTable;
	dwellMaxVersusRPMTable2Location           = (void*)&SmallTablesAFlash2.dwellMaxVersusRPMTable;

	/* TablesB */
	perCylinderFuelTrimsLocation  = (void*)&SmallTablesBFlash.perCylinderFuelTrims;
	perCylinderFuelTrims2Location = (void*)&SmallTablesBFlash2.perCylinderFuelTrims;

	/* TablesC */
	// TODO

	/* TablesD */
	// TODO

	/* filler defs */
	fillerALocation  = (void*)&SmallTablesAFlash.filler;
	fillerA2Location = (void*)&SmallTablesAFlash2.filler;
	fillerBLocation  = (void*)&SmallTablesBFlash.filler;
	fillerB2Location = (void*)&SmallTablesBFlash2.filler;
	fillerCLocation  = (void*)&SmallTablesCFlash.filler;
	fillerC2Location = (void*)&SmallTablesCFlash2.filler;
	fillerDLocation  = (void*)&SmallTablesDFlash.filler;
	fillerD2Location = (void*)&SmallTablesDFlash2.filler;
}


/**
 *
 */
//void initPagedRAMTune(){
//	/* Copy the tables from flash to RAM */
//	RPAGE = RPAGE_TUNE_ONE;
//	memcpy((void*)&TablesA,	SmallTablesAFlashLocation,	MAINTABLE_SIZE);
//	memcpy((void*)&TablesB,	SmallTablesBFlashLocation,	MAINTABLE_SIZE);
//	memcpy((void*)&TablesC,	SmallTablesCFlashLocation,	MAINTABLE_SIZE);
//	memcpy((void*)&TablesD,	SmallTablesDFlashLocation,	MAINTABLE_SIZE);
//	RPAGE = RPAGE_TUNE_TWO;
//	memcpy((void*)&TablesA,	SmallTablesAFlash2Location,	MAINTABLE_SIZE);
//	memcpy((void*)&TablesB,	SmallTablesBFlash2Location,	MAINTABLE_SIZE);
//	memcpy((void*)&TablesC,	SmallTablesCFlash2Location,	MAINTABLE_SIZE);
//	memcpy((void*)&TablesD,	SmallTablesDFlash2Location,	MAINTABLE_SIZE);
//}


/** @brief Buffer addresses of paged data
 *
 * Save the paged memory addresses to variables such that we can access them
 * from another paged block with no warnings.
 *
 * If you try to access paged data from the wrong place you get nasty warnings.
 * These calls to functions that live in the same page that they are addressing
 * prevent those warnings.
 *
 * @note Many thanks to Jean Bélanger for the inspiration/idea to do this!
 *
 * @author Fred Cooke
 */
void initAllPagedAddresses(){
	/* Setup pointers to lookup tables */
	initLookupAddresses();
	/* Setup pointers to the main tables */
	initFuelAddresses();
	initTimingAddresses();
	initTunableAddresses();
}


/** @brief Copies paged flash to RAM
 *
 * Take the tables and config from flash up to RAM to allow live tuning.
 *
 * For the main tables and other paged config we need to adjust
 * the RPAGE value to the appropriate one before copying up.
 *
 * This function is simply a delegator to the ones for each flash page. Each
 * one lives in the same paged space as the data it is copying up.
 *
 * @author Fred Cooke
 */
void initAllPagedRAM(){
	/* Setup the flash block pointers before copying flash to RAM using them */
	initAllPagedAddresses();

	/* Copy the tables up to their paged RAM blocks through the window from flash */
//	initPagedRAMFuel();
//	initPagedRAMTime();
//	initPagedRAMTune();
//
//	/* Default to page one for now, perhaps read the configured port straight out of reset in future? TODO */
//	setupPagedRAM(TRUE); // probably something like (PORTA & TableSwitchingMask)
}


/* Initialise and set up all running variables that require a non-zero start value here */
/* All other variables are initialised to zero by the premain built in code				*/
void initVariables(){
	/* And the opposite for the other halves */
	CoreVars = &CoreVars0;
	DerivedVars = &DerivedVars0;
	ADCArrays = &ADCArrays0;
	ADCArraysRecord = &ADCArrays1;
	asyncADCArrays = &asyncADCArrays0;
	asyncADCArraysRecord = &asyncADCArrays1;
	currentDwellMath = &currentDwell0;
	currentDwellRealtime = &currentDwell1;

	injectorMainPulseWidthsMath = injectorMainPulseWidths0;
	injectorMainPulseWidthsRealtime = injectorMainPulseWidths1;
	injectorStagedPulseWidthsMath = injectorStagedPulseWidths0;
	injectorStagedPulseWidthsRealtime = injectorStagedPulseWidths1;

	mathSampleTimeStamp = &ISRLatencyVars.mathSampleTimeStamp0; // TODO temp, remove
	mathSampleTimeStampRecord = &ISRLatencyVars.mathSampleTimeStamp1; // TODO temp, remove
	RPM = &RPM0; // TODO temp, remove
	RPMRecord = &RPM1; // TODO temp, remove

	/* Setup the pointers to the registers for fueling use, this does NOT work if done in global.c, I still don't know why. */
	injectorMainTimeRegisters[0] = TC2_ADDR;
	injectorMainTimeRegisters[1] = TC3_ADDR;
	injectorMainTimeRegisters[2] = TC4_ADDR;
	injectorMainTimeRegisters[3] = TC5_ADDR;
	injectorMainTimeRegisters[4] = TC6_ADDR;
	injectorMainTimeRegisters[5] = TC7_ADDR;
	injectorMainControlRegisters[0] = TCTL2_ADDR;
	injectorMainControlRegisters[1] = TCTL2_ADDR;
	injectorMainControlRegisters[2] = TCTL1_ADDR;
	injectorMainControlRegisters[3] = TCTL1_ADDR;
	injectorMainControlRegisters[4] = TCTL1_ADDR;
	injectorMainControlRegisters[5] = TCTL1_ADDR;

	configuredBasicDatalogLength = maxBasicDatalogLength;

	// TODO perhaps read from the ds1302 once at start up and init the values or different ones with the actual time and date then update them in RTI
}


/** @brief Flash module setup
 *
 * Initialise configuration registers for the flash module to allow burning of
 * non-volatile flash memory from within the firmware.
 *
 * The FCLKDIV register can be written once only after reset, thus the lower
 * seven bits and the PRDIV8 bit must be set at the same time.
 *
 * We want to put the flash clock as high as possible between 150kHz and 200kHz
 *
 * The oscillator clock is 16MHz and because that is not above 12.8MHz we will
 * not set the PRDIV8 bit to further divide by 8 bits as per the manual.
 *
 * 8MHz = 8000KHz
 *
 * 8000kHz / 200kHz = 40 thus we want to set the divide register to 40 or 0x0A
 *
 * @author Sean Keys
 *
 * @note If you use a different crystal lower than 12.8MHz PRDIV8 should not be set.
 *
 * @warning If the frequency you end up with is outside 150kHz - 200kHz you may
 *          damage your flash module or get corrupt data written to it.
 */
void initFlash(){
	// FCLKDIV: 0x28 = 40 (十进制)
	// Flash 时钟频率 = 总线频率 / (PRDIV8 ? 8 : 1) / (FCLKDIV + 1)
	// 对于 40MHz 总线：40MHz / 40 = 1MHz（在 150-200kHz 范围内，需要调整）
	FCLKDIV = 0x28;
	// FPROT: 0xFF = 禁用所有 flash 保护
	// 允许从代码中写入 flash
	FPROT = 0xFF;
	// FSTAT: 清除任何错误标志
	// PVIOL = 保护违反错误
	// ACCERR = 访问错误
	FSTAT = FSTAT | (PVIOL | ACCERR);
}

/* Set up the timer module and its various interrupts */
void initECTTimer(){

	// TODO 重新安排这些东西的顺序，并将使能和中断使能提取到 init 的最后一个函数调用中


#ifndef NO_INIT
	/* 定时器通道中断 */
	// TIE: 0x21 = 0b00100001
	// BIT0: 通道 0 输入捕获中断使能（主 RPM 输入）
	// BIT5: 通道 5 输入捕获中断使能（次 RPM 输入）
	// 通道 1-4,6-7 的输出比较中断禁用，这样在调度之前不会发生喷油器切换
	TIE = 0x21;//0x03;
	// TFLG: 清除所有标志，这样在它们首次发生之前我们就已经运行了
	TFLG = ONES;
	// TFLGOF: 清除所有溢出标志
	TFLGOF = ONES;

	/* TODO 打开定时器并设置速率和溢出中断 */
	// TSCR1: 0x80 = 0b10000000
	// BIT7 (TEN): 1 = 定时器使能
	TSCR1 = 0x80;
	// TSCR2: 0x84 = 0b10000100
	// BIT7 (TOI): 1 = 溢出中断使能
	// BIT2-0: 预分频器 = 4 (除以 16)
	// 定时器频率 = 40MHz / 16 = 2.5MHz，每个计数 = 0.4μs
	TSCR2 = 0x84;
	/* http://www.google.com/search?hl=en&safe=off&q=1+%2F+%2840MHz+%2F+32+%29&btnG=Search */
	/* http://www.google.com/search?hl=en&safe=off&q=1+%2F+%2840MHz+%2F+32+%29+*+2%5E16&btnG=Search */
	/* www.mecheng.adelaide.edu.au/robotics_novell/WWW_Devs/Dragon12/LM4_Timer.pdf */

	/* 初始操作 */
	/* ms2extra 引脚配置
	0 = RPM/位置输入主输入
	1 = 喷油器 1
	2 = 喷油器 3
	3 = 喷油器 2
	4 = 喷油器 4
	5 = RPM/位置输入次输入
	6 = IAC1
	7 = IAC2
	*/
	// TIOS: 0xDE = 0b11011110
	// 通道 0 和 5 是输入捕获，通道 1-4 和 6-7 是输出比较
	TIOS = 0xDE;
	// TCTL1: 在启动时设置为禁用，使用这些和其他标志在解码器内部开关燃油
	TCTL1 = ZEROS;
	// TCTL2: 0,1 的比较关闭，因为它们处于 IC 模式
	TCTL2 = ZEROS;
	// TCTL3: 0x0C = 0b00001100
	// IC5（次输入）在双边沿捕获，4,6,7 的捕获关闭
	TCTL3 = 0x0C;
	// TCTL4: 0x03 = 0b00000011
	// IC0（主输入）在双边沿捕获，1,2,3 的捕获关闭
	TCTL4 = 0x03;
#endif
}


/* Setup the sci module(s) that we need to use. */
void initSCIStuff(){
	/* 替代寄存器集选择器默认为零 */

	// 设置波特率/数据速度
	// SCI0BD: 波特率分频器，从配置中读取
	SCI0BD = fixedConfigs1.serialSettings.baudDivisor;

	// 等等

	/* 切换到替代寄存器集？ */

	// 等等

	/* 再次切换回来？ */

	/*
	 * SCI0CR1: 0x13 = 0b00010011
	 * BIT7 = 0: LOOPS (正常双线操作)
	 * BIT6 = 0: SCISWAI (等待模式开启)
	 * BIT5 = 0: RSRC (如果 loops=1，内部/外部接线)
	 * BIT4 = 1: M MODE (9 位操作)
	 * BIT3 = 0: WAKE (空闲线唤醒)
	 * BIT2 = 0: ILT (空闲线类型计数起始位置)
	 * BIT1 = 1: PE (奇偶校验开启)
	 * BIT0 = 1: PT (奇校验) (minicom 默认无奇偶校验)
	 *
	 * 00010011 = 0x13
	 */
	SCI0CR1 = 0x13;

	/*
	 * SCI0CR2: 0x2C = 0b00101100
	 * BIT7 = 0: TIE (发送数据空中断禁用)
	 * BIT6 = 0: TCIE (发送完成中断禁用)
	 * BIT5 = 1: RIE (接收满中断使能)
	 * BIT4 = 0: ILIE (空闲线中断禁用)
	 * BIT3 = 1: TE (发送使能)
	 * BIT2 = 1: RE (接收使能)
	 * BIT1 = 0: RWU (接收唤醒正常)
	 * BIT0 = 0: SBK (发送中断关闭)
	 *
	 * 00101100 = 0x2C
	 */
	SCI0CR2 = 0x2C;
}

/* TODO Load and calculate all configuration data required to run */
void initConfiguration(){
//	// TODO Calc TPS ADC range on startup or every time? this depends on whether we ensure that things work without a re init or reset or not.


	/* Add in tunable physical parameters at boot time TODO move to init.c TODO duplicate for secondary fuel? or split somehow?
	 *nstant = ((masterConst * perCylinderVolume) / (stoichiometricAFR * injectorFlow));
	 *nstant = ((139371764	 * 16384			) / (15053			   * 4096		 ));
	 * OR
	 *nstant = ((masterConst / injectorFlow) * perCylinderVolume) / stoichiometricAFR;
	 *nstant = ((139371764	 / 4096		   ) * 16384			) / 15053			 ;
	 * http://www.google.com/search?hl=en&safe=off&q=((139371764++%2F+4096+++++)+*+16384+++)+%2F+15053++++&btnG=Search */
	bootFuelConst = ((unsigned long)(masterFuelConstant / fixedConfigs1.engineSettings.injectorFlow) * fixedConfigs1.engineSettings.perCylinderVolume) / fixedConfigs1.engineSettings.stoichiometricAFR;

	/* The MAP range used to convert fake TPS from MAP and vice versa */
	TPSMAPRange = fixedConfigs2.sensorRanges.TPSOpenMAP - fixedConfigs2.sensorRanges.TPSClosedMAP;

	/* The ADC range used to generate TPS percentage */
	TPSADCRange = fixedConfigs2.sensorRanges.TPSMaximumADC - fixedConfigs2.sensorRanges.TPSMinimumADC;


	/* Use like flags for now, just add one for each later */
	unsigned char cumulativeConfigErrors = 0;

	/* Check various aspects of config which will cause problems */

	/* BRV max bigger than variable that holds it */
	if(((unsigned long)fixedConfigs2.sensorRanges.BRVMinimum + fixedConfigs2.sensorRanges.BRVRange) > 65535){
		//sendError(BRV_MAX_TOO_LARGE);
		cumulativeConfigErrors++;
	}

	// TODO check all critical variables here!

	/*
	 * check ignition settings for range etc, possibly check some of those on the fly too
	 * check fuel settings for being reasonable
	 * check all variable tables for correct sizing
	 * etc
	 */

	while(cumulativeConfigErrors > 0){
		sleep(1000);
		PORTS_BA ^= ONES16; // flash leds
		//send("There were ");
		//sendUC(cumulativeConfigErrors);
		//send(" config errors, init aborted!");
	} // TODO ensure that we can recieve config and settings via serial while this is occuring! If not a bad config will lock us out all together.
}


/* Set up all the remaining interrupts */
void initInterrupts(){
	/* 设置实时中断 */
	// RTICTL: 0x10 = 0b00010000
	// BIT7-4: 预分频器选择 = 1 (除以 2^1 = 2)
	// BIT3-0: 模数计数器 = 0 (除以 2^0 = 1)
	// RTI 周期 = 1 / (8MHz / 2^1 / 2^0) = 1 / 4MHz = 0.25μs * 512 = 128μs
	RTICTL = 0x10;
	// CRGINT: 启用 RTI 中断
	CRGINT |= 0x80;
	// CRGFLG: 清除 RTI 标志
	CRGFLG = 0x80;

	// TODO set up irq and xirq for testing
	// IRQCR for IRQ
	//

	/* VReg API setup (only for wait mode? i think so) */
//	VREGAPIR = 0x09C3; /* For 500ms period : (500ms - 0.2ms) / 0.2ms = 0b100111000011 = 2499 */
//	VREGAPICL = 0x02; /* Enable the interrupt */
//	VREGAPICL = 0x04; /* Start the counter running */
	/* Writing a one to the flag will set it if it is unset, so best not to mess with it here as it probably starts off unset */

	/* LVI Low Voltage Interrupt enable */
	VREGCTRL = 0x02; // Counts bad power events for diagnosis reasons
}
