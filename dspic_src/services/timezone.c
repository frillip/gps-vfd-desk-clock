#include "timezone.h"

extern EEPROM_DATA_STRUCT settings;

// DST is stupid
// Valid for UK and EU and other places where
// DST starts on the last Sunday of March
// and ends on the last Sunday of October
// at 0100 UTC
bool isDST(const time_t *time)
{
    bool dst = 0;
    
    if(settings.fields.dst.flags.automatic)
    {
        struct tm *disp_time;

        // Convert our time_t into a time struct
        disp_time = gmtime(time);
        uint8_t next_sunday = 0;
        uint8_t last_sunday = 0;


        // Are we in the DST danger zone?
        if(disp_time->tm_mon >= 2 && disp_time->tm_mon <= 9)
        {
            // If we're in April to September is always DST
            if(disp_time->tm_mon > 2 && disp_time->tm_mon < 9)
            {
                dst = 1;
            }

            // Are we in either March or October?
            else if(disp_time->tm_mon == 2 || disp_time->tm_mon == 9)
            {
                // Calculate next Sunday
                next_sunday = disp_time->tm_mday + (7 - disp_time->tm_wday);

                // And then the last Sunday of the month
                last_sunday = next_sunday;
                if(last_sunday > 31) 
                {
                    last_sunday -= 7;
                }
                else
                {
                    while((last_sunday + 7) <= 31)
                    {
                        last_sunday += 7;
                    }
                }

                // If we are in March
                if(disp_time->tm_mon == 2)
                {
                    // Are we past the last Sunday?
                    if(disp_time->tm_mday > last_sunday)
                    {
                        dst = 1;
                    }
                    // Is this the last Sunday in the month?
                    else if(disp_time->tm_mday == last_sunday)
                    {
                        // Apply DST if it's after 0100 UTC
                        if(disp_time->tm_hour >= 1)
                        {
                            dst = 1;
                        }
                    }
                }
                //If we are in October
                else if(disp_time->tm_mon == 9)
                {
                    // Are we still before the last Sunday?
                    if(disp_time->tm_mday < last_sunday)
                    {
                        dst = 1;
                    }
                    // Is this the last Sunday in the month?
                    if(disp_time->tm_mday == last_sunday)
                    {
                        // Apply DST if it's before 0100 UTC
                        if(disp_time->tm_hour < 1)
                        {
                            dst = 1;
                        }
                    }
                }
            }
        }
    }
    else dst = settings.fields.dst.flags.active;
    // Return DST status
    return dst;
}

time_t get_local_time(time_t utc)
{
    time_t local_time = utc;
    local_time += settings.fields.tz.offset;
    
    if(isDST(&utc))
    {
        local_time += settings.fields.dst.offset; 
    }
    
    return local_time;
}
