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

#define DF_SIZE (0x40*0x20)

int main(int argc, char *argv[])
{
    const char * devName = "/dev/i2c-0";
    FILE *fin;
    uint8_t flash_data[DF_SIZE];
    size_t flash_length;

    if (argc != 2)
    {
        printf("Usage: %s <dfi file>\n", argv[0]);
        exit(1);
    }

    // Open up the I2C bus
    int file = open(devName, O_RDWR);
    if (file == -1)
    {
        perror(devName);
        exit(1);
    }

    // Specify the address of the slave device.
    // don't use I2C_SLAVE_FORCE or disable i2c device locking, disable the sbs-battery driver instead!
    // the gg is too easily bricked to risk having the driver send commands at the same time as us.
    if (ioctl(file, I2C_SLAVE_FORCE, GG_ADDRESS) < 0)
    {
        perror("Failed to acquire bus access and/or talk to slave");
        exit(1);
    }

    fin = fopen(argv[1],"rb");
    if (fin == NULL)
    {
        perror("Unable to open dfi file\n");
        exit(1);
    }

    memset(flash_data, 0xFF, DF_SIZE);
    flash_length = fread(flash_data, 1, DF_SIZE, fin);
    if (flash_length <= 0)
    {
        perror("Failed to read in flash data\n");
        exit(1);
    }

    if (memcmp(flash_data+0xfc, "bq20z95", 7) != 0)
    {
        printf("Input file is not a valid dfi file!\n");
        exit(1);
    }

    printf("Flash length: %d\n", flash_length);

    enterBootRom(file);

    writeDataFlash(file, 0, flash_data, flash_length);

    if (verifyDataFlash(file, flash_data, flash_length))
    {
        printf("Flash verified, exiting BootROM mode\n");
        exitBootRom(file);
    }
    else
    {
        //uhoh
        printf("Flash verify failed, try running this again.\n");
        exit(1);
    }

    return 0;
}
