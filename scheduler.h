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
#include "mcc_generated_files/tmr2.h"
#include "mcc_generated_files/pin_manager.h"

void scheduler_run(void);
void scheduler_align(void);
void scheduler_reset(void);

#ifdef	__cplusplus
}
#endif

#endif	/* SCHEDULER_H */

