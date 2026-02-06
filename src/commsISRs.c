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


/** @brief Send And Increment
 *
 * Increment the pointer, decrement the length, and send it!
 *
 * @author Fred Cooke
 *
 * @note This is an extern inline function and as such is always inlined.
 *
 * @param rawValue is the raw byte to be sent down the serial line.
 */
extern inline void sendAndIncrement(unsigned char rawValue){
	SCI0DRL = rawValue;  // 将字节写入 SCI0 数据寄存器，启动发送
	TXPacketLengthToSendSCI0--;  // 递减待发送数据包长度
	TXBufferCurrentPositionSCI0++;  // 递增发送缓冲区指针
}


/** @brief Receive And Increment
 *
 * Store the value and add it to the checksum, then increment the pointer and length.
 *
 * @author Fred Cooke
 *
 * @note This is an extern inline function and as such is always inlined.
 *
 * @param value is the byte of data to store in the buffer and add to the checksum.
 */
extern inline void receiveAndIncrement(const unsigned char value){
	*RXBufferCurrentPosition = value;  // 将接收到的字节存储到接收缓冲区
	RXCalculatedChecksum += value;  // 将字节值累加到校验和中
	RXBufferCurrentPosition++;  // 递增接收缓冲区指针
	RXPacketLengthReceived++;  // 递增已接收数据包长度
}


/** @brief Reset Receive State
 *
 * Reset communications reception to the state provided.
 *
 * @author Fred Cooke
 *
 * @todo TODO this is in the wrong file!! Either move the header declaration or move the function!
 *
 * @param sourceIDState is the state to apply to the RX buffer state variable.
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


/** @brief Serial Communication Interface 0 ISR
 *
 * SCI0 ISR handles all interrupts for SCI0 by reading flags and acting
 * appropriately. Its functions are to send raw bytes out over the wire from a
 * buffer and to receive bytes from the wire un-escape them, checksum them and
 * store them in a buffer.
 *
 * @author Fred Cooke
 *
 * @todo TODO Move this code into an include file much like the fuel interrupts such that it can be used for multiple UART SCI devices without duplication.
 * @todo TODO Remove the debug code that uses the IO ports to light LEDs during specific actions.
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
