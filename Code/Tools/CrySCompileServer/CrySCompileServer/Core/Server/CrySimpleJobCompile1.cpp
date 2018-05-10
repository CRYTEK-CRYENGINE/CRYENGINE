// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "../StdTypes.hpp"
#ifdef _MSC_VER
#include <process.h>
#include <direct.h>
#endif
#ifdef UNIX
#include <pthread.h>
#endif
#include "../Error.hpp"
#include "../STLHelper.hpp"
#include "../tinyxml/tinyxml.h"
#include "CrySimpleSock.hpp"
#include "CrySimpleJobCompile1.hpp"


CCrySimpleJobCompile1::CCrySimpleJobCompile1(uint32_t requestIP, std::vector<uint8_t>* pRVec):
CCrySimpleJobCompile(requestIP, EPV_V001, pRVec)
{
}

size_t CCrySimpleJobCompile1::SizeOf(std::vector<uint8_t>& rVec)
{
	return rVec.size();
}
