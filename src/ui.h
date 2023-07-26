/* 
 * File:   ui.h
 * Author: Frillip
 *
 * Created on 26 July 2023, 21:49
 */

#ifndef UI_H
#define	UI_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "mcc_generated_files/pin_manager.h"

void print_iso8601_string(time_t iso);

#ifdef	__cplusplus
}
#endif

#endif	/* UI_H */

