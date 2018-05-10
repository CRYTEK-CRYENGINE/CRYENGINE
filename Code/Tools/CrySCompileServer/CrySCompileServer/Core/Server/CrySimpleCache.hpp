// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYSIMPLECACHE__
#define __CRYSIMPLECACHE__

#include <map>
#include <vector>
#include <list>
#include "CrySimpleMutex.hpp"
#include "../STLHelper.hpp"

/*class CCrySimpleCacheEntry
{
	tdCache
public:
protected:
private:
};*/

typedef std::map<tdHash,tdHash>				tdEntries;
typedef std::map<tdHash,tdDataVector> tdData;

class CCrySimpleCache
{
	volatile bool								m_CachingEnabled;
	int													m_Hit;
	int													m_Miss;
	tdEntries										m_Entries;
	tdData											m_Data;
	CCrySimpleMutex							m_Mutex;
	CCrySimpleMutex							m_FileMutex;
	
	std::list<tdDataVector*>		m_PendingCacheEntries;
	std::string									CreateFileName(const tdHash& rHash)const;

public:
	void												Init();
	bool												Find(const tdHash& rHash,tdDataVector& rData);
	void												Add(const tdHash& rHash,const tdDataVector& rData);

	bool												LoadCacheFile( const std::string &filename );
	void												Finalize();

	void                        ThreadFunc_SavePendingCacheEntries();

	static CCrySimpleCache&			Instance();


	std::list<tdDataVector*>&		PendingCacheEntries(){return m_PendingCacheEntries;}
	int													Hit()const{return m_Hit;}
	int													Miss()const{return m_Miss;}
	int													EntryCount()const{return static_cast<int>(m_Entries.size());}
};

#endif
