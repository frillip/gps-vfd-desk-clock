#include "eeprom.h"

EEPROM_DATA_STRUCT settings = { .raw ={0} };
EEPROM_DATA_STRUCT modified = { .raw ={0} };

// Reserve some program memory for setting 'eeprom' data
static __prog__  uint8_t eeprom_flash_page[FLASH_ERASE_PAGE_SIZE_IN_PC_UNITS] __attribute__((space(prog),aligned(FLASH_ERASE_PAGE_SIZE_IN_PC_UNITS)));

static void eeprom_read_flash(void)
{
    uint32_t flash_storage_address = FLASH_GetErasePageAddress((uint32_t)&eeprom_flash_page[0]);
    EEPROM_DATA_STRUCT read_data = { .raw ={0} }; // Temporary holding pen

    // Get flash page aligned address of flash reserved above for this test.
    flash_storage_address = FLASH_GetErasePageAddress((uint32_t)&eeprom_flash_page[0]);

    uint32_t i = 0;
    while(i < (sizeof(EEPROM_DATA_STRUCT)/sizeof(read_data.raw[0])))
    {
        read_data.raw[i] = FLASH_ReadWord16(flash_storage_address + (2 * i));
        i++;
    }
    
    memcpy(settings.raw, read_data.raw, sizeof(EEPROM_DATA_STRUCT));
}

static void eeprom_write_flash(void)
{
    uint32_t flash_storage_address = FLASH_GetErasePageAddress((uint32_t)&eeprom_flash_page[0]);
    bool result;
    EEPROM_DATA_STRUCT write_data = { .raw ={0} }; // Set up write buffer
    
    memcpy(write_data.raw, settings.raw, sizeof(EEPROM_DATA_STRUCT)); // Copy our settings
    
    FLASH_Unlock(FLASH_UNLOCK_KEY);

    result = FLASH_ErasePage(flash_storage_address);
    if (result == false)
    {
        // Do something maybe?
    }
    
    uint32_t i = 0;
    // For this product we must write two adjacent words at a one time.
    while(i < (sizeof(EEPROM_DATA_STRUCT)/sizeof(write_data.raw[0])))
    {
        result &= FLASH_WriteDoubleWord24(flash_storage_address + (2 * i), write_data.raw[i], write_data.raw[i+1]);
        i +=2 ;
    }

    if (result == false)
    {
        // Do something maybe?
    }

    // Clear Key for NVM Commands so accidental call to flash routines will not corrupt flash
    FLASH_Lock();
    
    // To do: re-read settings after write and compare?
}

void eeprom_init()
{
    eeprom_read_flash();
    if(!eeprom_check_settings())
    {
        printf("Invalid EEPROM header: %08lX\r\n", settings.fields.header);
        eeprom_reset_settings();
        printf("New EEPROM header: %08lX\r\n", settings.fields.header);
        eeprom_write();
    }
    memcpy(modified.raw, settings.raw, sizeof(EEPROM_DATA_STRUCT));
}

void eeprom_read(void)
{
    eeprom_read_flash();
    //eeprom_print_settings();
}

void eeprom_write(void)
{
    eeprom_write_flash();
    //eeprom_print_settings();
}

bool eeprom_check_settings(void)
{
    // To do: check settings after read, for now, just check the header value
    return settings.fields.header == EEPROM_HEADER_VALUE;
}

void eeprom_reset_settings(void)
{
    settings.fields.header = EEPROM_HEADER_VALUE;

    settings.fields.tz.flags.automatic = UI_TZ_AUTOMATIC_DEFAULT;
    settings.fields.tz.offset = UI_TZ_OFFSET_DEFAULT;

    settings.fields.dst.flags.automatic = UI_DST_AUTOMATIC_DEFAULT;
    settings.fields.dst.flags.active = UI_DST_ACTIVE_DEFAULT;
    settings.fields.dst.offset = UI_DST_OFFSET_DEFAULT;

    settings.fields.alarm.flags.enabled = UI_ALARM_ENABLED_DEFAULT;
    settings.fields.alarm.offset = UI_ALARM_OFFSET_DEFAULT;

    settings.fields.beep.flags.enabled = UI_BEEP_ENABLED_DEFAULT;
    
    settings.fields.display.flags.hour_12_format = UI_DISPLAY_HOUR_FORMAT_DEFAULT;
    settings.fields.display.selected = UI_DISPLAY_STATE_CLOCK_HHMM;
    
    settings.fields.reset.flags.wifi = UI_RESET_WIFI_DEFAULT;
    settings.fields.reset.flags.settings = UI_RESET_SETTINGS_DEFAULT;
    settings.fields.reset.flags.all = UI_RESET_ALL_DEFAULT;
    
    printf("Settings reset!\r\n");
}

void eeprom_print_settings(void)
{
    printf("\r\nCurrent settings:\r\n");
    printf("header: %08lX\r\n", settings.fields.header);
    printf("tz.flags.automatic: %u\r\n", settings.fields.tz.flags.automatic);
    printf("tz.offset: %li\r\n", settings.fields.tz.offset);
    printf("dst.flags.automatic: %u\r\n", settings.fields.dst.flags.automatic);
    printf("dst.flags.active: %u\r\n", settings.fields.dst.flags.active);
    printf("dst.offset: %li\r\n", settings.fields.dst.offset);
    printf("alarm.flags.enabled: %u\r\n", settings.fields.alarm.flags.enabled);
    printf("alarm.offset: %lu\r\n", settings.fields.alarm.offset);
    printf("beep.flags.enabled: %u\r\n", settings.fields.beep.flags.enabled);
    printf("display.flags.hour_12_format: %u\r\n", settings.fields.display.flags.hour_12_format);
    printf("display.selected: %u\r\n", settings.fields.display.selected);
    printf("reset.flags.wifi: %u\r\n", settings.fields.reset.flags.wifi);
    printf("reset.flags.settings: %u\r\n", settings.fields.reset.flags.settings);
    printf("reset.flags.all: %u\r\n", settings.fields.reset.flags.all);
    printf("\r\n");
}