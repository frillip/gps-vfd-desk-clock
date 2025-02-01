#include "pic_bootloader.h"

bool pic_bootloader_active = 0;

bool pic_is_in_bootloader(void)
{
  return pic_bootloader_active;
}

void pic_bootloader_sync(Stream* uart)
{
  while (uart->available()) uart->read(); // Discard any pending bytes
  while (!uart->available())
  {
    uart->printf("%c", 0xFF);
    delay(1);
  }
  while (uart->available()) uart->read(); // Discard any pending bytes
}

bool pic_bootloader_entry(Stream* uart)
{
  bool bootloader_wait = 1;
  bool bootloader_success = 0;

  struct CMD_STRUCT_0 get_version = {
    .cmd = 0x00,
    .dataLength = 0x0000,
    .unlockSequence = 0x00000000,
    .address = 0x00000000,
  };

  struct GET_VERSION_RESPONSE response;
  uint32_t expectedVersion = BOOT_CONFIG_VERSION;
  uint16_t expectedDeviceId = BOOT_CONFIG_DEVICE_ID;
  uint32_t bootloader_start_time = millis();
  size_t bytesReceived = 0;

  pic_bootloader_sync(uart);

  while(bootloader_wait)
  {
    uart->write((uint8_t*) &get_version, sizeof(CMD_STRUCT_0));
    uart->flush();

    uint8_t* buffer = (uint8_t*) &response;
    uint32_t rx_start_time = millis();
    while (bytesReceived < sizeof(GET_VERSION_RESPONSE))
    {
      if(uart->available())
      {
        buffer[bytesReceived++] = uart->read();

      }
      if(millis() - rx_start_time > 50)
      {
        bytesReceived = 0xFFFF;
      }
    }
    if(bytesReceived == sizeof(GET_VERSION_RESPONSE))
    {
      if(response.cmd == 0x00) // READ_VERSION is 0x00 for no reason
      {
        if(response.version == BOOT_CONFIG_VERSION)
        {
          if(response.deviceId == BOOT_CONFIG_DEVICE_ID)
          {
            bootloader_wait = 0;
            bootloader_success = 1;
            return bootloader_success;
          }
        }
      }
    }

    while (uart->available()) uart->read(); // Discard any pending bytes
    bytesReceived=0;

    if(millis() - bootloader_start_time > 500)
    {
      bootloader_wait = 0;
    }
  }
  return bootloader_success;
}

bool pic_bootloader_exit(Stream* uart)
{
  bool bootloader_wait = 1;
  bool bootloader_success = 0;

  struct CMD_STRUCT_0 reset_device = {
    .cmd = 0x09,
    .dataLength = 0x0000,
    .unlockSequence = 0x00000000,
    .address = 0x00000000,
  };

  //uint8_t bootloader_reset_bytes[11] = {0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // https://onlinedocs.microchip.com/oxy/GUID-4F4EDA17-350C-4EC3-B601-9F459DD028EF-en-US-10/GUID-06B128A4-516C-4D56-BDDC-E75AA0964F71.html

  struct RESPONSE_TYPE_0 response;
  uint32_t bootloader_start_time = millis();
  size_t bytesReceived = 0;

  pic_bootloader_sync(uart);

  while(bootloader_wait)
  {
    uart->write((uint8_t*) &reset_device, sizeof(reset_device));
    uart->flush();

    uint8_t* buffer = (uint8_t*) &response;
    uint32_t rx_start_time = millis();
    while (bytesReceived < sizeof(RESPONSE_TYPE_0))
    {
      if(uart->available())
      {
        buffer[bytesReceived++] = uart->read();
      }
      if(millis() - rx_start_time > 50)
      {
        bytesReceived = 0xFFFF;
      }
    }
    if(bytesReceived == sizeof(RESPONSE_TYPE_0))
    {
      if((response.cmd == 0x09) &&(response.success == 0x01)) // RESET is 0x09
      {
        bootloader_wait = 0;
        bootloader_success = 1;
        return bootloader_success;
      }
    }

    while (uart->available()) uart->read();
    bytesReceived=0;

    if(millis() - bootloader_start_time > 500)
    {
      bootloader_wait = 0;
    }
  }
  return bootloader_success;
}