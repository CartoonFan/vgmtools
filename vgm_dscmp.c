// vgm_dscmp.c - VGM Directory Size Compare

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>	// for isalnum()
#include <wchar.h>
#include <locale.h>	// for setlocale()
#include <zlib.h>

#ifdef WIN32
#include <windows.h>	// for Directory Listing
#else
#include <glob.h>
#include <sys/stat.h>
#endif

#include "stdtype.h"
#include "stdbool.h"
#include "VGMFile.h"
#include "common.h"


#ifdef WIN32
#define DIR_SEP	'\\'
#else
#define DIR_SEP	'/'
#endif


static void EnsureDirPath(char* folderPath);
static UINT8 GetFixLenPrecision(double value, UINT8 totalLen);
static void sprintUIntSize(char* buffer, UINT32 fSize);
static void sprintUIntGrouped(char* buffer, UINT32 fSize);
static bool OpenVGMFile(const char* FileName);
static void ReadDirectory(const char* DirName);


typedef struct track_list
{
	char* title;
	UINT32 dataSize;
} TRACK_LIST;


UINT32 TrackCount;
UINT32 TrackAlloc;
TRACK_LIST* TrackList;
UINT32 AllTrackSize;

int main(int argc, char* argv[])
{
	int argbase;
	int ErrVal;
	char FolderName1[MAX_PATH];
	char FolderName2[MAX_PATH];
	UINT32 dirSize1;
	UINT32 dirSize2;

	setlocale(LC_CTYPE, "C");	// enforce decimal dot

	printf("VGM Directory Size Compare\n--------------------------\n\n");

	ErrVal = 0;
	argbase = 1;

	printf("Folder Path 1:\t");
	if (argc <= argbase + 0)
	{
		ReadFilename(FolderName1, sizeof(FolderName1));
	}
	else
	{
		strcpy(FolderName1, argv[argbase + 0]);
		printf("%s\n", FolderName1);
	}
	if (! strlen(FolderName1))
		return 0;

	printf("Folder Path 2:\t");
	if (argc <= argbase + 1)
	{
		ReadFilename(FolderName2, sizeof(FolderName2));
	}
	else
	{
		strcpy(FolderName1, argv[argbase + 1]);
		printf("%s\n", FolderName2);
	}
	if (! strlen(FolderName2))
		return 0;

	EnsureDirPath(FolderName1);
	EnsureDirPath(FolderName2);

	TrackAlloc = 0;
	TrackList = NULL;

	TrackCount = 0;
	AllTrackSize = 0;
	ReadDirectory(FolderName1);
	{
		char bufferS[0x10];
		char bufferG[0x10];
		sprintUIntSize(bufferS, AllTrackSize);
		sprintUIntGrouped(bufferG, AllTrackSize);
		printf("Folder 1 Total: %u files, %s (%s bytes)\n", TrackCount, bufferS, bufferG);
	}
	dirSize1 = AllTrackSize;
	printf("\n");
	
	TrackCount = 0;
	AllTrackSize = 0;
	ReadDirectory(FolderName2);
	{
		char bufferS[0x10];
		char bufferG[0x10];
		sprintUIntSize(bufferS, AllTrackSize);
		sprintUIntGrouped(bufferG, AllTrackSize);
		printf("Folder 2 Total: %u files, %s (%s bytes)\n", TrackCount, bufferS, bufferG);
	}
	dirSize2 = AllTrackSize;
	printf("\n");

	{
		char buffer1[0x10];
		char buffer2[0x10];
		double ratioPerc = (double)dirSize2 / (double)dirSize1 * 100.0;
		UINT8 rPrecision = GetFixLenPrecision(ratioPerc, 4);
		sprintUIntSize(buffer1, dirSize1);
		sprintUIntSize(buffer2, dirSize2);
		printf("%s -> %s (%.*f %%)\n", buffer1, buffer2, rPrecision, ratioPerc);
	}

//EndProgram:
	DblClickWait(argv[0]);

	return ErrVal;
}

static const char* GetLastDirSeparator(const char* filePath)
{
	const char* sepPos1;
	const char* sepPos2;
	
	if (strncmp(filePath, "\\\\", 2))
		filePath += 2;	// skip Windows network prefix
	sepPos1 = strrchr(filePath, '/');
	sepPos2 = strrchr(filePath, '\\');
	if (sepPos1 == NULL)
		return sepPos2;
	else if (sepPos2 == NULL)
		return sepPos1;
	else
		return (sepPos1 < sepPos2) ? sepPos2 : sepPos1;
}

static char* GetFilePathTitle(const char* filePath)
{
	const char* dirSepPos = GetLastDirSeparator(filePath);
	return (dirSepPos != NULL) ? (char*)&dirSepPos[1] : (char*)filePath;
}

static void EnsureDirPath(char* folderPath)
{
	if (folderPath[strlen(folderPath) - 1] == DIR_SEP)
		return;

#ifdef WIN32
	if (GetFileAttributes(folderPath) & FILE_ATTRIBUTE_DIRECTORY)
#else
	struct stat st;
	if (stat(folderPath, &st) || S_ISDIR(st.st_mode))
#endif
	{
		char dirSep[] = {DIR_SEP, '\0'};
		strcat(folderPath, dirSep);
	}
	else
	{
		char* fTitle = GetFilePathTitle(folderPath);
		*fTitle = '\0';	// strip file title, leaving only the folder path
		if (*folderPath == '\0')
			strcpy(folderPath, ".");	// folder empty - set to "current directory"
	}

	return;
}

static UINT8 GetFixLenPrecision(double value, UINT8 totalLen)
{
	UINT8 decIntDigs;
	float remVal;
	
	decIntDigs = 1;
	remVal = (float)value;
	while(remVal >= 10.0f)
	{
		remVal /= 10.0f;
		decIntDigs ++;
	}
	// decIntDigs == digits before the decimal dot
	decIntDigs ++;	// count character of decimal dot
	if (decIntDigs < totalLen)
		return totalLen - decIntDigs;
	else
		return 0;
}

static void sprintUIntGrouped(char* buffer, UINT32 fSize)
{
	UINT8 numLen;
	UINT8 groupCnt;
	UINT8 chrPos;
	UINT8 groupRem;
	UINT32 remVal;
	
	numLen = 0;
	remVal = fSize;
	do
	{
		numLen ++;
		remVal /= 10;
	} while(remVal > 0);
	groupCnt = (numLen - 1) / 3;
	
	remVal = fSize;
	chrPos = numLen + groupCnt;
	buffer[chrPos] = '\0';
	groupRem = 3;
	while(chrPos > 0)
	{
		if (groupRem == 0)
		{
			chrPos --;
			buffer[chrPos] = ' ';
			groupRem = 3;
		}
		groupRem --;
		
		chrPos --;
		buffer[chrPos] = '0' + (remVal % 10);
		remVal /= 10;
	}
	
	return;
}

static void sprintUIntSize(char* buffer, UINT32 fSize)
{
	// In most cases, buffer[8] is sufficient.
	char prefixChrs[] = {' ', 'K', 'M', 'G', 'T', 'P', 'E', '\0'};
	
	if (fSize < 1018)	// 1018 equals 0.995*1024
	{
		sprintf(buffer, "%u B", fSize);
	}
	else
	{
		double scaledSize = (double)fSize;
		UINT8 unitIdx;
		UINT8 fltPrec;
		for (unitIdx = 0; prefixChrs[unitIdx + 1] != '\0'; unitIdx ++)
		{
			if (scaledSize < 1018.0)
				break;
			scaledSize /= 1024.0;
		}
		fltPrec = GetFixLenPrecision(scaledSize, 4);
		sprintf(buffer, "%.*f %cB", fltPrec, scaledSize, prefixChrs[unitIdx]);
	}
	
	return;
}

static bool OpenVGMFile(const char* FileName)
{
	VGM_HEADER VGMHead;
	gzFile hFile;
	UINT32 fccHeader;
	UINT32 CurPos;
	UINT32 TempLng;
	TRACK_LIST* CurTrkEntry;
	char* TempPnt;

	hFile = gzopen(FileName, "rb");
	if (hFile == NULL)
		return false;

	gzseek(hFile, 0x00, SEEK_SET);
	gzread(hFile, &fccHeader, 0x04);
	if (fccHeader != FCC_VGM)
		goto OpenErr;

	if (gzseek(hFile, 0x00, SEEK_SET) == -1)
	{
		printf("gzseek Error!!\n");
		if (gzrewind(hFile) == -1)
		{
			printf("gzrewind Error!!\n");
			goto OpenErr;
		}
	}
	TempLng = gztell(hFile);
	if (TempLng != 0)
	{
		printf("gztell returns invalid offset: 0x%X\n", TempLng);
		goto OpenErr;
	}
	gzread(hFile, &VGMHead, sizeof(VGM_HEADER));
	ZLIB_SEEKBUG_CHECK(VGMHead);

	// relative -> absolute addresses
	VGMHead.lngEOFOffset += 0x00000004;
	if (VGMHead.lngGD3Offset)
		VGMHead.lngGD3Offset += 0x00000014;
	if (VGMHead.lngVersion < 0x00000150)
	{
		VGMHead.lngDataOffset = 0x00000040;
	}
	else
	{
		if (! VGMHead.lngDataOffset)
			VGMHead.lngDataOffset = 0x0000000C;
		VGMHead.lngDataOffset += 0x00000034;
	}

	CurPos = VGMHead.lngDataOffset;
	TempLng = sizeof(VGM_HEADER);
	if (TempLng > CurPos)
		TempLng -= CurPos;
	else
		TempLng = 0x00;
	memset((UINT8*)&VGMHead + CurPos, 0x00, TempLng);

	// Allocate Memory for Track List
	if (TrackAlloc <= TrackCount)
	{
		TrackAlloc += 0x100;
		TrackList = (TRACK_LIST*)realloc(TrackList, sizeof(TRACK_LIST) * TrackAlloc);
	}
	CurTrkEntry = &TrackList[TrackCount];
	TrackCount ++;

	// Read GD3 Tag
	if (VGMHead.lngGD3Offset)
	{
		gzseek(hFile, VGMHead.lngGD3Offset, SEEK_SET);
		gzread(hFile, &fccHeader, 0x04);
		if (fccHeader != FCC_GD3)
			VGMHead.lngGD3Offset = 0x00000000;
	}

	{
		TempPnt = GetFilePathTitle(FileName);
		TempLng = strlen(TempPnt);
		CurTrkEntry->title = (char*)malloc(TempLng + 1);
		strcpy(CurTrkEntry->title, TempPnt);
	}

	{
		UINT32 dataEnd = VGMHead.lngEOFOffset;
		if (VGMHead.lngGD3Offset && VGMHead.lngGD3Offset < dataEnd)
			dataEnd = VGMHead.lngGD3Offset;
		CurTrkEntry->dataSize = dataEnd - VGMHead.lngDataOffset;
	}
	AllTrackSize += CurTrkEntry->dataSize;

	gzclose(hFile);

	{
		char bufferS[0x10];
		char bufferG[0x10];
		sprintUIntSize(bufferS, CurTrkEntry->dataSize);
		sprintUIntGrouped(bufferG, CurTrkEntry->dataSize);
		printf("%s: %s (%s bytes)\n", GetFilePathTitle(FileName), bufferS, bufferG);
	}
	return true;

OpenErr:

	gzclose(hFile);
	return false;
}

static void ReadDirectory(const char* DirName)
{
#ifdef WIN32
	HANDLE hFindFile;
	WIN32_FIND_DATA FindFileData;
	BOOL RetVal;
#else
	struct stat st;
	int i;
#endif
	char FileName[MAX_PATH];
	char* TempPnt;
	char* FileExt;

	strcpy(FileName, DirName);
	strcat(FileName, "*.vg?");

#ifdef WIN32
	hFindFile = FindFirstFile(FileName, &FindFileData);
	if (hFindFile == INVALID_HANDLE_VALUE)
	{
		//ShowErrMessage();
		printf("Error reading directory!\n");
		return;
	}
#else
	glob_t globbuf;
	glob(FileName, 0, NULL, &globbuf);
#endif
	strcpy(FileName, DirName);
	TempPnt = FileName + strlen(FileName);

#ifdef WIN32
	do
	{
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			goto SkipFile;
		FileExt = strrchr(FindFileData.cFileName, '.');
		if (FileExt == NULL)
			goto SkipFile;
		FileExt ++;

		if (stricmp(FileExt, "vgm") && stricmp(FileExt, "vgz"))
			goto SkipFile;

		strcpy(TempPnt, FindFileData.cFileName);
		if (! OpenVGMFile(FileName))
			printf("%s\tError opening the file!\n", TempPnt);
SkipFile:
		RetVal = FindNextFile(hFindFile, &FindFileData);
	} while(RetVal);
#else
	for (i = 0; i < globbuf.gl_pathc; i++)
	{
		if (!stat(globbuf.gl_pathv[i], &st))
		{
			FileExt = strrchr(globbuf.gl_pathv[i], '.');
			if (FileExt == NULL)
				continue;
			FileExt ++;

			if (stricmp(FileExt, "vgm") && stricmp(FileExt, "vgz"))
				continue;

			strcpy(TempPnt, globbuf.gl_pathv[i]);
			if (! OpenVGMFile(globbuf.gl_pathv[i]))
				printf("%s\tError opening the file!\n", TempPnt);
		}
	}
#endif

#ifdef WIN32
	RetVal = FindClose(hFindFile);
#else
	globfree(&globbuf);
#endif

	return;
}
