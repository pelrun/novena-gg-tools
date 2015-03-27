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

int main()
{
    const char * devName = "/dev/i2c-0";
    FILE *fin;
    uint8_t flash_data[DF_SIZE];
    size_t flash_length;

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
    if (ioctl(file, I2C_SLAVE, GG_ADDRESS) < 0)
    {
        perror("Failed to acquire bus access and/or talk to slave");
        exit(1);
    }

    fin = fopen("flash.dfi","rb");
    if (fin == NULL)
    {
        printf("Unable to open dfi file\n");
        return 1;
    }

    memset(flash_data, 0xFF, DF_SIZE);
    flash_length = fread(flash_data, 1, DF_SIZE, fin);
    if (flash_length <= 0)
    {
        perror("Failed to read in flash data\n");
        return 1;
    }

    printf("Flash length: %d\n", flash_length);

//    enterBootRom(file);

    writeDataFlash(file, 0, flash_data, flash_length);

//    eraseDataFlashRow(file, 0);

    dumpDataFlash(file, "afterflash.dfi");


    // don't do this until we're CERTAIN the dataflash is okay
    // exitBootRom(file);

    return 0;
}
