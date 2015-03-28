#ifndef _GG_H
#define _GG_H

#define GG_ADDRESS        (0x16 >> 1)

void setFlashOkVoltage(int file, uint16_t millivolts);

void setCellMode(int file);

int enterBootRom(int file);

int exitBootRom(int file);

int readDataFlashRow(int file, uint16_t rownum, uint8_t *data);

// rownum must have lsb clear, will erase row and the next one
int eraseDataFlashRow(int file, uint16_t rownum);

int writeDataFlash(int file, uint16_t rownum, uint8_t *data, uint16_t length);

// Instruction flash word is 22-bits wide.
// Charlie Miller saw random corruption of the return value, so re-read a few times!
// read a instruction flash word until we get a consistent answer
void readInstructionFlashWord(int file, uint16_t row_num, uint8_t col_num, uint32_t *word);

void readInstructionFlashRow(int file, uint16_t row_num, uint8_t *data);

int verifyDataFlash(int file, uint8_t *data, uint16_t length);

int dumpDataFlash(int file, char *filename);

int dumpInstructionFlash(int file, char *filename);

int getmfgr(int file, uint16_t reg, void *data, int size);

int firmwareVersion(int file);

#endif
