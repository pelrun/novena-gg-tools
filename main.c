#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "i2c-dev.h"

#define GG_ADDRESS        (0x16 >> 1)

#define DF_CONFIGURATION 64
#define DF_POWER 68

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

#define BR_SetAddr    0x09
#define BR_ReadRAMBlk 0x0c

int readBootRomData(int file, uint16_t rownum, uint8_t *data)
{
    uint16_t addr = 0x4000 + rownum*0x20;
    i2c_smbus_write_word_data(file, BR_SetAddr, addr);
    return i2c_smbus_read_block_data(file, BR_ReadRAMBlk, data);
}

int dumpBootRomData(int file, char *filename)
{
    FILE *fp;

    if ((fp = fopen(filename,"w")) != NULL)
    {
        int row = 0;
        uint8_t data[32];

        printf("eBR: %d\n", enterBootRom(file));
        for (row=0; row<0x40; row++)
        {
            readBootRomData(file, row, data);
            fwrite(data, 32, 1, fp);
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

int main()
{
    const char * devName = "/dev/i2c-0";

    // Open up the I2C bus
    int file = open(devName, O_RDWR);
    if (file == -1)
    {
        perror(devName);
        exit(1);
    }

    // Specify the address of the slave device.
    if (ioctl(file, I2C_SLAVE, GG_ADDRESS) < 0)
    {
        perror("Failed to acquire bus access and/or talk to slave");
        exit(1);
    }

//    dumpBootRomData(file, "gg.dfi");
//    setCellMode(file);

    firmwareVersion(file);
    setFlashOkVoltage(file, 0);

    return 0;
}
