// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include <CryCore/Assert/CryAssert_impl.h>
#include "ICryXML.h"
#include "XMLSerializer.h"

class CryXML : public ICryXML
{
public:
	CryXML();
	virtual void AddRef();
	virtual void Release();
	virtual IXMLSerializer* GetXMLSerializer();

private:
	int nRefCount;
	XMLSerializer serializer;
};

static CryXML* s_pCryXML = 0;

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	return TRUE;
}

ICryXML* __stdcall GetICryXML()
{
	if (s_pCryXML == 0)
		s_pCryXML = new CryXML;
	return s_pCryXML;
}

CryXML::CryXML()
:	nRefCount(0)
{
}

void CryXML::AddRef()
{
	++this->nRefCount;
}

void CryXML::Release()
{
	--this->nRefCount;
	if (this->nRefCount == 0)
		delete this;
}

IXMLSerializer* CryXML::GetXMLSerializer()
{
	return &this->serializer;
}

