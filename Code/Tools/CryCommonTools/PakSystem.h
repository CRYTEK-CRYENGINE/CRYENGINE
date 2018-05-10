// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PAKSYSTEM_H__
#define __PAKSYSTEM_H__

#include "IPakSystem.h"
#include "ZipDir/ZipDir.h" // TODO: get rid of thid include

enum PakSystemFileType
{
	PakSystemFileType_Unknown,
	PakSystemFileType_File,
	PakSystemFileType_PakFile
};
struct PakSystemFile
{
	PakSystemFile();
	PakSystemFileType type;

	// PakSystemFileType_File
	FILE* file;

	// PakSystemFileType_PakFile
	ZipDir::CachePtr zip;
	ZipDir::FileEntry* fileEntry;
	void* data;
	int dataPosition;
};

struct PakSystemArchive
{
	ZipDir::CacheRWPtr zip;
};

class PakSystem : public IPakSystem
{
public:
	PakSystem();

	// IPakSystem
	virtual PakSystemFile* Open(const char* filename, const char* mode);
	virtual bool ExtractNoOverwrite(const char* filename, const char* extractToFile = 0);
	virtual void Close(PakSystemFile* file);
	virtual int GetLength(PakSystemFile* file) const;
	virtual int Read(PakSystemFile* file, void* buffer, int size);
	virtual bool EoF(PakSystemFile* file);

	virtual PakSystemArchive* OpenArchive(const char* path, size_t fileAlignment, bool encrypted, const uint32 encryptionKey[4]);
	virtual void CloseArchive(PakSystemArchive* archive);
	virtual void AddToArchive(PakSystemArchive* archive, const char* path, void* data, int size, __time64_t modTime, int compressionLevel);
	virtual bool DeleteFromArchive(PakSystemArchive* archive, const char* path);
	virtual bool CheckIfFileExist(PakSystemArchive* archive, const char* path, __time64_t modTime );
};

#endif //__PAKSYSTEM_H__
