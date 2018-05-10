// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//#define EXTENSION_SYSTEM_INCLUDE_TESTCASES

#ifdef EXTENSION_SYSTEM_INCLUDE_TESTCASES

	#include <CryExtension/ICryUnknown.h>

struct ICryFactoryRegistryImpl;

void TestExtensions(ICryFactoryRegistryImpl* pReg);

struct IFoobar : public ICryUnknown
{
	CRYINTERFACE_DECLARE_GUID(IFoobar, "539e9c67-2cad-4a03-9ecd-8069c99a846b"_cry_guid);

	virtual void Foo() = 0;
};

DECLARE_SHARED_POINTERS(IFoobar);

struct IRaboof : public ICryUnknown
{
	CRYINTERFACE_DECLARE_GUID(IRaboof, "135ca25e-634b-4d13-9e44-67968a708822"_cry_guid);

	virtual void Rab() = 0;
};

DECLARE_SHARED_POINTERS(IRaboof);

struct IA : public ICryUnknown
{
	CRYINTERFACE_DECLARE_GUID(IA, "d93aaceb-35ec-427e-b64b-f8dec4997e67"_cry_guid);

	virtual void A() = 0;
};

DECLARE_SHARED_POINTERS(IA);

struct IB : public ICryUnknown
{
	CRYINTERFACE_DECLARE_GUID(IB, "e0d830c8-2642-4e11-9eac-fa19eaf31ffb"_cry_guid);

	virtual void B() = 0;
};

DECLARE_SHARED_POINTERS(IB);

struct IC : public ICryUnknown
{
	CRYINTERFACE_DECLARE_GUID(IC, "577509a2-0fc5-477c-8937-57c9ca88b27b"_cry_guid);

	virtual void C() = 0;
};

DECLARE_SHARED_POINTERS(IC);

struct ICustomC : public IC
{
	CRYINTERFACE_DECLARE_GUID(ICustomC, "2ac769da-4c74-43bf-8091-1033e21dfbcf"_cry_guid);

	virtual void C1() = 0;
};

DECLARE_SHARED_POINTERS(ICustomC);

//////////////////////////////////////////////////////////////////////////
// use of extension system without any of the helper macros/templates

struct IDontLikeMacros : public ICryUnknown
{
	template<class T> friend constexpr CryInterfaceID InterfaceCastSemantics::cryiidof();
protected:
	virtual ~IDontLikeMacros() {}

private:
	// It's very important that this static function is implemented for each interface!
	// Otherwise the consistency of cryinterface_cast<T>() is compromised because
	// cryiidof<T>() = cryiidof<baseof<T>>() {baseof<T> = ICryUnknown in most cases}
	static constexpr CryInterfaceID IID()
	{
		return "0f43b7e3-f136-4af0-b4a1-6a975bea3ec4"_cry_guid;
	}

public:
	virtual void CallMe() = 0;
};

DECLARE_SHARED_POINTERS(IDontLikeMacros);

#endif // #ifdef EXTENSION_SYSTEM_INCLUDE_TESTCASES
