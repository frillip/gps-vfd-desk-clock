#include "eeprom.h"

EEPROM_DATA_STRUCT settings = { .raw ={0} };
EEPROM_DATA_STRUCT modified = { .raw ={0} };

void eeprom_init()
{
    
    settings.fields.tz.flags.automatic = TZ_AUTOMATIC_DEFAULT;
    settings.fields.tz.offset = TZ_OFFSET_DEFAULT;

    settings.fields.dst.flags.automatic = DST_AUTOMATIC_DEFAULT;
    settings.fields.dst.flags.active = DST_ACTIVE_DEFAULT;
    settings.fields.dst.offset = DST_OFFSET_DEFAULT;

    settings.fields.alarm.flags.enabled = UI_ALARM_ENABLED_DEFAULT;
    settings.fields.alarm.offset = UI_ALARM_OFFSET_DEFAULT;

    settings.fields.beep.flags.enabled = UI_BEEP_ENABLED_DEFAULT;
    
    settings.fields.display.flags.hour_format = UI_DISPLAY_HOUR_FORMAT_DEFAULT;
    settings.fields.display.selected = UI_DISPLAY_STATE_CLOCK_HHMM;
}

void eeprom_read()
{
    
}

void eeprom_write()
{
    
}