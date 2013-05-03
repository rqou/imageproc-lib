#include "uart_driver.h"
#include "uart.h"
#include "carray.h"
#include "mac_packet.h"
#include "payload.h"
#include "ppool.h"
#include "utils.h"

// In/out packet FIFO queues
static CircArray tx_queue, rx_queue;

static MacPacket tx_packet = NULL;
static Payload tx_payload = NULL;
static unsigned char tx_idx;
static unsigned char tx_checksum;

static MacPacket rx_packet = NULL;
static Payload rx_payload = NULL;
static unsigned char rx_idx;
static unsigned char rx_checksum;

static packet_callback rx_callback = NULL;

static unsigned char uartSendPacket(MacPacket packet);

void uartInit(unsigned int tx_queue_length, unsigned int rx_queue_length, packet_callback rx_cb) {
    /// UART2 for RS-232 w/PC @ 230400, 8bit, No parity, 1 stop bit
    unsigned int U2MODEvalue, U2STAvalue, U2BRGvalue;
    U2MODEvalue = UART_EN & UART_IDLE_CON & UART_IrDA_DISABLE &
                  UART_MODE_SIMPLEX & UART_UEN_00 & UART_DIS_WAKE &
                  UART_DIS_LOOPBACK & UART_DIS_ABAUD & UART_UXRX_IDLE_ONE &
                  UART_BRGH_FOUR & UART_NO_PAR_8BIT & UART_1STOPBIT;
    U2STAvalue  = UART_INT_TX & UART_INT_RX_CHAR &UART_SYNC_BREAK_DISABLED &
                  UART_TX_ENABLE & UART_ADR_DETECT_DIS &
                  UART_IrDA_POL_INV_ZERO; // If not, whole output inverted.
    U2BRGvalue  = 9; // =3 for 2.5M Baud
    //U2BRGvalue  = 43; // =43 for 230500Baud (Fcy / ({16|4} * baudrate)) - 1
    //U2BRGvalue  = 86; // =86 for 115200 Baud
    //U2BRGvalue  = 1041; // =1041 for 9600 Baud
    

    OpenUART2(U2MODEvalue, U2STAvalue, U2BRGvalue);

    tx_idx = UART_TX_IDLE;
    rx_idx = UART_RX_IDLE;
    rx_callback = rx_cb;

    tx_queue = carrayCreate(tx_queue_length);
    rx_queue = carrayCreate(rx_queue_length);

    ConfigIntUART2(UART_TX_INT_EN & UART_TX_INT_PR4 & UART_RX_INT_EN & UART_RX_INT_PR4);
    EnableIntU2TX;
    EnableIntU2RX;
}


MacPacket uartDequeueRxPacket(void)
{
    return (MacPacket)carrayPopTail(rx_queue);
}

unsigned int uartEnqueueTxPacket(MacPacket packet) {
    return carrayAddTail(tx_queue, packet);
}

unsigned int uartTxQueueEmpty(void) {
    return carrayIsEmpty(tx_queue);
}

unsigned int uartTxQueueFull(void) {
    return carrayIsFull(tx_queue);
}

unsigned int uartGetTxQueueSize(void) {
    return carrayGetSize(tx_queue);
}

unsigned int uartRxQueueEmpty(void){
    return carrayIsEmpty(rx_queue);
}

unsigned int uartRxQueueFull(void) {
    return carrayIsFull(rx_queue);
}

unsigned int uartGetRxQueueSize(void) {
    return carrayGetSize(rx_queue);
}

void uartFlushQueues(void) {
    while (!carrayIsEmpty(tx_queue)) {
        ppoolReturnFullPacket((MacPacket)carrayPopTail(tx_queue));
    }
    while (!carrayIsEmpty(rx_queue)) {
        ppoolReturnFullPacket((MacPacket)carrayPopTail(rx_queue));
    }
}


void uartProcess(void) {
  // Process pending outgoing packets
  if(!uartTxQueueEmpty()) {
    uartSendPacket(carrayPopHead(tx_queue)); // Process outgoing buffer
    return;
  }
}

static unsigned char uartSendPacket(MacPacket packet) {
    CRITICAL_SECTION_START
    LED_3 = 1;
    if(tx_packet != NULL) {
        ppoolReturnFullPacket(tx_packet);
        tx_packet = NULL;
        tx_idx = UART_TX_IDLE;
    }

    if(tx_idx == UART_TX_IDLE && packet != NULL && packet->payload_length < UART_MAX_SIZE) {
        tx_packet = packet;
        tx_payload = packet->payload;
        tx_checksum = packet->payload_length + 3; // add three for size, size check, and checksum
        tx_idx = UART_TX_SEND_SIZE;
        WriteUART2(tx_checksum);
        CRITICAL_SECTION_END
        LED_3 = 0;
        return 1;
    } else {
        LED_3 = 0;
        CRITICAL_SECTION_END
        return 0;
    }
}

void __attribute__((interrupt, no_auto_psv)) _U2TXInterrupt(void) {
    unsigned char tx_byte;

    CRITICAL_SECTION_START
    _U2TXIF = 0;
    LED_3 = 1;
    if(tx_idx != UART_TX_IDLE) {
        if(tx_idx == UART_TX_SEND_SIZE) {
            tx_idx = 0;
            tx_byte = ~tx_checksum; // send size check
        } else if(tx_idx == tx_payload->data_length + PAYLOAD_HEADER_LENGTH) {
            ppoolReturnFullPacket(tx_packet);
            tx_packet = NULL;
            tx_idx = UART_TX_IDLE;
            tx_byte = tx_checksum;
        } else {
            tx_byte = tx_payload->pld_data[tx_idx++];
        }
        tx_checksum += tx_byte;
        WriteUART2(tx_byte);
    }
    LED_3 = 0;
    CRITICAL_SECTION_END
}

//read data from the UART, and call the proper function based on the Xbee code
void __attribute__((__interrupt__, no_auto_psv)) _U2RXInterrupt(void) {
    unsigned char rx_byte;

    CRITICAL_SECTION_START
    LED_3 = 1;

    while(U2STAbits.URXDA) {
        rx_byte = U2RXREG;

        if(rx_idx == UART_RX_IDLE && rx_byte < UART_MAX_SIZE) {
            rx_checksum = rx_byte;
            rx_idx = UART_RX_CHECK_SIZE;
        } else if(rx_idx == UART_RX_CHECK_SIZE) {
            if((rx_checksum ^ rx_byte) == 0xFF && rx_checksum < UART_MAX_SIZE) {
                rx_packet = ppoolRequestFullPacket(rx_checksum - (PAYLOAD_HEADER_LENGTH+3));
                rx_payload = rx_packet->payload;
                rx_checksum += rx_byte;
                rx_idx = 0;
            } else {
                rx_checksum = rx_byte;
            }
        } else if (rx_idx == rx_payload->data_length + PAYLOAD_HEADER_LENGTH) {
            if(rx_checksum == rx_byte && rx_callback != NULL) {
                // Call callback
                (rx_callback)(rx_packet);

                // Add to queue
                if(uartRxQueueFull()) { return; } // Don't bother if rx queue full
                if(!carrayAddTail(rx_queue, rx_packet)) {
                  ppoolReturnFullPacket(rx_packet); // Check for failure
                }
            } else {
                ppoolReturnFullPacket(rx_packet);
            }
            rx_idx = UART_RX_IDLE;
        } else {
            rx_checksum += rx_byte;
            rx_payload->pld_data[rx_idx++] = rx_byte;
        }
    }

    if(U2STAbits.OERR) {
        U2STAbits.OERR = 0;
    }
    
    _U2RXIF = 0;
    LED_3 = 0;
    CRITICAL_SECTION_END
}
