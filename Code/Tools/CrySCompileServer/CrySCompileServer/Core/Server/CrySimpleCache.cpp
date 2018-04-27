// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "../StdTypes.hpp"
#include "../Error.hpp"
#include "../STLHelper.hpp"
#include "CrySimpleCache.hpp"
#include "CrySimpleServer.hpp"

enum EFileEntryHeaderFlags
{
	EFEHF_NONE					=	(0<<0),
	EFEHF_REFERENCE		=	(1<<0),
};

#pragma pack(push, 1)
struct SFileEntryHeader
{
	char     signature[4]; // entry signature.
	uint32_t dataSize;  // Size of entry data.
	uint32_t flags;     // Flags
	uint8_t  hash[16];  // Hash code for the data.
};
#pragma pack(pop)


CCrySimpleCache& CCrySimpleCache::Instance()
{
	static CCrySimpleCache g_Cache;
	return g_Cache;
}

void CCrySimpleCache::Init()
{
	CCrySimpleMutexAutoLock Lock(m_Mutex);
	m_CachingEnabled	=	false;
	m_Hit		=	0;
	m_Miss	=	0;
}

std::string CCrySimpleCache::CreateFileName(const tdHash& rHash)const
{
	std::string Name;
	Name	=	CSTLHelper::Hash2String(rHash);
	char Tmp[4]="012";
	Tmp[0]	=	Name.c_str()[0];
	Tmp[1]	=	Name.c_str()[1];
	Tmp[2]	=	Name.c_str()[2];

	return SEnviropment::Instance().m_Cache+Tmp+"/"+Name;
}


bool CCrySimpleCache::Find(const tdHash& rHash,tdDataVector& rData)
{
	if(!m_CachingEnabled)
		return false;

	CCrySimpleMutexAutoLock Lock(m_Mutex);
	tdEntries::iterator it = m_Entries.find(rHash);
	if(it!=m_Entries.end())
	{
    tdData::iterator dataIt = m_Data.find(it->second);
    if(dataIt==m_Data.end())
    {
      m_Miss++;
      return false;
    }
		m_Hit++;
		rData = dataIt->second;
		return true;

		//if (CSTLHelper::Uncompress( it->second,rData ))
			//return true;

//		const std::string FileName=CreateFileName(rHash);
//		CSTLHelper::FromFile(FileName,rData);
	}
	m_Miss++;
	return false;
}

void CCrySimpleCache::Add(const tdHash& rHash,const tdDataVector& rData)
{
	if(!m_CachingEnabled)
		return;
	if (rData.size() > 0)
	{
		SFileEntryHeader hdr;
		memcpy(hdr.signature,"SHDR",4);
		hdr.dataSize = (uint32_t)rData.size();
		hdr.flags = EFEHF_NONE;
		memcpy(hdr.hash,&rHash,sizeof(hdr.hash));
		const uint8_t* pData	=	&rData[0];

		tdHash DataHash	=	CSTLHelper::Hash(rData);
		{
			CCrySimpleMutexAutoLock Lock(m_Mutex);
      m_Entries[rHash]	=	DataHash;
			if(m_Data.find(DataHash)==m_Data.end())
			{
				m_Data[DataHash]	=	rData;
			}
			else
			{
				hdr.flags |= EFEHF_REFERENCE;
				hdr.dataSize	=	sizeof(tdHash);
				pData	=	reinterpret_cast<const uint8_t*>(&DataHash);
			}
		}



		tdDataVector buf;
		buf.resize( sizeof(hdr) + hdr.dataSize );
		memcpy( &buf[0],&hdr,sizeof(hdr) );
		memcpy( &buf[sizeof(hdr)],pData,hdr.dataSize );

		tdDataVector *pPendingCacheEntry = new tdDataVector(buf);
		{
			CCrySimpleMutexAutoLock LockFile(m_FileMutex);
			m_PendingCacheEntries.push_back( pPendingCacheEntry );
			if (m_PendingCacheEntries.size() > 10000)
			{
				printf( "Warning: Too many pending entries not saved to disk!!!" );
			}
			//CSTLHelper::AppendToFile( SEnviropment::Instance().m_Cache+"Cache.dat",buf );
		}
	}
}


//////////////////////////////////////////////////////////////////////////
bool CCrySimpleCache::LoadCacheFile( const std::string &filename )
{
	DWORD t0 = GetTickCount();

	printf("Loading shader cache from %s\n",filename.c_str() );

	tdDataVector rData;

	tdHash hash;

	bool bLoadedOK = true;

	uint32_t Loaded=0;
	uint32_t num = 0;

	uint64_t nFileSize = 0;
	uint64_t nFilePos = 0;
	uint64_t nFilePos2 = 0;

	//////////////////////////////////////////////////////////////////////////
	FILE* file	=	fopen(filename.c_str(),"rb");
	if (!file)
	{
		return false;
	}
	_fseeki64(file,0,SEEK_END);
	nFileSize = _ftelli64(file);
	_fseeki64(file,0,SEEK_SET);

	uint64_t	SizeAdded=0,SizeAddedCount=0;
	uint64_t	SizeSaved=0,SizeSavedCount=0;
	
	while (!feof(file) && nFilePos < nFileSize)
	{
		SFileEntryHeader hdr;
		if (1 != fread( &hdr,sizeof(hdr),1,file ))
		{
			break;
		}

		if (memcmp(hdr.signature,"SHDR",4) != 0)
		{
			// Bad Entry!
			bLoadedOK = false;
			printf( "\nSkipping Invalid cache entry %d\n at file position: %I64u ",num,nFilePos );
			break;
		}

		if (hdr.dataSize > 1024*1024 || hdr.dataSize == 0)
		{
			// Too big entry, probably invalid.
			bLoadedOK = false;
			printf( "\nSkipping Invalid cache entry %d\n at file position: %I64u ",num,nFilePos );
			break;
		}

		rData.resize(hdr.dataSize);
		if (1 != fread( &rData[0],hdr.dataSize,1,file ))
		{
			break;
		}
		memcpy( &hash,hdr.hash,sizeof(hdr.hash) );

		if(hdr.flags&EFEHF_REFERENCE)
		{
			if(hdr.dataSize!=sizeof(tdHash))
			{
				// Too big entry, probably invalid.
				bLoadedOK = false;
				printf( "\nSkipping Invalid cache entry %d\n at file position: %I64u, was flagged as cache reference but size was %d",num,nFilePos,hdr.dataSize );
				break;
			}

      bool bSkip = false;

			tdHash DataHash	=	*reinterpret_cast<tdHash*>(&rData[0]);
			tdData::iterator it=m_Data.find(DataHash);
			if(it==m_Data.end())
			{
				// Too big entry, probably invalid.
        bSkip = true; // don't abort reading whole file just yet - skip only this entry
				printf( "\nSkipping Invalid cache entry %d\n at file position: %I64u, data-hash references to not existing data ",num,nFilePos,hdr.dataSize );
			}

      if(!bSkip)
      {
        m_Entries[hash]	=	DataHash;
        SizeSaved+=it->second.size();
        SizeSavedCount++;
      }
		}
		else
		{
			tdHash DataHash	=	CSTLHelper::Hash(rData);
			m_Entries[hash]	=	DataHash;
			if(m_Data.find(DataHash)==m_Data.end())
			{
				SizeAdded+=rData.size();
				m_Data[DataHash]	=	rData;
				SizeAddedCount++;
			}
			else
			{
				SizeSaved+=rData.size();
				SizeSavedCount++;
			}
		}

		if (num%1000 == 0)
		{
			DWORD t = GetTickCount();

			Loaded	=	static_cast<uint32_t>(nFilePos*100/nFileSize);
			printf("\rLoad:%3d%% %6dk t=%ds Compress: (Count)%d%% %dk:%dk (MB)%d%% %dMB:%dMB",Loaded,static_cast<uint32_t>(num/1000),(t-t0)/1000,
																																												SizeAddedCount/max((SizeAddedCount+SizeSavedCount)/100,1),
																																												SizeAddedCount/1000,SizeSavedCount/1000,
																																												SizeAdded/max((SizeAdded+SizeSaved)/100,1),
																																												SizeAdded/(1024*1024),SizeSaved/(1024*1024));
		}

		num++;
		nFilePos += hdr.dataSize + sizeof(SFileEntryHeader);
		//nFilePos2 = _ftelli64(file);

	}
	fclose(file);

	printf("\n%d shaders loaded from cache\n",num );

	return bLoadedOK;
}

void CCrySimpleCache::Finalize()
{
	m_CachingEnabled	=	true;
	printf("\n caching enabled\n" );
}

//////////////////////////////////////////////////////////////////////////
void CCrySimpleCache::ThreadFunc_SavePendingCacheEntries()
{
	// Check pending entries and save them to disk.
	bool bListEmpty = false;
	do
	{
		tdDataVector *pPendingCacheEntry = 0;

		{
			CCrySimpleMutexAutoLock LockFile(m_FileMutex);
			if (!m_PendingCacheEntries.empty())
			{
				pPendingCacheEntry = m_PendingCacheEntries.front();
				m_PendingCacheEntries.pop_front();
			}
			//CSTLHelper::AppendToFile( SEnviropment::Instance().m_Cache+"Cache.dat",buf );
			bListEmpty = m_PendingCacheEntries.empty();
		}

		if (pPendingCacheEntry)
		{
			CSTLHelper::AppendToFile( SEnviropment::Instance().m_Cache+"Cache.dat",*pPendingCacheEntry );
			delete pPendingCacheEntry;
		}
	} while (!bListEmpty);
}
