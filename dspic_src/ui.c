#include "ui.h"

extern time_t local;
extern uint32_t fosc_freq;
extern int32_t accumulated_clocks;
extern time_t accumulation_start;
extern time_t accumulation_delta;

uint8_t beep_seq = 0;
bool beep_start = 0;
uint8_t buzzer_buffer[BUZZER_BUFFER_LENGTH] = {0};

bool button_state;
bool button_last_state;
bool button_input_last_state;

bool switch_state;
bool switch_last_state;
bool switch_input_last_state;

bool print_data = 0;
bool disable_manual_print = 0;

extern uint16_t display_brightness;

extern bool display_update_pending;
extern time_t local;
extern time_t previous_local;

bool update_display = 0;

UI_DISPLAY_STATE ui_state_current = UI_DISPLAY_STATE_INIT;
uint16_t ui_display_timeout = 0;

UI_MENU_STATE ui_menu_current = UI_MENU_STATE_ROOT;

UI_BUTTON_STATE ui_button_action = UI_BUTTON_STATE_NO_PRESS;
uint16_t ui_button_counter = 0;

void ui_init(void)
{
    ui_state_current = UI_DISPLAY_STATE_CLOCK_HHMM;
}

void ui_tasks(void)
{
    ui_button_task();
    ui_buzzer_task();
    ui_display_task();
}

void ui_button_task(void)
{
    if(ui_button_input_state())
    {
        if(ui_button_action==UI_BUTTON_STATE_NO_PRESS)
        {
            if(ui_button_counter==UI_BUTTON_LONG_PRESS_COUNT)
            {
                ui_button_action=UI_BUTTON_STATE_LONG_PRESS;
            }
            ui_button_counter++;
        }
    }
    else
    {
        if(ui_button_action==UI_BUTTON_STATE_NO_PRESS)
        {
            if(ui_button_counter>UI_BUTTON_SHORT_PRESS_COUNT && ui_button_counter<UI_BUTTON_LONG_PRESS_COUNT)
            {
                ui_button_action=UI_BUTTON_STATE_SHORT_PRESS;
            }
        }
        ui_button_counter=0;
    }
    
    if(ui_switch_input_state() == switch_input_last_state)
    {
        switch_state = ui_switch_input_state();
        if(switch_state != switch_last_state) update_display = 1;
        switch_last_state = switch_state;
    }
    switch_input_last_state = ui_switch_input_state();
}

bool ui_button_state(void)
{
    return button_state;
}

bool ui_switch_state(void)
{
    return switch_state;
}

void ui_buzzer_task(void)
{
    
}

void ui_buzzer_interval_beep(void)
{
    struct tm *local_tm;
    local_tm = gmtime(&local);
    uint8_t second = local_tm->tm_sec;
    uint8_t minute = local_tm->tm_min;
    uint8_t hour = local_tm->tm_hour;

    if(ui_switch_state())
    {
        if(minute%BEEP_MINOR_INTERVAL==0 && !second)
        {
            memset(buzzer_buffer, 0, BUZZER_BUFFER_LENGTH);
            beep_start = 1;
            beep_seq = 0;

            uint8_t beep_count = 0;
            if(!minute) 
            {
                if(!hour) beep_count = 12;
                else if(hour>12) beep_count = (hour - 12);
                else beep_count = hour;
            }
            else beep_count = 1;

            uint8_t i=0;
            while(i<beep_count)
            {
                buzzer_buffer[i*2] = BEEP_LENGTH | 0x80;
                if(i && !((i+1) % BEEP_GROUP_SIZE)) buzzer_buffer[(i*2)+1] = BEEP_PAUSE_GAP;
                else buzzer_buffer[(i*2)+1] = BEEP_GAP;
                i++;
            }
        }
    }
}


void ui_buzzer_sounder(void)
{
    if(beep_start)
    {
        if(!buzzer_buffer[beep_seq])
        {
            ui_buzzer_off();
            beep_start = 0;
        }
        else
        {
            if(buzzer_buffer[beep_seq] & 0x80) ui_buzzer_on();
            else ui_buzzer_off();
            buzzer_buffer[beep_seq]--;
            if(!(buzzer_buffer[beep_seq] & 0x7F)) beep_seq++;
        }
    }
}

void ui_display_task(void)
{
    if(ui_state_current!=UI_DISPLAY_STATE_CLOCK_HHMM && !ui_switch_input_state())
    {
        ui_display_timeout++;
    }
    if(ui_display_timeout==UI_DISPLAY_TIMEOUT_COUNT)
    {
        ui_state_current=UI_DISPLAY_STATE_CLOCK_HHMM;
        update_display = 1;
        ui_display_timeout=0;
    }
    if(ui_button_action==UI_BUTTON_STATE_LONG_PRESS)
    {
        if(ui_state_current==UI_DISPLAY_STATE_CLOCK_HHMM) ui_state_current=UI_DISPLAY_STATE_CLOCK_MMSS;
        else if(ui_state_current==UI_DISPLAY_STATE_CLOCK_MMSS) ui_state_current=UI_DISPLAY_STATE_CLOCK_HHMM;
        update_display = 1;
    }
    else if(ui_button_action==UI_BUTTON_STATE_SHORT_PRESS)
    {
        if(ui_state_current==UI_DISPLAY_STATE_MENU)
        {
            if(ui_menu_current==UI_MENU_STATE_EXIT) ui_menu_current=UI_MENU_STATE_ROOT;
            else ui_menu_current++;
        }
        update_display = 1;
    }
    ui_button_action = UI_BUTTON_STATE_NO_PRESS;

    if(update_display)
    {
        ui_display_timeout=0;
        if(ui_state_current==UI_DISPLAY_STATE_CLOCK_HHMM)
        {
            if(display_update_pending)
            {
                display_time(&previous_local);
                display_latch();
                display_time(&local);
            }
            else
            {
                display_time(&local);
                display_latch();
            }
        }
        if(ui_state_current==UI_DISPLAY_STATE_CLOCK_MMSS)
        {
            if(display_update_pending)
            {
                display_mmss(&previous_local);
                display_latch();
                display_mmss(&local);
            }
            else
            {
                display_mmss(&local);
                display_latch();
            }
        }
        if(ui_state_current==UI_DISPLAY_STATE_MENU)
        {
            display_menu();
            display_latch();
        }
        update_display=0;
    }
}

void ui_uart1_input(char c)
{
    switch (c)
    {
        // print some data if enter has been pressed
        case 0x0d:
            if(!disable_manual_print)
            {
                print_data = 1;
                // Disable spamming in case of cats on keyboards
                disable_manual_print = 1;
            }
            break;

        // Press 'r' for manual resync
        case 0x72: 
            if(pic_pps_manual_resync_available())
            {
                printf("\r\nManual resync\r\n");
                sync_state_machine_set_state(SYNC_NOSYNC_MANUAL);
            }
            break;

        // Reset the entire device if we see 'R'
        case 0x52:
            printf("\r\nRESETTING!!!\r\n");
            __asm__ volatile ( "reset ");
            break; // Pointless, but good practise I guess

        // Brightness up on 'B'
        case 0x42:
            display_brightness_set_manual();
            display_brightness_up(DISPLAY_BRIGHTNESS_STEP);
            printf("BRI: %u\r\n", display_brightness);
            break;

        // Brightness down on 'b'
        case 0x62:
            display_brightness_set_manual();
            display_brightness_down(DISPLAY_BRIGHTNESS_STEP);
            printf("BRI: %u\r\n", display_brightness);
            break;

        // Max brightness on 'M'
        case 0x4D:
            display_brightness_set_manual();
            display_brightness_set(DISPLAY_BRIGHTNESS_MAX);
            printf("BRI: %u\r\n", display_brightness);
            break;

        // Min brightness on 'm'
        case 0x6D:
            display_brightness_set_manual();
            display_brightness_set(DISPLAY_BRIGHTNESS_MIN);
            printf("BRI: %u\r\n", display_brightness);
            break;

        // Full brightness
        case 0x4F:
            display_brightness_set_manual();
            display_brightness_on();
            printf("BRI: %u\r\n", display_brightness);
            break;

        // Display off
        case 0x6F:
            display_brightness_set_manual();
            display_brightness_off();
            printf("BRI: %u\r\n", display_brightness);
            break;
            
        // Auto brightness down on 'a'
        case 0x61:
            display_brightness_set_auto();
            printf("BRI: AUTO\r\n");
            break;

        default:
            break;
    }
}

void ui_print_iso8601_string(time_t iso)
{
    char buf[32] = {0}; // Allocate buffer
    struct tm *iso_time; // Allocate buffer
    iso_time = gmtime(&iso);
    strftime(buf, 32, "%Y-%m-%dT%H:%M:%SZ", iso_time);
    printf(buf);
}

void ui_print_clear_window(void)
{
    printf("\033[2J\033[1;1H"); // Clear the terminal window
}