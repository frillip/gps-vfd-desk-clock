/* 
 * File:   scheduler.h
 * Author: Frillip
 *
 * Created on 15 October 2021, 16:45
 */

#ifndef SCHEDULER_H
#define	SCHEDULER_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include <stdint.h>
#include <stdbool.h>
#include "mcc_generated_files/pin_manager.h"
    
#define SCHEDULER_PRECISION 4000 // (8:1 * 1000Hz) / 2 = 4000

void scheduler_init(void);
void scheduler_align(uint32_t fosc);

#ifdef	__cplusplus
}
#endif

#endif	/* SCHEDULER_H */

