#ifndef PIC_BOOTLOADER_H
#define	PIC_BOOTLOADER_H

#include <Arduino.h>
#include <stdint.h>
#include <HardwareSerial.h>

#define BOOT_CONFIG_PROGRAMMABLE_ADDRESS_LOW 0x2000
#define BOOT_CONFIG_PROGRAMMABLE_ADDRESS_HIGH 0x2A7FE

#define BOOT_CONFIG_DOWNLOAD_LOW 0x2000
#define BOOT_CONFIG_DOWNLOAD_HIGH 0x2A7FE

#define BOOT_CONFIG_VERIFICATION_APPLICATION_HEADER_SIZE 8

#define BOOT_CONFIG_APPLICATION_IMAGE_APPLICATION_HEADER_ADDRESS (BOOT_CONFIG_PROGRAMMABLE_ADDRESS_LOW)

#define BOOT_CONFIG_APPLICATION_RESET_ADDRESS (BOOT_CONFIG_PROGRAMMABLE_ADDRESS_LOW + BOOT_CONFIG_VERIFICATION_APPLICATION_HEADER_SIZE)

#define BOOT_CONFIG_USER_IVT_TABLE_ADDRESS 0x2200

#define BOOT_CONFIG_DEVICE_ID 0x3456

#define BOOT_CONFIG_VERSION 0x0102

#define BOOT_CONFIG_MAX_PACKET_SIZE 0x100


enum BOOT_COMMAND_RESPONSES
{
    COMMAND_SUCCESS = 0x01,
    UNSUPPORTED_COMMAND = 0xFF,
    BAD_ADDRESS = 0xFE,
    BAD_LENGTH  = 0xFD,
    VERIFY_FAIL = 0xFC
};

enum BOOT_COMMAND
{
    READ_VERSION = 0x00,
    READ_FLASH = 0x01,
    WRITE_FLASH = 0x02,
    ERASE_FLASH = 0x03,
    CALC_CHECKSUM = 0x08,
    RESET_DEVICE = 0x09,
    SELF_VERIFY = 0x0A,
    GET_MEMORY_ADDRESS_RANGE_COMMAND = 0x0B
};

enum BOOT_COMMAND_RESULT{
    BOOT_COMMAND_SUCCESS,
    BOOT_COMMAND_NONE,
    BOOT_COMMAND_INCOMPLETE,
    BOOT_COMMAND_ERROR,
};


#define MINIMUM_WRITE_BLOCK_SIZE 8u


#define SIZE_OF_CMD_STRUCT_0 11
struct __attribute__((__packed__)) CMD_STRUCT_0{
    uint8_t cmd;
    uint16_t dataLength;
    uint32_t unlockSequence;
    uint32_t address;
};

struct __attribute__((__packed__)) CMD_STRUCT_0_WITH_PAYLOAD{
    uint8_t cmd;
    uint16_t dataLength;
    uint32_t unlockSequence;
    uint32_t address;
    uint8_t data[BOOT_CONFIG_MAX_PACKET_SIZE - SIZE_OF_CMD_STRUCT_0];
};

struct __attribute__((__packed__)) GET_VERSION_RESPONSE{
    uint8_t cmd;
    uint16_t dataLength;
    uint32_t unlockSequence;
    uint32_t address;
    //---
    uint16_t version;
    uint16_t maxPacketLength;
    uint16_t unused1;
    uint16_t deviceId;
    uint16_t unused2;
    uint16_t eraseSize;
    uint16_t writeSize;
    uint32_t unused3;
    uint32_t userRsvdStartSddress;
    uint32_t userRsvdEndSddress;
};   
    
struct __attribute__((__packed__)) RESPONSE_TYPE_0{
    uint8_t cmd;
    uint16_t dataLength;
    uint32_t unlockSequence;
    uint32_t address;
    //---
    uint8_t success;
};
struct __attribute__((__packed__)) RESPONSE_TYPE_0_2_PAYLOAD{
    uint8_t cmd;
    uint16_t dataLength;
    uint32_t unlockSequence;
    uint32_t address;
    //---
    uint8_t success;
    uint16_t data;
};
struct __attribute__((__packed__)) RESPONSE_TYPE_0_WITH_PAYLOAD{
    uint8_t cmd;
    uint16_t dataLength;
    uint32_t unlockSequence;
    uint32_t address;
    //---
    uint8_t success;    
    uint8_t data[BOOT_CONFIG_MAX_PACKET_SIZE - SIZE_OF_CMD_STRUCT_0 - 1u];
}; 
struct __attribute__((__packed__)) GET_MEMORY_ADDRESS_RANGE_RESPONSE{
    uint8_t  cmd;
    uint16_t dataLength;  
    uint32_t unlockSequence; 
    uint32_t address; 
     uint8_t success; 
    //---
    uint32_t programFlashStart; 
    uint32_t programFlashEnd; 
};

bool pic_is_in_bootloader(void);
void pic_bootloader_sync(Stream* uart);
bool pic_bootloader_entry(Stream* uart);
bool pic_bootloader_exit(Stream* uart);

#endif	/* PIC_BOOTLOADER_H */