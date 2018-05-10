// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "PakSystem.h"
#include "PathHelpers.h"
#include "StringHelpers.h"

#include <CryString/CryPath.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>  // WIN32_FIND_DATA

#include "ZipDir/ZipDir.h"
#include <zlib.h>

PakSystemFile::PakSystemFile()
{
	type = PakSystemFileType_Unknown;
	file = NULL;
	zip = NULL;
	fileEntry = NULL;
	data = NULL;
	dataPosition = 0;
}

PakSystem::PakSystem()
{
}

PakSystemFile* PakSystem::Open(const char* a_path, const char* a_mode)
{
	string normalPath = a_path;

	string const zipExt = ".zip";
	bool bZip = StringHelpers::EndsWithIgnoreCase(normalPath, zipExt);

	if (bZip)
	{
		// If it's a .zip file, then we'll try to look for a file without .zip extension inside of the .zip file
		normalPath.erase(normalPath.length() - zipExt.length(), zipExt.length());
	}

	string zipPath = normalPath + zipExt;
	string filename = PathUtil::GetFile(normalPath);

	if (!normalPath.empty() && normalPath[0] == '@')
	{
		// File is inside pak file.
		int splitter = normalPath.find_first_of( "|;," );
		if (splitter >= 0)
		{
			zipPath = normalPath.substr(1,splitter-1);
			filename = StringHelpers::MakeLowerCase(normalPath.substr(splitter+1));
			bZip = true;
		}
		else
		{
			return 0;
		}
	}

	if (!bZip)
	{
		// Try to open the file.
		FILE* const f = fopen(normalPath.c_str(), a_mode);
		if (f)
		{
			std::unique_ptr<PakSystemFile> file(new PakSystemFile());
			file->type = PakSystemFileType_File;
			file->file = f;

			return file.release();
		}
	}

	// if it's simple and read-only, it's assumed it's read-only
	unsigned const nFactoryFlags = ZipDir::CacheFactory::FLAGS_DONT_COMPACT | ZipDir::CacheFactory::FLAGS_READ_ONLY;

	bool bFileExists = false;
	const uint32* decryptionKey = 0; // use default one

	if (bZip)
	{
		// a caller asked to open a .zip file. check if the .zip file on disk exist
		FILE* const f = fopen(zipPath.c_str(), "rb");
		if (f)
		{
			fclose(f);
			bFileExists = true;
		}
	}
	else
	{
		// a caller specified normal file. we already failed to find it on disk,
		// so the file could be within a .pak file. let's find all 'potential'
		// pak files and look within these for a matching file

		std::vector<string> foundFileCountainer; // pak files found

		for (string dirToSearch = normalPath;;)
		{
			dirToSearch = PathHelpers::GetDirectory(dirToSearch);

			const string search = PathUtil::Make(dirToSearch, string("*.pak"));

			WIN32_FIND_DATAA FindFileData;

			HANDLE hFind = FindFirstFileA(search.c_str(), &FindFileData);
			if (hFind != INVALID_HANDLE_VALUE)
			{
				do 
				{
					const string foundFilename(FindFileData.cFileName);
					if (StringHelpers::EqualsIgnoreCase(PathUtil::GetExt(foundFilename.c_str()), string("pak")))
					{
						foundFileCountainer.push_back(PathUtil::Make(dirToSearch, foundFilename));
					}
				} while (FindNextFileA(hFind, &FindFileData) != 0);

				FindClose(hFind);
			}

			if (PathUtil::GetFile(dirToSearch).empty())
			{
				// We've reached the top of the path
				break;
			}
		}

		// iterate through found containers and look for relevant files within them
		for (int iFile = 0; iFile < foundFileCountainer.size(); ++iFile)
		{
			zipPath = foundFileCountainer[ iFile ];
			string pathToZip = PathHelpers::GetDirectory( zipPath );

			// construct filename by removing path to zip from path to filename
			string pathToFile = PathHelpers::GetDirectory( string( normalPath ) );
			string pureFileName = PathUtil::GetFile( string( normalPath ) );
			if (pathToFile.length() != pathToZip.length() && pathToZip.length() > 0)
			{
				pathToFile = pathToFile.substr( pathToZip.length() + 1 );
			}
			filename = pathToFile.empty() 
				? pureFileName 
				: pathToFile + "\\" + pureFileName;

			ZipDir::CacheFactory factory(ZipDir::ZD_INIT_FAST, nFactoryFlags);
			ZipDir::CachePtr testZip = factory.New( zipPath.c_str(), decryptionKey );
			ZipDir::FileEntry* testFileEntry = (testZip ? testZip->FindFile(filename.c_str()) : 0);

			// break out if we have a testFileEntry, as we've found our first (and best) candidate.
			if (testFileEntry)
			{
				bFileExists = true;
				break;
			}
		}
	}

	{
		ZipDir::CacheFactory factory(ZipDir::ZD_INIT_FAST, nFactoryFlags);
		ZipDir::CachePtr zip = (bFileExists ? factory.New(zipPath.c_str(), decryptionKey) : 0);
		ZipDir::FileEntry* fileEntry = (zip ? zip->FindFile(filename.c_str()) : 0);

		if (fileEntry)
		{
			std::unique_ptr<PakSystemFile> file(new PakSystemFile());
			file->type = PakSystemFileType_PakFile;
			file->zip = zip;
			file->fileEntry = fileEntry;
			file->data = zip->AllocAndReadFile(file->fileEntry);
			file->dataPosition = 0;
			return file.release();
		}
	}

	return 0;
}


//Extracts archived file to disk without overwriting any files
//returns true on success, false on failure (due to potential overwrite or no file
//in archive
bool PakSystem::ExtractNoOverwrite(const char* fileToExtract, const char* extractToFile)
{
	if( 0 == extractToFile )
	{
		extractToFile = fileToExtract;
	}

	//open file using pak system
	PakSystemFile* fileZip = Open(fileToExtract, "r");
	if( !fileZip  )
	{
		return false;
	}

	// Try to open a writable file
	FILE* fFileOnDisk = fopen(extractToFile, "wb");
	if(!fFileOnDisk)
	{
		Close( fileZip );
		return false;
	}

	fwrite( fileZip->data, fileZip->fileEntry->desc.lSizeUncompressed, 1, fFileOnDisk );
	fclose( fFileOnDisk );

	Close(fileZip);

	return true;
}


void PakSystem::Close(PakSystemFile* file)
{
	if (file)
	{
		switch (file->type)
		{
		case PakSystemFileType_File:
			fclose(file->file);
			break;

		case PakSystemFileType_PakFile:
			file->zip->Free(file->data);
			break;
		}
		delete file;
	}
}


int PakSystem::GetLength(PakSystemFile* file) const
{
	if (file)
	{
		switch (file->type)
		{
			case PakSystemFileType_File:
			{
				if (file->file)
				{
					long pos = ftell(file->file);
					fseek(file->file, 0, SEEK_END);
					int result = ftell(file->file);
					fseek(file->file, pos, SEEK_SET);
					return result;
				}
				break;
			}
			case PakSystemFileType_PakFile:
			{
				if (file->fileEntry)
				{
					return file->fileEntry->desc.lSizeUncompressed;
				}
				break;
			}
			default:
			{
				break;
			}
		}
	}
	return 0;
}


int PakSystem::Read(PakSystemFile* file, void* buffer, int size)
{
	int readBytes = 0;

	if (file)
	{
		switch (file->type)
		{
		case PakSystemFileType_File:
			{
				readBytes = fread(buffer, 1, size, file->file);
			}
			break;

		case PakSystemFileType_PakFile:
			{
				int fileSize = file->fileEntry->desc.lSizeUncompressed;
				readBytes = (fileSize - file->dataPosition > size ? size : fileSize - file->dataPosition);
				memcpy(buffer, static_cast<char*>(file->data) + file->dataPosition, readBytes);
				file->dataPosition += readBytes;
			}
			break;
		}
	}

	return readBytes;
}

bool PakSystem::EoF(PakSystemFile* file)
{
	bool EoF = true;
	if (file)
	{
		switch (file->type)
		{
		case PakSystemFileType_File:
			{
				EoF = (0 != feof(file->file));
			}
			break;

		case PakSystemFileType_PakFile:
			{
				int fileSize = file->fileEntry->desc.lSizeUncompressed;
				EoF = (file->dataPosition >= fileSize);
			}
			break;
		}
	}

	return EoF;
}

PakSystemArchive* PakSystem::OpenArchive(const char* path, size_t fileAlignment, bool encrypted, const uint32 encryptionKey[4])
{
	//unsigned nFactoryFlags = ZipDir::CacheFactory::FLAGS_DONT_COMPACT | ZipDir::CacheFactory::FLAGS_CREATE_NEW;
	unsigned nFactoryFlags = 0;
	ZipDir::CacheFactory factory(ZipDir::ZD_INIT_FAST, nFactoryFlags);
	ZipDir::CacheRWPtr cache = factory.NewRW(path, fileAlignment, encrypted, encryptionKey);
	PakSystemArchive* archive = (cache ? new PakSystemArchive() : 0);
	if (archive)
		archive->zip = cache;
	return archive;
}

void PakSystem::CloseArchive(PakSystemArchive* archive)
{
	if (archive)
	{
		archive->zip->Close();
		delete archive;
	}
}

void PakSystem::AddToArchive(PakSystemArchive* archive, const char* path, void* data, int size,__time64_t modTime, int compressionLevel)
{
	int compressionMethod = ZipFile::METHOD_DEFLATE;
	if (compressionLevel == 0)
		compressionMethod = ZipFile::METHOD_STORE;
	archive->zip->UpdateFile(path, data, size, compressionMethod, compressionLevel, modTime);
}

//////////////////////////////////////////////////////////////////////////
bool PakSystem::CheckIfFileExist( PakSystemArchive* archive, const char* path, __time64_t modTime )
{
	assert(archive);

	ZipDir::FileEntry* pFileEntry = archive->zip->FindFile(path);
	if (pFileEntry)
	{
		return pFileEntry->CompareFileTimeNTFS(modTime);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool PakSystem::DeleteFromArchive( PakSystemArchive* archive, const char* path )
{
	ZipDir::ErrorEnum err = archive->zip->RemoveFile(path);
	return ZipDir::ZD_ERROR_SUCCESS == err;
}
