/**
 @file gpib/lifutils.c

 @brief LIF file utilities

 @par Edit History
 - [1.0]   [Mike Gore]  Initial revision of file.

 @par Copyright &copy; 2014-2017 Mike Gore, Inc. All rights reserved.

*/

#include "user_config.h"

#include "defines.h"
#include "drives.h"
#include <time.h>
#include "lifutils.h"


/// @brief
///  Help Menu for User invoked GPIB functions and tasks
///  See: int gpib_tests(char *str)
/// @return  void

void lif_help()
{
    printf(
		"lifhelp\n"
        "lifadd lifimage lifname file\n"
        "lifaddbin lifimage lifname file\n"
        "lifcreate lifimage label directory_sectors sectors\n"
        "lifdel\n"
        "lifdir\n"
        "lifextract lifimage lifname file\n"
        "lifextractbin lifimage lifname file\n"
        "lifrename old new\n"
        );
}

/// @brief LIFGuser tests
/// @return  1 matched token, 0 if not
int lif_tests(char *str)
{

    int len;
    char *ptr;

    ptr = skipspaces(str);

    if ((len = token(ptr,"lifhelp")) )
	{
		lif_help();
		return(1);
	}
    if ((len = token(ptr,"lifaddbin")) )
    {
        char name[64];
        char lifname[64];
        char user[64];

        ptr += len;

        // LIF name
        ptr = get_token(ptr, name, 63);

        // lif file name
        ptr = get_token(ptr, lifname, 63);

        // User file name
        ptr = get_token(ptr, user, 63);

		lif_add_lif_file(name, lifname, user);

        return(1);
    }
    if ((len = token(ptr,"lifadd")) )
    {
        char name[64];
        char lifname[64];
        char user[64];

        ptr += len;

        // LIF name
        ptr = get_token(ptr, name, 63);

        // lif file name
        ptr = get_token(ptr, lifname, 63);

        // User file name
        ptr = get_token(ptr, user, 63);

		lif_add_ascii_file_as_e010(name, lifname, user);

        return(1);
    }
    if ((len = token(ptr,"lifdel")) )
    {
        char name[64];
        char lifname[64];

        ptr += len;

        // LIF name
        ptr = get_token(ptr, name, 63);

        // lif file name
        ptr = get_token(ptr, lifname, 63);

		lif_del_file(name, lifname);

        return(1);
    }
    if ((len = token(ptr,"lifcreate")) )
    {
        char name[64],label[6];
        char num[12];
        long dirsecs, sectors, result;

        ptr += len;

        // LIF name
        ptr = get_token(ptr, name, 63);

        // LIF LABEL
        ptr = get_token(ptr, label, 7);

        // Directory Sectors
        ptr = get_token(ptr, num, 11);
        dirsecs = atol(num);

        // Image total Sectors
        ptr = get_token(ptr, num, 11);
        sectors= atol(num);

        ///@brief format LIF image
        result = lif_create_image(name,label,dirsecs,sectors);
        if(result != sectors)
        {
            if(debuglevel & 1)
                printf("create_format_image: failed\n");
        }
        return(1);
    }
    else if ((len = token(ptr,"lifdir")) )
    {
        char name[64];
        ptr += len;
        // LIF name
        ptr = get_token(ptr, name, 63);
        lif_dir(name);
        return(1);
    }
    if ((len = token(ptr,"lifextractbin")) )
	{

        char name[64];
        char lifname[64];
        char user[64];

        ptr += len;

        // LIF name
        ptr = get_token(ptr, name, 63);

        // lif file name
        ptr = get_token(ptr, lifname, 63);

        // User file name
        ptr = get_token(ptr, user, 63);

		lif_extract_lif_file(name, lifname, user);
        return(1);
	}
    if ((len = token(ptr,"lifextract")) )
	{

        char name[64];
        char lifname[64];
        char user[64];

        ptr += len;

        // LIF name
        ptr = get_token(ptr, name, 63);

        // lif file name
        ptr = get_token(ptr, lifname, 63);

        // User file name
        ptr = get_token(ptr, user, 63);

		lif_extract_e010_as_ascii(name, lifname, user);

        return(1);
	}
    if ((len = token(ptr,"lifrename")) )
    {
        char name[64];
        char oldlifname[64];
        char newlifname[64];

        ptr += len;

        // LIF name
        ptr = get_token(ptr, name, 63);

        // old lif file name
        ptr = get_token(ptr, oldlifname, 63);

        // new lif file name
        ptr = get_token(ptr, newlifname, 63);

		lif_rename_file(name, oldlifname, newlifname);

        return(1);
    }
	return(0);
}

/// @brief Open a file that must exist
/// @param[in] *name: file name of LIF image
/// @param[in] *mode: open mode - see fopen
/// @return FILE * pointer
FILE *lif_open(char *name, char *mode)
{
    FILE *fp = fopen(name, mode);
    if( fp == NULL)
    {
		if(debuglevel & 1)
			printf("lif_open: Can't open:[%s] mode:[%s]\n", name, mode);
        return(NULL);
    }
	return(fp);
}
/// @brief Stat a file 
/// @param[in] *name: file name of LIF image
/// @return struct stat *
stat_t *lif_stat(char *name, stat_t *p)
{
	if(stat(name, p) < 0)
	{
		if(debuglevel & 1)
			printf("lif_stat: Can't stat:%s\n", name);
        return(NULL);
	}
	return(p);
}

/// @brief Read data from a LIF image 
/// File is closed after read
/// WHY? We want time minimize file open to to avoid corruption on the SDCARD
/// @param[in] *name: file name of LIF image
/// @param[in] *buf: read buffer
/// @param[in] offset: read offset
/// @param[in] bytes: number of bytes to read
/// @return number of bytes read - OR - -1 on error
long lif_read(char *name, void *buf, long offset, int bytes)
{
	FILE *fp;
	long len;

	fp = lif_open(name, "r");
    if( fp == NULL)
        return(-1);

	if(fseek(fp, offset, SEEK_SET) < 0)
	{
		if(debuglevel & 1)
			printf("lif_read: Seek error %s @ %ld\n", name, offset);
		fclose(fp);
		return(-1);
	}

	///@brief Initial file position
	len = fread(buf, 1, bytes, fp);
	if( len != bytes)
	{
		if(debuglevel & 1)
			printf("lif_read: Read error %s @ %ld\n", name, offset);
		fclose(fp);
		return(-1);
	}
	fclose(fp);
	return(len);
}

/// @brief Write data to an LIF image 
/// File is closed, positioned to the end of file, after write
/// Why? We want time minimize file open to to avoid corruption on the SDCARD
/// @param[in] *name: file name of LIF image
/// @param[in] *buf: write buffer
/// @param[in] offset: write offset
/// @param[in] bytes: number of bytes to write
/// @return -1 on error or number of bytes written 
int lif_write(char *name, void *buf, long offset, int bytes)
{
	FILE *fp;
	long len;

	fp = lif_open(name, "r+");
    if( fp == NULL)
        return(-1);

	// Seek to write position
	if(fseek(fp, offset, SEEK_SET) < 0)
	{
		if(debuglevel & 1)
			printf("lif_write: Seek error %s @ %ld\n", name, offset);

		// try to seek to the end of file anyway
		fseek(fp, 0, SEEK_END);
		fclose(fp);
		return(-1);

	}

	///@brief Initial file position
	len = fwrite(buf, 1, bytes, fp);
	if( len != bytes)
	{
		if(debuglevel & 1)
			printf("lif_write: Write error %s @ %ld\n", name, offset);

		// seek to the end of file before close!
		fseek(fp, 0, SEEK_END);
		fclose(fp);
		return(len);
	}

	// seek to the end of file before close!
	fseek(fp, 0, SEEK_END);
	fclose(fp);
	return(len);
}



/// @brief Check if characters in a LIF volume or LIF file name are valid
/// @param[in] c: character to test
/// @param[in] index: index of character in volume or file name
/// @retrun c (optionally upper cased) or 0 if no match
int lif_chars(int c, int index)
{
	if(c == ' ')
		return(c);
	if(c >= 'a' && c <= 'z')
		return(c-0x20);
	if(c >= 'A' && c <= 'Z')
		return(c);
	if((index > 0) && (c >= '0' && c <= '9') )
		return(c);
	if((index > 0) && (( c == '_') || c == '-'))
		return(c);
	return(0);
}

/// @brief Convert LIF space padded string name into normal string
/// @param[in] *B: LIF name space padded
/// @param[out] *name: string result with traling spaces removed
/// @retrun 1 if string i ok or 0 if bad characters were found
int lif_B2S(uint8_t *B, uint8_t *name, int size)
{
    int i;
	int status = 1;
	for(i=0;i<size;++i)
	{
		if( !lif_chars(B[i],i))
			status = 0;
	}
	for(i=0;i<size;++i)
		name[i] = B[i];
	name[i] = 0;
	// remove space padding
	trim_tail((char *)name);
	return(status);
}
/// @brief Check that a IF volume name of directory name is valid
/// @param[in] *name: name to test
/// @retrun 1 if the string is ok or 0 if invalid LIF name characters on input string
int lif_checkname(char *name)
{
	int i;
	int status = 1;
    for(i=0;name[i];++i)
	{
		if(!lif_chars(name[i],i))
			status = 0;
	}
	return(status);
}

/// @brief string to LIF directory record
/// @param[out] *B: LIF result with added trailing spaces
/// @param[in] *name: string
/// @retrun 1 if the string is ok or 0 if invalid LIF name characters on input string
void lif_S2B(uint8_t *B, uint8_t *name, int size)
{
    int i;
    for(i=0;name[i] && i<size;++i)
	{
		B[i] = name[i];
	}
    for(;i<size;++i)
        B[i] = ' ';
}


///@brief Convert a file name (unix/fat32) format into a valid LIF name 
/// Only use the basename() part of the string and remove any file name extentions
/// LIF names may have only these characters: [A-Z][A-Z0-09_]+
/// LIF names are converted to upper case
/// LIF names are padded at the end with spaces
/// Any invalid input characters are converted into spaces
///@param[out] *B: output LIF string
///@param[in] *name: input string
///@param[in] size: maximum size of output string
///@return length of result
int lif_fixname(uint8_t *B, char *name, int size)
{
	uint8_t c,ret;
	int i,index;
	char *ptr;
	uint8_t *save = B;

	index = 0;
	ptr = basename(name);

	for(i=0; ptr[i] && index < size;++i)
	{
		c = ptr[i];
		// trim off extensions
		if(c == '.')
			break;
		if( (ret = lif_chars(c,i)) )
			*B++ = ret;
		else
			*B++ = ' ';
	}
	while(i < size)
	{
		*B++ = ' ';
		++i;
	};
	*B = 0;
	return(strlen((char *)save));
}


///@brief Convert volume part of LIF image into byte vector 
///@param[in] *LIF: LIF image structure
///@param[out] B: byte vector to pack data into
///@return NULL;
void lif_vol2str(lif_t *LIF, uint8_t *B)
{
	V2B_MSB(B,0,2,LIF->VOL.LIFid);
	lif_S2B(B+2,LIF->VOL.Label,6);
	V2B_MSB(B,8,4,LIF->VOL.DirStartSector);
	V2B_MSB(B,12,2,LIF->VOL.System3000LIFid);
	V2B_MSB(B,14,2,0);
	V2B_MSB(B,16,4,LIF->VOL.DirSectors);
	V2B_MSB(B,20,2,LIF->VOL.LIFVersion);
	V2B_MSB(B,22,2,0);
	V2B_MSB(B,24,4,LIF->VOL.tracks_per_side);
	V2B_MSB(B,28,4,LIF->VOL.sides);
	V2B_MSB(B,32,4,LIF->VOL.sectors_per_track);
	memcpy((void *) (B+36),LIF->VOL.date,6);
}

///@brief Convert byte vector into volume part of LIF image 
///@param[in] B: byte vector 
///@param[out] *LIF: LIF image structure
///@return 1 if the volume is good, 0 if bad
void lif_str2vol(uint8_t *B, lif_t *LIF)
{

	LIF->VOL.LIFid = B2V_MSB(B,0,2);
	lif_B2S(B+2,LIF->VOL.Label,6);
	LIF->VOL.DirStartSector = B2V_MSB(B,8,4);
	LIF->VOL.System3000LIFid = B2V_MSB(B,12,2);
	LIF->VOL.zero1 = B2V_MSB(B,14,2);
	LIF->VOL.DirSectors = B2V_MSB(B,16,4);
	LIF->VOL.LIFVersion = B2V_MSB(B,20,2);
	LIF->VOL.zero2 = B2V_MSB(B,22,2);
	LIF->VOL.tracks_per_side = B2V_MSB(B,24,4);
	LIF->VOL.sides = B2V_MSB(B,28,4);
	LIF->VOL.sectors_per_track = B2V_MSB(B,32,4);
	memcpy((void *) LIF->VOL.date, (B+36),6);
}

///@brief Convert current Directory Entry of LIF image into byte vector
///@param[int] *LIF: LIF image pointer
///@param[out] B: byte vector to pack data into
///@return void
void lif_dir2str(lif_t *LIF, uint8_t *B)
{
	lif_S2B(B,LIF->DIR.filename,10);				// 0
	V2B_MSB(B,10,2,LIF->DIR.FileType);			// 10
	V2B_MSB(B,12,4,LIF->DIR.FileStartSector);	// 12
	V2B_MSB(B,16,4,LIF->DIR.FileSectors);	// 16
	memcpy(B+20,LIF->DIR.date,6);				// 20
	V2B_MSB(B,26,2,LIF->DIR.VolNumber);			// 26
	V2B_LSB(B,28,2,LIF->DIR.FileBytes);			// 28
	V2B_LSB(B,30,2,LIF->DIR.SectorSize);			// 30
}

///@brief Convert byte vector into current Directory Entry of LIF image 
///@param[in] B: byte vector to extract data from
///@param[int] LIF: lifdir_t structure pointer
///@return LIF
void lif_str2dir(uint8_t *B, lif_t *LIF)
{
	lif_B2S(B,LIF->DIR.filename,10);
	LIF->DIR.FileType = B2V_MSB(B, 10, 2);
	LIF->DIR.FileStartSector = B2V_MSB(B, 12, 4);
	LIF->DIR.FileSectors = B2V_MSB(B, 16, 4);
	memcpy(LIF->DIR.date,B+20,6);
	LIF->DIR.VolNumber = B2V_MSB(B, 26, 2); 
	LIF->DIR.FileBytes = B2V_LSB(B, 28, 2);
	LIF->DIR.SectorSize= B2V_LSB(B, 30, 2);
}

/// @brief Convert number >= 0 and <= 99 to BCD.
///
///  - BCD format has each hex nibble has a digit 0 .. 9
///
/// @param[in] data: number to convert.
/// @return  BCD value
/// @warning we assume the number is in range.
uint8_t lif_BIN2BCD(uint8_t data)
{
    return(  ( (data/10U) << 4 ) | (data%10U) );
}


///@brief UNIX time to LIF time format
///@param[out] bcd: packed 6 byte BCD LIF time
///   YY,MM,DD,HH,MM,SS
///@param[in] t: UNIX time_t time value
///@see time() in time.c
///@return void
void lif_time2lif(uint8_t *bcd, time_t t)
{
	tm_t tm;
	localtime_r((time_t *) &t, (tm_t *)&tm);
	bcd[0] = lif_BIN2BCD(tm.tm_year & 100);
	bcd[1] = lif_BIN2BCD(tm.tm_mon);
	bcd[2] = lif_BIN2BCD(tm.tm_mday);
	bcd[3] = lif_BIN2BCD(tm.tm_hour);
	bcd[4] = lif_BIN2BCD(tm.tm_min);
	bcd[5] = lif_BIN2BCD(tm.tm_sec);
}

/// @brief Clear main lif_t LIF structure 
/// @param[in] *LIF: pointer to LIF structure
/// @return void
void lif_image_clear(lif_t *LIF)
{
	memset((void *) LIF,0,sizeof(lif_t));
}


/// @brief Clear lifdir_t DIR structure in LIF
/// @param[in] *LIF: pointer to LIF structure
/// @return void
void lif_dir_clear(lif_t *LIF)
{
	memset((void *) &LIF->DIR,0,sizeof(lifdir_t));
}

/// @brief Clear lifvol_t V structure in LIF
/// @param[in] *LIF: pointer to LIF structure
/// @return void
void lif_vol_clear(lif_t *LIF)
{
	memset((void *) &LIF->VOL,0,sizeof(lifvol_t));
}

void lif_dump_vol(lif_t *LIF)
{
   printf("LIF name:             %s\n", LIF->name);
   printf("LIF sectors:          %8lXh\n", (long)LIF->sectors);
   printf("LIF bytes:            %8lXh\n", (long)LIF->bytes);
   printf("LIF filestart:        %8lXh\n", (long)LIF->filestart);
   printf("LIF file sectors:     %8lXh\n", (long)LIF->filesectors);
   printf("LIF used:             %8lXh\n", (long)LIF->used);
   printf("LIF free:             %8lXh\n", (long)LIF->free);
   printf("LIF files:            %8lXh\n",(long)LIF->files);
   printf("LIF purged:           %8lXh\n",(long)LIF->purged);

   printf("VOL Label:            %s\n", LIF->VOL.Label);
   printf("VOL LIFid:            %8Xh\n",(int)LIF->VOL.LIFid);
   printf("VOL Dir start:        %8lXh\n",(long)LIF->VOL.DirStartSector);
   printf("VOL Dir sectors:      %8lXh\n",(long)LIF->VOL.DirSectors);
   printf("VOL 3000LIFid:        %8Xh\n",(long)LIF->VOL.System3000LIFid);
   printf("VOL LIFVersion:       %8Xh\n",(long)LIF->VOL.LIFVersion);

   printf("DIR File Name:        %s\n", LIF->DIR.filename);
   printf("DIR File Type:        %8Xh\n", (int)LIF->DIR.FileType);
   printf("DIR File Volume#:     %8Xh\n", (int)LIF->DIR.VolNumber);
   printf("DIR File start:       %8lXh\n", (long)LIF->DIR.FileStartSector);
   printf("DIR File sectors:     %8lXh\n", (long)LIF->DIR.FileSectors);
   printf("DIR File bytes:       %8lXh\n", (long)LIF->DIR.FileBytes);
   printf("DIR File sector size: %8Xh\n", (int)LIF->DIR.SectorSize);
}
///@brief Check Volume Table
///@param[in] *LIF: Image structure
///@return 1 of ok, 0 on eeror
int lif_check_volume(lif_t *LIF)
{
	int status = 1;
	if( !lif_checkname((char *)LIF->VOL.Label) )
	{
		status = 0;
		if(debuglevel & 1)
			printf("LIF Volume invalid Volume Name");
	}

	if(LIF->VOL.DirStartSector < 1)
	{
		status = 0;
		if(debuglevel & 1)
			printf("LIF Volume invalid start sector:%ld\n", LIF->VOL.DirStartSector);
	}
	if(LIF->VOL.DirSectors < 1)
	{
		if(debuglevel & 1)
			printf("LIF Volume invalid Directory Sector Count < 1\n");
		status = 0;
	}

	if(LIF->VOL.System3000LIFid != 0x1000)
	{
		status = 0;
		if(debuglevel & 1)
			printf("LIF Volume invalid System3000 ID (%04XH) expected 1000H\n", LIF->VOL.System3000LIFid);
	}
	if(LIF->VOL.LIFVersion != 0)
	{
		if(debuglevel & 1)
			printf("LIF Version: %04XH != 0\n", LIF->VOL.LIFVersion);
		status = 0;
	}

	if(LIF->VOL.zero1 != 0)
	{
		if(debuglevel & 1)
			printf("LIF Volume invalid bytes at offset 14&15 sgould be zero\n");
		status = 0;
	}
	if(LIF->VOL.zero2 != 0)
	{
		if(debuglevel & 1)
			printf("LIF Volume invalid bytes at offset 22&23 sgould be zero\n");
		status = 0;
	}

	return(status);
}

///@brief Check LIF headers
///@param[in] *LIF: Image structure
///@return 1 of ok, 0 on eeror
int lif_check_lif_headers(lif_t *LIF)
{
	int status = 1;


    // File start is after the directory area
    if(LIF->filestart > LIF->sectors)
    {
		if(debuglevel & 1)
            printf("LIF Volume invalid file area start > image size\n");
        status = 0;
    }

    // File start is after the directory area
    if(LIF->filestart < LIF->VOL.DirStartSector)
    {
		if(debuglevel & 1)
            printf("LIF Volume invalid file area start < directory start\n");
        status = 0;
    }

    // Check Directory pointers
    if(LIF->filesectors < 1)
    {
		if(debuglevel & 1)
            printf("LIF Volume invalid file area size < 1\n");
        status = 0;
    }

    // Check Directory pointers
    if(LIF->free < 1)
    {
		if(debuglevel & 1)
            printf("LIF Volume invalid file area size < 1\n");
        status = 0;
    }

	return(status);
}


///@brief Validate Directory record
/// We only do basic out of bounds tests for this record
/// We do NOT not check these values agains data from other directory records
/// Purged or EOF directory records are NOT checked and always return 1
///@param[in] *LIF: Image structure
///@param[in] debug: dispaly diagostice messages
///@return 1 of ok, 0 on eeror
int lif_check_dir(lif_t *LIF)
{
	int status = 1;


	LIF->filesectors = LIF->sectors - LIF->filestart;

	// We do not check purged or end of DIRECTORY ercordss
	if(LIF->DIR.FileType == 0)
	{
		return(1);
	}

	if(LIF->DIR.FileType == 0xffff)
	{
		return(1);
	}
	
	if( !lif_checkname((char *)LIF->DIR.filename) )
	{
		status = 0;
		if(debuglevel & 1)
			printf("LIF Directory invalid Name:[%s]\n",LIF->DIR.filename);
	}

	if(LIF->DIR.FileStartSector < LIF->filestart)
	{
		status = 0;
		if(debuglevel & 1)
			printf("LIF Directory invalid start sector:%lXh\n", (long)LIF->DIR.FileStartSector);
	}

	if( (LIF->DIR.FileStartSector + LIF->DIR.FileSectors) > (LIF->filestart + LIF->sectors) )
	{
		status = 0;
		if(debuglevel & 1)
			printf("LIF Directory invalid end sector:%lXh\n", (long)LIF->DIR.FileStartSector + LIF->DIR.FileSectors);
	}

	if(LIF->DIR.VolNumber != 0x8001)
	{
		status = 0;
		if(debuglevel & 1)
			printf("LIF Directory invalid Volume Number:%Xh\n", (int)LIF->DIR.VolNumber);
	}

//FIXME check file types!
	if( LIF->DIR.FileBytes && lif_bytes2sectors(LIF->DIR.FileBytes) != LIF->DIR.FileSectors )
	{
		status = 0;
		if(debuglevel & 1)
			printf("LIF Directory invalid FileBytes size:%ld\n", (long) LIF->DIR.FileBytes);
	}

//FIXME check file types!
	if(LIF->DIR.SectorSize != LIF_SECTOR_SIZE)
	{
		status = 0;
		if(debuglevel & 1)
			printf("LIF Directory invalid sector size:%ld\n", LIF->DIR.SectorSize);
	}

	return(status);
}


/// @brief Create a volume header 
/// @param[in] imagename:  Image name
/// @param[in] liflabel:   Volume Label
/// @param[in] dirstart:   Directory start sector
/// @param[in] dirsectors: Directory sectors
/// @return void
lif_t *lif_create_volume(char *imagename, char *liflabel, int dirstart, int dirsectors)
{
	FILE *fp;
	long size;
	int count;
	int i;
	uint8_t buffer[LIF_SECTOR_SIZE];

	lif_t *LIF = safecalloc(sizeof(lif_t)+4,1);
	if(LIF == NULL)
	{
		if(debuglevel & 1)
			printf("lif_create_volume:[%s] errro not enough ram\n", liflabel);
		return(NULL);
	}
	
	lif_image_clear(LIF);
    // Initialize volume header
	LIF->name[LIF_IMAGE_NAME_SIZE-1] = 0;
    LIF->VOL.LIFid = 0x8000;
    lif_fixname(LIF->VOL.Label, liflabel, 6);
    LIF->VOL.DirStartSector = dirstart;
    LIF->VOL.DirSectors = dirsectors;
    LIF->VOL.System3000LIFid = 0x1000;
    LIF->VOL.tracks_per_side = 0;
    LIF->VOL.sides = 0;
    LIF->VOL.sectors_per_track = 0;
    ///@brief Current Date
    lif_time2lif(LIF->VOL.date, time(NULL));

	memset(buffer,0,LIF_SECTOR_SIZE);

	lif_vol2str(LIF,buffer);

	// Write Volume header
	fp = lif_open(imagename,"w");
	if(fp == NULL)
	{
		lif_close_volume(LIF);
		return(NULL);
	}
	size = fwrite(buffer,1,LIF_SECTOR_SIZE,fp);	
	if(size < LIF_SECTOR_SIZE)
	{
		if(debuglevel & 1)
			printf("lif_create_volume:[%s] write error\n", liflabel);
		lif_close_volume(LIF);
		fclose(fp);
		return(NULL);
	}
	fclose(fp);

	// update LIF headers
	strncpy(LIF->name, imagename, LIF_IMAGE_NAME_SIZE);
	LIF->name[LIF_IMAGE_NAME_SIZE-1] = 0;
	LIF->sectors = (dirstart+dirsectors);
	LIF->bytes = LIF->sectors * LIF_SECTOR_SIZE;
	LIF->filestart = (dirstart+dirsectors);
	LIF->filesectors = 0;
	LIF->used = 0;
	LIF->free = 0;
	LIF->files = 0;
	LIF->purged = 0;
	LIF->index = -1;

	count = dirsectors * LIF_DIR_RECORDS_PER_SECTOR;
	for(i=0;i<count;++i)
	{
		if(!lif_writedirEOF(LIF,i))
		{
			lif_close_volume(LIF);
			return(NULL);

		}
	}

	lif_dump_vol(LIF);

	return(LIF);
}

/// @brief Free LIF structure
/// @param[in] *LIF: pointer to LIF Volume/Directoy structure
/// @return void
void lif_close_volume(lif_t *LIF)
{
	if(LIF)
	{
		lif_vol_clear(LIF);
		safefree(LIF);
	}
}



/// @brief Convert bytes into used sectors
/// @param[in] bytes: size in bytes
/// @return sectors
uint32_t lif_bytes2sectors(uint32_t bytes)
{
	uint32_t sectors = (bytes/LIF_SECTOR_SIZE);
	if(bytes % LIF_SECTOR_SIZE)
		++sectors;
	return(sectors);
}


/// @brief rewind LIF directory 
/// Note readdir pre increments the directory pointer index
/// Modeled after Linux rewinddir()
/// @param[in] *LIF: pointer to LIF Volume/Directoy structure
/// @return void
void lif_rewinddir(lif_t *LIF)
{
	// Directory index
	LIF->index = -1;
}

/// @brief Close LIF directory 
/// Modeled after Linux closedir()
/// clear and free lif_t structure
/// @param[in] *LIF: pointer to LIF Volume/Directoy structure
/// @return 0 on sucesss, -1 on error
int lif_closedir(lif_t *LIF)
{
	if(LIF)
	{
		lif_close_volume(LIF);
		return(0);
	}
	return(-1);
}

/// @brief check if directory index is outside of directory limits
/// @param[in] *LIF: LIF Volume/Diractoy structure 
/// @param[in] index: directory index
/// @return 1 inside, 0 outside
int lif_checkdirindex(lif_t * LIF, int index)
{
	if(index < 0)
		return(0);
 	if( lif_bytes2sectors((long) index * LIF_DIR_SIZE) > LIF->VOL.DirSectors)
		return(0);
	return(1);
}

/// @brief Read LIF directory record number N
/// @param[in] *LIF: to LIF Volume/Diractoy structure 
/// @param[in] index: director record number
/// @return 1 on success, 0 if error, bad directory record or outside of directory limits
int lif_readdirindex(lif_t *LIF, int index)
{
	uint32_t offset;
	uint8_t dir[LIF_DIR_SIZE];


	if( !lif_checkdirindex(LIF, index) )
	{
		if(debuglevel & 1)
			printf("lif_readdirindex:[%d] out of bounds\n", index);
		if(debuglevel & 0x400)
			lif_dump_vol(LIF);
		return(0);
	}

	offset = (index * LIF_DIR_SIZE) + (LIF->VOL.DirStartSector * LIF_SECTOR_SIZE);

	// read raw data
	if( (lif_read(LIF->name, dir, offset, sizeof(dir)) < (long)sizeof(dir)) )
	{
		if(debuglevel & 1)
			printf("lif_readdirindex:[%d] read error\n", index);
		if(debuglevel & 0x400)
			lif_dump_vol(LIF);
        return(0);
	}

	// Convert into directory structure
	lif_str2dir(dir, LIF);

	if( !lif_check_dir(LIF))
	{
		if(debuglevel & 0x400)
			lif_dump_vol(LIF);
		return(0);
	}
	return(1);
}

/// @brief Write LIF drectory record number N
/// @param[in] *LIF: LIF Volume/Diractoy structure 
/// @param[in] index: director record number
/// @return 1 on success, 0 on error and outsize directory limits
int lif_writedirindex(lif_t *LIF, int index)
{
	long offset;
	uint8_t dir[LIF_DIR_SIZE];

	// Validate the record
	if(!lif_check_dir(LIF))
	{
		if(debuglevel & 0x400)
			lif_dump_vol(LIF);
		return(0);
	}

	offset = (index * LIF_DIR_SIZE) + (LIF->VOL.DirStartSector * LIF_SECTOR_SIZE);

	// check for out of bounds
	if( !lif_checkdirindex(LIF, index))
	{
		if(debuglevel & 1)
			printf("lif_writedirindex:[%d] out of bounds\n", index);
		if(debuglevel & 0x400)
			lif_dump_vol(LIF);
		return(0);
	}

	// store LIF->DIR settings into dir
	lif_dir2str(LIF, dir);

	if( (lif_write(LIF->name, dir, offset, sizeof(dir)) < (int ) sizeof(dir)) )
	{
		if(debuglevel & 1)
			printf("lif_writedirindex:[%d] write error\n", index);
		if(debuglevel & 0x400)
			lif_dump_vol(LIF);
        return(0);
	}

	return(1);
}

/// @brief Write LIF drectory EOF
/// @param[in] *LIF: LIF Volume/Diractoy structure 
/// @param[in] index: director record number
/// @return 1 on success, 0 on error and outsize directory limits
int lif_writedirEOF(lif_t *LIF, int index)
{
	// Create a director EOF
	lif_dir_clear(LIF);
	LIF->DIR.FileType = 0xffff;
	return( lif_writedirindex(LIF,index));
}


/// @brief Read a directory record from a lif image and advance the directory index
/// We skip all purged LIF directory records
/// Modeled after Linux readdir()
/// @param[in] *LIF: to LIF Volume/Diractoy structure 
/// @return directory structure or NULL 
lifdir_t *lif_readdir(lif_t *LIF)
{
	while(1)
	{
		// Advance index first 
		// We start initialized at -1 by lif_opendir() and lif_rewinddir()
		LIF->index++;

		if( !lif_readdirindex(LIF, LIF->index) )
			break;

		if( LIF->DIR.FileType == 0xffffUL )
			break;

		if(LIF->DIR.FileType)
			return( (lifdir_t *) &LIF->DIR );

		// Skip purged records
	}
	return( NULL );
}


/// @brief Open LIF directory for reading
/// Modeled after Linux opendir()
/// @param[in] *name: file name of LIF image
/// @param[in] debug: Display extending error messages
/// @return NULL on error, lif_t pointer to LIF Volume/Directoy structure on sucesss
lif_t *lif_opendir(char *name)
{
	lif_t *LIF;
	int index = 0;
	stat_t sb, *sp;
	uint8_t buffer[LIF_SECTOR_SIZE];


	sp = lif_stat(name, (stat_t *)&sb);
	if(sp == NULL)
        return(NULL);

	LIF = safecalloc(sizeof(lif_t)+4,1);
	if(!LIF)
		return(NULL);

	strncpy(LIF->name, name, LIF_IMAGE_NAME_SIZE);
	LIF->name[LIF_IMAGE_NAME_SIZE-1] = 0;

	LIF->bytes = sp->st_size;
	LIF->sectors = lif_bytes2sectors(sp->st_size);

	// Used sectors
	LIF->used = 0;
	// Purged files
	LIF->purged= 0;
	// Files
	LIF->files = 0;

	// Must be at least bin enough to hold the volume header and directory
	if(LIF->sectors < 1)
	{
		if(debuglevel & 1)
			printf("lif_openimage:[%s] invalid volume header area too small:[%d]\n", LIF->name, (long)LIF->sectors);
	
		lif_closedir(LIF);
		return(NULL);
	}

	// Volume header must be it least one sector
 	if( lif_read(LIF->name, buffer, 0, LIF_SECTOR_SIZE) < LIF_SECTOR_SIZE)
	{
		lif_closedir(LIF);
        return(NULL);
	}

	// Unpack Volumes has the Directory start sector
	lif_str2vol(buffer, LIF);

	// File area start and size
    LIF->filestart = LIF->VOL.DirStartSector + LIF->VOL.DirSectors;
    LIF->filesectors = LIF->sectors - LIF->filestart;
	LIF->free = LIF->filesectors;

	// Validate 
	if( !lif_check_volume(LIF) || !lif_check_lif_headers(LIF))
	{
		if(debuglevel & 1)
			printf("lif_openimage:[%s] invalid volume header\n", LIF->name);
		if(debuglevel & 0x400)
			lif_dump_vol(LIF);
		lif_closedir(LIF);
		return(NULL);
	}

	index = 0;
	/// Update free
	while(1)
	{
		if( !lif_readdirindex(LIF,index) )
		{
			lif_closedir(LIF);
			return(NULL);
		}

		if(LIF->DIR.FileType == 0xffff)
			break;

		if(LIF->DIR.FileType == 0)
		{
			LIF->purged++;
			++index;
			continue;
		}
		LIF->used += LIF->DIR.FileSectors;
		LIF->free -= LIF->DIR.FileSectors;
		LIF->files++;
		++index;
	}
	// rewind
	lif_rewinddir(LIF);
	return(LIF);
}

/// @brief Display a LIF image file directory
/// @param[in] lifimagename: LIF disk image name
/// @return -1 on error or number of files found
int lif_dir(char *lifimagename)
{
	long bytes;
	lif_t *LIF;
	int index = 0;

	LIF = lif_opendir(lifimagename);
	if(LIF == NULL)
		return(-1);
	
	printf("Volume: [%s]\n", LIF->VOL.Label);
	printf("NAME         TYPE   START SECTOR        SIZE     RECSIZE\n");
	while(1)
	{
		if(!lif_readdirindex(LIF,index))
			break;
		if(LIF->DIR.FileType == 0xffff)
			break;
;
		bytes = LIF->DIR.FileBytes;
		if(!bytes)
			bytes = (LIF->DIR.FileSectors * LIF_SECTOR_SIZE);

		// name type start size
		printf("%-10s  %04Xh      %8lXh   %9ld       %4d\n", 
			(LIF->DIR.FileType ? (char *)LIF->DIR.filename : "<PURGED>"), 
			(int)LIF->DIR.FileType, 
			(long)LIF->DIR.FileStartSector, 
			(long)bytes, 
			(int)LIF->DIR.SectorSize  );

		++index;
	}	

	printf("\n");
	printf("%8ld Files\n", (long)LIF->files);
	printf("%8ld Purged\n", (long)LIF->purged);
	printf("%8ld Used sectors\n", (long)LIF->used);
	printf("%8ld Free sectors\n", (long)LIF->free);

	lif_closedir(LIF);
	return(LIF->files);
}


/// @brief Find a directory index for a file in a LIF image file
/// @param[in] *LIF: directory pointer
/// @param[in] liflabel: File name in LIF image
/// @return lifdir_t * directory record, or NULL if no match
int lif_find_file(lif_t *LIF, char *liflabel)
{
	int index;

	if( !lif_checkname(liflabel) )
	{
		if(debuglevel & 1)
			printf("lif_find_file:[%s] invalid characters\n", liflabel);
		return(-1);
	}
	if(strlen(liflabel) > 10)
	{
		if(debuglevel & 1)
			printf("lif_find_file:[%s] liflabel too big\n", liflabel);
		return(-1);
	}

	if(LIF == NULL)
		return(-1);
	
	index = 0;
	while(1)
	{
		if(!lif_readdirindex(LIF,index))
			return(-1);

		if(LIF->DIR.FileType == 0xffff)
			return(-1);

		if( LIF->DIR.FileType && (strcasecmp((char *)LIF->DIR.filename,liflabel) == 0) )
			break;
		++index;
	}
	return(index);
}

/// @brief Find directory slot index that can hold >= sectors in size
/// Unless a record is purged this will always be the last record
/// We update the directory start and size values
/// @param[in] *LIF: LIF pointer
/// @param[out] *free: free space directory index and file start and size
/// @param[in] sectors: size of free space we need
/// @return index or -1 on error
int lif_findfree_dirindex(lif_t *LIF, uint32_t sectors)
{
	// Directory index
	int index = 0;
	int purged = 0;

	// Master start of file area
	uint32_t start = LIF->filestart;

	// Clear free structure
	memset((void *)&LIF->space,0,sizeof(lifspace_t));

	if(LIF == NULL)
		return(-1);

	// Volume free space
	if(sectors > LIF->free)
		return(-1);

	while(1)
	{
		if(!lif_readdirindex(LIF,index))
			break;

		// EOF Record
		if(LIF->DIR.FileType == 0xffff)
		{
			// Start of free space is after last non-type 0 record - if we had a last record
			LIF->space.start = start;
			LIF->space.size = sectors;
			LIF->space.index = index;
			LIF->space.eof = 1;
			LIF->index = index;
			return(index);
		}

		// Purged Record, the specs say we can NOT use the file size or start
		if(LIF->DIR.FileType == 0)
		{
 			if(purged == 0)
			{
				// Start of possible free space is after last non-type 0 record - if we had a last record
				LIF->space.start = start;
				// INDEX of this record
				LIF->space.index = index;
				purged = 1;
			}
			++index;
			continue;
		}

		// Valid Record

		// We had a previous purged record
		// Is there enough free space between valid records to save a new file of sectors in size ?
		if(purged)
		{
			// Compute the gap between the last record, or start of free space, and this one
			LIF->space.size = LIF->DIR.FileStartSector - LIF->space.start;
			if(LIF->space.size >= sectors)
			{
				LIF->index = LIF->space.index;
				return(LIF->index);
			}
			purged = 0;
		}
		// Find the start of the NEXT record
		start = LIF->DIR.FileStartSector + LIF->DIR.FileSectors;
		++index;
	}

	return(-1);
}


/** @brief  HP85 E010 ASCII LIF records have a 3 byte header 
	ef [ff]* = end of data in this sector (no size) , pad with ff's optionally if there is room
	df size = string
	cf size = split accross sector end "6f size" at start of next sector 
		   but the 6f size bytes are not included in cf size! (yuck!)
	6f size = split continue (always starts at sector boundry)
	df 00 00 ef [ff]* = EOF (df size = 0) ef send of sector and optional padding
	size = 16 bits LSB MSB

	Example:
	000080e0 : 4b 7c 22 0d cf 29 00 31 34 20 44 49 53 50 20 22  : K|"..).14 DISP "
	000080f0 : 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 5f  :                _
	00008100 : 6f 10 00 5f 5f 5f 5f 5f 5f 5f 5f 5f 5f 5f 5f 5f  : o.._____________
	00008110 : 5f 22 0d df 2b 00 31 35 20 44 49 53 50 20 22 20  : _"..+.15 DISP "

	cf 29 00 (19 is to sector end) (new sector start with 6F 10 00 (10 is remainder)
	So 29 = 19 and 10 (yuck!)
*/


///@brief Convert an ASCII string and write in E)10 format to LIF image
/// When LIF is null we do not write, just compute the write size
/// @param[in] LIF: LIF image file sructure
/// @param[in] offset: offset to write to
/// @param[in] str: ASCII string to write
/// @return bytes written,  -1 on error
int lif_ascii_string_to_e010(lif_t *LIF, long offset, void *vptr)
{
	int ind;
	int len;
	int bytes;
	int pos,rem;
	
	char *str = vptr;

	uint8_t buf[LIF_SECTOR_SIZE*2+1];

	bytes = 0;

	// String size
	len = strlen(str);

	if(len >LIF_SECTOR_SIZE)
	{
		if(debuglevel & 1)
			printf("lif_ascii_string_to_e010: string too long:[%d]\n", len);
		str[LIF_SECTOR_SIZE] = 0;
		len = LIF_SECTOR_SIZE;
	}

	// Compute the current offset in this sector
	pos = (offset % LIF_SECTOR_SIZE);

	// Number of bytes free in this sector
	rem = LIF_SECTOR_SIZE - pos;

	// buffer index
	ind = 0;

	// Does the string + header fit ?
	if(rem < (3 + len))
	{
		// If we split a string we end up writting two headers
		//  So for less then 6 bytes is no point splitting those strings
		//  We just pad and write the string in the next sector
		//  
		if(rem < 6)
		{
			// PAD
			buf[ind++] = 0xEF;
			while(ind<rem)
				buf[ind++] = 0xff;

			// NEW SECTOR
			// Debugging make sure we are at sector boundry
			if(((offset + ind)  % LIF_SECTOR_SIZE))
			{
				if(debuglevel & 1)
					printf("Expected sector boundry, offset:%d\n", (int) ((offset + ind) % LIF_SECTOR_SIZE) );
				return(-1);
			}
			// Write string in new sector
			// The full string + header will fit
			buf[ind++] = 0xDF;
			buf[ind++] = len & 0xff;
			buf[ind++] = (len >> 8) & 0xff;
			// Write string
			while(*str)
				buf[ind++] = *str++;
		}
		else
		{
			// Split string
			// String spans sector , so split the string
			// Split string Header
			buf[ind++] = 0xCF;
			buf[ind++] = len & 0xff;
			buf[ind++] = (len >>8) & 0xff;
			// Write as much of the string as we can in this sector
			while(*str && ind<rem)
				buf[ind++] = *str++;

			// NEW SECTOR
			// Debugging make sure we are at sector boundry
			if(((offset + ind)  % LIF_SECTOR_SIZE))
			{
				if(debuglevel & 1)
					printf("Expected sector boundry, offset:%d\n", (int) ((offset + ind) % LIF_SECTOR_SIZE) );
				return(-1);
			}

			// Update remining string length
			len = strlen(str);
			// Write split string continuation heaader at start of new sector
			buf[ind++] = 0x6F;
			buf[ind++] = (len & 0xff);
			buf[ind++] = (len>>8) & 0xff;
			// Write string
			while(*str)
				buf[ind++] = *str++;
		}
	}
	else 
	{
		// The full string + header will fit
		buf[ind++] = 0xdf;
		buf[ind++] = len & 0xff;
		buf[ind++] = (len >> 8) & 0xff;
		while(*str)
			buf[ind++] = *str++;

	}
	// Now Write string
	if(LIF)
	{

		len = lif_write(LIF->name, buf, offset, ind);
		if(len < ind)
			return(-1);
	}

	offset += ind;
	bytes += ind;
	return( bytes );
}
	

/// @brief Wrapper/helper to convert and add ASCII file to the LIF image as type E010
/// We must know the convered file size BEFORE writting
/// So we must call this function TWICE
///   1) find out how big the converted file is (LIF is NULL)
///   2) Specify where to put it (LIF is set)
///
/// @param[in] userfile: User ASCII file source
/// @param[in] *LIF: Where to write file if set (not NULL)
/// @return size of LIF image in bytes, or -1 on error
long lif_add_ascii_file_as_e010_wrapper(char *name, uint32_t offset, lif_t *LIF)
{
	long bytes;
	long rem;
	int count;
	int ind;
	int i;
	int len;
	FILE *fi;

	uint8_t str[LIF_SECTOR_SIZE+1];

	fi = lif_open(name, "r");
	if(fi == NULL)
		return(-1);


	bytes = 0;

	count = 0;
	// Read user file and write LIF records
	// reserve 3 + LIF header bytes + 1 (EOS)
	while( fgets((char *)str,(int)sizeof(str) - 4, fi) != NULL )
	{
		trim_tail((char *)str);

		strcat((char *)str,"\r"); // HP85 lines end with "\r"

		// Write string
		ind = lif_ascii_string_to_e010(LIF, offset, str);
		if(ind < 0)
		{
			fclose(fi);
			return(-1);
		}

		offset += ind;
		bytes += ind;
		count += ind;

		if(count > 256)
		{		
			count = 0;
			if(LIF)
				printf("Wrote: %8ld\r", (long)bytes);
		}
	}

	fclose(fi);

	str[0] = 0;
	// Write EOF string
	ind = lif_ascii_string_to_e010(LIF, offset, str);
	if(ind < 0)
		return(-1);

	offset += ind;
	bytes += ind;

	if(LIF)
		printf("Wrote: %8ld\r", (long)bytes);

	// PAD the end of this last sector IF any bytes have been written to it
	// Note: we do not add the pad to the file size!

	rem = LIF_SECTOR_SIZE - (offset % LIF_SECTOR_SIZE);

	if(LIF && rem < LIF_SECTOR_SIZE)
	{
		str[0]  = 0xef;
		for(i=1;i<rem;++i)
			str[i]  = 0xff;

		len = lif_write(LIF->name, str, offset, rem);
		if(len < rem)
			return(-1);

		bytes += len;
	}

	if(LIF)
		printf("Wrote: %8ld\r",(long)bytes);

	return(bytes);
}

/// @brief Convert and add ASCII file to the LIF image as type E010
/// The basename of the lifname, without extensions, is used as the LIF file name
/// @param[in] lifimagename: LIF image name
/// @param[in] lifname: LIF file name
/// @param[in] userfile: userfile name
/// @return size of data written into to LIF image, or -1 on error
long lif_add_ascii_file_as_e010(char *lifimagename, char *lifname, char *userfile)
{
	long bytes;
	long sectors;
	long offset;
	int index;
	lif_t *LIF;

	if(!*lifimagename)
	{
		printf("lif_add_ascii_file_as_e010: lifimagename is empty\n");
		return(-1);
	}
	if(!*lifname)
	{
		printf("lif_add_ascii_file_as_e010: lifname is empty\n");
		return(-1);
	}
	if(!*userfile)
	{
		printf("lif_add_ascii_file_as_e010: userfile is empty\n");
		return(-1);
	}

	if(debuglevel & 0x400)
		printf("LIF image:[%s], LIF name:[%s], user file:[%s]\n", 
			lifimagename, lifname, userfile);

	// Find out how big converted file will be
	bytes = lif_add_ascii_file_as_e010_wrapper(userfile, 0, NULL);
	if(bytes < 0)
		return(-1);

	LIF = lif_opendir(lifimagename);
	if(LIF == NULL)
		return(-1);	

	sectors = lif_bytes2sectors(bytes);

	// Now find free record
	index = lif_findfree_dirindex(LIF, sectors);
	if(index == -1)
	{
		printf("LIF image:[%s], not enough free space for:[%s]\n", 
			lifimagename, userfile);
			lif_closedir(LIF);
			return(-1);
	}

	// We write file data first - then create the directory record
	// This method is best if we have errors
	// Setup directory record
	lif_fixname(LIF->DIR.filename, lifname,10);
	LIF->DIR.FileType = 0xe010;  			// 10
	LIF->DIR.FileStartSector = LIF->space.start;	// 12
	LIF->DIR.FileSectors = sectors;    		// 16
	memset(LIF->DIR.date,0,6);				// 20
	LIF->DIR.VolNumber = 0x8001;			// 26
	LIF->DIR.FileBytes = 0;					// 28
	LIF->DIR.SectorSize  = 0x100;			// 30

	offset = LIF->space.start * LIF_SECTOR_SIZE;

	// Write converted file into free space first
	bytes = lif_add_ascii_file_as_e010_wrapper(userfile, offset, LIF);

	if(debuglevel & 0x400)
	{
		printf("New Directory Information AFTER write\n");
		printf("Name:              %s\n", LIF->DIR.filename);
		printf("Index:            %4d\n", (int)index);
		printf("First Sector:     %4lxH\n", LIF->DIR.FileStartSector);
		printf("File Sectors:     %4lxH\n", LIF->DIR.FileSectors);
	}

	// Write directory record
	if( !lif_writedirindex(LIF,index))
	{
		lif_closedir(LIF);
		return(-1);
	}

	// Write EOF is this is the last record
	if(LIF->space.eof && !lif_writedirEOF(LIF,index+1))
	{
		lif_closedir(LIF);
		return(-1);
	}
	lif_closedir(LIF);


	printf("Wrote: %8ld\n", bytes);

	// Return file size
	return(bytes);
}


/// @brief Extract a type E010 from LIF image and save as user ASCII file
/// @param[in] lifimagename: LIF disk image name
/// @param[in] lifname:  name of file in LIF image
/// @param[in] username: name to call the extracted image
/// @return 1 on sucess or 0 on error
int lif_extract_e010_as_ascii(char *lifimagename, char *lifname, char *username)
{
	lif_t *LIF;
	uint32_t start, end;	// sectors
	long offset, bytes;		// bytes
	int index;
	int i, len,size;
	int status = 1;
	int done = 0;

	int ind,wind;
	FILE *fp;

	// read buffer
	uint8_t buf[LIF_SECTOR_SIZE+4];
	// Write buffer, FYI: will ALWAYS be smaller then the read data buffer
	uint8_t wbuf[LIF_SECTOR_SIZE+4];


	LIF = lif_opendir(lifimagename);
	if(LIF == NULL)
	{
		printf("LIF image not found:%s\n", lifimagename);
		return(0);
	}

	index = lif_find_file(LIF, lifname);
	if(index == -1)
	{
		printf("File not found:%s\n", username);
		lif_closedir(LIF);
		return(0);
	}

	if((LIF->DIR.FileType & 0xFFFC) != 0xE010)
	{
		printf("File %s has wrong type:[%04XH] expected 0xE010..0xE013\n", username, (int) LIF->DIR.FileType);
		lif_closedir(LIF);
		return(0);
	}

	start = LIF->DIR.FileStartSector;
	end = start + LIF->DIR.FileSectors;

	offset = start * LIF_SECTOR_SIZE;

	fp = lif_open(username,"w");
	if(fp == NULL)
	{
		lif_closedir(LIF);
		return(0);
	}

	printf("Extracting: %s\n", username);

	bytes = 0;
	wind = 0;
	ind = 0;

	while(lif_bytes2sectors(offset) <= end)
	{
		// LIF images are always multiples of LIF_SECTOR_SIZE
		size = lif_read(lifimagename, buf, offset, LIF_SECTOR_SIZE);
		if(size < LIF_SECTOR_SIZE)
		{
			status = 0;
			break;
		}

		ind = 0;
		while(ind < LIF_SECTOR_SIZE && !done)
		{
			if(buf[ind] == 0xDF || buf[ind] == 0xCF || buf[ind] == 0x6F)
			{
				++ind;
				len = buf[ind++] & 0xff;
				len |= ((buf[ind++] & 0xff) <<8); 
				// EOF ?
				if(len == 0)
				{
					done = 1;
					break;
				}
				if(len >= LIF_SECTOR_SIZE)
				{
					printf("lif_extract_e010_as_ascii: string too big size = %d\n", (int)len);
					status = 0;
					done = 1;
					break;
				}
			}
			else if(buf[ind] == 0xEF)
			{
				// skip remaining bytes in sector
				ind = 0;
				break;
			}
			else
			{
				printf("lif_extract_e010_as_ascii: unexpected control byte:[%02XH] @ offset: %8lx, ind:%02XH\n", (int)buf[ind], offset, (int)ind);
				status = 0;
				done = 1;
				break;
			}
			// write string
			for(i=0;i <len && ind < LIF_SECTOR_SIZE;++i)
			{
				if(buf[ind] == '\r' && i == len-1)
				{
					wbuf[wind++] = '\n';
					++ind;
					break;
				}
				else 
				{
					wbuf[wind++] = buf[ind++];
				}

				if(wind >= LIF_SECTOR_SIZE)
				{
					size = fwrite(wbuf,1,wind,fp);
					if(size < wind)
					{
						printf("lif_extract_e010_as_ascii: write error\n");
						status = 0;
						done = 1;
						break;
					}
					bytes += size;
					printf("Wrote: %8ld\r", bytes);
					wind = 0;
				}

			}   // for(i=0;i <len && ind < LIF_SECTOR_SIZE;++i)

		}  	// while(ind < LIF_SECTOR_SIZE && status)

		offset += LIF_SECTOR_SIZE;

	} 	// while(offset <= end)

	lif_closedir(LIF);
	// Flush any remaining bytes
	if(wind)
	{
		size = fwrite(wbuf,1,wind,fp);
		if(size < wind)
		{
			printf("lif_extract_e010_as_ascii: write error\n");
			status = 0;
		}
		bytes += size;
	}
	fclose(fp);
	sync();
	printf("Wrote: %8ld\n", bytes);
	return(status);
}

	
/// @brief Extract a file as a single file LIF image
/// @param[in] lifimagename: LIF disk image name
/// @param[in] lifname:  name of file in LIF image
/// @param[in] username: name to call the extracted LIF image
/// @return 1 on sucess or 0 on error
int lif_extract_lif_file(char *lifimagename, char *lifname, char *username)
{
	// Master image lif_t structure
	lif_t *LIF;
	lif_t *ULIF;

	long offset, uoffset, bytes;	
	int index;
	int i, size;

	uint8_t buf[LIF_SECTOR_SIZE+4];

	LIF = lif_opendir(lifimagename);
	if(LIF == NULL)
	{
		printf("LIF image not found:%s\n", lifimagename);
		return(0);
	}

	index = lif_find_file(LIF, lifname);
	if(index == -1)
	{
		printf("File not found:%s\n", lifname);
		lif_closedir(LIF);
		return(0);
	}

	//Initialize the user file lif_t structure
	ULIF = lif_create_volume(username, "HFSLIF",1,1);
	if(ULIF == NULL)
	{
		lif_closedir(LIF);
		return(0);
	}

	// Only the start sector changes

	// Copy directory record
	ULIF->DIR = LIF->DIR;
	ULIF->DIR.FileStartSector = 2;
	ULIF->filesectors = LIF->DIR.FileSectors;

	if( !lif_writedirindex(ULIF,0))
	{
		lif_closedir(LIF);
		lif_closedir(ULIF);
		return(0);
	}
	if( !lif_writedirEOF(ULIF,1) )
	{
		lif_closedir(LIF);
		lif_closedir(ULIF);
		return(0);
	}

	uoffset =  ULIF->filestart * LIF_SECTOR_SIZE;

	offset = LIF->DIR.FileStartSector * LIF_SECTOR_SIZE;

	bytes = uoffset;

	for(i=0;i<(int)LIF->DIR.FileSectors;++i)
	{
		size = lif_read(LIF->name, buf, offset,LIF_SECTOR_SIZE);
		if(size < LIF_SECTOR_SIZE)
		{
			lif_closedir(LIF);
			lif_closedir(ULIF);
			return(0);
		}

		lif_write(ULIF->name,buf,uoffset,LIF_SECTOR_SIZE);
		if(size < LIF_SECTOR_SIZE)
		{
			lif_closedir(LIF);
			lif_closedir(ULIF);
			return(0);
		}
		bytes += size;
		offset += size;
		uoffset += size;
		printf("Wrote: %8ld\r", bytes);
	}
	lif_closedir(LIF);
	lif_closedir(ULIF);
	printf("Wrote: %8ld\n", bytes);
	return(1);
}
	
/// @brief Add a LIF binary image to the LIF image
/// The basename of the lifname, without extensions, is used as the LIF file name
/// @param[in] lifimagename: LIF image name
/// @param[in] lifname: LIF file name
/// @param[in] userfile: userfile name
/// @return size of data written into to LIF image, or -1 on error
long lif_add_lif_file(char *lifimagename, char *lifname, char *userfile)
{
	// Master image lif_t structure
	lif_t *LIF;
	lif_t *ULIF;
	int index = 0;
	long offset, uoffset, bytes;
	int i, size;

	uint8_t buf[LIF_SECTOR_SIZE+4];

	if(!*lifimagename)
	{
		printf("lif_add: lifimagename is empty\n");
		return(-1);
	}
	if(!*lifname)
	{
		printf("lif_add: lifname is empty\n");
		return(-1);
	}
	if(!*userfile)
	{
		printf("lif_add: userfile is empty\n");
		return(-1);
	}

	if(debuglevel & 0x400)
		printf("LIF image:[%s], LIF name:[%s], user file:[%s]\n", 
			lifimagename, lifname, userfile);

	// open  userfile as LIF image
	ULIF = lif_opendir(userfile);
	if(ULIF == NULL)
		return(-1);	

	// find lif file in user image
	index = lif_find_file(ULIF, lifname);
	if(index == -1)
	{
		printf("File not found:%s\n", lifname);
		lif_closedir(ULIF);
		return(0);
	}

	LIF = lif_opendir(lifimagename);
	if(LIF == NULL)
		return(-1);	

	// Now find a new free record
	index = lif_findfree_dirindex(LIF, ULIF->DIR.FileSectors);
	if(index == -1)
	{
		printf("LIF image:[%s], not enough free space for:[%s]\n", 
		lifimagename, userfile);
		lif_closedir(LIF);
		lif_closedir(ULIF);
		return(-1);
	}

	// Copy user image directory record to master image directory record
	LIF->DIR = ULIF->DIR;

	// Adjust starting sector to point here
	LIF->DIR.FileStartSector = LIF->space.start;

	// Master lif image file start in bytes
	offset  = LIF->DIR.FileStartSector * LIF_SECTOR_SIZE;
	// User lif image file start in bytes
	uoffset = ULIF->DIR.FileStartSector * LIF_SECTOR_SIZE;
	bytes = 0;
	for(i=0;i<(int)LIF->DIR.FileSectors;++i)
	{
		// Read
		size = lif_read(ULIF->name, buf, uoffset, LIF_SECTOR_SIZE);
		if(size < LIF_SECTOR_SIZE)
		{
			lif_closedir(LIF);
			lif_closedir(ULIF);
			return(-1);
		}

		// Write
		size = lif_write(LIF->name, buf, offset, LIF_SECTOR_SIZE);
		if(size < LIF_SECTOR_SIZE)
		{
			lif_closedir(LIF);
			lif_closedir(ULIF);
			return(-1);
		}
		offset += LIF_SECTOR_SIZE;
		uoffset += LIF_SECTOR_SIZE;
		bytes += LIF_SECTOR_SIZE;
		printf("Wrote: %8ld\r", bytes);
	}
	lif_closedir(ULIF);

	// Write directory record
	if( !lif_writedirindex(LIF,index))
	{
		lif_closedir(LIF);
		return(-1);
	}
	if(LIF->space.eof && !lif_writedirEOF(LIF,index+1))
	{
		lif_closedir(LIF);
		return(-1);
	}
	lif_closedir(LIF);
	printf("Wrote: %8ld  \n", bytes);
	return(bytes);
}





/// @brief Delete LIF file in LIF image
/// @param[in] lifimagename: LIF image name
/// @param[in] lifname: LIF file name
/// @return 1 if deleted, 0 if not found, -1 error
int lif_del_file(char *lifimagename, char *lifname)
{
	lif_t *LIF;
	int index;

	if(!*lifimagename)
	{
		printf("lif_del_file: lifimagename is empty\n");
		return(-1);
	}
	if(!*lifname)
	{
		printf("lif_del_file: lifname is empty\n");
		return(-1);
	}
	if(debuglevel & 0x400)
		printf("LIF image:[%s], LIF name:[%s]\n", 
			lifimagename, lifname);


	LIF = lif_opendir(lifimagename);
	if(LIF == NULL)
		return(-1);	

	// Now find file record
	index = lif_find_file(LIF, lifname);
	if(index == -1)
	{
		lif_closedir(LIF);
		printf("LIF image:[%s] lif name:[%s] not found\n", lifimagename, lifname);
		return(0);
	}
	LIF->DIR.FileType = 0;

	// re-Write directory record
	if( !lif_writedirindex(LIF,index) )
	{
		lif_closedir(LIF);
		return(-1);
	}
	lif_closedir(LIF);
	printf("Deleted: %10s\n", lifname);

	return(1);
}

/// @brief Rename LIF file in LIF image
/// @param[in] lifimagename: LIF image name
/// @param[in] oldlifname: old LIF file name
/// @param[in] newlifname: new LIF file name
/// @return 1 if renamed, 0 if not found, -1 error
int lif_rename_file(char *lifimagename, char *oldlifname, char *newlifname)
{
	int index;
	lif_t *LIF;

	if(!*lifimagename)
	{
		printf("lif_rename_file: lifimagename is empty\n");
		return(-1);
	}
	if(!*oldlifname)
	{
		printf("lif_rename_file: old lifname is empty\n");
		return(-1);
	}
	if(!*newlifname)
	{
		printf("lif_rename_file: new lifname is empty\n");
		return(-1);
	}

	if(!lif_checkname(newlifname))
	{
		printf("lif_rename_file: new lifname contains bad characters\n");
		return(-1);

	}

	LIF = lif_opendir(lifimagename);
	if(LIF == NULL)
		return(-1);	

	// Now find file record
	index = lif_find_file(LIF, oldlifname);
	if(index == -1)
	{
		printf("LIF image:[%s] lif name:[%s] not found\n", lifimagename, oldlifname);
		lif_closedir(LIF);
		return(0);
	}
    lif_fixname(LIF->DIR.filename, newlifname, 10);

	// re-Write directory record
	if( !lif_writedirindex(LIF,index))
	{
		lif_closedir(LIF);
		return(-1);
	}
	lif_closedir(LIF);
	printf("renamed: %10s to %10s\n", oldlifname,newlifname);

	return(1);
}





/// @brief Create/Format a LIF disk image
/// This can take a while to run, about 1 min for 10,000,000 bytes
/// @param[in] lifimagename: LIF disk image name
/// @param[in] liflabel: LIF Volume Label name
/// @param[in] dirsecs: Number of LIF directory sectors
/// @param[in] sectors: total disk image size in sectors
///@return bytes writting to disk image
long lif_create_image(char *lifimagename, char *liflabel, uint32_t dirsecs, uint32_t sectors)
{
	long offset, count, size;
	int i;
	lif_t *LIF;
	uint8_t buffer[LIF_SECTOR_SIZE];
	

	if(!*lifimagename)
	{
		printf("lif_create_image: lifimagename is empty\n");
		return(-1);
	}
	if(!*liflabel)
	{
		printf("lif_create_image: liflabel is empty\n");
		return(-1);
	}
	if(!dirsecs)
	{
		printf("lif_create_image: dirsecs is 0\n");
		return(-1);
	}
	if(!sectors)
	{
		printf("lif_create_image: sectors is 0\n");
		return(-1);
	}

	LIF = lif_create_volume(lifimagename, liflabel, 2, dirsecs);
	if(LIF == NULL)
		return(-1);

	printf("Formating LIF image:[%s], Label:[%s], Dir Sectors:[%ld], sectors:[%ld]\n", 
		lifimagename, LIF->VOL.Label, (long)LIF->VOL.DirSectors, (long)sectors);

	///@brief Zero out remining disk image
	memset(buffer,0xff,LIF_SECTOR_SIZE);

	offset = LIF->filestart * LIF_SECTOR_SIZE;

	count = sectors - LIF->filestart;
	
	for(i=0;i<count;++i)
	{
		size = lif_write(LIF->name, buffer, offset, LIF_SECTOR_SIZE);
		if( size < LIF_SECTOR_SIZE)
		{
			printf("lif_create_image: Write error %s @ %ld\n", lifimagename, (long)i + LIF->filestart);
			lif_close_volume(LIF);
			return( -1 );
		}
		printf("sector: %ld\r", i + LIF->filestart);
	}
	lif_close_volume(LIF);
	printf("Formating: wrote:[%ld] sectors\n", sectors);
    return(sectors);
}
