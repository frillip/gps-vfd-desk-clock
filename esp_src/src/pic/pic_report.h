/*
 * The PIC's state as text, from the SML messages it sends, and names for the
 * SML wire codes.
 */
#ifndef PIC_REPORT_H
#define PIC_REPORT_H

#include <Arduino.h>
#include <stdint.h>

const char *pic_report_command_name(uint16_t command);
const char *pic_report_result_name(uint16_t result);
const char *pic_report_error_name(uint16_t error);
const char *pic_report_source_name(uint8_t source);
const char *pic_report_lock_name(uint8_t lock);
const char *pic_report_tz_source_name(uint8_t source);

// Time, synchroniser, loop, GNSS, display and link.
void pic_report_print(Stream *output);
// STAT-SOURCES: the PIC's time sources, ranked (pic-show-sources).
void pic_report_print_sources(Stream *output);
// The link counters only.
void pic_report_print_link(Stream *output);

#endif // PIC_REPORT_H
