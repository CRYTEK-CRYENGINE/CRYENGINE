// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <stdafx.h>
#include "PakManager.h"
#include <Shlwapi.h>  // PathRelativePathTo(), PathCanonicalize()
#pragma message("Note: including Shlwapi.lib")
#pragma comment(lib, "Shlwapi.lib")
#include "IRCLog.h"
#include "FileUtil.h"
#include "ResourceCompiler.h"
#include <CryCore/CryCrc32.h>
#include <CryString/CryPath.h>
#include "ZipEncryptor.h"

namespace
{

string CreateTempFileName(const string& filepath)
{
	const string ext = PathUtil::GetExt(filepath);

	return PathUtil::ReplaceExtension(filepath, string().Format("$%s", ext.c_str()));
}

}

//////////////////////////////////////////////////////////////////////////
PakManager::PakManager(IProgress* pProgress)
	: m_pProgress(pProgress)
	, m_maxThreads(1)
{
}

PakManager::~PakManager()
{
}

void PakManager::RegisterKeys(IResourceCompiler* pRC)
{
	pRC->RegisterKey("split_listfile_to_zips", "split a list file into multiple zip files");
	pRC->RegisterKey("zip", "Compress source files into the zip file specified with this parameter");
	pRC->RegisterKey("zip_encrypt", "Encrypts headers of zip files. Disabled by default.");
	pRC->RegisterKey("zip_encrypt_key", "Specifies a 128-bit key in hexadecimal format: 32-character string. Low endian format.");
	pRC->RegisterKey("zip_encrypt_content", "Encrypts files inside of zip. Works only when zip_encrypt enabled. Disabled by default.");
	pRC->RegisterKey("zip_compression", "Specify compression level for zipped files. [0-9] 0=no compression, 9=max compression. Default is 6.");
	pRC->RegisterKey("zip_sort", "Define sorting type when adding files to the pak, currently supported:\n"
	                             "nosort, size, streaming, suffix, alphabetically. Alphabetically is default.");
	pRC->RegisterKey("zip_split", "Define split type for distributing files into different paks automatically, currently supported:\n"
	                              "original, basedir, streaming, suffix. 'original' is default, except for streaming for which it is streaming.");
	pRC->RegisterKey("zip_maxsize", "Maximum compressed size of the zip in KBs");
	pRC->RegisterKey("zip_sizesplit", "Split zip files automatically when the maximum compressed size (configured or supported) has been reached");
	pRC->RegisterKey("zip_alignment", "Alignment of files inside zip. Default is 1 byte.");
	pRC->RegisterKey("zip_new", "Forces creation of new zip file overwriting existing one");
	pRC->RegisterKey("FolderInZip", "Put source files into this specified folder inside of zip file (see 'zip' command)");
	pRC->RegisterKey("sourceminsize", "only copy or zip a source file if its size is greater or equal than the size specified. used with 'copyonly' and 'zip' commands.");
	pRC->RegisterKey("sourcemaxsize", "only copy or zip a source file if its size is less or equal than the size specified. used with 'copyonly' and 'zip' commands.");
}

//////////////////////////////////////////////////////////////////////////
IPakSystem* PakManager::GetPakSystem()
{
	return &m_pPakSystem;
}

//////////////////////////////////////////////////////////////////////////
void PakManager::SetMaxThreads(int maxThreads)
{
	m_maxThreads = maxThreads;
}

//////////////////////////////////////////////////////////////////////////
int PakManager::GetMaxThreads() const
{
	return m_maxThreads;
}

//////////////////////////////////////////////////////////////////////////
bool PakManager::HasPakFiles() const
{
	return m_zipFiles.size() > 0;
}

//////////////////////////////////////////////////////////////////////////
PakManager::ECallResult PakManager::CompileFilesIntoPaks(
  const IConfig* config,
  const std::vector<RcFile>& m_allFiles)
{
	{
		const string pakFilePath = config->GetAsString("split_listfile_to_zips", "", "");
		if (!pakFilePath.empty())
		{
			std::vector<string> sourceRootsReversed;
			ResourceCompiler::GetSourceRootsReversed(config, sourceRootsReversed);

			return SplitListFileToPaks(config, sourceRootsReversed, m_allFiles, pakFilePath);
		}
	}

	{
		const string pakFilename = config->GetAsString("zip", "", "");
		if (!pakFilename.empty())
		{
			const string folderInPak = config->GetAsString("FolderInZip", "", "");
			const bool bUpdate = !config->GetAsBool("zip_new", false, true); // forces to recreate pak-file

			return CreatePakFile(config, m_allFiles, folderInPak, pakFilename, bUpdate);
		}
	}

	return eCallResult_Skipped;
}

//////////////////////////////////////////////////////////////////////////
PakManager::ECallResult PakManager::SplitListFileToPaks(
  const IConfig* config,
  const std::vector<string>& sourceRootsReversed,
  const std::vector<RcFile>& files,
  const string& pakFilePath)
{
	typedef std::map<string, std::vector<RcFile>> TSplitListMap;
	TSplitListMap splitListMap;
	for (size_t i = 0; i < files.size(); ++i)
	{
		const string line(files[i].m_sourceInnerPathAndName);
		const size_t splitter = line.find_first_of("|;,");
		if (splitter == string::npos)
		{
			continue;
		}

		string groupName = line.substr(0, splitter);
		string fileName = line.substr(splitter + 1);
		groupName.Trim();
		fileName.Trim();

		const string sourceLeftPath = ResourceCompiler::FindSuitableSourceRoot(sourceRootsReversed, fileName);
		splitListMap[groupName].push_back(RcFile(sourceLeftPath, fileName, ""));
	}

	for (TSplitListMap::iterator it = splitListMap.begin(); it != splitListMap.end(); ++it)
	{
		const string& groupName = it->first;
		std::vector<RcFile>& fileList = it->second;

		const string pakFilename = pakFilePath + groupName + ".pak";
		CreatePakFile(config, fileList, "", pakFilename, true);
	}

	return eCallResult_Succeeded;
}

//////////////////////////////////////////////////////////////////////////
PakManager::ECallResult PakManager::DeleteFilesFromPaks(
  const IConfig* config,
  const std::vector<string>& filesToDelete)
{
	if (HasPakFiles() && filesToDelete.size())
	{
		RCLog("Deleting files from zip archives");

		return SynchronizePaks(config, filesToDelete);
	}

	return eCallResult_Skipped;
}

PakManager::ECallResult PakManager::SynchronizePaks(
  const IConfig* config,
  const std::vector<string>& filesToDelete)
{
	const int nTotalToScan = m_zipFiles.size() * filesToDelete.size();
	const int zipFileAlignment = config->GetAsInt("zip_alignment", 1, 1);

	int nDeletedInZip = 0;
	int nScannedFiles = 0;

	// If we created some zip file, check if files need to be deleted from them.
	for (size_t nzip = 0; (nzip < m_zipFiles.size()) && (nTotalToScan > 0); ++nzip)
	{
		string zipFilename = m_zipFiles[nzip];

		PakSystemArchive* pPakFile = GetPakSystem()->OpenArchive(zipFilename.c_str(), zipFileAlignment);
		if (pPakFile)
		{
			const string progress = string("Deleting files from ") + zipFilename;

			RCLog("%s", zipFilename.c_str());

			for (size_t i = 0; i < filesToDelete.size(); ++i)
			{
				string filename = filesToDelete[i];
				if (GetPakSystem()->DeleteFromArchive(pPakFile, filename.c_str()))
				{
					++nDeletedInZip;

					RCLog("Remove file from zip: [%s] %s", zipFilename.c_str(), filename.c_str());
				}

				++nScannedFiles;
				m_pProgress->ShowProgress(progress.c_str(), nScannedFiles, nTotalToScan);
			}

			GetPakSystem()->CloseArchive(pPakFile);
		}
		else
		{
			RCLog("Can't open pak file: %s", zipFilename.c_str());
		}
	}

	return eCallResult_Succeeded;
}

//////////////////////////////////////////////////////////////////////////
PakManager::ECallResult PakManager::CreatePakFile(
  const IConfig* config,
  const std::vector<RcFile>& sourceFiles,
  const string& folderInPak,
  const string& requestedPakFilename,
  bool bUpdate)
{
	const bool bVerbose = config->GetAsInt("verbose", 0, 1) > 0;

	PakHelpers::ESortType eSortType = PakHelpers::eSortType_Alphabetically;
	const string sortType = config->GetAsString("zip_sort", "", "");
	if (!sortType.empty())
	{
		if (StringHelpers::EqualsIgnoreCase(sortType, "nosort"))
		{
			eSortType = PakHelpers::eSortType_NoSort;
		}
		else if (StringHelpers::EqualsIgnoreCase(sortType, "size"))
		{
			eSortType = PakHelpers::eSortType_Size;
		}
		else if (StringHelpers::EqualsIgnoreCase(sortType, "streaming"))
		{
			eSortType = PakHelpers::eSortType_Streaming;
		}
		else if (StringHelpers::EqualsIgnoreCase(sortType, "suffix"))
		{
			eSortType = PakHelpers::eSortType_Suffix;
		}
		else if (StringHelpers::EqualsIgnoreCase(sortType, "alphabetically"))
		{
			eSortType = PakHelpers::eSortType_Alphabetically;
		}
		else
		{
			RCLogError("Invalid zip_sort argument: '%s'. Creating of pak failed.", sortType.c_str());
			return eCallResult_BadArgs;
		}
	}

	PakHelpers::ESplitType eSplitType = PakHelpers::eSplitType_Original;
	if (eSortType == PakHelpers::eSortType_Streaming)
	{
		eSplitType = PakHelpers::eSplitType_ExtensionMipmap;
	}

	const string splitType = config->GetAsString("zip_split", "", "");
	if (!splitType.empty())
	{
		if (StringHelpers::EqualsIgnoreCase(splitType, "original"))
		{
			eSplitType = PakHelpers::eSplitType_Original;
		}
		else if (StringHelpers::EqualsIgnoreCase(splitType, "basedir"))
		{
			eSplitType = PakHelpers::eSplitType_Basedir;
		}
		else if (StringHelpers::EqualsIgnoreCase(splitType, "streaming"))
		{
			eSplitType = PakHelpers::eSplitType_ExtensionMipmap;
		}
		else if (StringHelpers::EqualsIgnoreCase(splitType, "suffix"))
		{
			eSplitType = PakHelpers::eSplitType_Suffix;
		}
		else
		{
			RCLogError("Invalid zip_split argument: '%s'. Creating of pak failed.", splitType.c_str());
			return eCallResult_BadArgs;
		}
	}

	if (!FileUtil::EnsureDirectoryExists(PathUtil::GetPathWithoutFilename(requestedPakFilename).c_str()))
	{
		RCLogError("Failed creating directory for %s", requestedPakFilename.c_str());
		return eCallResult_Failed;
	}

	std::map<string, std::vector<PakHelpers::PakEntry>> fileMap;

	{
		const size_t nCount = PakHelpers::CreatePakEntryList(sourceFiles, fileMap, eSortType, eSplitType, PathUtil::ReplaceExtension(requestedPakFilename, "pak"));
		if (nCount == 0)
		{
			return eCallResult_Failed;
		}

		RCLog("Requested %u files to be packed. Found %u valid files to add.", sourceFiles.size(), nCount);
	}

	std::set<unsigned int> crc32set;
	const bool name_as_crc32 = config->GetAsBool("name_as_crc32", false, true);

	const int nMinZipSize = sizeof(ZipFile::CDREnd);   // size of an empty CDR
	const int nMaxZipSize = config->GetAsInt("zip_maxsize", 0, 0) * 1024;

	const bool bSplitOnSizeOverflow = config->GetAsBool("zip_sizesplit", false, true);

	const int nMaxSrcSize = config->GetAsInt("sourcemaxsize", -1, -1);
	const int nMinSrcSize = config->GetAsInt("sourceminsize", 0, 0);

	const int zipCompressionLevel = config->GetAsInt("zip_compression", 6, 6);

	ECallResult bResult = eCallResult_Succeeded;
	for (std::map<string, std::vector<PakHelpers::PakEntry>>::iterator it = fileMap.begin(); it != fileMap.end(); ++it)
	{
		const string& pakFilename = it->first;
		const string tempPakFileName = CreateTempFileName(pakFilename);
		std::vector<PakHelpers::PakEntry>& files = it->second;

		RCLog("Found %u valid files to add to zip file %s", files.size(), pakFilename.c_str());

		if (!bUpdate)
		{
			// Delete old pak file.
			::SetFileAttributes(pakFilename.c_str(), FILE_ATTRIBUTE_ARCHIVE);
			::DeleteFile(pakFilename.c_str());
		}

		if (!FileUtil::EnsureDirectoryExists(PathUtil::GetPathWithoutFilename(pakFilename).c_str()))
		{
			RCLogError("Failed creating directory for %s", pakFilename.c_str());
			return eCallResult_Failed;
		}

		const int zipFileAlignment = config->GetAsInt("zip_alignment", 1, 1);

		const bool zipEncrypt = config->GetAsBool("zip_encrypt", false, true);
		const bool zipEncryptContent = config->GetAsBool("zip_encrypt_content", false, true);

		const string zipEncryptKey = config->GetAsString("zip_encrypt_key", "", "");
		uint32 encryptionKey[4];
		if (!zipEncryptKey.empty())
		{
			if (!ZipEncryptor::ParseKey(encryptionKey, zipEncryptKey.c_str()))
			{
				RCLogError("Misformed zip_encrypt_key: expected 128-bit integer in hexadecimal format (32 character)");
				return eCallResult_Failed;
			}
		}

		string pakFilenameToWrite = tempPakFileName;

		// check if pak is multi-part and redirect it before opening the pak
		bool bMultiPartPak = false;
		if (bSplitOnSizeOverflow)
		{
			string pakFilenameMultiPart = pakFilename;
			pakFilenameMultiPart.replace(".pak", "-part0.pak");
			if (FileUtil::FileExists(pakFilenameMultiPart))
			{
				RCLog("Found explicit multi-part zip, writing to zip file %s instead", pakFilenameMultiPart.c_str());

				bMultiPartPak = true;
				pakFilenameToWrite = CreateTempFileName(pakFilenameMultiPart);
			}
		}

		const size_t numFiles = files.size();
		size_t numFilesAdded = 0;
		size_t numFilesUpToDate = 0;
		size_t numFilesSkipped = 0;
		size_t numFilesMissing = 0;
		size_t numFilesFailed = 0;

		const string sProgressOp = "Adding files to zip file " + pakFilenameToWrite;
		m_pProgress->StartProgress();

		std::vector<string> realFilenames;
		std::vector<string> filenamesInZip;

		realFilenames.reserve(numFiles);
		filenamesInZip.reserve(numFiles);

		// create list of filenames
		for (size_t i = 0; i < numFiles; ++i)
		{

			string sFileNameInZip = PathHelpers::RemoveDuplicateSeparators(PathUtil::ToDosPath(PathUtil::Make(folderInPak, files[i].m_rcFile.m_sourceInnerPathAndName)));
			const string sRealFilename = PathUtil::Make(files[i].m_rcFile.m_sourceLeftPath, files[i].m_rcFile.m_sourceInnerPathAndName);

			// Skip files with extensions starting with "$" or "pak".
			{
				const string ext = PathUtil::GetExt(sRealFilename);
				if (!ext.empty() && (ext[0] == '$' || stricmp(ext, "pak") == 0))
				{
					++numFilesSkipped;
					continue;
				}
			}

			if (name_as_crc32)
			{
				const unsigned int crc32 = CCrc32::ComputeLowercase(sFileNameInZip.c_str());
				if (crc32set.find(crc32) != crc32set.end())
				{
					RCLogError("Duplicate CRC32 code %X for file %s when creating Pak File: %s", crc32, sFileNameInZip.c_str(), pakFilenameToWrite.c_str());
					++numFilesFailed;
					bResult = eCallResult_Erroneous;
					break;
				}

				crc32set.insert(crc32);
				sFileNameInZip.Format("%X", crc32);
			}

			filenamesInZip.push_back(sFileNameInZip);
			realFilenames.push_back(sRealFilename);
		}

		std::vector<const char*> realFilenamePtrs;
		std::vector<const char*> filenameInZipPtrs;
		std::vector<const char*> filenameInZipPtrsForDelete;

		size_t filenameCount = realFilenames.size();
		assert(filenameCount == filenamesInZip.size());

		realFilenamePtrs.resize(filenameCount);
		filenameInZipPtrs.resize(filenameCount);
		for (size_t i = 0; i < filenameCount; ++i)
		{
			realFilenamePtrs[i] = realFilenames[i].c_str();
			filenameInZipPtrs[i] = filenamesInZip[i].c_str();
		}

		size_t currentPakPart = 0;
		bool bKeepTrying = false;
		do
		{
			if (bUpdate)
			{
				const string existingPak = PathUtil::ReplaceExtension(pakFilenameToWrite, "pak");
				if (FileUtil::FileExists(existingPak))
				{
					::SetFileAttributes(existingPak.c_str(), FILE_ATTRIBUTE_ARCHIVE);
					::MoveFileEx(existingPak.c_str(), pakFilenameToWrite.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
				}
			}

			// Add them to pak file.
			PakSystemArchive* const pPakFile = GetPakSystem()->OpenArchive(pakFilenameToWrite.c_str(), zipFileAlignment, zipEncrypt, zipEncryptKey.empty() ? 0 : encryptionKey);
			if (!pPakFile)
			{
				RCLogError("Failed to create zip file %s", pakFilenameToWrite.c_str());
				return eCallResult_Failed;
			}

			// submit files for packing
			{
				struct ZipErrorReporter : public ZipDir::IReporter
				{
					IProgress&  m_progress;
					const char* m_zipFilename;
					bool        m_bVerbose;

					size_t      m_fileCount;
					size_t&     m_numFilesAdded;
					size_t&     m_numFilesUpToDate;
					size_t&     m_numFilesSkipped;
					size_t&     m_numFilesMissing;
					size_t&     m_numFilesFailed;

					ZipErrorReporter(IProgress& progress, const char* zipFilename, int numFiles, bool verbose,
					                 size_t& numFilesAdded, size_t& numFilesUpToDate, size_t& numFilesSkipped, size_t& numFilesMissing, size_t& numFilesFailed)
						: m_progress(progress)
						, m_zipFilename(zipFilename)
						, m_bVerbose(verbose)
						, m_fileCount(numFiles)
						, m_numFilesAdded(numFilesAdded)
						, m_numFilesUpToDate(numFilesUpToDate)
						, m_numFilesSkipped(numFilesSkipped)
						, m_numFilesMissing(numFilesMissing)
						, m_numFilesFailed(numFilesFailed)
					{
					}

					virtual void ReportAdded(const char* filename)
					{
						if (m_bVerbose)
						{
							RCLog("Zip [%s]: added file %s", m_zipFilename, filename);
						}
						++m_numFilesAdded;
						ShowProgress();
					}

					virtual void ReportMissing(const char* filename)
					{
						RCLogWarning("Zip [%s]: missing file %s", m_zipFilename, filename);
						++m_numFilesMissing;
						ShowProgress();
					}

					virtual void ReportUpToDate(const char* filename)
					{
						if (m_bVerbose)
						{
							RCLog("Zip [%s]: up to date %s", m_zipFilename, filename);
						}
						++m_numFilesUpToDate;
						ShowProgress();
					}

					virtual void ReportSkipped(const char* filename)
					{
						RCLog("Zip [%s]: skipped %s", m_zipFilename, filename);
						++m_numFilesSkipped;
						ShowProgress();
					}

					virtual void ReportFailed(const char* filename, const char* reason)
					{
						RCLog("Zip [%s]: failed to add %s. %s", m_zipFilename, filename, reason);
						++m_numFilesFailed;
						ShowProgress();
					}

					virtual void ReportSpeed(double speed)
					{
						RCLog("Zip [%s] compression speed: %.2f MB/sec", m_zipFilename, speed / 1024.0 / 1024.0);
						ShowProgress();
					}

					void ShowProgress()
					{
						size_t processedFiles = m_numFilesAdded + m_numFilesUpToDate + m_numFilesFailed + m_numFilesSkipped + m_numFilesMissing;
						m_progress.ShowProgress("Adding files into pak", processedFiles, m_fileCount);
					}
				};

				struct ZipSizeSplitter : public ZipDir::ISplitter
				{
					int    m_fileLast;
					int    m_fileCount;
					size_t m_fileSizeLimit;
					size_t m_fileSizeThreshold;

					ZipSizeSplitter(int filenameCount, size_t filesizeLimit)
						: m_fileLast(filenameCount - 1)
						, m_fileCount(filenameCount - 1)
						, m_fileSizeLimit(filesizeLimit)
						, m_fileSizeThreshold(0)
					{
					}

					virtual bool CheckWriteLimit(size_t total, size_t add, size_t sub) const
					{
						return ((total - sub) > (m_fileSizeLimit - add));
					}

					virtual void SetLastFile(size_t total, size_t add, size_t sub, int last)
					{
						m_fileLast = last;
						m_fileSizeThreshold = m_fileSizeLimit - add;
					}

					bool HasReachedWriteLimit() const
					{
						return m_fileLast < m_fileCount;
					}
				};

				ZipErrorReporter errorReporter(*m_pProgress, pakFilenameToWrite.c_str(), numFiles, bVerbose,
				                               numFilesAdded, numFilesUpToDate, numFilesSkipped, numFilesMissing, numFilesFailed);
				ZipSizeSplitter sizeSplitter(filenameCount, nMaxZipSize ? min(nMaxZipSize, INT_MAX) : INT_MAX);

				RCLog("Adding files into %s...", pakFilenameToWrite.c_str());
				const int threadCount = GetMaxThreads() == 1 ? 0 : GetMaxThreads();
				pPakFile->zip->UpdateMultipleFiles(&realFilenamePtrs[0], &filenameInZipPtrs[0], filenameCount,
				                                   zipCompressionLevel, zipEncrypt && zipEncryptContent, nMaxZipSize, nMinSrcSize, nMaxSrcSize,
				                                   threadCount, &errorReporter, bSplitOnSizeOverflow ? &sizeSplitter : nullptr);

				// divide files in case it has overflown the maximum allowed file-size
				if (bSplitOnSizeOverflow)
				{
					char cPart[16];
					char nPart[16];

					cry_sprintf(cPart, "-part%" PRISIZE_T ".$pak", currentPakPart + 0);
					cry_sprintf(nPart, "-part%" PRISIZE_T ".$pak", currentPakPart + 1);

					const size_t pos = pakFilenameToWrite.find(cPart);

					if (!bKeepTrying)
					{
						// delete previously consumed files from archive
						const size_t filenameCountForDelete = filenameInZipPtrsForDelete.size();
						for (size_t i = 0; i < filenameCountForDelete; ++i)
						{
							pPakFile->zip->RemoveFile(filenameInZipPtrsForDelete[i]);
						}
					}

					// move consumed filenames into deletable list
					const size_t filenameCountForDelete = filenameInZipPtrsForDelete.size();
					const size_t filenameCountConsumed = sizeSplitter.m_fileLast + 1;
					filenameInZipPtrsForDelete.resize(filenameCountForDelete + filenameCountConsumed);
					memcpy(&filenameInZipPtrsForDelete[filenameCountForDelete], &filenameInZipPtrs[0], sizeof(filenameInZipPtrs[0]) * filenameCountConsumed);

					// shift pending names to the front of the list
					filenameCount -= filenameCountConsumed;
					if (filenameCount > 0)
					{
						memmove(&realFilenamePtrs[0], &realFilenamePtrs[filenameCountConsumed], sizeof(realFilenamePtrs[0]) * filenameCount);
						memmove(&filenameInZipPtrs[0], &filenameInZipPtrs[filenameCountConsumed], sizeof(filenameInZipPtrs[0]) * filenameCount);

						if (!bKeepTrying)
						{
							// delete skipped over files from archive
							for (size_t i = 0; i < filenameCount; ++i)
							{
								pPakFile->zip->RemoveFile(filenameInZipPtrs[i]);
							}
						}
					}

					realFilenamePtrs.resize(filenameCount);
					filenameInZipPtrs.resize(filenameCount);

					if (sizeSplitter.HasReachedWriteLimit())
					{
						RCLog("Hitting limit of %d bytes on %s, trying reconsolidation...", sizeSplitter.m_fileSizeLimit, pakFilenameToWrite.c_str());

						// close archive
						GetPakSystem()->CloseArchive(pPakFile);

						// check if the reconsolidation of the pak gave more space free than we needed previously
						// in case it did, keep adding to the same file, instead of adding it to the next part
						const __int64 fileSize = FileUtil::GetFileSize(pakFilenameToWrite);
						if (fileSize < sizeSplitter.m_fileSizeThreshold)
						{
							// if we tried keepTrying without effect, don't try again
							if ((filenameCountConsumed != 0) || !bKeepTrying)
							{
								if (pos == string::npos)
								{
									RCLog("Reconsolidation on %s dropped at least %d bytes below %d, keep adding...", pakFilenameToWrite.c_str(), sizeSplitter.m_fileSizeLimit - sizeSplitter.m_fileSizeThreshold, sizeSplitter.m_fileSizeLimit);
								}
								else
								{
									RCLog("Reconsolidation on %s dropped at least %d bytes below %d, keep adding to part %d...", pakFilenameToWrite.c_str(), sizeSplitter.m_fileSizeLimit - sizeSplitter.m_fileSizeThreshold, sizeSplitter.m_fileSizeLimit, currentPakPart);
								}

								bKeepTrying = true;
								continue;
							}
						}

						// rename archive if it's the first time becoming multi-part
						if (pos == string::npos)
						{
							RCLog("Start splitting %s, writing to part %d...", pakFilename.c_str(), currentPakPart + 1);

							string pakFilenameToRename = pakFilenameToWrite;
							pakFilenameToRename.replace(".$pak", cPart);
							MoveFile(pakFilenameToWrite.c_str(), pakFilenameToRename.c_str());

							bMultiPartPak = true;
							pakFilenameToWrite = pakFilenameToRename;
						}
						else
						{
							RCLog("Continue splitting %s, writing to part %d...", pakFilename.c_str(), currentPakPart + 1);
						}

						// continue adding to the next part
						pakFilenameToWrite.replace(cPart, nPart);
						currentPakPart++;

						bKeepTrying = false;
						continue;
					}

					assert(filenameCount == 0);

					assert(realFilenamePtrs.size() == 0);
					assert(filenameInZipPtrs.size() == 0);
				}
				else
				{
					filenameCount = 0;
					realFilenamePtrs.clear();
					filenameInZipPtrs.clear();
				}
			}

			GetPakSystem()->CloseArchive(pPakFile);
			const __int64 fileSize = FileUtil::GetFileSize(pakFilenameToWrite);
			if (fileSize > INT_MAX)
			{
				RCLogError("PAK File size exceeds 2GB limit. This will not be loaded by Engine: %s", pakFilenameToWrite.c_str());
			}
			else if (bSplitOnSizeOverflow && bMultiPartPak)
			{
				// delete all 0 size pak-parts and close holes in the numbering of the parts
				bool trailingUntouched = false;
				int trailingPakPart = currentPakPart;
				for (;; )
				{
					char cPart[16];
					char nPart[16];

					cry_sprintf(cPart, "-part%d.$pak", trailingPakPart + 0);
					cry_sprintf(nPart, "-part%d.$pak", trailingPakPart + 1);

					string pakFilenameToDelete = tempPakFileName;
					pakFilenameToDelete.replace(".$pak", cPart);
					if (!FileUtil::FileExists(pakFilenameToDelete))
					{
						break;
					}

					// delete skipped over files from archives not touched
					if (trailingUntouched)
					{
						PakSystemArchive* const pPakFile = GetPakSystem()->OpenArchive(pakFilenameToDelete.c_str(), zipFileAlignment, zipEncrypt, zipEncryptKey.empty() ? 0 : encryptionKey);
						if (pPakFile)
						{
							const size_t filenameCountForDelete = filenameInZipPtrsForDelete.size();
							for (size_t i = 0; i < filenameCountForDelete; ++i)
							{
								pPakFile->zip->RemoveFile(filenameInZipPtrsForDelete[i]);
							}

							GetPakSystem()->CloseArchive(pPakFile);
						}
					}

					const __int64 fileSize = FileUtil::GetFileSize(pakFilenameToDelete);
					if (fileSize <= nMinZipSize)
					{
						// eliminate paks without content (may occur by filtering or reordering)
						DeleteFile(pakFilenameToDelete);

						// shift successive part-names into the hole left by the deleted pak
						for (int q = trailingPakPart;; q++)
						{
							char cPart[16];
							char nPart[16];

							cry_sprintf(cPart, "-part%d.$pak", q + 0);
							cry_sprintf(nPart, "-part%d.$pak", q + 1);

							string pakFilenameToReplace = tempPakFileName;
							string pakFilenameToRename = tempPakFileName;
							pakFilenameToReplace.replace(".$pak", cPart);
							pakFilenameToRename.replace(".$pak", nPart);
							if (!FileUtil::FileExists(pakFilenameToRename))
							{
								break;
							}

							MoveFile(pakFilenameToRename.c_str(), pakFilenameToReplace.c_str());
						}
					}
					else
					{
						++trailingPakPart;
					}

					trailingUntouched = true;
				}

				{
					string pakFilenameFirstPart = tempPakFileName;
					string pakFilenameNextPart = tempPakFileName;

					pakFilenameFirstPart.replace(".$pak", "-part0.$pak");
					pakFilenameNextPart.replace(".$pak", "-part1.$pak");

					// remove part-suffix if just one part exists after cleanup
					if (FileUtil::FileExists(pakFilenameFirstPart) &&
					    !FileUtil::FileExists(pakFilenameNextPart))
					{
						MoveFile(pakFilenameFirstPart.c_str(), tempPakFileName.c_str());
						m_zipFiles.push_back(tempPakFileName);
					}
					// register all parts of the pak for sucessive operations
					else
					{
						for (int q = 0; q < trailingPakPart; q++)
						{
							char cPart[16];

							cry_sprintf(cPart, "-part%d.$pak", q + 0);

							string pakFilenamePart(tempPakFileName);
							pakFilenamePart.replace(".$pak", cPart);
							m_zipFiles.push_back(pakFilenamePart);
						}
					}
				}
			}
			else if (fileSize <= nMinZipSize)
			{
				// eliminate paks without content (may occur by filtering)
				DeleteFile(pakFilenameToWrite);
			}
			else
			{
				// Add this zip to the array.
				m_zipFiles.push_back(pakFilenameToWrite);
			}

			bKeepTrying = false;
		}
		while (filenameCount);

		for (string& tempFileName : m_zipFiles)
		{
			const string fileName = PathUtil::ReplaceExtension(tempFileName, "pak");
			::MoveFileEx(tempFileName.c_str(), fileName.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
			tempFileName = fileName;
		}

		RCLog("Finished adding %d files to zip file %s:",
		      numFiles, pakFilename.c_str());
		RCLog("    %d added, %d up-to-date, %d skipped, %d missing, %d failed",
		      numFilesAdded, numFilesUpToDate, numFilesSkipped, numFilesMissing, numFilesFailed);
	}

	return bResult;
}
