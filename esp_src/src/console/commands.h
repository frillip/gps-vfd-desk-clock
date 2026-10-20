/*
 * Console commands (serial and telnet): one table of names, arguments and
 * help. PIC commands become SML-CMD-USER requests; the PIC's answer is
 * printed to the console that asked. An empty line prints the status.
 *
 * Also the ESP commands the PIC sends (SML_USER_ESP_*, from its menu).
 */
#ifndef COMMANDS_H
#define COMMANDS_H

#include <Arduino.h>
#include <stdint.h>

// Runs one line; line is modified.
void commands_execute(Stream *output, char *line);
// The ESP's and the PIC's state.
void commands_print_status(Stream *output);
// For pic_link_set_command_handler().
uint16_t commands_from_pic(uint16_t command, uint32_t argument, uint16_t *detail, bool *restart);

#endif // COMMANDS_H
