/* 
 * File:   sc16is7x0.h
 * Author: Frillip
 *
 * Created on 26 February 2022, 22:14
 */

#ifndef SC16IS7X0_H
#define	SC16IS7X0_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "mcc_generated_files/i2c1.h"

#define SC16IS7X0_ADDRESS 0x48

//registers
#define SC16IS7X0_REG_RHR 0x00
#define SC16IS7X0_REG_THR 0x00
#define SC16IS7X0_REG_IER 0x01
#define SC16IS7X0_REG_FCR 0x02
#define SC16IS7X0_REG_LCR 0x03
#define SC16IS7X0_REG_MCR 0x04
#define SC16IS7X0_REG_LSR 0x05
#define SC16IS7X0_REG_MSR 0x06
#define SC16IS7X0_REG_SPR 0x07
#define SC16IS7X0_REG_TXLVL 0x08
#define SC16IS7X0_REG_RXLVL 0x09
#define SC16IS7X0_REG_IODIR 0x0A
#define SC16IS7X0_REG_IOSTATE 0X0B
#define SC16IS7X0_REG_IOINTEN 0X0C
#define SC16IS7X0_REG_IOCONTROL 0x0E
#define SC16IS7X0_REG_EFCR 0X0F

//Special register set
#define SC16IS7X0_REG_DLL 0x00
#define SC16IS7X0_REG_DLH 0X01

#define SC16IS7X0_XTAL 14745600UL

void sc16is7x0_init(uint32_t freq, uint32_t baud);
bool sc16is7x0_available(void);
uint8_t sc16is7x0_rx_lvl(void);
void sc16is7x0_clear_rx_fifo(void);
uint8_t sc16is7x0_tx_lvl(void);
void sc16is7x0_clear_tx_fifo(void);
char sc16is7x0_read(void);
bool sc16is7x0_nread(char *buf, uint8_t n);
void sc16is7x0_write(char c);
bool sc16is7x0_nwrite(char *buf, uint8_t n);
uint8_t sc16is7x0_read_reg(uint8_t reg);
bool sc16is7x0_write_reg(uint8_t reg, uint8_t val);

#ifdef	__cplusplus
}
#endif

#endif	/* SC16IS7X0_H */

