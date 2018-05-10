// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYSIMPLEJOBCACHE__
#define __CRYSIMPLEJOBCACHE__

#include "CrySimpleJob.hpp"


class CCrySimpleJobCache	:	public	CCrySimpleJob
{
	tdHash						m_HashID;

protected:

	void							CheckHashID(std::vector<uint8_t>& rVec,size_t Size);

public:
										CCrySimpleJobCache(uint32_t requestIP);

	tdHash						HashID()const{return m_HashID;}
};

#endif
