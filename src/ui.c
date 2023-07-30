#include "ui.h"

extern time_t local;

uint8_t beep_seq = 0;
bool beep_start = 0;
uint8_t buzzer_buffer[BUZZER_BUFFER_LENGTH] = {0};

bool button_state;
bool button_last_state;

bool switch_state;
bool switch_last_state;

void ui_init(void)
{

}

void ui_tasks(void)
{
    ui_button_task();
    ui_buzzer_task();
    ui_display_task();
}

void ui_button_task(void)
{
    bool update_display = 0;

    if(ui_button_input_state() == button_last_state)
    {
        button_state = ui_button_input_state();
        update_display = 1;
    }
    button_last_state = ui_button_input_state();
    
    if(ui_switch_input_state() == switch_last_state)
    {
        switch_state = ui_switch_input_state();
        update_display = 1;
    }
    switch_last_state = ui_switch_input_state();
    
    if(update_display) ui_display_task();
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
    uint8_t minute = local_tm->tm_min;
    uint8_t hour = local_tm->tm_hour;
    
    if(ui_switch_state())
    {
        if(minute%BEEP_MINOR_INTERVAL==0)
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
    display_time(&local);
    display_latch();
}

void print_iso8601_string(time_t iso)
{
    char buf[32] = {0}; // Allocate buffer
    struct tm *iso_time; // Allocate buffer
    iso_time = gmtime(&iso);
    strftime(buf, 32, "%Y-%m-%dT%H:%M:%SZ", iso_time);
    printf(buf);
}