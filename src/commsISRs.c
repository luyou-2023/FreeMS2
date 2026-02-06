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


/**	@file commsISRs.c
 * @ingroup interruptHandlers
 * @ingroup communicationsFiles
 *
 * @brief Send and receive bytes serially
 *
 * This file contains the code for both send and receive of serial bytes
 * through the UART SCI0 device. It is purely interrupt driven and controlled
 * by a set of register and non-register flags that are toggled both inside
 * and outside this file. Some additional helper functions are also kept here.
 *
 * @todo TODO SCI0ISR() needs to be split into some hash defines and an include file that formats it to be the ISR for a specific channel.
 *
 * @author Fred Cooke
 */


#define COMMSISRS_C
#include "inc/FreeMS2.h"
#include "inc/interrupts.h"
#include "inc/utils.h"
#include "inc/commsCore.h"
#include "inc/commsISRs.h"


/* The C89 standard is used in the 3.3.6 GCC compiler, please	*
 * see the following URL for more info on inline functions :	*
 * http://gcc.gnu.org/onlinedocs/gcc-3.3.6/Inline.html#Inline	*/


/** @brief 发送字节并递增指针
 *
 * 将字节写入串口数据寄存器启动发送，然后递增发送缓冲区指针并递减待发送长度。
 * 这是串口发送 ISR 中的核心操作，用于逐个字节发送数据包。
 * 
 * @details 为什么需要这个方法：
 * 串口发送是逐字节进行的，每次发送中断只能发送一个字节。此函数：
 * 1. 将字节写入数据寄存器启动发送
 * 2. 更新发送缓冲区指针（指向下一个要发送的字节）
 * 3. 更新剩余长度（用于判断是否发送完成）
 * 
 * 内联函数的原因：
 * - 在 ISR 中频繁调用，内联可以减少函数调用开销
 * - 代码简单，内联不会显著增加代码大小
 * - 提高实时性能
 *
 * @param rawValue 要发送的原始字节（0-255）
 * 
 * @return 无返回值
 *
 * @author Fred Cooke
 * 
 * @note 这是一个 extern inline 函数，总是被内联
 */
extern inline void sendAndIncrement(unsigned char rawValue){
	SCI0DRL = rawValue;  // 将字节写入 SCI0 数据寄存器，启动发送
	TXPacketLengthToSendSCI0--;  // 递减待发送数据包长度
	TXBufferCurrentPositionSCI0++;  // 递增发送缓冲区指针
}


/** @brief 接收字节并递增指针
 *
 * 将接收到的字节存储到接收缓冲区，累加到校验和中，然后递增接收缓冲区指针和长度。
 * 这是串口接收 ISR 中的核心操作，用于逐个字节接收数据包。
 * 
 * @details 为什么需要这个方法：
 * 串口接收是逐字节进行的，每次接收中断只能接收一个字节。此函数：
 * 1. 将字节存储到接收缓冲区
 * 2. 累加到校验和（用于后续验证数据完整性）
 * 3. 更新接收缓冲区指针（指向下一个存储位置）
 * 4. 更新已接收长度（用于判断是否接收完成）
 * 
 * 校验和计算：
 * - 在接收过程中实时计算校验和
 * - 接收完成后与数据包中的校验和比较
 * - 如果不匹配，丢弃数据包
 * 
 * 内联函数的原因：
 * - 在 ISR 中频繁调用，内联可以减少函数调用开销
 * - 代码简单，内联不会显著增加代码大小
 * - 提高实时性能
 *
 * @param value 要存储的字节数据（0-255），同时用于校验和计算
 * 
 * @return 无返回值
 *
 * @author Fred Cooke
 * 
 * @note 这是一个 extern inline 函数，总是被内联
 */
extern inline void receiveAndIncrement(const unsigned char value){
	*RXBufferCurrentPosition = value;  // 将接收到的字节存储到接收缓冲区
	RXCalculatedChecksum += value;  // 将字节值累加到校验和中
	RXBufferCurrentPosition++;  // 递增接收缓冲区指针
	RXPacketLengthReceived++;  // 递增已接收数据包长度
}


/** @brief 重置接收状态
 *
 * 将通信接收状态重置为指定状态，用于错误恢复或开始新的数据包接收。
 * 清除所有接收相关的变量和标志，使接收系统回到初始状态。
 * 
 * @details 为什么需要这个方法：
 * 串口通信中可能发生各种错误：
 * - 数据包损坏（校验和错误）
 * - 数据包过长（超过缓冲区大小）
 * - 通信错误（噪声、过载、帧错误、奇偶校验错误）
 * - 同步丢失（接收到意外的起始字节）
 * 
 * 当发生错误时，必须重置接收状态：
 * 1. 清除接收缓冲区指针和长度
 * 2. 重置校验和
 * 3. 清除状态标志
 * 4. 禁用其他通信接口（如果正在使用缓冲区）
 * 5. 重新启用接收（准备接收下一个数据包）
 * 
 * 如果不重置状态，系统可能：
 * - 继续使用损坏的数据
 * - 缓冲区溢出
 * - 无法接收新数据包
 *
 * @param sourceIDState 要应用到 RX 缓冲区状态变量的状态
 *                      - CLEAR_ALL_SOURCE_ID_FLAGS: 清除所有源 ID 标志
 *                      - COM_SET_SCI0_INTERFACE_ID: 设置 SCI0 接口 ID
 * 
 * @return 无返回值
 *
 * @author Fred Cooke
 * 
 * @todo TODO 此函数在错误的文件中！要么移动头文件声明，要么移动函数！
 */
void resetReceiveState(unsigned char sourceIDState){
	/* 将接收缓冲区指针设置到开始位置 */
	RXBufferCurrentPosition = (unsigned char*)&RXBuffer;

	/* 清零标志、缓冲区长度和校验和 */
	RXPacketLengthReceived = 0;  // 重置已接收数据包长度为 0
	RXCalculatedChecksum = 0;  // 重置计算的校验和为 0
	RXStateFlags = 0;  // 清零接收状态标志

	/* 设置源 ID 状态（清除所有标志或保留一个标志） */
	RXBufferContentSourceID = sourceIDState;

	/* 无论我们设置哪个接口，它都是我们来自的接口。根据定义，
	 * 它必须是打开的，我们希望它保持打开，所以只需关闭所有其他接口。 */
	if(sourceIDState & COM_SET_SCI0_INTERFACE_ID){
		/* 在这里关闭所有其他接口 */
		/// @todo TODO CAN0CTL1 &= CANCTL1_RX_DISABLE;  // 待实现：禁用 CAN0 接收
		/// @todo TODO CAN0CTL1 &= CANCTL1_RX_ISR_DISABLE;  // 待实现：禁用 CAN0 接收中断
		/* SPI ? I2C ? SCI1 ? */  // 待实现：其他接口
	}else if(sourceIDState & COM_SET_CAN0_INTERFACE_ID){
		/* 在这里关闭所有其他接口 */
		/* 目前只有 SCI */
		SCI0CR2 &= SCICR2_RX_DISABLE;  // 禁用 SCI0 接收
		SCI0CR2 &= SCICR2_RX_ISR_DISABLE;  // 禁用 SCI0 接收中断
		/* SPI ? I2C ? SCI1 ? */  // 待实现：其他接口
	}else{ /* 如果清除所有标志，则在所有接口上启用接收 */
		/* 目前只有 SCI */
		SCI0CR2 |= SCICR2_RX_ENABLE;  // 启用 SCI0 接收
		SCI0CR2 |= SCICR2_RX_ISR_ENABLE;  // 启用 SCI0 接收中断
		/// @todo TODO CAN0CTL1 |= CANCTL1_RX_ENABLE;  // 待实现：启用 CAN0 接收
		/// @todo TODO CAN0CTL1 |= CANCTL1_RX_ISR_ENABLE;  // 待实现：启用 CAN0 接收中断
		/* SPI ? I2C ? SCI1 ? */  // 待实现：其他接口
	}
}


/** @brief 串口通信接口 0 中断服务程序
 *
 * SCI0 ISR 处理 SCI0 的所有中断，通过读取标志位并采取相应行动。
 * 主要功能是从缓冲区发送原始字节，以及接收字节、转义处理、校验和计算并存储到缓冲区。
 * 
 * @details 为什么需要这个方法：
 * 串口通信是异步的，必须使用中断驱动方式：
 * - 接收：当数据到达时，硬件触发中断，ISR 立即读取数据
 * - 发送：当发送寄存器空时，硬件触发中断，ISR 发送下一个字节
 * 
 * 如果使用轮询方式：
 * - 会浪费大量 CPU 时间等待数据
 * - 可能错过数据（接收缓冲区溢出）
 * - 无法满足实时性要求
 * 
 * 接收处理：
 * 1. 错误检测：噪声、过载、帧错误、奇偶校验错误
 * 2. 起始字节检测：开始新数据包
 * 3. 转义序列处理：处理特殊字节（ESCAPE_BYTE, START_BYTE, STOP_BYTE）
 * 4. 停止字节检测：完成数据包接收
 * 5. 校验和验证：验证数据完整性
 * 6. 设置处理标志：通知主循环处理数据包
 * 
 * 发送处理：
 * 1. 转义特殊字节：确保特殊字节不会破坏数据包格式
 * 2. 逐个字节发送：每次中断发送一个字节
 * 3. 发送完成检测：所有字节发送完成后发送停止字节
 * 
 * 转义机制：
 * 数据包使用特殊字节（START_BYTE, STOP_BYTE, ESCAPE_BYTE）来标记边界。
 * 如果数据中包含这些字节，需要进行转义：
 * - ESCAPE_BYTE + ESCAPED_START_BYTE → START_BYTE
 * - ESCAPE_BYTE + ESCAPED_STOP_BYTE → STOP_BYTE
 * - ESCAPE_BYTE + ESCAPED_ESCAPE_BYTE → ESCAPE_BYTE
 * 
 * @return 无返回值
 *
 * @author Fred Cooke
 * 
 * @todo TODO 将此代码移到包含文件中，类似于燃油中断，以便可以在多个 UART SCI 设备中使用而不重复
 * @todo TODO 移除使用 IO 端口在特定操作期间点亮 LED 的调试代码
 */
void SCI0ISR(){
	/* 读取状态寄存器 */
	unsigned char flags = SCI0SR1;
	/* 注意：结合读取或写入数据寄存器，这也会清除标志。 */

	/* 开始计时（用于性能测量） */
	unsigned short start = TCNT;

	/* 如果接收中断已启用，检查接收相关标志 */
	if(SCI0CR2 & SCICR2_RX_ISR_ENABLE){
		/* 从寄存器中获取接收到的字节 */
		unsigned char rawByte = SCI0DRL;

		//PORTB |= BIT0;  // 已注释：调试 LED
		PORTB = ONES;  // 调试：设置所有 PORTB 位（用于调试）

		/* 始终记录错误条件 */
		unsigned char resetOnError = 0;
		/* 如果接收线上有噪声，记录它 */
		if(flags & SCISR1_RX_NOISE){
			Counters.serialNoiseErrors++;  // 增加噪声错误计数
			resetOnError++;  // 标记需要重置
		}/* 如果发生溢出，记录它 */
		if(flags & SCISR1_RX_OVERRUN){
			Counters.serialOverrunErrors++;  // 增加溢出错误计数
			resetOnError++;  // 标记需要重置
		}/* 如果发生帧错误，记录它 */
		if(flags & SCISR1_RX_FRAMING){
			Counters.serialFramingErrors++;  // 增加帧错误计数
			resetOnError++;  // 标记需要重置
		}/* 如果发生奇偶校验错误，记录它 */
		if(flags & SCISR1_RX_PARITY){
			Counters.serialParityErrors++;  // 增加奇偶校验错误计数
			resetOnError++;  // 标记需要重置
		}

		/* 由于错误标志而退出 */
		if(resetOnError){
			resetReceiveState(CLEAR_ALL_SOURCE_ID_FLAGS);  // 重置接收状态
			PORTB |= BIT1;  // 调试：设置错误指示 LED
			return;  // 退出 ISR
		}

		/* 如果有数据等待接收 */
		if(flags & SCISR1_RX_REGISTER_FULL){
			PORTB |= BIT2;  // 调试：设置接收指示 LED
			/* 查找起始字节以指示新数据包 */
			if(rawByte == START_BYTE){
				PORTM ^= BIT3;  // 调试：切换起始字节指示 LED
				/* 如果另一个接口正在使用它（注意，清除标志，不正常） */
				if(RXBufferContentSourceID & COM_CLEAR_SCI0_INTERFACE_ID){
					/* 关闭我们的接收 */
					SCI0CR2 &= SCICR2_RX_DISABLE;  // 禁用 SCI0 接收
					SCI0CR2 &= SCICR2_RX_ISR_DISABLE;  // 禁用 SCI0 接收中断
					PORTB |= BIT4;  // 调试：设置接口冲突指示 LED
				}else{
					PORTB |= BIT5;  // 调试：设置正常起始字节指示 LED
					/* 如果我们正在使用它 */
					if(RXBufferContentSourceID & COM_SET_SCI0_INTERFACE_ID){
						/* 增加计数器（在数据包内收到起始字节） */
						Counters.serialStartsInsideAPacket++;
					}
					/* 重置为我们使用它，除非其他人正在使用 */
					resetReceiveState(COM_SET_SCI0_INTERFACE_ID);  // 重置接收状态，设置 SCI0 接口 ID
				}
			}else if(RXPacketLengthReceived >= RX_BUFFER_SIZE){
				/* 缓冲区已满，记录并重置 */
				Counters.serialPacketsOverLength++;  // 增加超长数据包计数
				resetReceiveState(CLEAR_ALL_SOURCE_ID_FLAGS);  // 重置接收状态
				PORTB |= BIT6;  // 调试：设置缓冲区满指示 LED
			}else if(RXBufferContentSourceID & COM_SET_SCI0_INTERFACE_ID){
				// 如果设置了转义下一个字节标志（上一个字节是转义字节）
				if(RXStateFlags & RX_SCI_ESCAPED_NEXT){
					PORTB |= BIT7;  // 调试：设置转义处理指示 LED
					/* 清除转义下一个字节标志，感谢 Karsten！ ((~ != !) == (! ~= ~)) == LOL */
					RXStateFlags &= RX_SCI_NOT_ESCAPED_NEXT;

					// 处理转义的字节
					if(rawByte == ESCAPED_ESCAPE_BYTE){
						/* 存储并校验转义字节 */
						receiveAndIncrement(ESCAPE_BYTE);  // 转义的转义字节 → 转义字节
					}else if(rawByte == ESCAPED_START_BYTE){
						/* 存储并校验起始字节 */
						receiveAndIncrement(START_BYTE);  // 转义的起始字节 → 起始字节
					}else if(rawByte == ESCAPED_STOP_BYTE){
						/* 存储并校验停止字节 */
						receiveAndIncrement(STOP_BYTE);  // 转义的停止字节 → 停止字节
					}else{
						/* 否则重置并记录为数据错误 */
						resetReceiveState(CLEAR_ALL_SOURCE_ID_FLAGS);  // 重置接收状态
						Counters.serialEscapePairMismatches++;  // 增加转义对不匹配计数
					}
				}else if(rawByte == ESCAPE_BYTE){
					PORTA |= BIT0;  // 调试：设置转义字节指示 LED
					/* 设置标志以指示下一个字节应该被解转义。 */
					RXStateFlags |= RX_SCI_ESCAPED_NEXT;  // 标记下一个字节是转义的
				}else if(rawByte == STOP_BYTE){
					PORTM ^= BIT4;  // 调试：切换停止字节指示 LED
					/* 关闭接收 */
					SCI0CR2 &= SCICR2_RX_DISABLE;  // 禁用 SCI0 接收
					SCI0CR2 &= SCICR2_RX_ISR_DISABLE;  // 禁用 SCI0 接收中断

					/* 将校验和恢复到应该的位置
					 * 停止字节前一个字节是接收到的校验和 */
					unsigned char RXReceivedChecksum = (unsigned char)*(RXBufferCurrentPosition - 1);
					RXCalculatedChecksum -= RXReceivedChecksum;  // 从计算的校验和中减去接收到的校验和

					/* 检查校验和是否匹配，以及数据包是否足够大（包含头部、ID、校验和） */
					if(RXPacketLengthReceived < 4){
						resetReceiveState(CLEAR_ALL_SOURCE_ID_FLAGS);  // 数据包太短，重置
						Counters.commsPacketsUnderMinLength++;  // 增加最小长度不足计数
					}else if(RXCalculatedChecksum == RXReceivedChecksum){
						/* 如果校验和匹配，设置处理标志 */
						RXStateFlags |= RX_READY_TO_PROCESS;  // 标记数据包准备就绪，通知主循环处理
						PORTA |= BIT2;  // 调试：设置校验和匹配指示 LED
					}else{
						PORTA |= BIT3;  // 调试：设置校验和不匹配指示 LED
						/* 否则重置状态并记录它 */
						resetReceiveState(CLEAR_ALL_SOURCE_ID_FLAGS);  // 校验和不匹配，重置
						Counters.commsChecksumMismatches++;  // 增加校验和不匹配计数
					}
				}else{
					PORTM ^= BIT5;  // 调试：切换正常数据字节指示 LED
					/* 如果它不是特殊字节，正常处理它！ */
					receiveAndIncrement(rawByte);  // 存储字节并更新校验和
				}
			}else{
				/* 什么都不做：丢弃字节（接口 ID 不匹配） */
				PORTA |= BIT5;  // 调试：设置丢弃字节指示 LED
			}
		}
	}

	/* 如果发送中断已启用，检查寄存器空标志。 */
	if((SCI0CR2 & SCICR2_TX_ISR_ENABLE) && (flags & SCISR1_TX_REGISTER_EMPTY)){
		/* 从缓冲区获取要发送的字节 */
		unsigned char rawValue = *TXBufferCurrentPositionSCI0;

		if(TXPacketLengthToSendSCI0 > 0){
			// 如果当前没有待发送的转义字节
			if(TXByteEscaped == 0){
				/* 如果原始值需要转义 */
				if(rawValue == ESCAPE_BYTE){
					SCI0DRL = ESCAPE_BYTE;  // 先发送转义字节
					TXByteEscaped = ESCAPED_ESCAPE_BYTE;  // 标记下一个字节是转义的转义字节
				}else if(rawValue == START_BYTE){
					SCI0DRL = ESCAPE_BYTE;  // 先发送转义字节
					TXByteEscaped = ESCAPED_START_BYTE;  // 标记下一个字节是转义的起始字节
				}else if(rawValue == STOP_BYTE){
					SCI0DRL = ESCAPE_BYTE;  // 先发送转义字节
					TXByteEscaped = ESCAPED_STOP_BYTE;  // 标记下一个字节是转义的停止字节
				}else{ /* 否则直接发送它 */
					sendAndIncrement(rawValue);  // 发送字节并更新指针和长度
				}
			}else{
				// 发送转义的字节（第二个字节）
				sendAndIncrement(TXByteEscaped);  // 发送转义后的字节
				TXByteEscaped = 0;  // 清除转义标志
			}
		}else{ /* 长度为 0（数据包发送完成） */
			/* 关闭发送中断 */
			SCI0CR2 &= SCICR2_TX_ISR_DISABLE;  // 禁用发送中断
			/* 发送停止字节 */
			SCI0DRL = STOP_BYTE;  // 发送第一个停止字节
			while(!(SCI0SR1 & 0x80)){/* 等待直到能够发送然后继续 */}
			SCI0DRL = STOP_BYTE; // 发送第二个停止字节（临时解决方案，确保至少发送一个，最多两个停止字节，这样能工作，但很混乱...必须有更好的方法）
			/* 清除发送进行中标志 */
//			TXBufferInUseFlags &= COM_CLEAR_SCI0_INTERFACE_ID;  // 已注释
		}
	}

	/* 记录操作花费的时间（用于性能测量） */
	RuntimeVars.serialISRRuntime = TCNT - start;  // 计算 ISR 执行时间
}
