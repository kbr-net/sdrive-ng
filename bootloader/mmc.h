#ifndef MMC_H
#define MMC_H


/* Results of Disk Functions */
#define	RES_OK		0	/* 0: Successful */
#define	RES_ERROR	1	/* 1: R/W Error */

#define MMC_SECTOR_SIZE 512

extern uint8_t mmc_init (void);
//extern uint8_t mmc_read (uint8_t *buff, uint32_t sector);
extern uint8_t mmc_read (uint32_t sector);

#endif /* MMC_H */

