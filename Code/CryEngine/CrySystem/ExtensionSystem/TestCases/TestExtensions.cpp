// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TestExtensions.h"

#ifdef EXTENSION_SYSTEM_INCLUDE_TESTCASES

	#include <CryExtension/ClassWeaver.h>
	#include <CryExtension/ICryFactoryRegistryImpl.h>
	#include <CryExtension/CryCreateClassInstance.h>

//////////////////////////////////////////////////////////////////////////

namespace TestComposition
{
struct ITestExt1 : public ICryUnknown
{
	CRYINTERFACE_DECLARE_GUID(ITestExt1, "9d9e0dcf-a576-4cb0-a737-01595f75bd32"_cry_guid);

	virtual void Call1() const = 0;
};

DECLARE_SHARED_POINTERS(ITestExt1);

class CTestExt1 : public ITestExt1
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(ITestExt1)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CTestExt1, "TestExt1", "43b04e7c-c1be-45ca-9df6-ccb1c0dc1ad8"_cry_guid)

public:
	virtual void Call1() const;

private:
	int i;
};

CRYREGISTER_CLASS(CTestExt1)

CTestExt1::CTestExt1()
{
	i = 1;
}

CTestExt1::~CTestExt1()
{
	printf("Inside CTestExt1 dtor\n");
}

void CTestExt1::Call1() const
{
	printf("Inside CTestExt1::Call1()\n");
}

//////////////////////////////////////////////////////////////////////////

struct ITestExt2 : public ICryUnknown
{
	CRYINTERFACE_DECLARE_GUID(ITestExt2, "8eb7a4b3-9987-4b9c-b96b-d6da7a8c72f9"_cry_guid);

	virtual void Call2() = 0;
};

DECLARE_SHARED_POINTERS(ITestExt2);

class CTestExt2 : public ITestExt2
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(ITestExt2)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CTestExt2, "TestExt2", "25b3ebf8-f175-4b9a-b549-4e3da7cdd80f"_cry_guid);

public:
	virtual void Call2();

private:
	int i;
};

CRYREGISTER_CLASS(CTestExt2)

CTestExt2::CTestExt2()
{
	i = 2;
}

CTestExt2::~CTestExt2()
{
	printf("Inside CTestExt2 dtor\n");
}

void CTestExt2::Call2()
{
	printf("Inside CTestExt2::Call2()\n");
}

//////////////////////////////////////////////////////////////////////////

class CComposed : public ICryUnknown
{
	CRYGENERATE_CLASS_GUID(CComposed, "Composed", "0439d74b-8dcd-4b7f-9287-dcdf7e26a3a5"_cry_guid)

	CRYCOMPOSITE_BEGIN()
	CRYCOMPOSITE_ADD(m_pTestExt1, "Ext1")
	CRYCOMPOSITE_ADD(m_pTestExt2, "Ext2")
	CRYCOMPOSITE_END(CComposed)

	CRYINTERFACE_BEGIN()
	CRYINTERFACE_END()

private:
	ITestExt1Ptr m_pTestExt1;
	ITestExt2Ptr m_pTestExt2;
};

CRYREGISTER_CLASS(CComposed)

CComposed::CComposed()
	: m_pTestExt1()
	, m_pTestExt2()
{
	CryCreateClassInstance("TestExt1", m_pTestExt1);
	CryCreateClassInstance("TestExt2", m_pTestExt2);
}

CComposed::~CComposed()
{
}

//////////////////////////////////////////////////////////////////////////

struct ITestExt3 : public ICryUnknown
{
	CRYINTERFACE_DECLARE_GUID(ITestExt3, "dd017935-a213-4898-bd2f-ffa145551876"_cry_guid);
	virtual void Call3() = 0;
};

DECLARE_SHARED_POINTERS(ITestExt3);

class CTestExt3 : public ITestExt3
{
	CRYGENERATE_CLASS_GUID(CTestExt3, "TestExt3", "eceab40b-c4bb-4988-a9f6-3c1db85a69b1"_cry_guid);

	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(ITestExt3)
	CRYINTERFACE_END()

public:
	virtual void Call3();

private:
	int i;
};

CRYREGISTER_CLASS(CTestExt3)

CTestExt3::CTestExt3()
{
	i = 3;
}

CTestExt3::~CTestExt3()
{
	printf("Inside CTestExt3 dtor\n");
}

void CTestExt3::Call3()
{
	printf("Inside CTestExt3::Call3()\n");
}

//////////////////////////////////////////////////////////////////////////

class CComposed2 : public ICryUnknown
{
	CRYGENERATE_CLASS_GUID(CComposed2, "Composed2", "0439d74b-8dcd-4b7e-9287-dcdf7e26a3a6"_cry_guid)

	CRYCOMPOSITE_BEGIN()
	CRYCOMPOSITE_ADD(m_pTestExt3, "Ext3")
	CRYCOMPOSITE_END(CComposed2)

	CRYINTERFACE_BEGIN()
	CRYINTERFACE_END()

private:
	ITestExt3Ptr m_pTestExt3;
};

CRYREGISTER_CLASS(CComposed2)

CComposed2::CComposed2()
	: m_pTestExt3()
{
	CryCreateClassInstance("TestExt3", m_pTestExt3);
}

CComposed2::~CComposed2()
{
}

//////////////////////////////////////////////////////////////////////////

class CTestExt4 : public ITestExt1, public ITestExt2, public ITestExt3
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(ITestExt1)
	CRYINTERFACE_ADD(ITestExt2)
	CRYINTERFACE_ADD(ITestExt3)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CTestExt4, "TestExt4", "43204e7c-c1be-45ca-9df4-ccb1c0dc1ad8"_cry_guid)

public:
	virtual void Call1() const;
	virtual void Call2();
	virtual void Call3();

private:
	int i;
};

CRYREGISTER_CLASS(CTestExt4)

CTestExt4::CTestExt4()
{
	i = 4;
}

CTestExt4::~CTestExt4()
{
	printf("Inside CTestExt4 dtor\n");
}

void CTestExt4::Call1() const
{
	printf("Inside CTestExt4::Call1()\n");
}

void CTestExt4::Call2()
{
	printf("Inside CTestExt4::Call2()\n");
}

void CTestExt4::Call3()
{
	printf("Inside CTestExt4::Call3()\n");
}

//////////////////////////////////////////////////////////////////////////

class CMegaComposed : public CComposed, public CComposed2
{
	CRYGENERATE_CLASS_GUID(CMegaComposed, "MegaComposed", "05127875-59f8-4503-421a-c1af66f2fb6f"_cry_guid)

	CRYCOMPOSITE_BEGIN()
	CRYCOMPOSITE_ADD(m_pTestExt4, "Ext4")
	CRYCOMPOSITE_ENDWITHBASE2(CMegaComposed, CComposed, CComposed2)

	CRYINTERFACE_BEGIN()
	CRYINTERFACE_END()

private:
	std::shared_ptr<CTestExt4> m_pTestExt4;
};

CRYREGISTER_CLASS(CMegaComposed)

CMegaComposed::CMegaComposed()
	: m_pTestExt4()
{
	printf("Inside CMegaComposed ctor\n");
	m_pTestExt4 = CTestExt4::CreateClassInstance();
}

CMegaComposed::~CMegaComposed()
{
	printf("Inside CMegaComposed dtor\n");
}

//////////////////////////////////////////////////////////////////////////

static void TestComposition()
{
	printf("\nTest composition:\n");

	ICryUnknownPtr p;
	if (CryCreateClassInstance("MegaComposed", p))
	{
		ITestExt1Ptr p1 = cryinterface_cast<ITestExt1>(crycomposite_query(p, "Ext1"));
		if (p1)
			p1->Call1();     // calls CTestExt1::Call1()
		ITestExt2Ptr p2 = cryinterface_cast<ITestExt2>(crycomposite_query(p, "Ext2"));
		if (p2)
			p2->Call2();     // calls CTestExt2::Call2()
		ITestExt3Ptr p3 = cryinterface_cast<ITestExt3>(crycomposite_query(p, "Ext3"));
		if (p3)
			p3->Call3();     // calls CTestExt3::Call3()
		p3 = cryinterface_cast<ITestExt3>(crycomposite_query(p, "Ext4"));
		if (p3)
			p3->Call3();     // calls CTestExt4::Call3()

		p1 = cryinterface_cast<ITestExt1>(crycomposite_query(p.get(), "Ext4"));
		p2 = cryinterface_cast<ITestExt2>(crycomposite_query(p.get(), "Ext4"));

		bool b = CryIsSameClassInstance(p1, p2);     // true
	}

	{
		ICryUnknownConstPtr pCUnk = p;
		ICryUnknownConstPtr pComp1 = crycomposite_query(pCUnk.get(), "Ext1");
		//ICryUnknownPtr pComp1 = crycomposite_query(pCUnk, "Ext1"); // must fail to compile due to const rules

		ITestExt1ConstPtr p1 = cryinterface_cast<const ITestExt1>(pComp1);
		if (p1)
			p1->Call1();
	}
}

} // namespace TestComposition

//////////////////////////////////////////////////////////////////////////

namespace TestExtension
{

class CFoobar : public IFoobar
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IFoobar)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CFoobar, "Foobar", "76c8dd6d-1663-4531-95d3-b1cfabcf7ef4"_cry_guid)

public:
	virtual void Foo();
};

CRYREGISTER_CLASS(CFoobar)

CFoobar::CFoobar()
{
}

CFoobar::~CFoobar()
{
}

void CFoobar::Foo()
{
	printf("Inside CFoobar::Foo()\n");
}

static void TestFoobar()
{
	std::shared_ptr<CFoobar> p = CFoobar::CreateClassInstance();
	{
		CryInterfaceID iid = cryiidof<IFoobar>();
		CryClassID clsid = p->GetFactory()->GetClassID();
		int t = 0;
	}

	{
		IAPtr sp_ = cryinterface_cast<IA>(p);     // sp_ == NULL

		ICryUnknownPtr sp1 = cryinterface_cast<ICryUnknown>(p);
		IFoobarPtr sp = cryinterface_cast<IFoobar>(sp1);
		sp->Foo();
	}

	{
		CFoobar* pF = p.get();
		pF->Foo();
		ICryUnknown* p1 = cryinterface_cast<ICryUnknown>(pF);
	}

	IFoobar* pFoo = cryinterface_cast<IFoobar>(p.get());
	ICryFactory* pF1 = pFoo->GetFactory();
	pFoo->Foo();

	int t = 0;
}

//////////////////////////////////////////////////////////////////////////

class CRaboof : public IRaboof
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IRaboof)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CRaboof, "Raabof", "ba482ce1-2b2e-4309-8238-ed8b52cb1f1e"_cry_guid)

public:
	virtual void Rab();
};

CRYREGISTER_CLASS(CRaboof)

CRaboof::CRaboof()
{
}

CRaboof::~CRaboof()
{
}

void CRaboof::Rab()
{
	printf("Inside CRaboof::Rab()\n");
}

static void TestRaboof()
{
	std::shared_ptr<CRaboof> pFoo0_ = CRaboof::CreateClassInstance();
	IRaboofPtr pFoo0 = cryinterface_cast<IRaboof>(pFoo0_);
	ICryUnknownPtr p0 = cryinterface_cast<ICryUnknown>(pFoo0);

	CryInterfaceID iid = cryiidof<IRaboof>();
	CryClassID clsid = p0->GetFactory()->GetClassID();

	std::shared_ptr<CRaboof> pFoo1 = CRaboof::CreateClassInstance();

	pFoo0->Rab();
	pFoo1->Rab();
}

//////////////////////////////////////////////////////////////////////////

class CAB : public IA, public IB
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IA)
	CRYINTERFACE_ADD(IB)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CAB, "AB", "b9e54711-a644-48c0-a481-9b4ed3024d04"_cry_guid)

public:
	virtual void A();
	virtual void B();

private:
	int i;
};

CRYREGISTER_CLASS(CAB)

CAB::CAB()
{
	i = 0x12345678;
}

CAB::~CAB()
{
}

void CAB::A()
{
	printf("Inside CAB::A()\n");
}

void CAB::B()
{
	printf("Inside CAB::B()\n");
}

//////////////////////////////////////////////////////////////////////////

class CABC : public CAB, public IC
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IC)
	CRYINTERFACE_ENDWITHBASE(CAB)

	CRYGENERATE_CLASS_GUID(CABC, "ABC", "4e61feae-1185-4be7-a161-57c5f8baadd9"_cry_guid)

public:
	virtual void C();

private:
	int a;
};

CRYREGISTER_CLASS(CABC)

CABC::CABC()
//: CAB()
{
	a = 0x87654321;
}

CABC::~CABC()
{
}

void CABC::C()
{
	printf("Inside CABC::C()\n");
}

//////////////////////////////////////////////////////////////////////////

class CCustomC : public ICustomC
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IC)
	CRYINTERFACE_ADD(ICustomC)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CCustomC, "CustomC", "ee61760b-98a4-4b71-a05e-7372b44bd0fd"_cry_guid)

public:
	virtual void C();
	virtual void C1();

private:
	int a;
};

CRYREGISTER_CLASS(CCustomC)

CCustomC::CCustomC()
{
	a = 0x87654321;
}

CCustomC::~CCustomC()
{
}

void CCustomC::C()
{
	printf("Inside CCustomC::C()\n");
}

void CCustomC::C1()
{
	printf("Inside CCustomC::C1()\n");
}

//////////////////////////////////////////////////////////////////////////

class CMultiBase : public CAB, public CCustomC
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ENDWITHBASE2(CAB, CCustomC)

	CRYGENERATE_CLASS_GUID(CMultiBase, "MultiBase", "75966b8f-9864-4d42-8fbd-d489e94cc29e"_cry_guid)

public:
	virtual void A();
	virtual void C1();

	int i;
};

CRYREGISTER_CLASS(CMultiBase)

CMultiBase::CMultiBase()
{
	i = 0x87654321;
}

CMultiBase::~CMultiBase()
{
}

void CMultiBase::C1()
{
	printf("Inside CMultiBase::C1()\n");
}

void CMultiBase::A()
{
	printf("Inside CMultiBase::A()\n");
}

//////////////////////////////////////////////////////////////////////////

static void TestComplex()
{
	{
		ICPtr p;
		if (CryCreateClassInstance("75966b8f-9864-4d42-8fbd-d489e94cc29e"_cry_guid, p))
		{
			p->C();
		}
	}

	{
		ICustomCPtr p;
		if (CryCreateClassInstance("MultiBase", p))
		{
			p->C();
		}
	}

	{
		IFoobarPtr p;
		if (CryCreateClassInstance("75966b8f-9864-4d42-8fbd-d489e94cc29e"_cry_guid, p))
		{
			p->Foo();
		}
	}

	{
		std::shared_ptr<CMultiBase> p = CMultiBase::CreateClassInstance();
		std::shared_ptr<const CMultiBase> pc = p;

		{
			ICryUnknownPtr pUnk = cryinterface_cast<ICryUnknown>(p);
			ICryUnknownConstPtr pCUnk0 = cryinterface_cast<const ICryUnknown>(p);
			ICryUnknownConstPtr pCUnk1 = cryinterface_cast<const ICryUnknown>(pc);
			//ICryUnknownPtr pUnkF = cryinterface_cast<ICryUnknown>(pc); // must fail to compile due to const rules

			ICryFactory* pF = pUnk->GetFactory();

			int t = 0;
		}

		ICPtr pC = cryinterface_cast<IC>(p);
		ICustomCPtr pCC = cryinterface_cast<ICustomC>(pC);

		p->C();
		p->C1();

		pC->C();
		pCC->C1();

		IAPtr pA = cryinterface_cast<IA>(p);
		pA->A();
		p->A();
	}

	{
		std::shared_ptr<CCustomC> p = CCustomC::CreateClassInstance();

		ICPtr pC = cryinterface_cast<IC>(p);
		ICustomCPtr pCC = cryinterface_cast<ICustomC>(pC);

		p->C();
		p->C1();

		pC->C();
		pCC->C1();
	}
	{
		CryInterfaceID ia = cryiidof<IA>();
		CryInterfaceID ib = cryiidof<IB>();
		CryInterfaceID ic = cryiidof<IC>();
		CryInterfaceID ico = cryiidof<ICryUnknown>();
	}

	{
		std::shared_ptr<CAB> p = CAB::CreateClassInstance();
		CryClassID clsid = p->GetFactory()->GetClassID();

		IAPtr pA = cryinterface_cast<IA>(p);
		IBPtr pB = cryinterface_cast<IB>(p);

		IBPtr pB1 = cryinterface_cast<IB>(pA);
		IAPtr pA1 = cryinterface_cast<IA>(pB);

		pA->A();
		pB->B();

		ICryUnknownPtr p1 = cryinterface_cast<ICryUnknown>(pA);
		ICryUnknownPtr p2 = cryinterface_cast<ICryUnknown>(pB);
		const ICryUnknown* p3 = cryinterface_cast<const ICryUnknown>(pB.get());

		int t = 0;
	}

	{
		std::shared_ptr<CABC> pABC = CABC::CreateClassInstance();
		CryClassID clsid = pABC->GetFactory()->GetClassID();

		ICryFactory* pFac = pABC->GetFactory();
		pFac->ClassSupports(cryiidof<IA>());
		pFac->ClassSupports(cryiidof<IRaboof>());

		IAPtr pABC0 = cryinterface_cast<IA>(pABC);
		IBPtr pABC1 = cryinterface_cast<IB>(pABC0);
		ICPtr pABC2 = cryinterface_cast<IC>(pABC1);

		pABC2->C();
		pABC1->B();

		pABC2->GetFactory();

		const IC* pCconst = pABC2.get();
		const ICryUnknown* pOconst = cryinterface_cast<const ICryUnknown>(pCconst);
		const IA* pAconst = cryinterface_cast<const IA>(pOconst);
		const IB* pBconst = cryinterface_cast<const IB>(pAconst);

		//const IA* pA11 = cryinterface_cast<IA>(pOconst);

		pCconst = cryinterface_cast<const IC>(pBconst);

		IC* pC = static_cast<IC*>(static_cast<void*>(pABC1.get()));
		pC->C();     // calls IB::B()

		int t = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
// use of extension system without any of the helper macros/templates

class CDontLikeMacrosFactory : public ICryFactory
{
	// ICryFactory
public:
	virtual const char* GetClassName() const
	{
		return "DontLikeMacros";
	}
	virtual const CryClassID& GetClassID() const
	{
		static constexpr CryGUID cid = "73c3ab00-42e6-488a-89ca-1a3763365565"_cry_guid;
		return cid;
	}
	virtual bool ClassSupports(const CryInterfaceID& iid) const
	{
		return iid == cryiidof<ICryUnknown>() || iid == cryiidof<IDontLikeMacros>();
	}
	virtual void ClassSupports(const CryInterfaceID*& pIIDs, size_t& numIIDs) const
	{
		static const CryInterfaceID iids[2] = { cryiidof<ICryUnknown>(), cryiidof<IDontLikeMacros>() };
		pIIDs = iids;
		numIIDs = 2;
	}
	virtual ICryUnknownPtr CreateClassInstance() const;

public:
	static CDontLikeMacrosFactory& Access()
	{
		return s_factory;
	}

private:
	CDontLikeMacrosFactory() {}
	~CDontLikeMacrosFactory() {}

private:
	static CDontLikeMacrosFactory s_factory;
};

CDontLikeMacrosFactory CDontLikeMacrosFactory::s_factory;

class CDontLikeMacros : public IDontLikeMacros
{
	// ICryUnknown
public:
	virtual ICryFactory* GetFactory() const
	{
		return &CDontLikeMacrosFactory::Access();
	};

protected:
	virtual void* QueryInterface(const CryInterfaceID& iid) const
	{
		if (iid == cryiidof<ICryUnknown>())
			return (void*)(ICryUnknown*) this;
		else if (iid == cryiidof<IDontLikeMacros>())
			return (void*)(IDontLikeMacros*) this;
		else
			return 0;
	}

	virtual void* QueryComposite(const char*) const
	{
		return 0;
	}

	// IDontLikeMacros
public:
	virtual void CallMe()
	{
		printf("Yey, no macros...\n");
	}

	CDontLikeMacros() {}

protected:
	virtual ~CDontLikeMacros() {}
};

ICryUnknownPtr CDontLikeMacrosFactory::CreateClassInstance() const
{
	std::shared_ptr<CDontLikeMacros> p(new CDontLikeMacros);
	return ICryUnknownPtr(*static_cast<std::shared_ptr<ICryUnknown>*>(static_cast<void*>(&p)));
}

static SRegFactoryNode g_dontLikeMacrosFactory(&CDontLikeMacrosFactory::Access());

//////////////////////////////////////////////////////////////////////////

static void TestDontLikeMacros()
{
	ICryFactory* f = &CDontLikeMacrosFactory::Access();

	f->ClassSupports(cryiidof<ICryUnknown>());
	f->ClassSupports(cryiidof<IDontLikeMacros>());

	const CryInterfaceID* pIIDs = 0;
	size_t numIIDs = 0;
	f->ClassSupports(pIIDs, numIIDs);

	ICryUnknownPtr p = f->CreateClassInstance();
	IDontLikeMacrosPtr pp = cryinterface_cast<IDontLikeMacros>(p);

	ICryUnknownPtr pq = crycomposite_query(p, "blah");

	pp->CallMe();
}

} // namespace TestExtension

//////////////////////////////////////////////////////////////////////////

void TestExtensions(ICryFactoryRegistryImpl* pReg)
{
	printf("Test extensions:\n");

	struct MyCallback : public ICryFactoryRegistryCallback
	{
		virtual void OnNotifyFactoryRegistered(ICryFactory* pFactory)
		{
			int test = 0;
		}
		virtual void OnNotifyFactoryUnregistered(ICryFactory* pFactory)
		{
			int test = 0;
		}
	};

	//pReg->RegisterCallback((ICryFactoryRegistryCallback*) 0x4);
	//pReg->RegisterCallback((ICryFactoryRegistryCallback*) 0x1);
	//pReg->RegisterCallback((ICryFactoryRegistryCallback*) 0x3);
	//pReg->RegisterCallback((ICryFactoryRegistryCallback*) 0x3);
	//pReg->RegisterCallback((ICryFactoryRegistryCallback*) 0x2);

	//pReg->UnregisterCallback((ICryFactoryRegistryCallback*) 0x2);
	//pReg->UnregisterCallback((ICryFactoryRegistryCallback*) 0x2);
	//pReg->UnregisterCallback((ICryFactoryRegistryCallback*) 0x4);
	//pReg->UnregisterCallback((ICryFactoryRegistryCallback*) 0x3);
	//pReg->UnregisterCallback((ICryFactoryRegistryCallback*) 0x1);

	//MyCallback callback0;
	//pReg->RegisterCallback(&callback0);
	//pReg->RegisterFactories(g_pHeadToRegFactories);

	//pReg->RegisterFactories(g_pHeadToRegFactories);
	//pReg->UnregisterFactories(g_pHeadToRegFactories);

	ICryFactory* pF[4];
	size_t numFactories = 4;
	pReg->IterateFactories(cryiidof<IA>(), pF, numFactories);
	pReg->IterateFactories("ffffffff-ffff-ffff-ffff-ffffffffffff"_cry_guid, pF, numFactories);

	numFactories = (size_t)-1;
	pReg->IterateFactories(cryiidof<ICryUnknown>(), 0, numFactories);

	MyCallback callback1;
	pReg->RegisterCallback(&callback1);
	pReg->UnregisterCallback(&callback1);

	pReg->GetFactory("ee61760b-98a4-4b71-a05e-7372b44bd0fd"_cry_guid);
	pReg->GetFactory("CustomC");
	pReg->GetFactory("ABC");
	pReg->GetFactory(nullptr);

	pReg->GetFactory("DontLikeMacros");
	pReg->GetFactory("73c3ab00-42e6-488a-89ca-1a3763365565"_cry_guid);

	TestExtension::TestFoobar();
	TestExtension::TestRaboof();
	TestExtension::TestComplex();
	TestExtension::TestDontLikeMacros();

	TestComposition::TestComposition();
}

#endif // #ifdef EXTENSION_SYSTEM_INCLUDE_TESTCASES
