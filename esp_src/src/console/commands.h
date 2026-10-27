/*
 * Console commands (serial and telnet), one grammar for all of them:
 *
 *   <command>                  shows it (-v: everything)
 *   <command> <setting>        shows that setting
 *   <command> <setting> <v>    sets it, at once; kept once saved (save)
 *   <command> <action> [arg]   does it
 *
 * A command that is itself a setting takes its value directly ("brightness
 * auto", "beeps on"). The table of commands, their settings and actions,
 * the values each takes and the help are in commands.cpp. Settings held by
 * the PIC are written whole with CMD-SETTINGS (pic_link_write_settings), and
 * PIC actions sent as CMD-USER; the PIC's answer is printed to the console
 * that asked. An empty line prints the status.
 *
 * Also the ESP commands the PIC sends (SML_USER_ESP_*, from its menu).
 */
#ifndef COMMANDS_H
#define COMMANDS_H

#include <Arduino.h>
#include <stdint.h>

// Runs one line; line is modified.
void commands_execute(Stream *output, char *line);
// The ESP's and the PIC's state: a line or so each, or everything.
void commands_print_status(Stream *output, bool verbose);
// Why the ESP last started, as text.
const char *commands_reset_reason(void);
// For pic_link_set_command_handler().
uint16_t commands_from_pic(uint16_t command, uint32_t argument, uint16_t *detail, bool *restart);

#endif // COMMANDS_H
