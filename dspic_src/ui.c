#include "ui.h"

extern uint32_t fosc_freq;
extern int32_t accumulated_clocks;
extern time_t accumulation_start;
extern time_t accumulation_delta;

uint8_t beep_seq = 0;
bool beep_start = 0;
struct _buzzer_buffer_element buzzer_buffer[BUZZER_BUFFER_LENGTH];

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
extern time_t display;
extern time_t previous_display;

bool update_display = 0;

UI_DISPLAY_STATE ui_state_current = UI_DISPLAY_STATE_INIT;
UI_DISPLAY_STATE ui_state_selected = UI_DISPLAY_STATE_INIT;
uint16_t ui_display_timeout = 0;

UI_MENU_STATE ui_menu_current = UI_MENU_STATE_ROOT;
bool ui_menu_flash = 0;
bool ui_menu_flash_off = 0;
int16_t ui_menu_flash_counter = 0;

UI_BUTTON_STATE ui_button_action = UI_BUTTON_STATE_NO_PRESS;
uint16_t ui_button_counter = 0;

extern CLOCK_SOURCE utc_source;
extern time_t utc;
extern bool dst_active;
extern int32_t bme280_temperature;

extern EEPROM_DATA_STRUCT settings;
extern EEPROM_DATA_STRUCT modified;

void ui_init(void)
{
    ui_state_current = UI_DISPLAY_STATE_INIT;
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
    struct tm *display_tm;
    display_tm = gmtime(&display);
    uint8_t second = display_tm->tm_sec;
    uint8_t minute = display_tm->tm_min;
    uint8_t hour = display_tm->tm_hour;

    if(settings.fields.beep.flags.enabled && !beep_start)
    {
        if(minute % UI_BEEP_MINOR_INTERVAL==0 && second == 0)
        {
            beep_start = 1;
            beep_seq = 0;

            uint8_t beep_count = ui_buzzer_get_beep_count(hour, minute);
            ui_buzzer_interval_generate_buffer(beep_count);
        }
    }
}

uint8_t ui_buzzer_get_beep_count(uint8_t hour, uint8_t minute)
{
    if(minute == 0) 
    {
        if(UI_BEEP_MIDNIGHT_HOUR)
        {
            return UI_BEEP_MIDNIGHT_BEEP_COUNT;
        }
        else if(hour > UI_BEEP_AM_PM_HOUR_THRESHOLD)
        {
            return (hour - UI_BEEP_AM_PM_HOUR_THRESHOLD);
        }
        else
        {
            return hour;
        }
    }
    return 1;
}

void ui_buzzer_interval_generate_buffer(uint8_t beep_count)
{
    memset(buzzer_buffer, 0, sizeof(buzzer_buffer));
    
    uint8_t i=0;
    while(i<beep_count)
    {
        buzzer_buffer[i*2].state = BUZZER_ON;
        buzzer_buffer[i*2].length = UI_BEEP_LENGTH;
        if(i && !((i+1) % UI_BEEP_GROUP_SIZE))
        {
            buzzer_buffer[(i*2)+1].state = BUZZER_OFF;
            buzzer_buffer[(i*2)+1].length = UI_BEEP_PAUSE_GAP;
        }
        else
        {
            buzzer_buffer[(i*2)+1].state = BUZZER_OFF;
            buzzer_buffer[(i*2)+1].length = UI_BEEP_GAP;
        }
        i++;
    }
}

void ui_buzzer_button_beep(uint8_t beep_count)
{
    if(settings.fields.beep.flags.enabled && !beep_start)
    {
        beep_start = 1;
        beep_seq = 0;
        ui_buzzer_button_generate_buffer(beep_count);
    }
}

void ui_buzzer_button_generate_buffer(uint8_t beep_count)
{
    memset(buzzer_buffer, 0, sizeof(buzzer_buffer));
    
    uint8_t i=0;
    while(i<beep_count)
    {
        buzzer_buffer[i*2].state = BUZZER_ON;
        buzzer_buffer[i*2].length = UI_BEEP_LENGTH_BUTTON;
        buzzer_buffer[(i*2)+1].state = BUZZER_OFF;
        buzzer_buffer[(i*2)+1].length = UI_BEEP_GAP_BUTTON;
        i++;
    }
}

void ui_buzzer_mute(uint8_t length)
{
    beep_start = 1;
    beep_seq = 0;

    memset(buzzer_buffer, 0, sizeof(buzzer_buffer));
    buzzer_buffer[0].state = BUZZER_OFF;
    buzzer_buffer[0].length = length;
}

void ui_buzzer_sounder(void)
{
    if(beep_start)
    {
        if(buzzer_buffer[beep_seq].state == BUZZER_OFF && buzzer_buffer[beep_seq].length == 0)
        {
            ui_buzzer_off();
            beep_start = 0;
        }
        else
        {
            if(buzzer_buffer[beep_seq].state == BUZZER_ON) ui_buzzer_on();
            else ui_buzzer_off();
            buzzer_buffer[beep_seq].length--;
            if((buzzer_buffer[beep_seq].length == 0)) beep_seq++;
        }
    }
}

void ui_display_task(void)
{
    if(ui_menu_flash) ui_menu_flash_counter++;
    if(ui_menu_flash_counter > UI_MENU_FLASH_PERIOD)
    {
        ui_menu_flash_counter = UI_MENU_FLASH_RESET;
        ui_menu_flash_off = 0;
        update_display = 1;
    }
    else if(ui_menu_flash_counter > UI_MENU_FLASH_ON_PERIOD)
    {
        ui_menu_flash_off = 1;
        update_display = 1;
    }
    
    if(utc_source!=CLOCK_SOURCE_NONE)
    {
        if(ui_state_current==UI_DISPLAY_STATE_INIT || ui_state_current==UI_DISPLAY_STATE_DASHES)
        {
            ui_state_current=UI_DISPLAY_STATE_CLOCK_HHMM;
            display_local_time(utc);
            display_latch();
        }
    }
    if(ui_state_current!=settings.fields.display.selected)
    {
        ui_display_timeout++;
    }
    if(ui_display_timeout==UI_DISPLAY_TIMEOUT_COUNT)
    {
        ui_state_current=settings.fields.display.selected;
        ui_menu_change_state(UI_MENU_STATE_ROOT);
        ui_menu_stop_flash();
        memcpy(modified.raw, settings.raw, sizeof(settings.raw));
        update_display = 1;
        ui_display_timeout=0;
    }
    if(ui_button_action==UI_BUTTON_STATE_LONG_PRESS)
    {
        if(ui_state_current==UI_DISPLAY_STATE_MENU)
        {
            ui_menu_long_press();
        }
        else
        {
            ui_state_current=UI_DISPLAY_STATE_MENU;
        }
        ui_buzzer_button_beep(UI_BEEP_COUNT_BUTTON_LONG);
        ui_display_timeout=0;
        update_display = 1;
    }
    else if(ui_button_action==UI_BUTTON_STATE_SHORT_PRESS)
    {
        if(ui_state_current==UI_DISPLAY_STATE_MENU)
        {
            ui_menu_short_press();
        }
        else
        {
            ui_display_cycle();
        }
        ui_buzzer_button_beep(UI_BEEP_COUNT_BUTTON_SHORT);
        ui_display_timeout=0;
        update_display = 1;
    }
    ui_button_action = UI_BUTTON_STATE_NO_PRESS;

    if(update_display)
    {
        if(!ui_menu_flash) ui_display_timeout=0;
        ui_update_display();
        update_display=0;
    }
}

void ui_update_display(void)
{
    if(ui_state_current==UI_DISPLAY_STATE_CLOCK_HHMM)
    {
        if(display_update_pending)
        {
            display_hhmm(&previous_display);
            display_latch();
            display_hhmm(&display);
        }
        else
        {
            display_hhmm(&display);
            display_latch();
        }
    }
    if(ui_state_current==UI_DISPLAY_STATE_CLOCK_MMSS)
    {
        if(display_update_pending)
        {
            display_mmss(&previous_display);
            display_latch();
            display_mmss(&display);
        }
        else
        {
            display_mmss(&display);
            display_latch();
        }
    }
    if(ui_state_current==UI_DISPLAY_STATE_CLOCK_SSMM)
    {
        display_ssmm(&utc);
        display_latch();
    }
    if(ui_state_current==UI_DISPLAY_STATE_CLOCK_YYYY)
    {
        display_yyyy(&utc);
        display_latch();
    }
    if(ui_state_current==UI_DISPLAY_STATE_CLOCK_MMDD)
    {
        display_mmdd(&utc);
        display_latch();
    }
    if(ui_state_current==UI_DISPLAY_STATE_TEMP)
    {
        display_temp(bme280_temperature);
        display_latch();
    }
    if(ui_state_current==UI_DISPLAY_STATE_MENU)
    {
        display_menu();
        display_latch();
    }
    if(ui_state_current==UI_DISPLAY_STATE_DASHES)
    {
        display_dashes();
        display_latch();
    }
    if(ui_state_current==UI_DISPLAY_STATE_INIT)
    {
        display_blank();
        display_latch();
    }
}

void ui_set_display_off(void)
{
    ui_state_current=UI_DISPLAY_STATE_INIT;
    ui_update_display();
}

void ui_set_display_hhmm(void)
{
    ui_state_current=UI_DISPLAY_STATE_CLOCK_HHMM;
    ui_update_display();
}

void ui_set_display_mmss(void)
{
    ui_state_current=UI_DISPLAY_STATE_CLOCK_MMSS;
    ui_update_display();
}

void ui_set_display_ssmm(void)
{
    ui_state_current=UI_DISPLAY_STATE_CLOCK_SSMM;
    ui_update_display();
}

void ui_set_display_dashes(void)
{
    ui_state_current=UI_DISPLAY_STATE_DASHES;
    ui_update_display();
}

void ui_display_cycle(void)
{
    switch(ui_state_current)
    {
        case UI_DISPLAY_STATE_CLOCK_HHMM:
            ui_state_current = UI_DISPLAY_STATE_CLOCK_MMSS;
            break;

        case UI_DISPLAY_STATE_CLOCK_MMSS:
            ui_state_current = UI_DISPLAY_STATE_CLOCK_SSMM;
            break;

        case UI_DISPLAY_STATE_CLOCK_SSMM:
            ui_state_current = UI_DISPLAY_STATE_CLOCK_YYYY;
            break;

        case UI_DISPLAY_STATE_CLOCK_YYYY:
            ui_state_current = UI_DISPLAY_STATE_CLOCK_MMDD;
            break;

        case UI_DISPLAY_STATE_CLOCK_MMDD:
            ui_state_current = UI_DISPLAY_STATE_CLOCK_HHMM;
            break;
            
        default:
            break;
    }
}

void ui_set_display_menu(void)
{
    ui_state_current=UI_DISPLAY_STATE_MENU;
    ui_update_display();
}

void ui_menu_change_state(UI_MENU_STATE new_state)
{
    ui_menu_current = new_state;
}

void ui_menu_start_flash(void)
{
    ui_menu_flash_counter = UI_MENU_FLASH_INITIAL;
    ui_menu_flash_off = 0;
    ui_menu_flash = 1;
}

void ui_menu_stop_flash(void)
{
    ui_menu_flash_counter = UI_MENU_FLASH_INITIAL;
    ui_menu_flash_off = 0;
    ui_menu_flash = 0;
}

void ui_menu_reset_flash(void)
{
    ui_menu_flash_counter = UI_MENU_FLASH_INITIAL;
    ui_menu_flash_off = 0;
}

void ui_menu_long_press(void)
{
    ui_menu_reset_flash();
    switch(ui_menu_current)
    {
        case UI_MENU_STATE_ROOT:
            ui_state_current=settings.fields.display.selected;
            break;
            
        case UI_MENU_STATE_TZ:
            ui_menu_change_state(UI_MENU_STATE_TZ_AUTO);
            break;
            
            case UI_MENU_STATE_TZ_AUTO:
                ui_menu_start_flash();
                modified.fields.tz.flags.automatic = settings.fields.tz.flags.automatic;
                ui_menu_change_state(UI_MENU_STATE_TZ_AUTO_SEL);
                break;
                
            case UI_MENU_STATE_TZ_AUTO_SEL:
                ui_menu_stop_flash();
                settings.fields.tz.flags.automatic = modified.fields.tz.flags.automatic;
                ui_menu_change_state(UI_MENU_STATE_TZ_AUTO);
                break;
            
            case UI_MENU_STATE_TZ_SET:
                ui_menu_start_flash();
                modified.fields.tz.offset = settings.fields.tz.offset;
                ui_menu_change_state(UI_MENU_STATE_TZ_SET_HH);
                break;

                case UI_MENU_STATE_TZ_SET_HH:
                    ui_menu_change_state(UI_MENU_STATE_TZ_SET_MM);
                    break;

                case UI_MENU_STATE_TZ_SET_MM:
                    ui_menu_stop_flash();
                    settings.fields.tz.offset = modified.fields.tz.offset;
                    ui_menu_change_state(UI_MENU_STATE_TZ_SET);
                    break;
            
            case UI_MENU_STATE_TZ_BACK:
                ui_menu_change_state(UI_MENU_STATE_TZ);
                break;

        case UI_MENU_STATE_DST:
            ui_menu_change_state(UI_MENU_STATE_DST_AUTO);
            break;
            
            case UI_MENU_STATE_DST_AUTO:
                ui_menu_start_flash();
                modified.fields.dst.flags.automatic = settings.fields.dst.flags.automatic;
                ui_menu_change_state(UI_MENU_STATE_DST_AUTO_SEL);
                break;
                
                case UI_MENU_STATE_DST_AUTO_SEL:
                    ui_menu_stop_flash();
                    if(modified.fields.dst.flags.automatic && !settings.fields.dst.flags.automatic)
                    {
                        settings.fields.dst.offset = UI_DST_OFFSET_DEFAULT;
                    }
                    settings.fields.dst.flags.automatic = modified.fields.dst.flags.automatic;
                    dst_active = isDST(&utc);
                    ui_menu_change_state(UI_MENU_STATE_DST_AUTO);
                    break;
                
            case UI_MENU_STATE_DST_STATE:
                ui_menu_start_flash();
                modified.fields.dst.flags.active = settings.fields.dst.flags.active;
                ui_menu_change_state(UI_MENU_STATE_DST_STATE_SEL);
                break;
                
                case UI_MENU_STATE_DST_STATE_SEL:
                    ui_menu_stop_flash();
                    settings.fields.dst.flags.active = modified.fields.dst.flags.active;
                    dst_active = isDST(&utc);
                    ui_menu_change_state(UI_MENU_STATE_DST_STATE);
                    break;

            case UI_MENU_STATE_DST_OFFSET:
                ui_menu_start_flash();
                modified.fields.dst.offset = settings.fields.dst.offset;
                ui_menu_change_state(UI_MENU_STATE_DST_OFFSET_SEL);
                break;
                
                case UI_MENU_STATE_DST_OFFSET_SEL:
                    ui_menu_stop_flash();
                    settings.fields.dst.offset = modified.fields.dst.offset;
                    ui_menu_change_state(UI_MENU_STATE_DST_OFFSET);
                    break;
                    
            case UI_MENU_STATE_DST_BACK:
                ui_menu_change_state(UI_MENU_STATE_DST);
                break;

        case UI_MENU_STATE_ALARM:
            ui_menu_change_state(UI_MENU_STATE_ALARM_ENABLED);
            break;
            
            case UI_MENU_STATE_ALARM_ENABLED:
                if(settings.fields.alarm.flags.enabled) settings.fields.alarm.flags.enabled = 0;
                else settings.fields.alarm.flags.enabled = 1;
                break;
                
            case UI_MENU_STATE_ALARM_SET:
                ui_menu_change_state(UI_MENU_STATE_ALARM_SET_HH);
                break;

                case UI_MENU_STATE_ALARM_SET_HH:
                    ui_menu_change_state(UI_MENU_STATE_ALARM_SET_MM);
                    break;

                case UI_MENU_STATE_ALARM_SET_MM:
                    ui_menu_change_state(UI_MENU_STATE_ALARM_SET);
                    break;

        case UI_MENU_STATE_BEEP:
            ui_menu_change_state(UI_MENU_STATE_BEEP_ENABLE);
            break;
            
            case UI_MENU_STATE_BEEP_ENABLE:
                ui_menu_start_flash();
                modified.fields.beep.flags.enabled = settings.fields.beep.flags.enabled;
                ui_menu_change_state(UI_MENU_STATE_BEEP_ENABLE_SEL);
                break;

            case UI_MENU_STATE_BEEP_ENABLE_SEL:
                ui_menu_stop_flash();
                settings.fields.beep.flags.enabled = modified.fields.beep.flags.enabled;
                if(!settings.fields.beep.flags.enabled ) ui_buzzer_mute(UI_BEEP_MUTE_LENGTH);
                ui_menu_change_state(UI_MENU_STATE_BEEP_ENABLE);
                break;
                
            case UI_MENU_STATE_BEEP_BACK:
                ui_menu_change_state(UI_MENU_STATE_BEEP);
                break;
                
        case UI_MENU_STATE_DISPLAY:
            ui_menu_change_state(UI_MENU_STATE_DISPLAY_FORMAT);
            break;

            case UI_MENU_STATE_DISPLAY_FORMAT:
                ui_menu_start_flash();
                modified.fields.display.flags.hour_format = settings.fields.display.flags.hour_format;
                ui_menu_change_state(UI_MENU_STATE_DISPLAY_FORMAT_SEL);
                break;

                case UI_MENU_STATE_DISPLAY_FORMAT_SEL:
                    ui_menu_stop_flash();
                    settings.fields.display.flags.hour_format = modified.fields.display.flags.hour_format;
                    ui_menu_change_state(UI_MENU_STATE_DISPLAY_FORMAT);
                    break;

            case UI_MENU_STATE_DISPLAY_SET:
                ui_menu_start_flash();
                ui_menu_change_state(UI_MENU_STATE_DISPLAY_SET_SEL);
                break;

                case UI_MENU_STATE_DISPLAY_SET_SEL:
                    ui_menu_stop_flash();
                    ui_menu_change_state(UI_MENU_STATE_DISPLAY_SET);
                    break;

            case UI_MENU_STATE_DISPLAY_BACK:
                ui_menu_change_state(UI_MENU_STATE_DISPLAY);
                break;
            
        case UI_MENU_STATE_RESET:
            ui_menu_change_state(UI_MENU_STATE_RESET_WIFI);
            break;
            
            case UI_MENU_STATE_RESET_WIFI:
                ui_menu_start_flash();
                ui_menu_change_state(UI_MENU_STATE_RESET_WIFI_YN);
                break;            
            
                case UI_MENU_STATE_RESET_WIFI_YN:
                    if(modified.fields.reset.flags.wifi)
                    {
                        esp_tx_net(1);
                        modified.fields.reset.flags.wifi = 0;
                    }
                    ui_menu_stop_flash();
                    ui_menu_change_state(UI_MENU_STATE_RESET_WIFI);
                    break;
                
            case UI_MENU_STATE_RESET_SETTINGS:
                ui_menu_start_flash();
                ui_menu_change_state(UI_MENU_STATE_RESET_SETTINGS_YN);
                break;            

                case UI_MENU_STATE_RESET_SETTINGS_YN:
                    if(modified.fields.reset.flags.settings)
                    {
                        eeprom_init();
                        modified.fields.reset.flags.settings = 0;
                    }
                    ui_menu_stop_flash();
                    ui_menu_change_state(UI_MENU_STATE_RESET_SETTINGS);
                    break;
                
            case UI_MENU_STATE_RESET_ALL:
                ui_menu_start_flash();
                ui_menu_change_state(UI_MENU_STATE_RESET_ALL_YN);
                break;
                
                case UI_MENU_STATE_RESET_ALL_YN:
                    if(modified.fields.reset.flags.all)
                    {
                        esp_tx_net(1);
                        eeprom_init();
                        modified.fields.reset.flags.all = 0;
                    }
                    ui_menu_stop_flash();
                    ui_menu_change_state(UI_MENU_STATE_RESET_ALL);
                    break;
                    
            case UI_MENU_STATE_RESET_BACK:
                ui_menu_change_state(UI_MENU_STATE_RESET);
                break;
            
        case UI_MENU_STATE_EXIT:
            ui_state_current=settings.fields.display.selected;
            ui_menu_change_state(UI_MENU_STATE_ROOT);
            break;
            
        default:
            break;
    }
}

void ui_menu_short_press(void)
{
    ui_menu_flash_counter = UI_MENU_FLASH_INITIAL;
    ui_menu_flash_off = 0;
    switch(ui_menu_current)
    {
        case UI_MENU_STATE_ROOT:
            ui_menu_change_state(UI_MENU_STATE_TZ);
            break;

            
        case UI_MENU_STATE_TZ:
            ui_menu_change_state(UI_MENU_STATE_DST);
            break;
            
            case UI_MENU_STATE_TZ_AUTO:
                if(settings.fields.tz.flags.automatic)
                {
                    ui_menu_change_state(UI_MENU_STATE_TZ_BACK);
                }
                else ui_menu_change_state(UI_MENU_STATE_TZ_SET);
                break;
                
                case UI_MENU_STATE_TZ_AUTO_SEL:
                    modified.fields.tz.flags.automatic = !modified.fields.tz.flags.automatic;
                    break;

            case UI_MENU_STATE_TZ_SET:
                ui_menu_change_state(UI_MENU_STATE_TZ_BACK);
                break;
                
                case UI_MENU_STATE_TZ_SET_HH:
                    ui_tz_offset_incr_hh();
                    break;
                    
                case UI_MENU_STATE_TZ_SET_MM:
                    ui_tz_offset_incr_mm();
                    break;

            case UI_MENU_STATE_TZ_BACK:
                ui_menu_change_state(UI_MENU_STATE_TZ_AUTO);
                break;

            
        case UI_MENU_STATE_DST:
            ui_menu_change_state(UI_MENU_STATE_ALARM);
            break;

            case UI_MENU_STATE_DST_AUTO:
                if(settings.fields.dst.flags.automatic)
                {
                    ui_menu_change_state(UI_MENU_STATE_DST_BACK);
                }
                else ui_menu_change_state(UI_MENU_STATE_DST_STATE);
                break;
                
                case UI_MENU_STATE_DST_AUTO_SEL:
                    modified.fields.dst.flags.automatic = !modified.fields.dst.flags.automatic;
                    break;

            case UI_MENU_STATE_DST_STATE:
                ui_menu_change_state(UI_MENU_STATE_DST_OFFSET);
                break;

                case UI_MENU_STATE_DST_STATE_SEL:
                    modified.fields.dst.flags.active = !modified.fields.dst.flags.active;
                    break;

            case UI_MENU_STATE_DST_OFFSET:
                ui_menu_change_state(UI_MENU_STATE_DST_BACK);
                break;
                
                case UI_MENU_STATE_DST_OFFSET_SEL:
                    ui_dst_offset_incr();
                    break;
                
            case UI_MENU_STATE_DST_BACK:
                ui_menu_change_state(UI_MENU_STATE_DST_AUTO);
                break;

            
        case UI_MENU_STATE_ALARM:
            ui_menu_change_state(UI_MENU_STATE_BEEP);
            break;

            case UI_MENU_STATE_ALARM_ENABLED:
                ui_menu_change_state(UI_MENU_STATE_ALARM_SET);
                break;

            case UI_MENU_STATE_ALARM_SET:
                ui_menu_change_state(UI_MENU_STATE_ALARM_BACK);
                break;
                
                case UI_MENU_STATE_ALARM_SET_HH:
                    //ui_menu_change_state(UI_MENU_STATE_ALARM_BACK);
                    break;

                case UI_MENU_STATE_ALARM_SET_MM:
                    //ui_menu_change_state(UI_MENU_STATE_ALARM_BACK);
                    break;

            case UI_MENU_STATE_ALARM_BACK:
                ui_menu_change_state(UI_MENU_STATE_ALARM_ENABLED);
                break;


        case UI_MENU_STATE_BEEP:
            ui_menu_change_state(UI_MENU_STATE_DISPLAY);
            break;

            case UI_MENU_STATE_BEEP_ENABLE:
                ui_menu_change_state(UI_MENU_STATE_BEEP_BACK);
                break;
                
            case UI_MENU_STATE_BEEP_ENABLE_SEL:
                modified.fields.beep.flags.enabled = !modified.fields.beep.flags.enabled;
                break;

            case UI_MENU_STATE_BEEP_BACK:
                ui_menu_change_state(UI_MENU_STATE_BEEP_ENABLE);
                break;
            
        case UI_MENU_STATE_DISPLAY:
            ui_menu_change_state(UI_MENU_STATE_RESET);
            break;
            
            case UI_MENU_STATE_DISPLAY_FORMAT:
                ui_menu_change_state(UI_MENU_STATE_DISPLAY_SET);
                break;
                
                case UI_MENU_STATE_DISPLAY_FORMAT_SEL:
                    modified.fields.display.flags.hour_format = !modified.fields.display.flags.hour_format;
                    break;
            
            case UI_MENU_STATE_DISPLAY_SET:
                ui_menu_change_state(UI_MENU_STATE_DISPLAY_BACK);
                break;
                
                case UI_MENU_STATE_DISPLAY_SET_SEL:
                    switch(modified.fields.display.selected)
                    {
                        case UI_DISPLAY_STATE_CLOCK_HHMM:
                            modified.fields.display.selected = UI_DISPLAY_STATE_CLOCK_MMSS;
                            break;

                        case UI_DISPLAY_STATE_CLOCK_MMSS:
                            modified.fields.display.selected = UI_DISPLAY_STATE_CLOCK_SSMM;
                            break;

                        case UI_DISPLAY_STATE_CLOCK_SSMM:
                            modified.fields.display.selected = UI_DISPLAY_STATE_CLOCK_YYYY;
                            break;

                        case UI_DISPLAY_STATE_CLOCK_YYYY:
                            modified.fields.display.selected = UI_DISPLAY_STATE_CLOCK_MMDD;
                            break;

                        case UI_DISPLAY_STATE_CLOCK_MMDD:
                            modified.fields.display.selected = UI_DISPLAY_STATE_CLOCK_HHMM;
                            break;
                    }
                    break;

            case UI_MENU_STATE_DISPLAY_BACK:
                ui_menu_change_state(UI_MENU_STATE_DISPLAY_FORMAT);
                break;


        case UI_MENU_STATE_RESET:
            ui_menu_change_state(UI_MENU_STATE_EXIT);
            break;

            case UI_MENU_STATE_RESET_WIFI:
                ui_menu_change_state(UI_MENU_STATE_RESET_SETTINGS);
                break;
                
                case UI_MENU_STATE_RESET_WIFI_YN:
                    modified.fields.reset.flags.wifi = !modified.fields.reset.flags.wifi;
                    break;
            
            case UI_MENU_STATE_RESET_SETTINGS:
                ui_menu_change_state(UI_MENU_STATE_RESET_ALL);
                break;

                case UI_MENU_STATE_RESET_SETTINGS_YN:
                    modified.fields.reset.flags.settings = !modified.fields.reset.flags.settings;
                    break;
                    
            case UI_MENU_STATE_RESET_ALL:
                ui_menu_change_state(UI_MENU_STATE_RESET_BACK);
                break;

                case UI_MENU_STATE_RESET_ALL_YN:
                    modified.fields.reset.flags.all = !modified.fields.reset.flags.all;
                    break;

            case UI_MENU_STATE_RESET_BACK:
                ui_menu_change_state(UI_MENU_STATE_RESET_WIFI);
                break;
            

        case UI_MENU_STATE_EXIT:
            ui_menu_change_state(UI_MENU_STATE_ROOT);
            break;

        default:
            break;
    }
}


void ui_tz_offset_incr(void)
{
    modified.fields.tz.offset = modified.fields.tz.offset + UI_TZ_OFFSET_STEP_SIZE;
    if(modified.fields.tz.offset > UI_TZ_OFFSET_MAX)
    {
        modified.fields.tz.offset = UI_TZ_OFFSET_MIN;
    }
}

void ui_tz_offset_incr_hh(void)
{
    int32_t tz_hour = modified.fields.tz.offset / 3600;
    int32_t tz_minute = (modified.fields.tz.offset - (tz_hour * 3600)) / 60;
    tz_hour++;
    if(tz_hour > 14)
    {
        tz_hour = -12;
        tz_minute = tz_minute * -1;
    }
    
    modified.fields.tz.offset = tz_hour * 3600;
    modified.fields.tz.offset += tz_minute * 60;
}

void ui_tz_offset_incr_mm(void)
{
    int32_t tz_hour = modified.fields.tz.offset / 3600;
    int32_t tz_minute = (modified.fields.tz.offset - (tz_hour * 3600)) / 60;
    if(modified.fields.tz.offset >= 0)
    {
        tz_minute = tz_minute + 15;
        if(tz_minute >= 60)
        {
            tz_minute = 0;
        }
    }
    else 
    {
        tz_minute = tz_minute - 15;
        if(tz_minute <= -60)
        {
            tz_minute = 0;
        }
    }
    modified.fields.tz.offset = (tz_hour * 3600) + (tz_minute * 60);
}


void ui_dst_offset_incr(void)
{
    modified.fields.dst.offset += UI_DST_OFFSET_STEP_SIZE;
    if(modified.fields.dst.offset > UI_DST_OFFSET_MAX)
    {
        modified.fields.dst.offset = UI_DST_OFFSET_MIN;
    }
}


void pic_reset(void)
{
    printf("\r\nRESETTING!!!\r\n");
    __asm__ volatile ( "reset ");
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
            pic_reset();
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

void ui_print_iso8601_string_local(time_t local)
{
    local += settings.fields.tz.offset;
    
    if(isDST(&local))
    {
        local += settings.fields.dst.offset; 
    }
    
    char buf[32] = {0}; // Allocate buffer
    struct tm *local_time; // Allocate buffer
    local_time = gmtime(&local);
    strftime(buf, 32, "%Y-%m-%dT%H:%M:%S", local_time);
    printf(buf);
    
    int32_t total_offset = local - utc;
    if((total_offset)>=0)
    {
        printf("+"); 
    }
    else
    {
        printf("-"); 
        total_offset = total_offset*-1;
    }
    uint32_t total_offset_hours = total_offset/3600;
    uint32_t total_offset_minutes = (total_offset-(total_offset_hours*3600))/60;
    printf("%02lu:%02lu",total_offset_hours,total_offset_minutes);
}

void ui_print_clear_window(void)
{
    printf("\033[2J\033[1;1H"); // Clear the terminal window
}
