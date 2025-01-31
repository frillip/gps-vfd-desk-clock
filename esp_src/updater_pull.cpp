#include "updater_pull.h"
const char* updater_json_url = UPDATER_JSON_URL_DEFAULT;

ESP32OTAPull ota;

bool updater_check(Stream *output)
{
  int ret = ota.CheckForOTAUpdate(updater_json_url, VERSION, ESP32OTAPull::DONT_DO_UPDATE);
  output->printf("CheckForOTAUpdate returned %d (%s)\n\n", ret, updater_errtext(ret));
  String otaVersion = ota.GetVersion();
  output->printf("OTA Version Available: %s\n", otaVersion.c_str());
}

/*
output->printf("Check for update and download it, but don't reboot.  Display dots.\n");
ret = ota
  .SetCallback(callback_dots)
  .CheckForOTAUpdate(JSON_URL, VERSION, ESP32OTAPull::UPDATE_BUT_NO_BOOT);
output->printf("CheckForOTAUpdate returned %d (%s)\n\n", ret, errtext(ret));

delay(3000);

// Example 3
output->printf("Download and install downgrade, but only if the configuration string matches.  Display percentages.\n");
ret = ota
  .SetCallback(callback_percent)
  .AllowDowngrades(true)
//.SetConfig("4MB RAM")
  .CheckForOTAUpdate(JSON_URL, VERSION, ESP32OTAPull::UPDATE_AND_BOOT);
output->printf("CheckForOTAUpdate returned %d (%s)\n\n", ret, errtext(ret));
*/

void updater_callback_percent(int offset, int totallength)
{
	static int prev_percent = -1;
	int percent = 100 * offset / totallength;
	if (percent != prev_percent)
	{
		Serial.printf("Updating %d of %d (%02d%%)...\n", offset, totallength, 100 * offset / totallength);
		prev_percent = percent;
	}
}

const char *updater_errtext(int code)
{
	switch(code)
	{
		case ESP32OTAPull::UPDATE_AVAILABLE:
			return "An update is available but wasn't installed";
		case ESP32OTAPull::NO_UPDATE_PROFILE_FOUND:
			return "No profile matches";
		case ESP32OTAPull::NO_UPDATE_AVAILABLE:
			return "Profile matched, but update not applicable";
		case ESP32OTAPull::UPDATE_OK:
			return "An update was done, but no reboot";
		case ESP32OTAPull::HTTP_FAILED:
			return "HTTP GET failure";
		case ESP32OTAPull::WRITE_ERROR:
			return "Write error";
		case ESP32OTAPull::JSON_PROBLEM:
			return "Invalid JSON";
		case ESP32OTAPull::OTA_UPDATE_FAIL:
			return "Update fail (no OTA partition?)";
		default:
			if (code > 0)
				return "Unexpected HTTP response code";
			break;
	}
	return "Unknown error";
}