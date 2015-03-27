#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>

#include "i2c-dev.h"
#include "gg.h"
#include "gg_api.h"

#define RETRIES 200

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
    return i2c_smbus_write_word_data(file, 0, 0xf00);
}

int exitBootRom(int file)
{
    return i2c_smbus_write_byte(file, 8);
}

int readDataFlashRow(int file, uint16_t rownum, uint8_t *data)
{
    uint16_t addr = 0x4000 + rownum*0x20;
    i2c_smbus_write_word_data(file, BR_SetAddr, addr);
    return (i2c_smbus_read_block_data(file, BR_ReadRAMBlk, data) == 0x20);
}

int writeDataFlashRow(int file, uint16_t rownum, uint8_t *data)
{
    uint8_t colnum;
    int retry, ret;

    printf("Writing data flash row %d\n", rownum);

    for (colnum=0; colnum<32; colnum++)
    {
        uint16_t addr = 0x4000 + rownum*0x20 + colnum;

        uint8_t block[3];
        block[0] = addr & 0xFF;
        block[1] = addr >> 8;
        block[2] = data[colnum];

        for (retry = RETRIES; retry > 0; retry--)
        {
            if (i2c_smbus_write_block_data(file, BR_Smb_FdataProgWord, 3, block) == 0)
            {
                break;
            }
            printf("Retrying write...\n");
        }
        if (retry == 0)
        {
            return 0;
        }
    }

    return 1;
}

int verifyErasedRows(int file, uint16_t rownum)
{
    uint8_t verify[32];

    if (readDataFlashRow(file, rownum, verify))
    {
        int col = 0;
        for (col = 0; col < 32; col++)
        {
            if (verify[col] != 0xFF)
            {
                return 0;
            }
        }
    }
    else
    {
        return 0;
    }

    return 1;
}

// WARNING: this erases two rows at a time
int eraseDataFlashRow(int file, uint16_t rownum)
{
    uint8_t block[2];
    int retry;

    if (rownum & 1)
    {
        printf("ERASING WRONG ROW %d\n", rownum);
        return 0;
    }

    printf("Erasing rows %d-%d...\n", rownum, rownum+1);

    block[0] = rownum & 0xFF;
    block[1] = rownum >> 8;

    for (retry = RETRIES; retry > 0; retry--)
    {
        if (i2c_smbus_write_word_data(file, BR_Smb_FdataEraseRow, rownum) == 0)
        {
            sleep(1);
            if (verifyErasedRows(file, rownum) && verifyErasedRows(file, rownum+1))
            {
                return 1;
            }
            printf("Verify erased row failed\n");
        }
        printf("Retrying erase...\n");
    }

    return 0;
}

int verifyDataFlashRow(int file, uint16_t rownum, uint8_t *data)
{
    uint8_t verify[32];

    if (readDataFlashRow(file, rownum, verify))
    {
        if (memcmp(verify, data, 32) == 0)
        {
            return 1;
        }
    }

    return 0;
}

int writeDataFlash(int file, uint16_t rownum, uint8_t *data, uint16_t length)
{
    int bytes_remaining = length;
    uint16_t current_row;
    uint16_t num_rows = ((length>>6) | 1) * 2;
    uint8_t padded_data[32];
    int retry = 0;

    if (rownum&1)
    {
        // the erase row semantics means starting from an odd row would delete the preceding row
        // so don't do it.
        return 0;
    }

    for (current_row = rownum; current_row<0x3e && bytes_remaining>0; current_row++)
    {
        if ((current_row & 1) == 0)
        {
            // erase the next two rows
            if (eraseDataFlashRow(file, current_row) == 0)
            {
                return 0;
            }
        }

        memset(padded_data, 0xFF, 32);
        memcpy(padded_data, data+(current_row*0x20), (bytes_remaining>=32) ? 32 : bytes_remaining);

        for (retry = RETRIES; retry>0; retry--)
        {
            writeDataFlashRow(file, current_row, padded_data);
            if (verifyDataFlashRow(file, current_row, padded_data))
            {
                break;
            }
            printf("Row verify failed\n");
        }
        if (retry == 0)
        {
            return 0;
        }

        bytes_remaining -= 32;
    }

    return 1;
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
    int num_rows = 0x40;

    if ((fp = fopen(filename,"w")) != NULL)
    {
        int row = 0;
        uint8_t data[32];

        for (row=0; row<num_rows; row++)
        {
            readDataFlashRow(file, row, data);
            fwrite(data, 32, 1, fp);
            printf("Read data row %d/%d\n", row, num_rows-1);
        }
        fclose(fp);
    }

    return 0;
}

int dumpInstructionFlash(int file, char *filename)
{
    FILE *fp;
    int num_rows=0x400;

    if ((fp = fopen(filename,"w")) != NULL)
    {
        int row = 0;
        uint8_t data[32*3]; //22-bit instruction word

        for (row=0; row<num_rows; row++)
        {
            readInstructionFlashRow(file, row, data);
            fwrite(data, 32, 3, fp);
            printf("Read instruction row %d/%d\n", row, num_rows-1);
        }
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

