#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "i2c-dev.h"

#include "gg.h"

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
    // don't use I2C_SLAVE_FORCE or disable i2c device locking, disable the sbs-battery driver instead!
    // the gg is too easily bricked to risk having the driver send commands at the same time as us.
    if (ioctl(file, I2C_SLAVE, GG_ADDRESS) < 0)
    {
        perror("Failed to acquire bus access and/or talk to slave");
        exit(1);
    }

    firmwareVersion(file);

    return 0;
}
