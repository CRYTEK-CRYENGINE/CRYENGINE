// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "../StdTypes.hpp"
#include "../Error.hpp"
#include "../STLHelper.hpp"
#include "../Common.h"
#include "../tinyxml/tinyxml.h"
#include "CrySimpleJobCache.hpp"
#include "CrySimpleCache.hpp"

CCrySimpleJobCache::CCrySimpleJobCache(uint32_t requestIP):
CCrySimpleJob(requestIP)
{

}

void CCrySimpleJobCache::CheckHashID(std::vector<uint8_t>& rVec,size_t Size)
{
	m_HashID = CSTLHelper::Hash(rVec,Size);

	if(CCrySimpleCache::Instance().Find(m_HashID,rVec))
	{
		State(ECSJS_CACHEHIT);
		logmessage("\r"); // Just update cache hit number
	}
}
