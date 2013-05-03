/*
 * Copyright (c) 2012, Regents of the University of California
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the University of California, Berkeley nor the names
 *   of its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Header for wrapper of UART read/write functionality with packet parsing
 *
 * by Austin D. Buchan
 *
 * v.beta
 */

#include "uart.h"
#include "payload.h"
#include "mac_packet.h"

#include <stdio.h>

#ifndef UART_H
#define	UART_H

#define UART_TX_IDLE        0xFF
#define UART_TX_SEND_SIZE   0xFE

#define UART_RX_IDLE        0xFF
#define UART_RX_CHECK_SIZE  0xFE

#define UART_MAX_SIZE 200

typedef void (*packet_callback)(MacPacket);

void uartInit(unsigned int tx_queue_length, unsigned int rx_queue_length, packet_callback rx_cb);

unsigned int uartEnqueueTxPacket(MacPacket packet);
MacPacket uartDequeueRxPacket(void);

unsigned int uartTxQueueEmpty(void);
unsigned int uartTxQueueFull(void);
unsigned int uartGetTxQueueSize(void);
unsigned int uartRxQueueEmpty(void);
unsigned int uartRxQueueFull(void);
unsigned int uartGetRxQueueSize(void);
// Clear all packets off of queue
void uartFlushQueues(void);

// Processes queues and internals
// Should be called regularly
void uartProcess(void);

#endif	/* UART_H */

