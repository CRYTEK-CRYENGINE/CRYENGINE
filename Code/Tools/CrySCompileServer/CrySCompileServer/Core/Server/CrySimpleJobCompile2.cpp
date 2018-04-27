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
#include "CrySimpleJobCompile2.hpp"


CCrySimpleJobCompile2::CCrySimpleJobCompile2(uint32_t requestIP, std::vector<uint8_t>* pRVec):
CCrySimpleJobCompile(requestIP, EPV_V002, pRVec)
{
}

size_t CCrySimpleJobCompile2::SizeOf(std::vector<uint8_t>& rVec)
{
	const char* pXML		=	reinterpret_cast<const char*>(&rVec[0]);
	const char* pFirst	=	strstr(pXML,"HashStop");
	return pFirst?pFirst-pXML:rVec.size();
}
