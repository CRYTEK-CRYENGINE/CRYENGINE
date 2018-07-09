// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "../StdTypes.hpp"
#include "../Error.hpp"
#include "../STLHelper.hpp"
#include "../Common.h"
#include "../tinyxml/tinyxml.h"
#include "CrySimpleJobRequest.hpp"
#include "ShaderList.hpp"

CCrySimpleJobRequest::CCrySimpleJobRequest(uint32_t requestIP):
CCrySimpleJob(requestIP)
{
	
}

bool CCrySimpleJobRequest::Execute(const TiXmlElement* pElement)
{
	const char* pPlatform						= pElement->Attribute("Platform");
	const char* pShaderRequestLine	=	pElement->Attribute("ShaderRequest");
	if(!pPlatform)
	{
		State(ECSJS_ERROR_INVALID_PLATFORM);
		CrySimple_ERROR("Missing Platform for shader request");
		return false;
	}
	if(!pShaderRequestLine)
	{
		State(ECSJS_ERROR_INVALID_SHADERREQUESTLINE);
		CrySimple_ERROR("Missing shader request line");
		return false;
	}

	std::string ShaderRequestLine(pShaderRequestLine);
	tdEntryVec Toks;
	CSTLHelper::Tokenize(Toks,ShaderRequestLine,";");
	for(size_t a=0,S=Toks.size();a<S;a++)
		CShaderList::Instance().Add(pPlatform,Toks[a].c_str());

	//	CShaderList::Instance().Add(pPlatform,pShaderRequestLine );
	State(ECSJS_DONE);

	return true;
}
