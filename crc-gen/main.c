#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h>

#define lo8(x) (x & 0xFF)
#define hi8(x) ((x >> 8) & 0xFF)

unsigned short crc_ccitt_update (uint16_t crc, uint8_t data)
{
  data ^= lo8 (crc);
  data ^= data << 4;
  return ((((uint16_t)data << 8) | hi8 (crc)) ^ (uint8_t)(data >> 4) ^ ((uint16_t)data << 3));
}	

int main(int argc, char *argv[])
{
  if (argc != 2)
  {
  	printf("Usage: crcgen <filename>\n");
  	return 1;
	}
	
	FILE *f;
	
	f = fopen(argv[1], "rb+");
	
	if (f == 0)
	{
		printf("Unable to open file %s\n", argv[1]);
		return 1;
	}
	
	struct stat buf;
	stat(argv[1], &buf);
	
	unsigned long l;
	unsigned short crc = 0xFFFF;
	unsigned char c;
	
	for (l=0; l < buf.st_size - 2; l++)
		crc = crc_ccitt_update(crc, fgetc(f));
	
	fseek(f, -2, SEEK_END);
	fwrite (&crc, 2, 1, f);
	
	fclose(f);
	
	printf("Written CRC 0x%.4X to file %s\n", crc, argv[1]);

  return 0;
}

