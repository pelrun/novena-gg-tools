#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "i2c-dev.h"
#include "gg.h"
#include "gg_api.h"

void setFlashOkVoltage(int file, uint16_t millivolts)
{
    uint8_t data[32];
    int len = 0;

    i2c_smbus_write_word_data(file, 0x77, DF_POWER);

    if ((len = i2c_smbus_read_block_data(file, 0x78, data)) > 0)
    {
        int i;
        for (i=0; i<len; i++)
        {
            printf("%02X",data[i]);
        }
        printf("\n");

        data[0] = millivolts>>8;
        data[1] = millivolts&0xFF;

        i2c_smbus_write_word_data(file, 0x77, DF_POWER);
        i2c_smbus_write_block_data(file, 0x78, len, data);
    }
    else
    {
        printf("No bytes returned\n");
    }
}

void setCellMode(int file)
{
    uint8_t data[32];
    int len = 0;

    i2c_smbus_write_word_data(file, 0x77, 64);

    if ((len = i2c_smbus_read_block_data(file, 0x78, data)) > 0)
    {
        int i;
        for (i=0; i<len; i++)
        {
            printf("%02X",data[i]);
        }
        printf("\n");

        data[0] &= ~3;
        data[0] |= 2;

        i2c_smbus_write_word_data(file, 0x77, 64);
        i2c_smbus_write_block_data(file, 0x78, len, data);
    }
    else
    {
        printf("No bytes returned\n");
    }
}

int enterBootRom(int file)
{
    return i2c_smbus_write_word_data(file, 0,0xf00);
}

int exitBootRom(int file)
{
    return i2c_smbus_write_byte(file, 8);
}

int readDataFlashRow(int file, uint16_t rownum, uint8_t *data)
{
    uint16_t addr = 0x4000 + rownum*0x20;
    i2c_smbus_write_word_data(file, BR_SetAddr, addr);
    return i2c_smbus_read_block_data(file, BR_ReadRAMBlk, data);
}

// Instruction flash word is 22-bits wide.
// Charlie Miller saw random corruption of the return value, so re-read a few times!
int readInstructionFlashWordUnsafe(int file, uint16_t row_num, uint8_t col_num, uint32_t *word)
{
    uint8_t addr[3];
    uint8_t temp[3];
    
    addr[0] = row_num & 0xFF;
    addr[1] = row_num >> 8;
    addr[2] = col_num;
    
    i2c_smbus_write_block_data(file, BR_Smb_FlashWrAddr, 3, addr);
    
    if (i2c_smbus_read_block_data(file, BR_Smb_FlashRdWord, temp) == 3)
    {
      *word = temp[0] | temp[1]<<8 | temp[2]<<16;
      return 1;
    }
    
    return 0;
}

#define RIF_RETRIES 1000
#define RIF_THRESHOLD 3

// read a instruction flash word until we get a consistent answer
void readInstructionFlashWord(int file, uint16_t row_num, uint8_t col_num, uint32_t *word)
{
  int retry;
  uint32_t prevWord=0x55FFFFFF; // guaranteed to not match an instruction flash word, which are only 22bit
  int count = 0;
  
  for (retry = 0; retry<RIF_RETRIES; retry++)
  {
    if (readInstructionFlashWordUnsafe(file, row_num, col_num, word))
    {
      if (*word == prevWord)
      {
        if (++count == RIF_THRESHOLD)
        {
          return;
        }
      }
      else
      {
        prevWord = *word;
        count = 0;
      }
    }
  }
  exitBootRom(file);
  // complete failure
  perror("Failed to get good instruction flash word");
  exit(1);
}

void readInstructionFlashRow(int file, uint16_t row_num, uint8_t *data)
{
// Can't use the readflashrow command, as it requires a 96 byte smbus transfer
// which we can't do here. So read it a word at a time.
  int col_num, index;
  uint32_t word;
  
  for (col_num=0; col_num<32; col_num++)
  {
    readInstructionFlashWord(file, row_num, col_num, &word);
    data[index++] = word & 0xFF;
    data[index++] = (word>>8) & 0xFF;
    data[index++] = (word>>16) & 0xFF;
  }
}

int dumpDataFlash(int file, char *filename)
{
    FILE *fp;

    if ((fp = fopen(filename,"w")) != NULL)
    {
        int row = 0;
        uint8_t data[32];

        enterBootRom(file);
        for (row=0; row<0x40; row++)
        {
            readDataFlashRow(file, row, data);
            fwrite(data, 32, 1, fp);
        }
        exitBootRom(file);
        fclose(fp);
    }

    return 0;
}

int dumpInstructionFlash(int file, char *filename)
{
    FILE *fp;

    if ((fp = fopen(filename,"w")) != NULL)
    {
        int row = 0;
        uint8_t data[32*3]; //22-bit instruction word

        enterBootRom(file);
        for (row=0; row<0x300; row++)
        {
            readInstructionFlashRow(file, row, data);
            fwrite(data, 32, 3, fp);
            printf("Read row %d\n", row);
        }
        exitBootRom(file);
        fclose(fp);
    }

    return 0;
}

int getmfgr(int file, uint16_t reg, void *data, int size)
{
  i2c_smbus_write_word_data(file, 0, reg);

  if (data && size)
  {
     return i2c_smbus_read_i2c_block_data(file, 0, size, data);
  }

  return 0;
}

int firmwareVersion(int file)
{
    uint8_t data[255];
    int status;

    status = getmfgr(file, 2, data, 2);
    printf("Firmware Version: %02X.%02X\n", data[1], data[0]);

    return 0;
}

