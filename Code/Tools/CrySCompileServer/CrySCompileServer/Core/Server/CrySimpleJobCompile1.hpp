// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYSIMPLEJOBCOMPILE1__
#define __CRYSIMPLEJOBCOMPILE1__

#include "CrySimpleJobCompile.hpp"


class CCrySimpleJobCompile1	:	public	CCrySimpleJobCompile
{

	virtual size_t		SizeOf(std::vector<uint8_t>& rVec);

public:
										CCrySimpleJobCompile1(uint32_t requestIP, std::vector<uint8_t>* pRVec);
};

#endif
