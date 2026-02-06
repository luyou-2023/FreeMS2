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


/**	@file injectionISRs.c
 * @ingroup interruptHandlers
 *
 * @brief Injection ISR substitutions
 *
 * This file defines the pin specific names for each interrupt and all of it's
 * pin specific variables then imports the actual code from inc/injectorISR.c
 * for each pin such that each one is unique and references a separate set of
 * values specific to it while only maintaining a single copy of the code.
 *
 * @see injectorISR.c
 *
 * @author Fred Cooke
 */


#define INJECTIONISRS_C
#include "inc/FreeMS2.h"
#include "inc/interrupts.h"
#include "inc/injectionISRs.h"


/* 为每个通道正确定义变量，然后导入代码
 * 使用宏定义和 #include 来生成多个 ISR 实例，避免代码重复 */

/* 通道 1 */
#define INJECTOR_CHANNEL_NUMBER 0  // 定义通道编号为 0
#define InjectorXISR Injector1ISR  // 定义 ISR 函数名为 Injector1ISR
#define STAGEDXOFF STAGED1OFF  // 定义分级喷油器关闭掩码
#define STAGEDXON STAGED1ON  // 定义分级喷油器开启掩码
#include "inc/injectorISR.c"  // 包含共享的 ISR 代码，宏定义会被替换
#undef InjectorXISR  // 取消定义，为下一个通道准备
#undef STAGEDXOFF  // 取消定义
#undef STAGEDXON  // 取消定义
#undef INJECTOR_CHANNEL_NUMBER  // 取消定义

/* 通道 2 */
#define INJECTOR_CHANNEL_NUMBER 1  // 定义通道编号为 1
#define InjectorXISR Injector2ISR  // 定义 ISR 函数名为 Injector2ISR
#define STAGEDXOFF STAGED2OFF  // 定义分级喷油器关闭掩码
#define STAGEDXON STAGED2ON  // 定义分级喷油器开启掩码
#include "inc/injectorISR.c"  // 包含共享的 ISR 代码
#undef InjectorXISR  // 取消定义
#undef STAGEDXOFF  // 取消定义
#undef STAGEDXON  // 取消定义
#undef INJECTOR_CHANNEL_NUMBER  // 取消定义

/* 通道 3 */
#define INJECTOR_CHANNEL_NUMBER 2  // 定义通道编号为 2
#define InjectorXISR Injector3ISR  // 定义 ISR 函数名为 Injector3ISR
#define STAGEDXOFF STAGED3OFF  // 定义分级喷油器关闭掩码
#define STAGEDXON STAGED3ON  // 定义分级喷油器开启掩码
#include "inc/injectorISR.c"  // 包含共享的 ISR 代码
#undef InjectorXISR  // 取消定义
#undef STAGEDXOFF  // 取消定义
#undef STAGEDXON  // 取消定义
#undef INJECTOR_CHANNEL_NUMBER  // 取消定义

/* 通道 4 */
#define INJECTOR_CHANNEL_NUMBER 3  // 定义通道编号为 3
#define InjectorXISR Injector4ISR  // 定义 ISR 函数名为 Injector4ISR
#define STAGEDXOFF STAGED4OFF  // 定义分级喷油器关闭掩码
#define STAGEDXON STAGED4ON  // 定义分级喷油器开启掩码
#include "inc/injectorISR.c"  // 包含共享的 ISR 代码
#undef InjectorXISR  // 取消定义
#undef STAGEDXOFF  // 取消定义
#undef STAGEDXON  // 取消定义
#undef INJECTOR_CHANNEL_NUMBER  // 取消定义

/* 通道 5 */
#define INJECTOR_CHANNEL_NUMBER 4  // 定义通道编号为 4
#define InjectorXISR Injector5ISR  // 定义 ISR 函数名为 Injector5ISR
#define STAGEDXOFF STAGED5OFF  // 定义分级喷油器关闭掩码
#define STAGEDXON STAGED5ON  // 定义分级喷油器开启掩码
#include "inc/injectorISR.c"  // 包含共享的 ISR 代码
#undef InjectorXISR  // 取消定义
#undef STAGEDXOFF  // 取消定义
#undef STAGEDXON  // 取消定义
#undef INJECTOR_CHANNEL_NUMBER  // 取消定义

/* 通道 6 */
#define INJECTOR_CHANNEL_NUMBER 5  // 定义通道编号为 5
#define InjectorXISR Injector6ISR  // 定义 ISR 函数名为 Injector6ISR
#define STAGEDXOFF STAGED6OFF  // 定义分级喷油器关闭掩码
#define STAGEDXON STAGED6ON  // 定义分级喷油器开启掩码
#include "inc/injectorISR.c"  // 包含共享的 ISR 代码
#undef InjectorXISR  // 取消定义
#undef STAGEDXOFF  // 取消定义
#undef STAGEDXON  // 取消定义
#undef INJECTOR_CHANNEL_NUMBER  // 取消定义

/* 如果切换到 8 个 OC 通道且使用非 IC 发动机输入，在这里放置另外两组定义 :-)
 * （当然还需要所有其他必要的修改） */
