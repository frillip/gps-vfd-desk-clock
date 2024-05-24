#include "eeprom.h"

EEPROM_DATA_STRUCT settings = { .raw ={0} };
EEPROM_DATA_STRUCT modified = { .raw ={0} };

void eeprom_init()
{
    
    settings.fields.tz.flags.automatic = UI_TZ_AUTOMATIC_DEFAULT;
    settings.fields.tz.offset = UI_TZ_OFFSET_DEFAULT;

    settings.fields.dst.flags.automatic = UI_DST_AUTOMATIC_DEFAULT;
    settings.fields.dst.flags.active = UI_DST_ACTIVE_DEFAULT;
    settings.fields.dst.offset = UI_DST_OFFSET_DEFAULT;

    settings.fields.alarm.flags.enabled = UI_ALARM_ENABLED_DEFAULT;
    settings.fields.alarm.offset = UI_ALARM_OFFSET_DEFAULT;

    settings.fields.beep.flags.enabled = UI_BEEP_ENABLED_DEFAULT;
    
    settings.fields.display.flags.hour_format = UI_DISPLAY_HOUR_FORMAT_DEFAULT;
    settings.fields.display.selected = UI_DISPLAY_STATE_CLOCK_HHMM;
    
    memcpy(modified.raw, settings.raw, sizeof(settings.raw));
}

void eeprom_read()
{
    
}

void eeprom_write()
{
    
}