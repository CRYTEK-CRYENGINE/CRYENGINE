// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IPAKSYSTEM_H__
#define __IPAKSYSTEM_H__

struct PakSystemFile;
struct PakSystemArchive;
struct IPakSystem
{
	virtual PakSystemFile* Open(const char* filename, const char* mode) = 0;
	virtual bool ExtractNoOverwrite(const char* filename, const char* extractToFile = 0) = 0;
	virtual void Close(PakSystemFile* file) = 0;
	virtual int GetLength(PakSystemFile* file) const = 0;
	virtual int Read(PakSystemFile* file, void* buffer, int size) = 0;
	virtual bool EoF(PakSystemFile* file) = 0;

	virtual PakSystemArchive* OpenArchive(const char* path, size_t fileAlignment = 1, bool encrypted = false, const uint32 encryptionKey[4] = 0) = 0;
	virtual void CloseArchive(PakSystemArchive* archive) = 0;
	
	// Summary:
	//   Adds a new file to the pak or update an existing one.
	//   Adds a directory (creates several nested directories if needed)
	// Arguments:
	//   path - relative path inside archive
	//   data, size - file content
	//   modTime - modification timestamp of the file
	//   compressionLevel - level of compression (correnponds to zlib-levels):
	//                      -1 or [0-9] where -1=default compression, 0=no compression, 9=best compression
	virtual void AddToArchive(PakSystemArchive* archive, const char* path, void* data, int size, __time64_t modTime, int compressionLevel = -1) = 0;

	virtual bool DeleteFromArchive(PakSystemArchive* archive, const char* path) = 0;
	virtual bool CheckIfFileExist(PakSystemArchive* archive, const char* path, __time64_t modTime ) = 0;
};

#endif //__IPAKSYSTEM_H__
