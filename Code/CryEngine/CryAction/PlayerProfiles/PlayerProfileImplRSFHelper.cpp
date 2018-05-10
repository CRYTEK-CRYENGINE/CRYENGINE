// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryCore/Platform/platform.h>
#include <CrySystem/XML/IXml.h>
#include <CryString/StringUtils.h>
#include "PlayerProfileImplRSFHelper.h"
#include "PlayerProfile.h"
#include "Serialization/XmlSaveGame.h"
#include "Serialization/XmlLoadGame.h"
#include "BMPHelper.h"
#include "RichSaveGameTypes.h"
#include "CryAction.h"
#include <CryCore/Platform/IPlatformOS.h>
#include <CryString/StringUtils.h>

#define RSF_USE_COMPRESSION // write compressed XML data
// #undef RSF_USE_COMPRESSSION

#ifndef MAKEFOURCC
	#if defined(NEED_ENDIAN_SWAP) // big endian
		#define MAKEFOURCC(ch0, ch1, ch2, ch3)              \
		  ((DWORD)(BYTE)(ch3) | ((DWORD)(BYTE)(ch2) << 8) | \
		   ((DWORD)(BYTE)(ch1) << 16) | ((DWORD)(BYTE)(ch0) << 24))
	#else // little endian
		#define MAKEFOURCC(ch0, ch1, ch2, ch3)              \
		  ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) | \
		   ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24))
	#endif
#endif //defined(MAKEFOURCC)

#ifdef min
	#undef min
#endif

#ifdef max
	#undef max
#endif

#define TEST_THUMBNAIL_AUTOCAPTURE // auto-screen capture
#undef TEST_THUMBNAIL_AUTOCAPTURE

#define TEST_THUMBNAIL_WRITE // write capture into separate file
#undef TEST_THUMBNAIL_WRITE

#define TEST_THUMBNAIL_REWRITE // write capture into separate file
#undef TEST_THUMBNAIL_REWRITE

// Layout of our RichSaveGames
// RICH_GAME_MEDIA_HEADER (as provided)
// optional thumbnail (see RICH_GAME_MEDIA_HEADER)
// uint32 RM_META_DATA_TAG
// uint32 metaDataLength;
// char   metaData[metaDataLength]                   or
// uint32 RM_SAVEDATA_TAG                            uint32 RM_COMPRESSED_SAVEDATA_TAG
// uint32 saveGameDataLength;                        uint32 saveGameDataLength
// char   saveGameData[saveGameDataLength]           uint32 uncompressedDataLength
//                                                   char   [saveGameDataLength-sizeof(uint32)]
// thumbnail is a BMP (can be any size)
// metaData and saveGameData is a non-zero-terminated XMLString
// [saveGameData actually contains meta-data also]
// first metaDataBlock is needed to get metainformation without accessing
// full savegamedata
namespace RichSaveGames
{
// only used if TEST_THUMBNAIL_AUTOCAPTURE
//	static const int THUMBNAIL_DEFAULT_WIDTH  = 256;   // 16:9
//	static const int THUMBNAIL_DEFAULT_HEIGHT = 144;   //
//	static const int THUMBNAIL_DEFAULT_DEPTH = 4;   // write out with alpha
//	static const bool THUMBNAIL_KEEP_ASPECT_RATIO = true; // keep renderes aspect ratio and surround with black borders
// ~only used if TEST_THUMBNAIL_AUTOCAPTURE

// our tags in the binary file
static const uint32 RM_METADATA_TAG = MAKEFOURCC('M', 'E', 'T', 'A');            // META
static const uint32 RM_SAVEDATA_TAG = MAKEFOURCC('D', 'A', 'T', 'A');            // DATA
static const uint32 RM_COMPRESSED_SAVEDATA_TAG = MAKEFOURCC('D', 'A', 'T', 'C'); // DATC compressed data
static const uint32 RM_MAGICNUMBER = MAKEFOURCC('R', 'G', 'M', 'H');
//static const char* gGameGUID = // "{8236D2E9-2528-4C5C-ABA3-E0B8B657A297}";
//																	"{CDC82B4A-7540-45A5-B92E-9A7C7033DBF2}";
}; // ~namespace RichSaveGames

//------------------------------------------------------------------------
// COMMON RichSaveGameHelper used by both CPlayerProfileImplFSDir and CPlayerProfileImplFS
//------------------------------------------------------------------------

namespace
{
//-----------------------------------------------------------------------------
// Converts a string to a GUID
//-----------------------------------------------------------------------------
const char* ConvertGUIDToString(const RichSaveGames::GUID* pGuid)
{
	static char guidString[64];
	const RichSaveGames::GUID& guid = *pGuid;
	cry_sprintf(guidString, "{%.8X-%.4X-%.4X-%.2X%.2X-%.2X%.2X%.2X%.2X%.2X%.2X}", guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1],
	            guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	return guidString;
}

//-----------------------------------------------------------------------------
// Converts a string to a GUID
//-----------------------------------------------------------------------------
bool ConvertStringToGUID(const char* strIn, RichSaveGames::GUID* pGuidOut)
{
	unsigned int aiTmp[10];

	if (sscanf(strIn, "{%8X-%4X-%4X-%2X%2X-%2X%2X%2X%2X%2X%2X}",
	           &pGuidOut->Data1,
	           &aiTmp[0], &aiTmp[1],
	           &aiTmp[2], &aiTmp[3],
	           &aiTmp[4], &aiTmp[5],
	           &aiTmp[6], &aiTmp[7],
	           &aiTmp[8], &aiTmp[9]) != 11)
	{
		memset(pGuidOut, 0, sizeof(RichSaveGames::GUID));
		return false;
	}
	else
	{
		pGuidOut->Data2 = (uint16) aiTmp[0];
		pGuidOut->Data3 = (uint16) aiTmp[1];
		pGuidOut->Data4[0] = (unsigned char) aiTmp[2];
		pGuidOut->Data4[1] = (unsigned char) aiTmp[3];
		pGuidOut->Data4[2] = (unsigned char) aiTmp[4];
		pGuidOut->Data4[3] = (unsigned char) aiTmp[5];
		pGuidOut->Data4[4] = (unsigned char) aiTmp[6];
		pGuidOut->Data4[5] = (unsigned char) aiTmp[7];
		pGuidOut->Data4[6] = (unsigned char) aiTmp[8];
		pGuidOut->Data4[7] = (unsigned char) aiTmp[9];
		return true;
	}
}

template<class T> void CopyToWideString(T& t, const string& str)
{
	size_t maxCount = std::min(CRY_ARRAY_COUNT(t) - 1, str.length());
#if CRY_PLATFORM_ANDROID
	/*UTF32* wt = reinterpret_cast<UTF32*>(d);
	   UTF32** wt_start = &wt;
	   const UTF8* out =  reinterpret_cast<const UTF8*>(s);
	   const UTF8** out_start = &out;

	   unsigned int bytes_used;
	   if ( ConvertUTF8toUTF32(out_start, out + str.length(),wt_start,wt + maxCount, strictConversion) == conversionOK)
	   {
	   d[maxCount] = L'\0';
	   }
	   else
	   {
	   CryLogAlways("Failed to convert single byte chart to multibyte char");
	   d[0] = L'\0';
	   }*/
	wstring wstr = CryStringUtils::UTF8ToWStr(str);
	memcpy(t, wstr.c_str(), sizeof(wchar_t) * maxCount);
	t[maxCount] = L'\0';
#else
	wchar_t* d = &t[0];
	const char* s = str.c_str();
	while (maxCount-- > 0)
		mbtowc(d++, s++, 1);
	*d = L'\0';
#endif
}

/*
   // some helpers
   bool SaveXMLFile(const string& filename, const XmlNodeRef& rootNode)
   {
    if (rootNode == 0)
      return true;

    const bool ok = rootNode->saveToFile(filename.c_str(), 1024*1024);
    if (!ok)
      GameWarning("[PlayerProfiles] CRichSaveGames: Cannot save XML file '%s'", filename.c_str());
    return ok;
   }

   XmlNodeRef LoadXMLFile(const string& filename)
   {
    XmlNodeRef rootNode = GetISystem()->LoadXmlFromFile(filename.c_str());
    if (rootNode == 0)
    {
      GameWarning("[PlayerProfiles] CRichSaveGames: Cannot load XML file '%s'", filename.c_str());
    }
    return rootNode;
   }
 */
};

bool ExtractMetaDataFromXML(XmlNodeRef& root, CPlayerProfileManager::SSaveGameMetaData& metaData)
{
	// TODO: use CXmlLoadGame for this
	XmlNodeRef metaDataNode = root;
	if (metaDataNode->isTag("Metadata") == false)
		metaDataNode = root->findChild("Metadata");
	if (metaDataNode == 0)
		return false;
	bool ok = true;
	ok &= GetAttr(metaDataNode, "level", metaData.levelName);
	ok &= GetAttr(metaDataNode, "gameRules", metaData.gameRules);
	ok &= GetAttr(metaDataNode, "version", metaData.fileVersion);
	ok &= GetAttr(metaDataNode, "build", metaData.buildVersion);
	ok &= GetTimeAttr(metaDataNode, "saveTime", metaData.saveTime);
	metaData.loadTime = metaData.saveTime;
	metaData.xmlMetaDataNode = metaDataNode;
	return ok;
}

string tagToString(uint32 tag)
{
	char tagString[5];
	tagString[0] = (char) (tag & 0xFF);
	tagString[1] = (char) ((tag >> 8) & 0xFF);
	tagString[2] = (char) ((tag >> 16) & 0xFF);
	tagString[3] = (char) ((tag >> 24) & 0xFF);
	tagString[4] = 0;
	return string(tagString);
}

// writes out the Tag ID, length, and data (if length > 0), compresses if wanted
bool WriteXMLNode(const uint32 tag, const XmlNodeRef& node, FILE* pFile, bool bCompress, const char* debugFilename = "")
{
	ICryPak* pCryPak = gEnv->pCryPak;
	if (node != 0)
	{
		_smart_ptr<IXmlStringData> pXmlStrData = node->getXMLData(16000000);
		size_t xmlDataLength = pXmlStrData->GetStringLength();

		if (debugFilename && *debugFilename)
		{
			FILE* pDebugFile = pCryPak->FOpen(debugFilename, "wb");
			if (pDebugFile)
			{
				pCryPak->FWrite((void*)pXmlStrData->GetString(), pXmlStrData->GetStringLength(), 1, pDebugFile);
				pCryPak->FClose(pDebugFile);
			}
		}

		if (bCompress == false)
		{
			// write the tag
			pCryPak->FWrite(&tag, 1, pFile);
			// assert (xmlDataLength <= 0xFFFFFFFF)
			uint32 dataLength = xmlDataLength;
			pCryPak->FWrite(&dataLength, 1, pFile);
			if (xmlDataLength > 0)
			{
				pCryPak->FWrite((const void*)pXmlStrData->GetString(), xmlDataLength, 1, pFile);
			}
		}
		else
		{
			char* compressedBuf = static_cast<char*>(pCryPak->PoolMalloc(xmlDataLength));
			size_t compressedLength = xmlDataLength;
			bool bOK = gEnv->pSystem->CompressDataBlock((const void*)pXmlStrData->GetString(), xmlDataLength, (void*) compressedBuf, compressedLength);
			if (!bOK)
			{
				string tagString = tagToString(tag);
				GameWarning("CRichSaveGameHelper:WriteXMLNode: Cannot compress data block while writing tag '%s'", tagString.c_str());
				pCryPak->PoolFree(compressedBuf);
				return false;
			}
			// write the tag
			pCryPak->FWrite(&tag, 1, pFile);
			uint32 dataLength = (uint32) compressedLength;
			dataLength += sizeof(uint32); // because we store the uncompressed size as well
			// write size of the complete tag
			pCryPak->FWrite(&dataLength, 1, pFile);
			// write size of uncompressed buffer for decompression later
			uint32 uncompressedSize = (uint32) xmlDataLength;
			pCryPak->FWrite(&uncompressedSize, 1, pFile);
			pCryPak->FWrite((void*) compressedBuf, compressedLength, 1, pFile);
			pCryPak->PoolFree(compressedBuf);
		}
	}
	else
	{
		// write the tag
		pCryPak->FWrite(&tag, 1, pFile);
		uint32 dataLength = 0;
		pCryPak->FWrite(&dataLength, 1, pFile);
	}
	return true;
}

bool ReadTag(FILE* pFile, uint32& outTag, bool bRestorePos)
{
	uint32 tmp;
	ICryPak* pCryPak = gEnv->pCryPak;
	long curOffset = pCryPak->FTell(pFile);
	if (pCryPak->FRead(&tmp, 1, pFile) != 1)
		return false;
	outTag = tmp;
	if (bRestorePos)
		pCryPak->FSeek(pFile, curOffset, SEEK_SET);
	return true;
}

bool SkipXMLTagData(const uint32 tag, FILE* pFile)
{
	uint32 tmp = 0;
	ICryPak* pCryPak = gEnv->pCryPak;
	size_t fileSize = pCryPak->FGetSize(pFile);
	long curOffset = pCryPak->FTell(pFile);
	if (pCryPak->FRead(&tmp, 1, pFile) != 1)
		return false;
	if (tag != tmp)
	{
		GameWarning("CRichSaveGameHelper:SkipXMLTagData: Expected tag '%s' not found (read '%s')", tagToString(tag).c_str(), tagToString(tmp).c_str());
		pCryPak->FSeek(pFile, curOffset, SEEK_SET);
		return false;
	}
	uint32 len = 0;
	if (pCryPak->FRead(&len, 1, pFile) != 1)
	{
		GameWarning("CRichSaveGameHelper:SkipXMLTagData: tag='%s': Error while reading stored length", tagToString(tag).c_str());
		pCryPak->FSeek(pFile, curOffset, SEEK_SET);
		return false;
	}

	// verify that length is somehow reasonable (less than filesize for now)
	if (len > fileSize)
	{
		GameWarning("CRichSaveGameHelper:SkipXMLTagData: tag='%s': Read size is invalid (read=%d filesize=%d)", tagToString(tag).c_str(), len, (uint32)fileSize);
		return 0;
	}

	pCryPak->FSeek(pFile, len, SEEK_CUR);
	return true;
}

char* ReadXMLTagData(const uint32 tag, FILE* pFile, bool bIsCompressed)
{
	uint32 tmp = 0;
	ICryPak* pCryPak = gEnv->pCryPak;
	if (pCryPak->FRead(&tmp, 1, pFile) != 1)
		return 0;
	if (tag != tmp)
	{
		GameWarning("CRichSaveGameHelper:ReadXMLTagData: Expected tag '%s' not found (read '%s')", tagToString(tag).c_str(), tagToString(tmp).c_str());
		return 0;
	}
	uint32 len = 0;
	if (pCryPak->FRead(&len, 1, pFile) != 1)
	{
		GameWarning("CRichSaveGameHelper:ReadXMLTagData: tag='%s': Error while reading stored length", tagToString(tag).c_str());
		return 0;
	}

	// verify that length is somehow reasonable (less than filesize for now)
	size_t fileSize = pCryPak->FGetSize(pFile);
	if (len > fileSize)
	{
		GameWarning("CRichSaveGameHelper:ReadXMLTagData: tag='%s': Read size is invalid (read=%d filesize=%d)", tagToString(tag).c_str(), len, (uint32)fileSize);
		return 0;
	}

	uint32 uncompressedSize = 0;
	if (bIsCompressed)
	{
		if (pCryPak->FRead(&uncompressedSize, 1, pFile) != 1)
		{
			GameWarning("CRichSaveGameHelper:ReadXMLTagData: tag='%s': Error while reading uncompresssed length", tagToString(tag).c_str());
			return 0;
		}
		len -= sizeof(uint32);
	}

	char* buf = new char[len + 1];
	const size_t readBytes = pCryPak->FReadRaw(buf, 1, len, pFile);
	if (readBytes != len)
	{
		GameWarning("CRichSaveGameHelper:ReadXMLTagData: tag='%s': Error while reading (read=%" PRISIZE_T " expected=%u)", tagToString(tag).c_str(), readBytes, len);
		delete[] buf;
		return 0;
	}
	buf[len] = '\0';

	if (bIsCompressed && uncompressedSize > 0)
	{
		char* uncompressedData = new char[uncompressedSize + 1];
		if (uncompressedData == 0)
		{
			GameWarning("CRichSaveGameHelper:ReadXMLTagData: tag='%s': Error while allocating decompression buffer. (compressedSize=%d, uncompressedSize=%d)", tagToString(tag).c_str(), len, uncompressedSize);
			delete[] buf;
			return 0;
		}
		size_t longUncompressedSize = uncompressedSize;
		const bool bOK = gEnv->pSystem->DecompressDataBlock((void*) buf, len, uncompressedData, longUncompressedSize);
		if (bOK == false)
		{
			GameWarning("CRichSaveGameHelper:ReadXMLTagData: tag='%s': Error while decompressing. (compressedSize=%d, uncompressedSize=%d)", tagToString(tag).c_str(), len, uncompressedSize);
			delete[] uncompressedData;
			delete[] buf;
			return 0;
		}
		// delete the compressed buffer
		delete[] buf;
		// and assign the uncompressed buffer for return value
		buf = uncompressedData;
		buf[uncompressedSize] = 0;
	}
	return buf;
}

bool ReadRichGameMediaHeader(const char* filename, FILE* pFile, RichSaveGames::RICH_GAME_MEDIA_HEADER& header)
{
	ICryPak* pCryPak = gEnv->pCryPak;
	memset(&header, 0, sizeof(RichSaveGames::RICH_GAME_MEDIA_HEADER));
	size_t len = pCryPak->FRead(&header, 1, pFile);
	if (len != 1 || header.dwMagicNumber != RichSaveGames::RM_MAGICNUMBER)
	{
		GameWarning("CXMLRichLoadGame:GetSaveGameThumbnail: File '%s' is not a RichSaveGame", filename);
		pCryPak->FClose(pFile);
		return false;
	}

	const char* guid = ConvertGUIDToString(&header.guidGameId);
	if (strcmp(guid, CCryAction::GetCryAction()->GetGameGUID()) != 0)
	{
		GameWarning("CXMLRichLoadGame:GetSaveGameThumbnail: GUID '%s' in File '%s' does not match this game's '%s'", guid, filename, CCryAction::GetCryAction()->GetGameGUID());
		// pCryPak->FClose(pFile);
		// return false;
	}
	return true;
}

bool ReadRichGameMetaData(const string& filename, CPlayerProfileManager::SSaveGameMetaData& metaData)
{
	ICryPak* pCryPak = gEnv->pCryPak;
	FILE* pFile = pCryPak->FOpen(filename, "rbx"); // x=don't chache full file
	if (!pFile)
		return false;

	RichSaveGames::RICH_GAME_MEDIA_HEADER savedHeader;
	if (ReadRichGameMediaHeader(filename.c_str(), pFile, savedHeader) == false)
	{
		GameWarning("CXMLRichLoadGame:ReadRichGameMetaData: Can't read rich game media header from file '%s'", filename.c_str());
		pCryPak->FClose(pFile);
		return false;
	}

	// for now, skip thumbnails
	int64 thumbNailOffset = savedHeader.liThumbnailOffset;
	DWORD thumbNailSize = savedHeader.dwThumbnailSize;
	if (thumbNailOffset > 0)
	{
		pCryPak->FSeek(pFile, (int)thumbNailOffset, SEEK_CUR);
	}
	if (thumbNailSize > 0)
	{
		pCryPak->FSeek(pFile, thumbNailSize, SEEK_CUR);
	}

	const char* const metaDataBuf = ReadXMLTagData(RichSaveGames::RM_METADATA_TAG, pFile, false);
	if (metaDataBuf == 0)
	{
		GameWarning("CXMLRichLoadGame:ReadRichGameMetaData: Can't read meta data from file '%s'", filename.c_str());
		pCryPak->FClose(pFile);
		return false;
	}

	XmlNodeRef xmlMetaDataNode = gEnv->pSystem->LoadXmlFromBuffer(metaDataBuf, strlen(metaDataBuf));
	if (xmlMetaDataNode == 0)
	{
		GameWarning("CXMLRichLoadGame:ReadRichGameMetaData: Can't parse XML meta data from file '%s'", filename.c_str());
		pCryPak->FClose(pFile);
		delete[] metaDataBuf;
		return false;
	}

	bool bOK = ExtractMetaDataFromXML(xmlMetaDataNode, metaData);
	if (!bOK)
	{
		GameWarning("CXMLRichLoadGame:ReadRichGameMetaData: Can't extract XML meta data from file '%s'", filename.c_str());
	}

	delete[] metaDataBuf;
	pCryPak->FClose(pFile);
	return bOK;
}

bool CRichSaveGameHelper::FetchMetaData(XmlNodeRef& root, CPlayerProfileManager::SSaveGameMetaData& metaData)
{
	return ExtractMetaDataFromXML(root, metaData);
}

bool CRichSaveGameHelper::GetSaveGames(CPlayerProfileManager::SUserEntry* pEntry, CPlayerProfileManager::TSaveGameInfoVec& outVec, const char* altProfileName)
{
	// Scan savegames directory for XML files
	// we scan only for save game meta information
	string path;
	string profileName = (altProfileName && *altProfileName) ? altProfileName : pEntry->pCurrentProfile->GetName();
	m_pImpl->InternalMakeFSSaveGamePath(pEntry, profileName, path, true);

	const bool bNeedProfilePrefix = m_pImpl->GetManager()->IsSaveGameFolderShared();
	string profilePrefix = profileName;
	profilePrefix += '_';
	size_t profilePrefixLen = profilePrefix.length();

	ICryPak* pCryPak = gEnv->pCryPak;
	_finddata_t fd;

	path.TrimRight("/\\");
	string search;
	search.Format("%s/*%s", path.c_str(), CRY_SAVEGAME_FILE_EXT);

	IPlatformOS* os = GetISystem()->GetPlatformOS();
	unsigned int userIndex = os->UserGetPlayerIndex(profileName);
	IPlatformOS::IFileFinderPtr fileFinder = os->GetFileFinder(userIndex);
	intptr_t handle = fileFinder->FindFirst(search.c_str(), &fd);

	if (handle != -1)
	{
		CPlayerProfileManager::SSaveGameInfo sgInfo;
		do
		{
			if (strcmp(fd.name, ".") == 0 || strcmp(fd.name, "..") == 0)
				continue;

			if (bNeedProfilePrefix)
			{
				if (strnicmp(profilePrefix, fd.name, profilePrefixLen) != 0)
					continue;
			}

			sgInfo.name = fd.name;
			if (bNeedProfilePrefix) // skip profile_ prefix (we made sure this is valid by comparism above)
				sgInfo.humanName = fd.name + profilePrefixLen;
			else
				sgInfo.humanName = fd.name;

			PathUtil::RemoveExtension(sgInfo.humanName);
			sgInfo.description = "no description";

			string filename = path;
			filename.append("/");
			filename.append(fd.name);

			bool ok = ReadRichGameMetaData(filename, sgInfo.metaData);

			if (ok)
			{
				outVec.push_back(sgInfo);
			}
			else
			{
				GameWarning("CRichSaveGameHelper::GetSaveGames: SaveGame '%s' of user '%s' is invalid", fd.name, pEntry->userId.c_str());
			}
		}
		while (fileFinder->FindNext(handle, &fd) >= 0);

		fileFinder->FindClose(handle);
	}

	// temp debug. write out again the bmp files
#ifdef TEST_THUMBNAIL_REWRITE
	for (int i = 0; i < outVec.size(); ++i)
	{
		CPlayerProfileManager::SSaveGameInfo& sgInfo = outVec[i];
		CPlayerProfileManager::SThumbnail img;
		if (GetSaveGameThumbnail(pEntry, sgInfo.name, img))
		{
			string newName = path;
			newName.append("/");
			newName.append(sgInfo.name);
			newName.append("_new.bmp");
			BMPHelper::SaveBMP(newName, (uint8*) img.data.begin(), img.width, img.height, img.depth, true);
		}
	}

#endif

	return true;
}

class CXMLRichSaveGame : public CXmlSaveGame
{
public:
	CXMLRichSaveGame(ICommonProfileImpl* pImpl, CPlayerProfileImplFSDir::SUserEntry* pEntry)
	{
		m_pProfileImpl = pImpl;
		m_pEntry = pEntry;
		m_thumbnailWidth = 0;
		m_thumbnailHeight = 0;
		m_thumbnailDepth = 0;
		assert(m_pProfileImpl != 0);
		assert(m_pEntry != 0);
	}

	// ILoadGame
	virtual bool Init(const char* name)
	{
		assert(m_pEntry->pCurrentProfile != 0);
		if (m_pEntry->pCurrentProfile == 0)
		{
			GameWarning("CXMLRichSaveGame: Entry for user '%s' has no current profile", m_pEntry->userId.c_str());
			return false;
		}

#ifdef TEST_THUMBNAIL_AUTOCAPTURE
		// the image file we write out is always in 16:9 format, e.g. 256x144
		// or scaled depending on renderer height
		const int h = gEnv->pRenderer->GetHeight();
		const int imageDepth = RichSaveGames::THUMBNAIL_DEFAULT_DEPTH;
		int imageHeight = std::min(RichSaveGames::THUMBNAIL_DEFAULT_HEIGHT, h);
		int imageWidth = imageHeight * 16 / 9;
		SetThumbnail(0, imageWidth, imageHeight, imageDepth);
#endif

		string path;
		m_pProfileImpl->InternalMakeFSSaveGamePath(m_pEntry, m_pEntry->pCurrentProfile->GetName(), path, false);
		// make directory or use the SaveXMLFile helper function
		// CryCreateDirectory(...)
		string strippedName = PathUtil::GetFile(name);
		path.append(strippedName);
		return CXmlSaveGame::Init(path.c_str());
	}

	// BGR or BGRA
	virtual uint8* SetThumbnail(const uint8* imageData, int width, int height, int depth)
	{
		m_thumbnailWidth = width;
		m_thumbnailHeight = height;
		m_thumbnailDepth = depth;

		size_t size = width * height * depth;
		m_thumbnailData.resize(size);
		if (imageData)
			memcpy(m_thumbnailData.begin(), imageData, size);
		else
		{
			if (m_thumbnailDepth == 3)
			{
				uint8* p = (uint8*) m_thumbnailData.begin();
				size_t n = size;
				while (n)
				{
					*p++ = 0x00; // B
					*p++ = 0x00; // G
					*p++ = 0x00; // R
					n -= 3;
				}
			}
			else if (m_thumbnailDepth == 4)
			{
				const uint32 col = RGBA8(0x00, 0x00, 0x00, 0x00); // alpha see through
				uint32* p = (uint32*) m_thumbnailData.begin();
				size_t n = size >> 2;
				while (n--)
					*p++ = col;
			}
			else
			{
				memset(m_thumbnailData.begin(), 0, size);
			}
		}
		return m_thumbnailData.begin();
	}

	virtual bool SetThumbnailFromBMP(const char* filename)
	{
		int width = 0;
		int height = 0;
		int depth = 0;
		bool bSuccess = BMPHelper::LoadBMP(filename, 0, width, height, depth, true);
		if (bSuccess)
		{
			CPlayerProfileManager::SThumbnail thumbnail;
			thumbnail.data.resize(width * height * depth);
			bSuccess = BMPHelper::LoadBMP(filename, thumbnail.data.begin(), width, height, depth, true);
			if (bSuccess)
			{
				SetThumbnail(thumbnail.data.begin(), width, height, depth);
			}
		}
		return bSuccess;
	}

	size_t CalcThumbnailSize()
	{
		if (m_thumbnailWidth * m_thumbnailHeight * m_thumbnailDepth == 0)
			return 0;
		const size_t size = BMPHelper::CalcBMPSize(m_thumbnailWidth, m_thumbnailHeight, m_thumbnailDepth);
		return size;
	}

	virtual bool Write(const char* filename, XmlNodeRef data)
	{
		ICryPak* pCryPak = gEnv->pCryPak;
		FILE* pFile = pCryPak->FOpen(filename, "wb");
		if (!pFile)
			return false;

		DWORD thumbnailSize = CalcThumbnailSize();

		const string fname(filename);
		// fill in RSF

		RichSaveGames::RICH_GAME_MEDIA_HEADER savedHeader;
		memset(&savedHeader, 0, sizeof(RichSaveGames::RICH_GAME_MEDIA_HEADER));
		savedHeader.dwMagicNumber = RichSaveGames::RM_MAGICNUMBER;
		savedHeader.dwHeaderVersion = 1;
		savedHeader.dwHeaderSize = sizeof(RichSaveGames::RICH_GAME_MEDIA_HEADER);
		// Change this string to the gameID GUID found in the game's GDF file
		ConvertStringToGUID(CCryAction::GetCryAction()->GetGameGUID(), &savedHeader.guidGameId);
		// Point to the embedded thumbnail (optional)
		// The offset it relative to the end of the RICH_GAME_MEDIA_HEADER structure.
		savedHeader.liThumbnailOffset = 0; // put it right RSF header
		savedHeader.dwThumbnailSize = thumbnailSize;

		CopyToWideString(savedHeader.szSaveName, fname);
		savedHeader.szComments[0] = L'\0';

		CPlayerProfileManager::SSaveGameMetaData metaData;
		if (ExtractMetaDataFromXML(data, metaData))
		{
			CopyToWideString(savedHeader.szGameName, metaData.gameRules);
			CopyToWideString(savedHeader.szLevelName, metaData.levelName);
		}
		else
		{
			savedHeader.szGameName[0] = L'\0';
			savedHeader.szLevelName[0] = L'\0';
		}

		// write out header
		pCryPak->FWrite(&savedHeader, 1, pFile);

		const bool bFlipImage = false;
#ifdef TEST_THUMBNAIL_AUTOCAPTURE
		// debug: get screen shot here
		if (thumbnailSize > 0)
		{
			int w = gEnv->pRenderer->GetWidth();
			int h = gEnv->pRenderer->GetHeight();

			// initialize to stretch thumbnail
			int captureDestWidth = m_thumbnailWidth;
			int captureDestHeight = m_thumbnailHeight;
			int captureDestOffX = 0;
			int captureDestOffY = 0;

			const bool bKeepAspectRatio = RichSaveGames::THUMBNAIL_KEEP_ASPECT_RATIO;

			// should we keep the aspect ratio of the renderer?
			if (bKeepAspectRatio)
			{
				captureDestHeight = m_thumbnailHeight;
				captureDestWidth = captureDestHeight * w / h;

				// adjust for SCOPE formats, like 2.35:1
				if (captureDestWidth > RichSaveGames::THUMBNAIL_DEFAULT_WIDTH)
				{
					captureDestHeight = captureDestHeight * RichSaveGames::THUMBNAIL_DEFAULT_WIDTH / captureDestWidth;
					captureDestWidth = RichSaveGames::THUMBNAIL_DEFAULT_WIDTH;
				}

				captureDestOffX = (m_thumbnailWidth - captureDestWidth) * 0.5f;
				captureDestOffY = (m_thumbnailHeight - captureDestHeight) * 0.5f;

				// CryLogAlways("CXMLRichSaveGame: TEST_THUMBNAIL_AUTOCAPTURE: capWidth=%d capHeight=%d (off=%d,%d) thmbw=%d thmbh=%d rw=%d rh=%d",
				//	captureDestWidth, captureDestHeight, captureDestOffX, captureDestOffY, m_thumbnailWidth, m_thumbnailHeight, w,h);

				if (captureDestWidth > m_thumbnailWidth || captureDestHeight > m_thumbnailHeight)
				{
					assert(false);
					GameWarning("CXMLRichSaveGame: TEST_THUMBNAIL_AUTOCAPTURE: capWidth=%d capHeight=%d", captureDestWidth, captureDestHeight);
					captureDestHeight = m_thumbnailHeight;
					captureDestWidth = m_thumbnailWidth;
					captureDestOffX = captureDestOffY = 0;
				}
			}

			const bool bAlpha = m_thumbnailDepth == 4;
			const int bpl = m_thumbnailWidth * m_thumbnailDepth;
			uint32* pBuf = static_cast<uint32*>(m_thumbnailData.begin() + captureDestOffY * bpl + captureDestOffX * m_thumbnailDepth);
			gEnv->pRenderer->ReadFrameBuffer(pBuf, captureDestWidth, captureDestHeight); // no inverse needed
			// gEnv->pRenderer->ReadFrameBuffer(pBuf, m_thumbnailWidth, m_thumbnailHeight); // needs inverse
			// bFlipImage = true;
		}
#endif

#ifdef TEST_THUMBNAIL_WRITE
		// write thumbnail also separately for debugging
		// write out thumbnail
		if (thumbnailSize > 0)
		{
			string imgName = PathUtil::ReplaceExtension(fname, ".bmp");
			BMPHelper::SaveBMP(imgName, m_thumbnailData.begin(), m_thumbnailWidth, m_thumbnailHeight, m_thumbnailDepth, bFlipImage);
		}
#endif

		// write out thumbnail into savegame
		if (thumbnailSize > 0)
		{
			BMPHelper::SaveBMP(pFile, m_thumbnailData.begin(), m_thumbnailWidth, m_thumbnailHeight, m_thumbnailDepth, bFlipImage);
		}

		WriteXMLNode(RichSaveGames::RM_METADATA_TAG, data->findChild("Metadata"), pFile, false);

		string debugFilename;
		if (CPlayerProfileManager::sRSFDebugWrite != 0)
			debugFilename = PathUtil::ReplaceExtension(fname, ".xml");

#if defined RSF_USE_COMPRESSION
		// try to write compressed
		const bool bOK = WriteXMLNode(RichSaveGames::RM_COMPRESSED_SAVEDATA_TAG, data, pFile, true, debugFilename);
		if (bOK == false) // try to write uncompressed
		{
			WriteXMLNode(RichSaveGames::RM_SAVEDATA_TAG, data, pFile, false, debugFilename);
		}
#else
		WriteXMLNode(RichSaveGames::RM_SAVEDATA_TAG, data, pFile, false, debugFilename);
#endif

		pCryPak->FClose(pFile);

		return true;
	}

	ICommonProfileImpl*                  m_pProfileImpl;
	CPlayerProfileImplFSDir::SUserEntry* m_pEntry;
	DynArray<uint8>                      m_thumbnailData;
	int m_thumbnailWidth;
	int m_thumbnailHeight;
	int m_thumbnailDepth;

};

ISaveGame* CRichSaveGameHelper::CreateSaveGame(CPlayerProfileManager::SUserEntry* pEntry)
{
	return new CXMLRichSaveGame(m_pImpl, pEntry);
}

class CXMLRichLoadGame : public CXmlLoadGame
{
public:
	CXMLRichLoadGame(ICommonProfileImpl* pImpl, CPlayerProfileImplFSDir::SUserEntry* pEntry)
	{
		m_pImpl = pImpl;
		m_pEntry = pEntry;
		assert(m_pImpl != 0);
		assert(m_pEntry != 0);
	}

	// ILoadGame
	virtual bool Init(const char* name)
	{
		assert(m_pEntry->pCurrentProfile != 0);
		if (m_pEntry->pCurrentProfile == 0)
		{
			GameWarning("CXMLRichLoadGame: Entry for user '%s' has no current profile", m_pEntry->userId.c_str());
			return false;
		}

		string filename;
		// figure out, if 'name' is an absolute path or a profile-relative path
		if (gEnv->pCryPak->IsAbsPath(name) == false)
		{
			// no full path, assume 'name' is local to profile directory
			bool bNeedFolder = true;
			if (m_pImpl->GetManager()->IsSaveGameFolderShared())
			{
				// if the savegame's name doesn't start with a profile_ prefix
				// add one (for quickload)
				string profilePrefix = m_pEntry->pCurrentProfile->GetName();
				profilePrefix.append("_");
				size_t profilePrefixLen = profilePrefix.length();
				if (strnicmp(name, profilePrefix, profilePrefixLen) != 0)
					bNeedFolder = false;
			}
			m_pImpl->InternalMakeFSSaveGamePath(m_pEntry, m_pEntry->pCurrentProfile->GetName(), filename, bNeedFolder);
			string strippedName = PathUtil::GetFile(name);
			filename.append(strippedName);
		}
		else
		{
			// it's an abs path, assign it
			filename.assign(name);
		}

		ICryPak* pCryPak = gEnv->pCryPak;
		FILE* pFile = pCryPak->FOpen(filename, "rbx"); // x=don't chache full file
		if (!pFile)
			return false;

		RichSaveGames::RICH_GAME_MEDIA_HEADER savedHeader;
		if (ReadRichGameMediaHeader(filename.c_str(), pFile, savedHeader) == false)
		{
			GameWarning("CXMLRichLoadGame:GetSaveGameThumbnail: Can't read rich game media header from file '%s'", filename.c_str());
			return false;
		}

		// for now, skip thumbnails
		int64 thumbNailOffset = savedHeader.liThumbnailOffset;
		DWORD thumbNailSize = savedHeader.dwThumbnailSize;
		if (thumbNailOffset > 0)
		{
			pCryPak->FSeek(pFile, (int)thumbNailOffset, SEEK_CUR);
		}
		if (thumbNailSize > 0)
		{
			pCryPak->FSeek(pFile, thumbNailSize, SEEK_CUR);
		}

		bool bSuccess = SkipXMLTagData(RichSaveGames::RM_METADATA_TAG, pFile);
		if (bSuccess == false)
		{
			GameWarning("CXMLRichLoadGame: Cannot read metadata for file '%s'.", filename.c_str());
			pCryPak->FClose(pFile);
			return false;
		}

		uint32 dataTag = 0;
		const bool bOK = ReadTag(pFile, dataTag, true);
		if (!bOK)
		{
			GameWarning("CXMLRichLoadGame: Cannot read data tag for file '%s'.", filename.c_str());
			pCryPak->FClose(pFile);
			return false;
		}

		char* dataBuf = 0;
		if (dataTag == RichSaveGames::RM_SAVEDATA_TAG)
		{
			dataBuf = ReadXMLTagData(RichSaveGames::RM_SAVEDATA_TAG, pFile, false);
		}
		else if (dataTag == RichSaveGames::RM_COMPRESSED_SAVEDATA_TAG)
		{
			dataBuf = ReadXMLTagData(RichSaveGames::RM_COMPRESSED_SAVEDATA_TAG, pFile, true);
		}
		else
		{
			const string tagString = tagToString(dataTag);
			GameWarning("CXMLRichLoadGame: Unknown data tag'%s'.", tagString.c_str());
		}

		if (dataBuf == 0)
		{
			GameWarning("CXMLRichLoadGame: Cannot read data for file '%s'.", filename.c_str());
			pCryPak->FClose(pFile);
			return false;
		}

		// write out
		if (CPlayerProfileManager::sRSFDebugWriteOnLoad)
		{
			string outUncompress = filename;
			PathUtil::RemoveExtension(outUncompress);
			outUncompress.append("_uncompressed.xml");
			FILE* const pUnFile = pCryPak->FOpen(outUncompress.c_str(), "wb");
			if (pUnFile)
			{
				pCryPak->FWrite(dataBuf, strlen(dataBuf), pUnFile);
				pCryPak->FClose(pUnFile);
			}
		}

		// parse the file
		XmlNodeRef xmlRootNode = gEnv->pSystem->LoadXmlFromBuffer(dataBuf, strlen(dataBuf));
		if (xmlRootNode == 0)
		{
			GameWarning("CXMLRichLoadGame: Cannot parse XML Data in '%s'", filename.c_str());
			delete[] dataBuf;
			pCryPak->FClose(pFile);
			return false;
		}
		delete[] dataBuf;
		pCryPak->FClose(pFile);
		return CXmlLoadGame::Init(xmlRootNode, name);
	}

	ICommonProfileImpl*                  m_pImpl;
	CPlayerProfileImplFSDir::SUserEntry* m_pEntry;

};

ILoadGame* CRichSaveGameHelper::CreateLoadGame(CPlayerProfileManager::SUserEntry* pEntry)
{
	return new CXMLRichLoadGame(m_pImpl, pEntry);
}

bool CRichSaveGameHelper::DeleteSaveGame(CPlayerProfileManager::SUserEntry* pEntry, const char* name)
{
	string filename;
	m_pImpl->InternalMakeFSSaveGamePath(pEntry, pEntry->pCurrentProfile->GetName(), filename, true);
	string strippedName = PathUtil::GetFile(name);
	filename.append(strippedName);
	bool bOK = gEnv->pCryPak->RemoveFile(filename.c_str());
	if (bOK && CPlayerProfileManager::sRSFDebugWrite != 0)
	{
		// remove the debug .xml as well
		filename = PathUtil::ReplaceExtension(filename, ".xml");
		gEnv->pCryPak->RemoveFile(filename.c_str());
	}
	return bOK;
}

bool CRichSaveGameHelper::MoveSaveGames(CPlayerProfileManager::SUserEntry* pEntry, const char* oldProfileName, const char* newProfileName)
{
	// move savegames or, if savegame folder is shared, rename them
	string oldSaveGamesPath;
	string newSaveGamesPath;
	m_pImpl->InternalMakeFSSaveGamePath(pEntry, oldProfileName, oldSaveGamesPath, true);
	m_pImpl->InternalMakeFSSaveGamePath(pEntry, newProfileName, newSaveGamesPath, true);

	CPlayerProfileManager* pMgr = m_pImpl->GetManager();
	if (pMgr->IsSaveGameFolderShared() == false)
	{
		// move complete folder
		pMgr->MoveFileHelper(oldSaveGamesPath, newSaveGamesPath);
	}
	else
	{
		// save game folder is shared, move file by file
		CPlayerProfileManager::TSaveGameInfoVec saveGameInfoVec;
		if (GetSaveGames(pEntry, saveGameInfoVec, oldProfileName))
		{
			CPlayerProfileManager::TSaveGameInfoVec::iterator iter = saveGameInfoVec.begin();
			CPlayerProfileManager::TSaveGameInfoVec::iterator iterEnd = saveGameInfoVec.end();
			string oldPrefix = oldProfileName;
			oldPrefix += "_";
			size_t oldPrefixLen = oldPrefix.length();
			string newPrefix = newProfileName;
			newPrefix += "_";
			while (iter != iterEnd)
			{
				const string& oldSGName = iter->name;
				// begins with old profile's prefix?
				if (strnicmp(oldSGName, oldPrefix, oldPrefixLen) == 0)
				{
					string newSGName = newPrefix;
					newSGName.append(oldSGName, oldPrefixLen, oldSGName.length() - oldPrefixLen);
					string oldPath = oldSaveGamesPath + oldSGName;
					string newPath = newSaveGamesPath + newSGName;
					pMgr->MoveFileHelper(oldPath, newPath); // savegame

					if (CPlayerProfileManager::sRSFDebugWrite != 0)
					{
						// in case we wrote some debug savegames, remove it as well
						oldPath = PathUtil::ReplaceExtension(oldPath, ".xml");
						newPath = PathUtil::ReplaceExtension(newPath, ".xml");
						pMgr->MoveFileHelper(oldPath, newPath); // debug xml file
					}
				}
				++iter;
			}
		}
	}
	return true;
}

bool CRichSaveGameHelper::GetSaveGameThumbnail(CPlayerProfileManager::SUserEntry* pEntry, const char* saveGameName, CPlayerProfileManager::SThumbnail& thumbnail)
{
	assert(pEntry->pCurrentProfile != 0);
	if (pEntry->pCurrentProfile == 0)
	{
		GameWarning("CXMLRichLoadGame:GetSaveGameThumbnail: Entry for user '%s' has no current profile", pEntry->userId.c_str());
		return false;
	}
	const char* name = saveGameName;
	string filename;
	m_pImpl->InternalMakeFSSaveGamePath(pEntry, pEntry->pCurrentProfile->GetName(), filename, true);
	string strippedName = PathUtil::GetFile(name);
	filename.append(strippedName);

	ICryPak* pCryPak = gEnv->pCryPak;
	FILE* pFile = pCryPak->FOpen(filename, "rbx"); // x=don't chache full file
	if (!pFile)
		return false;

	RichSaveGames::RICH_GAME_MEDIA_HEADER savedHeader;
	if (ReadRichGameMediaHeader(filename.c_str(), pFile, savedHeader) == false)
	{
		GameWarning("CXMLRichLoadGame:GetSaveGameThumbnail: Can't read rich game media header from file '%s'", filename.c_str());
		return false;
	}

	bool bSuccess = false;
	int64 thumbNailOffset = savedHeader.liThumbnailOffset;
	DWORD thumbNailSize = savedHeader.dwThumbnailSize;
	if (thumbNailOffset > 0)
	{
		pCryPak->FSeek(pFile, (int)thumbNailOffset, SEEK_CUR);
	}
	if (thumbNailSize > 0)
	{
		int width = 0;
		int height = 0;
		int depth = 0;
		bSuccess = BMPHelper::LoadBMP(pFile, 0, width, height, depth);
		if (bSuccess)
		{
			thumbnail.data.resize(width * height * depth);
			bSuccess = BMPHelper::LoadBMP(pFile, thumbnail.data.begin(), width, height, depth);
			if (bSuccess)
			{
				thumbnail.height = height;
				thumbnail.width = width;
				thumbnail.depth = depth;
			}
		}
	}
	pCryPak->FClose(pFile);
	if (!bSuccess)
		thumbnail.ReleaseData();
	return bSuccess;
}
