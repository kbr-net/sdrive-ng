#include <avr/io.h>
#include "fat.h"

uint8_t fat_buf[MMC_SECTOR_SIZE];
uint32_t filesize;

static uint32_t  RootDirRegionStartSec;
static uint32_t  DataRegionStartSec;
static uint16_t  RootDirRegionSize;
static uint8_t   SectorsPerCluster;
static uint32_t  FATRegionStartSec;
static uint8_t   part_type;

#ifdef USE_FAT32
static uint32_t currentfatsector;
#else
static uint16_t currentfatsector;
#endif


uint8_t fat_init(void)
{
	mbr_t *mbr = (mbr_t*) fat_buf;
	vbr_t *vbr = (vbr_t*) fat_buf;
	uint8_t init, i;
	uint32_t  fss = 0;

	init = mmc_init();
	
	if (init != 0)
		return 1;
		
	//Load MBR
	mmc_read(fss);

	if (mbr->sector.magic != 0xAA55)
		return 2;
		
	//Try sector 0 as MBR
	part_type = 0;
	for (i = 0; i < 4; i++) {
		if ((mbr->sector.partition[i].state & 0x7f)  == 0) {
			switch(mbr->sector.partition[i].typeId) {
				//case PARTTYPE_FAT16S:
				case PARTTYPE_FAT16L:
					part_type = PARTTYPE_FAT16L;
					break;
				#ifdef USE_FAT32
				case PARTTYPE_FAT32:
				case PARTTYPE_FAT32L:
					part_type = PARTTYPE_FAT32L;
					break;
				#endif
				#ifdef USE_FAT12
				case PARTTYPE_FAT12:
					part_type = PARTTYPE_FAT12;
					break;
				#endif
			}
			if(part_type) {
				fss = mbr->sector.partition[i].sectorOffset;
				break;
			}
		}
	}

	//Load VBR
	mmc_read(fss);
	
	SectorsPerCluster  		= vbr->bsSecPerClus;		// 8
	
	// Calculation Algorithms
	fss			+= vbr->bsRsvdSecCnt;			// 6
	FATRegionStartSec	= fss;
	RootDirRegionSize	= vbr->bsRootEntCnt / (MMC_SECTOR_SIZE/sizeof(direntry_t));	// 32
	#ifdef USE_FAT32
	if(part_type == PARTTYPE_FAT32L) {
		DataRegionStartSec 	= fss + vbr->bsNumFATs * vbr->SecPerFAT32;
		RootDirRegionStartSec	= (vbr->RootDirCluster - 2) * SectorsPerCluster + DataRegionStartSec;
	}
	else
	#endif
	{
		RootDirRegionStartSec	= fss + (vbr->bsNumFATs * vbr->bsNrSeProFAT16);			// 496	
		DataRegionStartSec 	= RootDirRegionStartSec + RootDirRegionSize;			// 528
	}

	return 0;
}


#ifdef USE_FAT32
uint32_t fat_readRootDirEntry(uint16_t entry_num)
#else
uint16_t fat_readRootDirEntry(uint16_t entry_num)
#endif
{
	uint16_t   dirsector;
	direntry_t *dir;	//Zeiger auf einen Verzeichniseintrag
	
	dirsector = entry_num / (MMC_SECTOR_SIZE/sizeof(direntry_t));
        if (RootDirRegionSize && (dirsector >= RootDirRegionSize))
		//return 0xFFFF; //End of root dir region reached!
		return -1; //End of root dir region reached!
	entry_num %= MMC_SECTOR_SIZE / sizeof(direntry_t);
	mmc_read(RootDirRegionStartSec + dirsector);

	dir = (direntry_t *) fat_buf + entry_num;

	#ifdef USE_FAT32
	if ((dir->name[0] == 0) || (dir->name[0] == 0xE5) || (dir->fstclust == 0 && dir->fstclusthi == 0))
	#else
	if ((dir->name[0] == 0) || (dir->name[0] == 0xE5) || (dir->fstclust == 0))
	#endif
		//return 0xFFFF;
		return -1;
		
	filesize = dir->filesize;
	
	#ifdef USE_FAT32
	return ((uint32_t) dir->fstclust | (uint32_t) dir->fstclusthi <<16);
	#else
	return dir->fstclust;
	#endif
}

#ifdef USE_FAT32
static void load_fat_sector(uint32_t sec)
#else
static void load_fat_sector(uint16_t sec)
#endif
{
	if (currentfatsector != sec)
	{
		mmc_read(FATRegionStartSec + sec);
		currentfatsector = sec;
	}
}


#ifdef USE_FAT32
void fat_readfilesector(uint32_t startcluster, uint32_t filesector)
#else
void fat_readfilesector(uint16_t startcluster, uint16_t filesector)
#endif
{
	uint16_t clusteroffset;
	uint8_t  temp, secoffset;
	uint32_t templong;
	
	fatsector_t *fatsector = (fatsector_t*) fat_buf;
	
	clusteroffset = filesector; // seit uint16_t filesector gehts nun doch ;)
	temp = SectorsPerCluster >> 1;
	while(temp)
	{
		clusteroffset >>= 1;
		temp >>= 1;
	}

	secoffset = (uint8_t)filesector & (uint8_t)(SectorsPerCluster-1); // SectorsPerCluster is always power of 2 !
	
	//currentfatsector = 0xFFFF;
	currentfatsector = -1;
	
	while (clusteroffset)
	{
		#ifdef USE_FAT12
		if (part_type == PARTTYPE_FAT12)
		{//FAT12
			uint32_t fat12_bytenum;
			uint8_t clust1, clust2;
	
			//fat12_bytenum = (startcluster * 3 / 2);
			fat12_bytenum = (startcluster + (startcluster>>1));
		
			load_fat_sector(fat12_bytenum / MMC_SECTOR_SIZE);
			clust1 = fatsector->fat_bytes[fat12_bytenum % MMC_SECTOR_SIZE];
			fat12_bytenum++;
			load_fat_sector(fat12_bytenum / MMC_SECTOR_SIZE);
			clust2 = fatsector->fat_bytes[fat12_bytenum % MMC_SECTOR_SIZE];
		
			if (startcluster & (uint8_t)0x01)
				startcluster = ((clust1 & 0xF0) >> 4) | (clust2 << 4);
			else
				startcluster = clust1 | ((clust2 & 0x0F) << 8);
		}
		else
		#endif
		#ifdef USE_FAT32
		if (part_type == PARTTYPE_FAT32L) {
			load_fat_sector(startcluster / 128);
			startcluster = fatsector->fat32_entries[startcluster % 128];
		}
		else
		#endif
		{//FAT16
			load_fat_sector(startcluster / 256 );
			startcluster = fatsector->fat16_entries[startcluster % 256];
		}
		
		clusteroffset--;
	}
	
	if (startcluster < 2)
		return; //Free/reserved Sector
	
	#ifdef USE_FAT32
	if (part_type == PARTTYPE_FAT32L) {
		if ((startcluster & 0xFFFFFFF0) == 0xFFFFFFF0) 
			return; //Reserved / bad / last cluster
	}
	else
	#endif
	{
		if ((startcluster & 0xFFF0) == 0xFFF0) //FAT16 hat nur 16Bit Clusternummern
			return; //Reserved / bad / last cluster
	}

	templong = startcluster - 2;
	temp = SectorsPerCluster >> 1;
	while(temp)
	{
		templong <<= 1;
		temp >>= 1;
	}
		
	mmc_read(templong + DataRegionStartSec + secoffset);
}
