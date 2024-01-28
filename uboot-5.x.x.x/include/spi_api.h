#ifndef _SPI_API_H_
#define _SPI_SPI_H_

unsigned long raspi_init(void);
int spi_env_init(void);

int raspi_read(char *buf, int offset, int len);
int raspi_erase_write(char *buf, int offset, int len);

int raspi_erase(int offset, int len);
int raspi_write(char *buf, int offset, int len);

#endif
