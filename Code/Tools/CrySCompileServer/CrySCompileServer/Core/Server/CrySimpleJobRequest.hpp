// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYSIMPLEJOBREQUEST__
#define __CRYSIMPLEJOBREQUEST__

#include "CrySimpleJob.hpp"


class CCrySimpleJobRequest	:	public	CCrySimpleJob
{
public:
											CCrySimpleJobRequest(uint32_t requestIP);

	virtual bool					Execute(const TiXmlElement* pElement);		
};

#endif
