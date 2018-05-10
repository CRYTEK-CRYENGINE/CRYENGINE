// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <ICryMannequinDefs.h>

#include <CrySerialization/IClassFactory.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/CRCRef.h>
#include <CrySystem/CryUnitTest.h>

namespace mannequin
{
// The wrapping is required since having the Serialize function is currently a requirement for Serialization::SStruct
template<typename TSCRC>
struct SCRCRefWrapper
{
	SCRCRefWrapper()
	{
	}

	SCRCRefWrapper(const char* const s)
		: wrapped(s)
	{
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(wrapped, "Wrapped", "Wrapped");
	}

	TSCRC wrapped;
};

//////////////////////////////////////////////////////////////////////////
CRY_UNIT_TEST(SCRCRef_Serialization_StoreString)
{
	const SCRCRefWrapper<SCRCRef<1>> src("Test");
	const XmlNodeRef xmlSrc = Serialization::SaveXmlNode(src, "Root");
	CRY_UNIT_TEST_ASSERT(xmlSrc);

	SCRCRefWrapper<SCRCRef<1>> dst;
	const bool serializationSuccess = Serialization::LoadXmlNode(dst, xmlSrc);
	CRY_UNIT_TEST_ASSERT(serializationSuccess);

	CRY_UNIT_TEST_ASSERT(src.wrapped.crc == dst.wrapped.crc);
	CRY_UNIT_TEST_ASSERT(strcmpi(src.wrapped.c_str(), dst.wrapped.c_str()) == 0);
}

//////////////////////////////////////////////////////////////////////////
CRY_UNIT_TEST(SCRCRef_Serialization_StoreString_Empty)
{
	const SCRCRefWrapper<SCRCRef<1>> src;
	const XmlNodeRef xmlSrc = Serialization::SaveXmlNode(src, "Root");
	CRY_UNIT_TEST_ASSERT(xmlSrc);

	SCRCRefWrapper<SCRCRef<1>> dst("NotEmpty");
	const bool serializationSuccess = Serialization::LoadXmlNode(dst, xmlSrc);
	CRY_UNIT_TEST_ASSERT(serializationSuccess);

	CRY_UNIT_TEST_ASSERT(src.wrapped.crc == dst.wrapped.crc);
	CRY_UNIT_TEST_ASSERT(strcmpi(src.wrapped.c_str(), dst.wrapped.c_str()) == 0);
	CRY_UNIT_TEST_ASSERT(dst.wrapped.crc == SCRCRef<0>::INVALID);
}

//////////////////////////////////////////////////////////////////////////
CRY_UNIT_TEST(SCRCRef_Serialization_StoreString_To_NotStoreString)
{
	const SCRCRefWrapper<SCRCRef<1>> src("Test");
	const XmlNodeRef xmlSrc = Serialization::SaveXmlNode(src, "Root");
	CRY_UNIT_TEST_ASSERT(xmlSrc);

	SCRCRefWrapper<SCRCRef<0>> dst;
	const bool serializationSuccess = Serialization::LoadXmlNode(dst, xmlSrc);
	CRY_UNIT_TEST_ASSERT(serializationSuccess);

	CRY_UNIT_TEST_ASSERT(src.wrapped.crc == dst.wrapped.crc);
}

//////////////////////////////////////////////////////////////////////////
CRY_UNIT_TEST(SCRCRef_Serialization_StoreString_To_NotStoreString_Empty)
{
	const SCRCRefWrapper<SCRCRef<1>> src;
	const XmlNodeRef xmlSrc = Serialization::SaveXmlNode(src, "Root");
	CRY_UNIT_TEST_ASSERT(xmlSrc);

	SCRCRefWrapper<SCRCRef<0>> dst("NotEmpty");
	const bool serializationSuccess = Serialization::LoadXmlNode(dst, xmlSrc);
	CRY_UNIT_TEST_ASSERT(serializationSuccess);

	CRY_UNIT_TEST_ASSERT(src.wrapped.crc == dst.wrapped.crc);
	CRY_UNIT_TEST_ASSERT(dst.wrapped.crc == SCRCRef<0>::INVALID);
}

//////////////////////////////////////////////////////////////////////////
CRY_UNIT_TEST(SCRCRef_Serialization_NotStoreString)
{
	const SCRCRefWrapper<SCRCRef<0>> src("Test");
	const XmlNodeRef xmlSrc = Serialization::SaveXmlNode(src, "Root");
	CRY_UNIT_TEST_ASSERT(xmlSrc);

	SCRCRefWrapper<SCRCRef<0>> dst;
	const bool serializationSuccess = Serialization::LoadXmlNode(dst, xmlSrc);
	CRY_UNIT_TEST_ASSERT(serializationSuccess);

	CRY_UNIT_TEST_ASSERT(src.wrapped.crc == dst.wrapped.crc);
}

//////////////////////////////////////////////////////////////////////////

//// Currently broken. (June 2017)
//// Serialization::SaveXmlNode should produce xmlnode with 2 attributes ("CryXmlVersion" + "Wrapped") but somehow only "CryXmlVersion" was there.
//CRY_UNIT_TEST(SCRCRef_Serialization_NotStoreString_Empty)
//{
//	const SCRCRefWrapper<SCRCRef<0>> src;
//	const XmlNodeRef xmlSrc = Serialization::SaveXmlNode(src, "Root");
//	CRY_UNIT_TEST_ASSERT(xmlSrc);
//
//	SCRCRefWrapper<SCRCRef<0>> dst("NotEmpty");
//	const bool serializationSuccess = Serialization::LoadXmlNode(dst, xmlSrc);
//	CRY_UNIT_TEST_ASSERT(serializationSuccess);
//
//	CRY_UNIT_TEST_CHECK_EQUAL(src.wrapped.crc, dst.wrapped.crc);
//	CRY_UNIT_TEST_CHECK_EQUAL(dst.wrapped.crc, static_cast<uint32_t>(SCRCRef<0>::INVALID));//cast to suppress orbis-clang from wrongly choosing overload for static const
//}

//////////////////////////////////////////////////////////////////////////
CRY_UNIT_TEST(SCRCRef_Operator_LessThan_NonStoreString)
{
	{
		const SCRCRef<0> src0("A");
		const SCRCRef<0> src1("B");

		CRY_UNIT_TEST_ASSERT(src0 < src1 || src1 < src0);
	}

	{
		const SCRCRef<0> src0("A");
		const SCRCRef<0> src1("A");

		CRY_UNIT_TEST_ASSERT(!(src0 < src1) && !(src1 < src0));
	}
}

//////////////////////////////////////////////////////////////////////////
CRY_UNIT_TEST(SCRCRef_Operator_LessThan_StoreString)
{
	{
		const SCRCRef<1> src0("A");
		const SCRCRef<1> src1("B");

		CRY_UNIT_TEST_ASSERT(src0 < src1 || src1 < src0);
	}

	{
		const SCRCRef<1> src0("A");
		const SCRCRef<1> src1("A");

		CRY_UNIT_TEST_ASSERT(!(src0 < src1) && !(src1 < src0));
	}
}

//////////////////////////////////////////////////////////////////////////
CRY_UNIT_TEST(SCRCRef_Operator_LessThan_Mixed)
{
	{
		const SCRCRef<1> src0("A");
		const SCRCRef<0> src1("B");

		CRY_UNIT_TEST_ASSERT(src0 < src1 || src1 < src0);
	}

	{
		const SCRCRef<0> src0("A");
		const SCRCRef<1> src1("B");

		CRY_UNIT_TEST_ASSERT(src0 < src1 || src1 < src0);
	}

	{
		const SCRCRef<0> src0("A");
		const SCRCRef<1> src1("A");

		CRY_UNIT_TEST_ASSERT(!(src0 < src1) && !(src1 < src0));
	}

	{
		const SCRCRef<1> src0("A");
		const SCRCRef<0> src1("A");

		CRY_UNIT_TEST_ASSERT(!(src0 < src1) && !(src1 < src0));
	}
}

//////////////////////////////////////////////////////////////////////////
CRY_UNIT_TEST(SCRCRef_Operator_Equal_NonStoreString)
{
	{
		const SCRCRef<0> src0("A");
		const SCRCRef<0> src1("B");

		CRY_UNIT_TEST_ASSERT(!(src0 == src1));
	}

	{
		const SCRCRef<0> src0("A");
		const SCRCRef<0> src1("A");

		CRY_UNIT_TEST_ASSERT(src0 == src1);
	}
}

//////////////////////////////////////////////////////////////////////////
CRY_UNIT_TEST(SCRCRef_Operator_Equal_StoreString)
{
	{
		const SCRCRef<1> src0("A");
		const SCRCRef<1> src1("B");

		CRY_UNIT_TEST_ASSERT(!(src0 == src1));
	}

	{
		const SCRCRef<1> src0("A");
		const SCRCRef<1> src1("A");

		CRY_UNIT_TEST_ASSERT(src0 == src1);
	}
}

//////////////////////////////////////////////////////////////////////////
CRY_UNIT_TEST(SCRCRef_Operator_Equal_Mixed)
{
	{
		const SCRCRef<0> src0("A");
		const SCRCRef<1> src1("B");

		CRY_UNIT_TEST_ASSERT(!(src0 == src1));
	}

	{
		const SCRCRef<1> src0("A");
		const SCRCRef<0> src1("B");

		CRY_UNIT_TEST_ASSERT(!(src0 == src1));
	}

	{
		const SCRCRef<0> src0("A");
		const SCRCRef<1> src1("A");

		CRY_UNIT_TEST_ASSERT(src0 == src1);
	}

	{
		const SCRCRef<1> src0("A");
		const SCRCRef<0> src1("A");

		CRY_UNIT_TEST_ASSERT(src0 == src1);
	}
}

//////////////////////////////////////////////////////////////////////////
CRY_UNIT_TEST(SCRCRef_Operator_NotEqual_NonStoreString)
{
	{
		const SCRCRef<0> src0("A");
		const SCRCRef<0> src1("B");

		CRY_UNIT_TEST_ASSERT(src0 != src1);
	}

	{
		const SCRCRef<0> src0("A");
		const SCRCRef<0> src1("A");

		CRY_UNIT_TEST_ASSERT(!(src0 != src1));
	}
}

//////////////////////////////////////////////////////////////////////////
CRY_UNIT_TEST(SCRCRef_Operator_NotEqual_StoreString)
{
	{
		const SCRCRef<1> src0("A");
		const SCRCRef<1> src1("B");

		CRY_UNIT_TEST_ASSERT(src0 != src1);
	}

	{
		const SCRCRef<1> src0("A");
		const SCRCRef<1> src1("A");

		CRY_UNIT_TEST_ASSERT(!(src0 != src1));
	}
}

//////////////////////////////////////////////////////////////////////////
CRY_UNIT_TEST(SCRCRef_Operator_NotEqual_Mixed)
{
	{
		const SCRCRef<0> src0("A");
		const SCRCRef<1> src1("B");

		CRY_UNIT_TEST_ASSERT(src0 != src1);
	}

	{
		const SCRCRef<1> src0("A");
		const SCRCRef<0> src1("B");

		CRY_UNIT_TEST_ASSERT(src0 != src1);
	}

	{
		const SCRCRef<0> src0("A");
		const SCRCRef<1> src1("A");

		CRY_UNIT_TEST_ASSERT(!(src0 != src1));
	}

	{
		const SCRCRef<1> src0("A");
		const SCRCRef<0> src1("A");

		CRY_UNIT_TEST_ASSERT(!(src0 != src1));
	}
}
}
