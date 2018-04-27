// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CrySystem/CryUnitTest.h>
#include "GameTypeInfo.h"

#include <cstring>


bool CGameTypeInfo::IsCastable(const CGameTypeInfo* type) const
{
	for (const CGameTypeInfo* ptr = this; ptr; ptr = ptr->GetParent())
	{
		if (ptr == type)
			return true;
	}
	return false;
}




class CBase
{
public:
	virtual ~CBase() {}
	CRY_DECLARE_GTI_BASE(CBase);
};
CRY_IMPLEMENT_GTI_BASE(CBase);



class CType1 : public CBase
{
public:
	CRY_DECLARE_GTI(CType1);
};
CRY_IMPLEMENT_GTI(CType1, CBase);



class CType2 : public CBase
{
public:
	CRY_DECLARE_GTI(CType2);
};
CRY_IMPLEMENT_GTI(CType2, CBase);



CRY_UNIT_TEST_SUITE(CryGameTypeInfoTest)
{
	CRY_UNIT_TEST(GoodDownCast)
	{
		CBase* obj = new CType2();
		CType2* casted = crygti_cast<CType2*>(obj);
		CRY_UNIT_TEST_ASSERT(casted==obj);
		delete obj;
	}

	CRY_UNIT_TEST(BadDownCast)
	{
		CBase* obj = new CType2();
		CType1* casted = crygti_cast<CType1*>(obj);
		CRY_UNIT_TEST_ASSERT(casted == 0);
		delete obj;
	}

	CRY_UNIT_TEST(UpCast)
	{
		CType2* obj = new CType2();
		CBase* casted = crygti_cast<CBase*>(obj);
		CRY_UNIT_TEST_ASSERT(casted == obj);
		delete obj;
	}

	CRY_UNIT_TEST(ConstGoodDownCast)
	{
		const CBase* obj = new CType2();
		const CType2* casted = crygti_cast<const CType2*>(obj);
		CRY_UNIT_TEST_ASSERT(casted==obj);
		delete obj;
	}

	CRY_UNIT_TEST(ConstBadDownCast)
	{
		const CBase* obj = new CType2();
		const CType1* casted = crygti_cast<const CType1*>(obj);
		CRY_UNIT_TEST_ASSERT(casted == 0);
		delete obj;
	}

	CRY_UNIT_TEST(ConstUpCast)
	{
		const CType2* obj = new CType2();
		const CBase* casted = crygti_cast<const CBase*>(obj);
		CRY_UNIT_TEST_ASSERT(casted == obj);
		delete obj;
	}

	CRY_UNIT_TEST(SameCast)
	{
		CType2* obj = new CType2();
		CType2* casted = crygti_cast<CType2*>(obj);
		CRY_UNIT_TEST_ASSERT(casted == obj);
		delete obj;
	}

	CRY_UNIT_TEST(NullCast)
	{
		CType2* obj = 0;
		CType2* casted = crygti_cast<CType2*>(obj);
		CRY_UNIT_TEST_ASSERT(casted == 0);
	}

	CRY_UNIT_TEST(BaseTypeNotCastableToSubType_BugFix)
	{
		CBase* obj = new CBase();
		CType1* casted = crygti_cast<CType1*>(obj);
		CRY_UNIT_TEST_ASSERT(casted == 0);
		delete obj;
	}

	CRY_UNIT_TEST(TrueDownCastIsOf)
	{
		CBase* obj = new CType2();
		CRY_UNIT_TEST_ASSERT(crygti_isof<CType2>(obj) == true);
		delete obj;
	}

	CRY_UNIT_TEST(FalseDownCastIsOf)
	{
		CBase* obj = new CType2();
		CRY_UNIT_TEST_ASSERT(crygti_isof<CType1>(obj) == false);
		delete obj;
	}

	CRY_UNIT_TEST(TrueUpCastIsOf)
	{
		CBase* obj = new CType2();
		CRY_UNIT_TEST_ASSERT(crygti_isof<CBase>(obj) == true);
		delete obj;
	}

	CRY_UNIT_TEST(NullIsOf)
	{
		CBase* obj = 0;
		CRY_UNIT_TEST_ASSERT(crygti_isof<CBase>(obj) == false);
		delete obj;
	}

	CRY_UNIT_TEST(GetName)
	{
		CBase* obj = new CType2();
		CRY_UNIT_TEST_ASSERT(std::strcmp(obj->GetRunTimeType()->GetName(), "CType2") == 0);
		delete obj;
	}

}
