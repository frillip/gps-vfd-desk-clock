#include "updater_pull.h"
String updater_json_https = UPDATER_JSON_HTTPS_DEFAULT;
String updater_json_server = UPDATER_JSON_SERVER_DEFAULT;
String updater_json_path = UPDATER_JSON_PATH_DEFAULT;
String updater_json_url = updater_json_https + updater_json_server + updater_json_path;

ESP32OTAPull ota;
Stream *output_stream;

void updater_set_server(const char* new_server)
{
  updater_json_server = new_server;
  updater_regenerate_url();
}

void updater_set_path(const char* new_path)
{
  updater_json_path = new_path;
  if(updater_json_path.charAt(0) != '/') updater_json_path = String("/") + updater_json_path;
  updater_regenerate_url();
}

void updater_regenerate_url(void)
{
  updater_json_url = updater_json_https + updater_json_server + updater_json_path;
}

bool updater_check(Stream *output)
{
  int ret = ota.CheckForOTAUpdate(updater_json_url.c_str(), ESP_VERSION, ESP32OTAPull::DONT_DO_UPDATE);
  output->printf("CheckForOTAUpdate returned %d (%s)\n\n", ret, updater_errtext(ret));
  String otaVersion = ota.GetVersion();
  output->printf("ESP Version:           %s\n", ESP_VERSION);
  output->printf("OTA Version Available: %s\n", otaVersion.c_str());
  return 0;
}

void updater_pull(Stream *output)
{
  output_stream = output;
  int ret = ota
    .SetCallback(updater_callback_percent)
    .CheckForOTAUpdate(updater_json_url.c_str(), ESP_VERSION, ESP32OTAPull::UPDATE_BUT_NO_BOOT);
  output->printf("CheckForOTAUpdate returned %d (%s)\n\n", ret, updater_errtext(ret));
  output->printf("Use esp-reset to finish update.\n");
}

void updater_force(Stream *output)
{
  output_stream = output;
  int ret = ota
    .SetCallback(updater_callback_percent)
    .AllowDowngrades(true)
    .CheckForOTAUpdate(updater_json_url.c_str(), ESP_VERSION, ESP32OTAPull::UPDATE_AND_BOOT);
  output->printf("CheckForOTAUpdate returned %d (%s)\n\n", ret, updater_errtext(ret));
  output->printf("Rebooting...\n");
}

void updater_callback_percent(int offset, int totallength)
{
	static int prev_percent = -1;
	int percent = 100 * offset / totallength;
	if (percent != prev_percent)
	{
		output_stream->printf("Updating %d of %d (%02d%%)...\n", offset, totallength, 100 * offset / totallength);
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