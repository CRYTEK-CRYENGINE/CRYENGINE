// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYSIMPLEJOBCOMPILE2__
#define __CRYSIMPLEJOBCOMPILE2__

#include "CrySimpleJobCompile.hpp"


class CCrySimpleJobCompile2	:	public	CCrySimpleJobCompile
{

	virtual size_t		SizeOf(std::vector<uint8_t>& rVec);

public:
										CCrySimpleJobCompile2(uint32_t requestIP, std::vector<uint8_t>* pRVec);
};

#endif
