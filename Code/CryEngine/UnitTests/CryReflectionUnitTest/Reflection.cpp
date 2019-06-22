// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <UnitTest.h>
#include <CryReflection/Framework.h>
#include <CryReflection/Any.h>
#include <CryReflection/AnyArray.h>
#include <CryReflection/IModule.h>
#include <CryReflection/ITypeDesc.h>
#include <CryReflection/Function/Delegate.h>
#include <CryReflection/Variable/MemberVariablePointer.h>

#include <gtest/gtest-spi.h>

#include "Module.h"
#include "ReflectionBase.h"
#include "../CrySystem/Serialization/ArchiveHost.h"

YASLI_ENUM_BEGIN(EBankType, "BankType")
YASLI_ENUM(eBankType_Retail, "BankRetail", "Retail")
YASLI_ENUM(eBankType_Buisness, "BankBuisness", "Buisness")
YASLI_ENUM(eBankType_Investment, "BankInvestment", "Investment")
YASLI_ENUM_END()

// Reflection Module Mockup
class CReflectionUnitTest : public ::testing::Test
{
protected:
	virtual void SetUp() override
	{
		m_pArchiveHost = Serialization::CreateArchiveHost();

		m_pModule = new Cry::Reflection::CModule();
		gEnv->pReflection = m_pModule;

		// Register all types.
		Cry::Reflection::CTypeRegistrationChain::Execute();

		// Prepare TypeDesc for test use.
		constexpr Cry::CTypeId typeIdUint64 = Cry::Type::IdOf<uint64>();
		constexpr Cry::CTypeId typeIdInt64 = Cry::Type::IdOf<int64>();
		constexpr Cry::CTypeId typeIdCVault = Cry::Type::IdOf<CVault>();
		constexpr Cry::CTypeId typeIdCBank = Cry::Type::IdOf<CBank>();

		m_pTypeDescUint64 = Cry::Reflection::CoreRegistry::GetTypeById(typeIdUint64);
		m_pTypeDescInt64 = Cry::Reflection::CoreRegistry::GetTypeById(typeIdInt64);
		m_pTypeDescCVault = Cry::Reflection::CoreRegistry::GetTypeById(typeIdCVault);
		m_pTypeDescCBank = Cry::Reflection::CoreRegistry::GetTypeById(typeIdCBank);
	}

	virtual void TearDown() override
	{
	}

	Cry::Reflection::IModule* m_pModule;
	Serialization::IArchiveHost* m_pArchiveHost;

	const Cry::Reflection::ITypeDesc* m_pTypeDescUint64;
	const Cry::Reflection::ITypeDesc* m_pTypeDescInt64;
	const Cry::Reflection::ITypeDesc* m_pTypeDescCVault;
	const Cry::Reflection::ITypeDesc* m_pTypeDescCBank;

};
// ~Reflection Module Mockup

TEST_F(CReflectionUnitTest, ReflectionTypeDescs)
{
	REQUIRE(m_pTypeDescUint64);
	REQUIRE(m_pTypeDescInt64);
	REQUIRE(m_pTypeDescCVault);
	REQUIRE(m_pTypeDescCBank);
}

namespace TypeIdUnitTests
{
	constexpr Cry::Type::CTypeId typeId; // constexpr default constructible
	constexpr Cry::Type::CTypeId typeId2 = typeId; // constexpr copy constructible
	constexpr Cry::Type::CTypeId typeIdUint64 = Cry::Type::IdOf<uint64>(); // constexpr IdOf
}

// Compile time tests
namespace SCompileTimeUnitTests
{
	// TODO: This should work for all compilers. Enable it, as soon as the GCC workaround in Type\TypeUtils.h is removed.
#if defined(CRY_COMPILER_MSVC) || (defined(CRY_COMPILER_CLANG) && !defined(CRY_PLATFORM_ORBIS))
	constexpr bool CompareCompileTimeString(const char* a, const char* b, int64 len = 1)
	{
		return *a == *b && (len - 1 <= 0 || CompareCompileTimeString(a + 1, b + 1, len - 1));
	}
	static_assert(CompareCompileTimeString("bool",
		Cry::Type::Utils::SCompileTime_TypeInfo<bool>::GetName().GetBegin(),
		Cry::Type::Utils::SCompileTime_TypeInfo<bool>::GetName().GetLength()), "TypeInfo name doesn't match expected name.");
#endif
	// ~TODO
}

// Cry::Type::TypeDesc
struct TypeDescInfo
{
	Cry::Type::CTypeId   typeId;
	Cry::Type::CTypeDesc typeDesc;
};

TypeDescInfo typeDescInfo[] =
{
	{ Cry::Type::IdOf<bool>(),   Cry::Type::DescOf<bool>()   },
	{ Cry::Type::IdOf<char>(),   Cry::Type::DescOf<char>()   },
	{ Cry::Type::IdOf<float>(),  Cry::Type::DescOf<float>()  },
	{ Cry::Type::IdOf<uint8>(),  Cry::Type::DescOf<uint8>()  },
	{ Cry::Type::IdOf<uint16>(), Cry::Type::DescOf<uint16>() },
	{ Cry::Type::IdOf<uint32>(), Cry::Type::DescOf<uint32>() },
	{ Cry::Type::IdOf<uint64>(), Cry::Type::DescOf<uint64>() },
	{ Cry::Type::IdOf<int8>(),   Cry::Type::DescOf<int8>()   },
	{ Cry::Type::IdOf<int16>(),  Cry::Type::DescOf<int16>()  },
	{ Cry::Type::IdOf<int32>(),  Cry::Type::DescOf<int32>()  },
	{ Cry::Type::IdOf<int64>(),  Cry::Type::DescOf<int64>()  }
};
const size_t count = CRY_ARRAY_COUNT(typeDescInfo);

TEST_F(CReflectionUnitTest, TypeDescFlags)
{
	struct TypeDescFlagTests
	{
		Cry::Reflection::ETypeCategory category;
		bool                           isAbstract    : 1;
		bool                           isArray       : 1;
		bool                           isClass       : 1;
		bool                           isFundamental : 1;
		bool                           isUnion       : 1;
	};

	TypeDescFlagTests tests[count];
	for (int i = 0; i < count; ++i)
	{
		tests[i] = { Cry::Reflection::ETypeCategory::Fundamental, false, false, false, true, false };
	}

	for (int i = 0; i < count; ++i)
	{
		const TypeDescFlagTests& curTest = tests[i];

		// Cry::Type::CTypeDesc
		const Cry::Type::CTypeDesc typeDesc = typeDescInfo[i].typeDesc;
		REQUIRE(typeDesc.IsAbstract() == curTest.isAbstract);
		REQUIRE(typeDesc.IsArray() == curTest.isArray);
		REQUIRE(typeDesc.IsClass() == curTest.isClass);
		REQUIRE(typeDesc.IsFundamental() == curTest.isFundamental);
		REQUIRE(typeDesc.IsUnion() == curTest.isUnion);

		// Cry::Reflection::CTypeDesc
		const Cry::Reflection::ITypeDesc* pReflectedTypeDesc = Cry::Reflection::CoreRegistry::GetTypeById(typeDescInfo[i].typeId);
		REQUIRE(pReflectedTypeDesc);
		if (pReflectedTypeDesc)
		{
			REQUIRE(pReflectedTypeDesc->GetCategory() == curTest.category);
			REQUIRE(pReflectedTypeDesc->IsAbstract() == curTest.isAbstract);
			REQUIRE(pReflectedTypeDesc->IsArray() == curTest.isArray);
			REQUIRE(pReflectedTypeDesc->IsClass() == curTest.isClass);
			REQUIRE(pReflectedTypeDesc->IsFundamental() == curTest.isFundamental);
			REQUIRE(pReflectedTypeDesc->IsUnion() == curTest.isUnion);
		}
	}
}

TEST_F(CReflectionUnitTest, TypeDescSizeAndAlignment)
{
	struct TypeDescSizeAndAlignment
	{
		uint16 size;
		uint16 alignment;
	};

	TypeDescSizeAndAlignment tests[] =
	{
		{ 1, 1 },
		{ 1, 1 },
		{ 4, 4 },
		{ 1, 1 }, { 2, 2 }, { 4, 4 }, { 8, 8 },
		{ 1, 1 }, { 2, 2 }, { 4, 4 }, { 8, 8 }
	};

	for (int i = 0; i < count; ++i)
	{
		const TypeDescSizeAndAlignment& curTest = tests[i];

		// Cry::Type::CTypeDesc
		const Cry::Type::CTypeDesc typeDesc = typeDescInfo[i].typeDesc;
		REQUIRE(typeDesc.GetSize() == curTest.size);
		REQUIRE(typeDesc.GetAlignment() == curTest.alignment);

		// Cry::Reflection::CTypeDesc
		const Cry::Reflection::ITypeDesc* pReflectedTypeDesc = Cry::Reflection::CoreRegistry::GetTypeById(typeDescInfo[i].typeId);
		REQUIRE(pReflectedTypeDesc);
		if (pReflectedTypeDesc)
		{
			REQUIRE(pReflectedTypeDesc->GetSize() == curTest.size);
			REQUIRE(pReflectedTypeDesc->GetAlignment() == curTest.alignment);
		}
	}
}

TEST_F(CReflectionUnitTest, CTypeDescDefaultConstructor)
{
	// uint64
	Cry::Type::CTypeDesc typeDesc = Cry::Type::DescOf<uint64>();
	uint64 source = 0xc0ffee;

	Cry::Type::CDefaultConstructor defaultConstructor = typeDesc.GetDefaultConstructor();
	REQUIRE(defaultConstructor);
	if (defaultConstructor)
	{
		defaultConstructor(&source);
		REQUIRE(source == 0); // Default for uint64 is 0.
	}

	// CBank
	typeDesc = Cry::Type::DescOf<CBank>();
	CBank bank;
	bank.SetAccount(source);
	bank.m_type = eBankType_Retail;

	defaultConstructor = typeDesc.GetDefaultConstructor();
	REQUIRE(defaultConstructor);
	if (defaultConstructor)
	{
		defaultConstructor(&bank);
		REQUIRE(bank.GetAccount() == -1);
		REQUIRE(bank.m_type == eBankType_Investment);
	}
}

TEST_F(CReflectionUnitTest, CTypeDescCopyConstructor)
{
	// uint64
	Cry::Type::CTypeDesc typeDesc = Cry::Type::DescOf<uint64>();
	const uint64 source = 0xc0ffee;
	uint64 target = 0;

	Cry::Type::CCopyConstructor copyConstructor = typeDesc.GetCopyConstructor();
	REQUIRE(copyConstructor);
	if (copyConstructor)
	{
		copyConstructor(&target, &source);
		REQUIRE(target == source);
	}

	// CBank
	typeDesc = Cry::Type::DescOf<CBank>();
	CBank sourceBank;
	sourceBank.SetAccount(0xc0ffee);
	sourceBank.m_type = eBankType_Retail;

	CBank targetBank;
	copyConstructor = typeDesc.GetCopyConstructor();
	REQUIRE(copyConstructor);
	if (copyConstructor)
	{
		copyConstructor(&targetBank, &sourceBank);
		REQUIRE(targetBank.GetAccount() == sourceBank.GetAccount());
		REQUIRE(targetBank.m_type == sourceBank.m_type);
	}
}

TEST_F(CReflectionUnitTest, CTypeDescMoveConstructor)
{
	// uint64
	Cry::Type::CTypeDesc typeDesc = Cry::Type::DescOf<uint64>();
	uint64 source = 0xc0ffee;
	uint64 target = 0;

	Cry::Type::CMoveConstructor moveConstructor = typeDesc.GetMoveConstructor();
	REQUIRE(moveConstructor);
	if (moveConstructor)
	{
		moveConstructor(&target, &source);
		REQUIRE(target == source);
	}

	// CBank
	typeDesc = Cry::Type::DescOf<CBank>();
	CBank sourceBank;
	sourceBank.SetAccount(source);
	sourceBank.m_type = eBankType_Retail;

	CBank targetBank;
	moveConstructor = typeDesc.GetMoveConstructor();
	REQUIRE(moveConstructor);
	if (moveConstructor)
	{
		moveConstructor(&targetBank, &sourceBank);
		REQUIRE(targetBank.GetAccount() == source);
		REQUIRE(targetBank.m_type == eBankType_Retail);
	}
}
 
TEST_F(CReflectionUnitTest, CTypeDescDefaultNewOperator)
{
	// uint64
	Cry::Type::CTypeDesc typeDesc = Cry::Type::DescOf<uint64>();

	Cry::Type::CDefaultNewOperator defaultNewOperator = typeDesc.GetDefaultNewOperator();
	REQUIRE(defaultNewOperator);
	if (defaultNewOperator)
	{
		uint64* pSource = static_cast<uint64*>(defaultNewOperator());
		REQUIRE(pSource);

		if (pSource)
		{
			delete pSource;
		}
	}

	// CBank
	typeDesc = Cry::Type::DescOf<CBank>();

	defaultNewOperator = typeDesc.GetDefaultNewOperator();
	REQUIRE(defaultNewOperator);
	if (defaultNewOperator)
	{
		CBank* pBank = static_cast<CBank*>(defaultNewOperator());
		REQUIRE(pBank->GetAccount() == -1);
		REQUIRE(pBank->m_type == eBankType_Investment);

		if (pBank)
		{
			delete pBank;
		}
	}
}

TEST_F(CReflectionUnitTest, CTypeDescDeleteOperator)
{
	// uint64
	Cry::Type::CTypeDesc typeDesc = Cry::Type::DescOf<uint64>();
	uint64* pSource = new uint64;

	Cry::Type::CDeleteOperator deleteOperator = typeDesc.GetDeleteOperator();
	REQUIRE(deleteOperator);
	if (deleteOperator)
	{
		deleteOperator(pSource);
	}

	// CBank
	typeDesc = Cry::Type::DescOf<CBank>();
	CBank* pBank = new CBank();

	deleteOperator = typeDesc.GetDeleteOperator();
	REQUIRE(deleteOperator);
	if (deleteOperator)
	{
		deleteOperator(pBank);
	}
}

TEST_F(CReflectionUnitTest, CTypeDescNewArrayOperator)
{
	// uint64
	Cry::Type::CTypeDesc typeDesc = Cry::Type::DescOf<uint64>();
	Cry::Type::CNewArrayOperator newArrayOperator = typeDesc.GetNewArrayOperator();
	REQUIRE(newArrayOperator);
	if (newArrayOperator)
	{
		const size_t valueCount = 10;
		uint64* pValues = reinterpret_cast<uint64*>(newArrayOperator(valueCount));
		REQUIRE(pValues);

		const uint64 source = 0xc0ffee;
		pValues[0] = source;
		REQUIRE(pValues[0] == source);

		if (pValues)
		{
			delete[] pValues;
		}
	}

	// CBank
	typeDesc = Cry::Type::DescOf<CBank>();
	newArrayOperator = typeDesc.GetNewArrayOperator();
	REQUIRE(newArrayOperator);
	if (newArrayOperator)
	{
		const size_t bankCount = 10;
		CBank* pBanks = reinterpret_cast<CBank*>(newArrayOperator(bankCount));
		REQUIRE(pBanks);

		const uint64 source = 0xc0ffee;
		pBanks[0].SetAccount(source);
		REQUIRE(pBanks[0].GetAccount() == source);

		if (pBanks)
		{
			delete[] pBanks;
		}
	}
}

TEST_F(CReflectionUnitTest, CTypeDescDeleteArrayOperator)
{
	// uint64
	Cry::Type::CTypeDesc typeDesc = Cry::Type::DescOf<uint64>();
	Cry::Type::CDeleteArrayOperator deleteArrayOperator = typeDesc.GetDeleteArrayOperator();
	REQUIRE(deleteArrayOperator);
	if (deleteArrayOperator)
	{
		const size_t valueCount = 10;
		uint64* pValues = new uint64[valueCount];

		deleteArrayOperator(pValues);
	}

	// CBank
	typeDesc = Cry::Type::DescOf<CBank>();

	deleteArrayOperator = typeDesc.GetDeleteArrayOperator();
	REQUIRE(deleteArrayOperator);
	if (deleteArrayOperator)
	{
		const size_t bankCount = 10;
		CBank* pBanks = new CBank[bankCount];

		deleteArrayOperator(pBanks);
	}
}

TEST_F(CReflectionUnitTest, CTypeDescCopyAssignOperator)
{
	// uint64
	Cry::Type::CTypeDesc typeDesc = Cry::Type::DescOf<uint64>();
	const uint64 source = 0xc0ffee;
	uint64 target = 0;

	Cry::Type::CCopyAssignOperator copyAssignOperator = typeDesc.GetCopyAssignOperator();
	REQUIRE(copyAssignOperator);
	if (copyAssignOperator)
	{
		copyAssignOperator(&target, &source);
		REQUIRE(target == source);
	}

	// CBank
	typeDesc = Cry::Type::DescOf<CBank>();
	CBank sourceBank;
	sourceBank.SetAccount(0xc0ffee);
	sourceBank.m_type = eBankType_Retail;

	CBank targetBank;
	copyAssignOperator = typeDesc.GetCopyAssignOperator();
	REQUIRE(copyAssignOperator);
	if (copyAssignOperator)
	{
		copyAssignOperator(&targetBank, &sourceBank);
		REQUIRE(targetBank.GetAccount() == sourceBank.GetAccount());
		REQUIRE(targetBank.m_type == sourceBank.m_type);
	}
}

TEST_F(CReflectionUnitTest, CTypeDescMoveAssignOperator)
{
	// uint64
	Cry::Type::CTypeDesc typeDesc = Cry::Type::DescOf<uint64>();
	uint64 source = 0xc0ffee;
	uint64 reference = source;
	uint64 target = 0;

	Cry::Type::CMoveAssignOperator moveAssignOperator = typeDesc.GetMoveAssignOperator();
	REQUIRE(moveAssignOperator);
	if (moveAssignOperator)
	{
		moveAssignOperator(&target, &source);
		REQUIRE(target == reference);
	}

	// CBank
	typeDesc = Cry::Type::DescOf<CBank>();
	CBank sourceBank;
	sourceBank.SetAccount(0xc0ffee);
	sourceBank.m_type = eBankType_Retail;

	CBank referenceBank(sourceBank);

	CBank targetBank;
	moveAssignOperator = typeDesc.GetMoveAssignOperator();
	REQUIRE(moveAssignOperator);
	if (moveAssignOperator)
	{
		moveAssignOperator(&targetBank, &sourceBank);
		REQUIRE(targetBank.GetAccount() == referenceBank.GetAccount());
		REQUIRE(targetBank.m_type == referenceBank.m_type);

		REQUIRE(sourceBank.GetAccount() == -1);
		REQUIRE(sourceBank.m_type == eBankType_Investment);
	}
}

TEST_F(CReflectionUnitTest, CTypeDescEqualOperator)
{
	// uint64
	Cry::Type::CTypeDesc typeDesc = Cry::Type::DescOf<uint64>();
	uint64 a = 0xc0ffee;
	uint64 b = 0xdeadbeef;
	uint64 c = a;

	Cry::Type::CEqualOperator equalOperator = typeDesc.GetEqualOperator();
	REQUIRE(equalOperator);
	if (equalOperator)
	{
		REQUIRE(equalOperator(&a, &c) == true);
		REQUIRE(equalOperator(&a, &b) == false);
	}

	// CBank
	typeDesc = Cry::Type::DescOf<CBank>();
	CBank d;
	CBank e;
	CBank f;
	e.SetAccount(0xc0ffee);
	e.m_type = eBankType_Retail;

	CBank targetBank;
	equalOperator = typeDesc.GetEqualOperator();
	REQUIRE(equalOperator);
	if (equalOperator)
	{
		REQUIRE(equalOperator(&d, &f) == true);
		REQUIRE(equalOperator(&d, &e) == false);
	}
}

TEST_F(CReflectionUnitTest, CTypeDescSerialize)
{
	REQUIRE(m_pArchiveHost);
	if (m_pArchiveHost)
	{
		DynArray<char> buffer;
		Cry::Type::CTypeDesc typeDesc = Cry::Type::DescOf<CBank>();

		CBank sourceBank;
		sourceBank.SetAccount(0xc0ffee);
		sourceBank.m_type = eBankType_Retail;

		CBank targetBank;
		Cry::Type::CSerializeFunction serializeFunction = typeDesc.GetSerializeFunction();
		REQUIRE(serializeFunction);
		if (serializeFunction)
		{
			m_pArchiveHost->SaveBinaryBuffer(buffer, Serialization::SStruct(sourceBank));
			m_pArchiveHost->LoadBinaryBuffer(Serialization::SStruct(targetBank), buffer.data(), buffer.size());

			REQUIRE(targetBank.GetAccount() == sourceBank.GetAccount());
			REQUIRE(targetBank.m_type == sourceBank.m_type);
		}

		typeDesc = Cry::Type::DescOf<CVault>();
		serializeFunction = typeDesc.GetSerializeFunction();
		REQUIRE(serializeFunction);
	}
}

TEST_F(CReflectionUnitTest, CTypeDescToString)
{
	Cry::Type::CTypeDesc typeDesc = Cry::Type::DescOf<CBank>();
	const uint64 source = 0xc0ffee;

	CBank bank;
	bank.SetAccount(source);

	Cry::Type::CToStringFunction toStringFunction = typeDesc.GetToStringFunction();
	REQUIRE(toStringFunction);
	if (toStringFunction)
	{
		string sourceString;
		sourceString.Format("{ \"m_type\": %i, \"m_account\": %li }", eBankType_Investment, source);
		string targetString = toStringFunction(&bank);

		REQUIRE(targetString == sourceString);
	}

	typeDesc = Cry::Type::CTypeDesc::Create<bool>();
	toStringFunction = typeDesc.GetToStringFunction();
	REQUIRE(toStringFunction);
	if (toStringFunction)
	{
		bool source = true;
		string sourceString("1");
		string targetString = toStringFunction(&source);
		REQUIRE(targetString == sourceString);
	}
}

TEST_F(CReflectionUnitTest, CAnyFlags)
{
	struct CAnyFlagsTests
	{
		bool isEmpty       : 1;
		bool isConst       : 1;
		bool isPointerType : 1;
	};

	CAnyFlagsTests tests[count];
	for (int i = 0; i < count; ++i)
	{
		tests[i] = { false, false, false };
	}

	for (int i = 0; i < count; ++i)
	{
		const CAnyFlagsTests& curTest = tests[i];

		Cry::Reflection::CAny any(*Cry::Reflection::CoreRegistry::GetTypeById(typeDescInfo[i].typeId));
		REQUIRE(any.IsEmpty() == curTest.isEmpty);
		REQUIRE(any.IsConst() == curTest.isConst);
		REQUIRE(any.IsPointer() == curTest.isPointerType);
	}
}

TEST_F(CReflectionUnitTest, RegisterType)
{
	const Cry::Reflection::ITypeDesc* pTypeDesc1 = Cry::Reflection::CoreRegistry::GetTypeById(Cry::Type::IdOf<int64>());
	REQUIRE(pTypeDesc1);
	if (pTypeDesc1)
	{
		const Cry::Reflection::ITypeDesc* pTypeDesc2 = Cry::Reflection::CoreRegistry::GetTypeByGuid("{64E9AB2C-E182-45B2-822C-3E2BD70B96FE}"_cry_guid);
		REQUIRE(pTypeDesc2);
		if (pTypeDesc2)
		{
			REQUIRE(pTypeDesc1 == pTypeDesc2);
			REQUIRE(pTypeDesc1->IsFundamental());
			REQUIRE(pTypeDesc1->IsClass() == false);
		}
	}
}

TEST_F(CReflectionUnitTest, RegisterClassType)
{
	const Cry::Reflection::ITypeDesc* pTypeDesc1 = Cry::Reflection::CoreRegistry::GetTypeById(Cry::Type::IdOf<CVault>());
	REQUIRE(pTypeDesc1);
	if (pTypeDesc1)
	{
		REQUIRE(pTypeDesc1->IsFundamental() == false);
		REQUIRE(pTypeDesc1->IsClass());

		const Cry::Reflection::ITypeDesc* pTypeDesc2 = Cry::Reflection::CoreRegistry::GetTypeByGuid("{6F6C65AA-0551-4DDB-8DC2-B873A8A72052}"_cry_guid);
		REQUIRE(pTypeDesc2);
		if (pTypeDesc2)
		{
			REQUIRE(pTypeDesc1 == pTypeDesc2);

			size_t variableCount = pTypeDesc1->GetVariableCount();
			REQUIRE(variableCount == 2);

			const Cry::Reflection::IVariableDesc* pVariableDesc = pTypeDesc1->GetVariableByIndex(0);
			REQUIRE(pVariableDesc);

			pVariableDesc = pTypeDesc1->GetVariableByIndex(1);
			REQUIRE(pVariableDesc);
		}
	}

	const Cry::Reflection::ITypeDesc* pTypeDesc3 = Cry::Reflection::CoreRegistry::GetTypeById(Cry::Type::IdOf<CBank>());
	REQUIRE(pTypeDesc3);
	if (pTypeDesc3)
	{
		REQUIRE(pTypeDesc3->IsClass());
		REQUIRE(pTypeDesc3->GetBaseTypeCount() == 1);
		REQUIRE(pTypeDesc3->GetVariableCount() == 2);
		REQUIRE(pTypeDesc3->GetFunctionCount() == 2);

		const Cry::Reflection::IFunctionDesc* pFunctionDesc = pTypeDesc3->GetFunctionByIndex(0);
		REQUIRE(pFunctionDesc);

		const Cry::Reflection::IBaseTypeDesc* pBaseTypeDesc = pTypeDesc3->GetBaseTypeByIndex(0);
		REQUIRE(pBaseTypeDesc);
		if (pBaseTypeDesc)
		{
			REQUIRE(pBaseTypeDesc->GetTypeId() == pTypeDesc1->GetTypeId());
		}
	}
}

TEST_F(CReflectionUnitTest, RegisterEnumValueType)
{
	const Cry::Reflection::ITypeDesc* pTypeDesc = Cry::Reflection::CoreRegistry::GetTypeById(Cry::Type::IdOf<EBankType>());
	REQUIRE(pTypeDesc);
	if (pTypeDesc)
	{
		REQUIRE(pTypeDesc->IsEnum());

		size_t count = pTypeDesc->GetEnumValueCount();
		REQUIRE(count == 3);

		const int exptectedValues[] = { eBankType_Retail, eBankType_Buisness, eBankType_Investment };
		for (size_t i = 0; i < count; ++i)
		{
			const Cry::Reflection::IEnumValueDesc* pEnumValueDesc = pTypeDesc->GetEnumValueByIndex(i);
			REQUIRE(pEnumValueDesc);
			if (pEnumValueDesc)
			{
				REQUIRE(exptectedValues[i] == pEnumValueDesc->GetValue());
			}
		}
	}
}

TEST_F(CReflectionUnitTest, ReflectedMemberVariable)
{
	CBank* pBank = new CBank();
	if (m_pTypeDescCBank)
	{
		const Cry::Reflection::IVariableDesc* pVariableDesc = m_pTypeDescCBank->GetVariableByIndex(0);
		REQUIRE(pVariableDesc);
		if (pVariableDesc)
		{
			uint64 source = 0xeeffc0;
			*reinterpret_cast<uint64*>(reinterpret_cast<char*>(pBank) + pVariableDesc->GetOffset()) = source;
			REQUIRE(pBank->GetAccount() == source);
		}

		pVariableDesc = m_pTypeDescCBank->GetVariableByIndex(1);
		REQUIRE(pVariableDesc);
		if (pVariableDesc)
		{
			EBankType type = *reinterpret_cast<EBankType*>(reinterpret_cast<char*>(pBank) + pVariableDesc->GetOffset());
			REQUIRE(type == eBankType_Investment);
		}
	}

	delete pBank;
}

TEST_F(CReflectionUnitTest, ReflectedBaseMemberVariable)
{
	CBank* pBank = new CBank();

	Cry::Reflection::CMemberVariablePointer variablePointer(pBank, Cry::Type::IdOf<CVault>(), 0);
	REQUIRE(variablePointer);
	if (variablePointer)
	{
		REQUIRE(variablePointer.GetTypeDesc()->GetTypeId() == Cry::Type::IdOf<uint64>());
		REQUIRE(pBank->m_public == *static_cast<uint64*>(variablePointer.GetPointer()));

		uint64 source = 0xeeffc0;
		*static_cast<uint64*>(variablePointer.GetPointer()) = source;
		REQUIRE(pBank->m_public == source);
	}

	if (m_pTypeDescCBank)
	{
		pBank->m_public = 42;
		variablePointer = Cry::Reflection::CMemberVariablePointer(static_cast<void*>(pBank), Cry::Type::IdOf<CVault>(), *m_pTypeDescCBank, 0);
		REQUIRE(variablePointer);
		if (variablePointer)
		{
			REQUIRE(variablePointer.GetTypeDesc()->GetTypeId() == Cry::Type::IdOf<uint64>());
			REQUIRE(pBank->m_public == *static_cast<uint64*>(variablePointer.GetPointer()));

			uint64 source = 0xeeffc0;
			*static_cast<uint64*>(variablePointer.GetPointer()) = source;
			REQUIRE(pBank->m_public == source);
		}
	}

	delete pBank;
}

TEST_F(CReflectionUnitTest, ReflectedMemberFunction)
{
	CBank* pBank = new CBank();
	if (m_pTypeDescCBank)
	{
		const Cry::Reflection::IFunctionDesc* pFunctionDesc = m_pTypeDescCBank->GetFunctionByIndex(0); // SetAccount
		REQUIRE(pFunctionDesc);
		if (pFunctionDesc)
		{
			const int64 source = 0xeeffc0;
			Cry::Reflection::CDelegate setAccountDelegate(pBank, *pFunctionDesc);
			setAccountDelegate.Invoke(source);
			REQUIRE(pBank->GetAccount() == source);

			pFunctionDesc = m_pTypeDescCBank->GetFunctionByIndex(1); // GetAccount
			REQUIRE(pFunctionDesc);
			if (pFunctionDesc)
			{
				REQUIRE(cry_strcmp("GetAccount", pFunctionDesc->GetLabel()) == 0);
				Cry::Reflection::CDelegate getAccountDelegate(static_cast<void*>(pBank), *pFunctionDesc);
				Cry::Reflection::CAny ret = getAccountDelegate.Invoke();
				REQUIRE(ret.GetTypeIndex() == m_pTypeDescInt64->GetIndex());

				const int64* pDestination = ret.GetConstPointer<int64>();
				REQUIRE(pDestination);
				if (pDestination)
				{
					REQUIRE(*pDestination == source);
				}
			}
		}
	}

	delete pBank;
}

TEST_F(CReflectionUnitTest, CMemberVariablePointer)
{
	if (m_pTypeDescCVault)
	{
		const Cry::Reflection::IVariableDesc* pVariableDesc = m_pTypeDescCVault->GetVariableByIndex(0);
		REQUIRE(pVariableDesc);
		if (pVariableDesc)
		{
			CVault* pVault = new CVault();
			uint64 source = pVault->m_public;
			Cry::Reflection::CMemberVariablePointer variablePointer(pVault, 0);
			Cry::Reflection::CMemberVariablePointer variablePointer2(static_cast<void*>(pVault), *m_pTypeDescCVault, 0);

			REQUIRE(variablePointer.GetPointer() == variablePointer2.GetPointer());
			REQUIRE(source == *(static_cast<uint64*>(variablePointer.GetPointer())));
			REQUIRE(source == *(static_cast<uint64*>(variablePointer2.GetPointer())));
			REQUIRE(pVariableDesc->GetTypeId() == variablePointer.GetTypeDesc()->GetTypeId());
			REQUIRE(pVariableDesc->GetTypeId() == variablePointer2.GetTypeDesc()->GetTypeId());

			delete pVault;
		}
	}
}

TEST_F(CReflectionUnitTest, CAnyCopyConstructor)
{
	if (m_pTypeDescInt64)
	{
		const int64 source = 0xeeffc0;
		Cry::Reflection::CAny anySource(*m_pTypeDescInt64);
		anySource.AssignValue(source, false);

		Cry::Reflection::CAny anyTarget(anySource);

		REQUIRE(anySource.IsEqual(anyTarget));
	}
}

TEST_F(CReflectionUnitTest, CAnyCopyAssign)
{
	if (m_pTypeDescInt64)
	{
		const int64 source = 0xeeffc0;
		Cry::Reflection::CAny anySource(*m_pTypeDescInt64);
		anySource.AssignValue(source, false);

		Cry::Reflection::CAny anyTarget = anySource;

		REQUIRE(anySource.IsEqual(anyTarget));
	}
}

TEST_F(CReflectionUnitTest, CAnyMoveAssign)
{
	if (m_pTypeDescInt64)
	{
		const int64 source = 0xeeffc0;
		Cry::Reflection::CAny anySource(*m_pTypeDescInt64);
		Cry::Reflection::CAny anyValue(*m_pTypeDescInt64);
		anySource.AssignValue(source, false);
		anyValue.AssignValue(source, false);

		Cry::Reflection::CAny anyTarget = std::move(anyValue);

		REQUIRE(anySource.IsEqual(anyTarget));
	}
}

// TODO: For now this only tests on uint64, maybe we want more complex types in the future
TEST_F(CReflectionUnitTest, CAnyCreateDestroy)
{
	if (m_pTypeDescUint64)
	{
		Cry::Reflection::CAny any(*m_pTypeDescUint64);
		Cry::Reflection::TypeIndex index = m_pTypeDescUint64->GetIndex();

		REQUIRE(any.IsEmpty() == false);
		REQUIRE(any.IsType(index));

		any.Destruct();
		REQUIRE(any.IsEmpty());
		REQUIRE(any.IsType(index) == false);
	}
}

TEST_F(CReflectionUnitTest, CAnyAssignValue)
{
	if (m_pTypeDescUint64)
	{
		const Cry::Reflection::TypeIndex index = m_pTypeDescUint64->GetIndex();

		uint64 source = 0xeeffc0;
		Cry::Reflection::CAny any;

		// template<typename VALUE_TYPE> void AssignValue(const VALUE_TYPE& value, bool isConst = false);
		any.AssignValue(source, false);
		uint64* pDestination = any.GetPointer<uint64>();
		REQUIRE(pDestination);
		if (pDestination) { REQUIRE(*pDestination == source); }

		const uint64* pConstDestination = any.GetConstPointer<uint64>();
		REQUIRE(pConstDestination);
		if (pConstDestination) { REQUIRE(*pConstDestination == source); }
		++source;

		any.Destruct();

		any.AssignValue(source, true);
		pDestination = any.GetPointer<uint64>();
		REQUIRE(pDestination == nullptr);

		pConstDestination = any.GetConstPointer<uint64>();
		REQUIRE(pConstDestination);
		if (pConstDestination) { REQUIRE(*pConstDestination == source); }
		++source;

		any.Destruct();

		// template<typename VALUE_TYPE> void AssignValue(const VALUE_TYPE& value, TypeIndex typeIndex, bool isConst = false);
		any.AssignValue(source, index, false);
		pDestination = any.GetPointer<uint64>();
		REQUIRE(pDestination);
		if (pDestination) { REQUIRE(*pDestination == source); }

		pConstDestination = any.GetConstPointer<uint64>();
		REQUIRE(pConstDestination);
		if (pConstDestination) { REQUIRE(*pConstDestination == source); }
		++source;

		any.Destruct();

		any.AssignValue(source, index, true);
		pDestination = any.GetPointer<uint64>();
		REQUIRE(pDestination == nullptr);

		pConstDestination = any.GetConstPointer<uint64>();
		REQUIRE(pConstDestination);
		if (pConstDestination) { REQUIRE(*pConstDestination == source); }
		++source;

		any.Destruct();

		// template<typename VALUE_TYPE> void AssignValue(const VALUE_TYPE& value, const ITypeDesc& srcTypeDesc, bool isConst = false);
		any.AssignValue(source, *m_pTypeDescUint64, false);
		pDestination = any.GetPointer<uint64>();
		REQUIRE(pDestination);
		if (pDestination) { REQUIRE(*pDestination == source); }

		pConstDestination = any.GetConstPointer<uint64>();
		REQUIRE(pConstDestination);
		if (pConstDestination) { REQUIRE(*pConstDestination == source); }
		++source;

		any.Destruct();

		any.AssignValue(source, *m_pTypeDescUint64, true);
		pDestination = any.GetPointer<uint64>();
		REQUIRE(pDestination == nullptr);

		pConstDestination = any.GetConstPointer<uint64>();
		REQUIRE(pConstDestination);
		if (pConstDestination) { REQUIRE(*pConstDestination == source); }
		++source;

		any.Destruct();

		// void AssignValue(const CAny& value, bool isConst = false);
		Cry::Reflection::CAny otherAny(source);
		any.AssignValue(otherAny, false);
		pDestination = any.GetPointer<uint64>();
		REQUIRE(pDestination);
		if (pDestination) { REQUIRE(*pDestination == source); }

		pConstDestination = any.GetConstPointer<uint64>();
		REQUIRE(pConstDestination);
		if (pConstDestination) { REQUIRE(*pConstDestination == source); }
		++source;

		any.Destruct();

		otherAny.Destruct();
		otherAny.AssignValue(source);
		any.AssignValue(otherAny, true);
		pDestination = any.GetPointer<uint64>();
		REQUIRE(pDestination == nullptr);

		pConstDestination = any.GetConstPointer<uint64>();
		REQUIRE(pConstDestination);
		if (pConstDestination) { REQUIRE(*pConstDestination == source); }
		++source;

		any.Destruct();

		// void AssignValue(const void* pValue, TypeIndex typeIndex, bool isConst = false);
		const void* pConstSource = static_cast<const void*>(&source);
		any.AssignValue(pConstSource, index, false);
		pDestination = any.GetPointer<uint64>();
		REQUIRE(pDestination);
		if (pDestination) { REQUIRE(*pDestination == source); }

		pConstDestination = any.GetConstPointer<uint64>();
		REQUIRE(pConstDestination);
		if (pConstDestination) { REQUIRE(*pConstDestination == source); }

		any.Destruct();

		++source;
		pConstSource = static_cast<const void*>(&source);

		any.AssignValue(pConstSource, index, true);
		pDestination = any.GetPointer<uint64>();
		REQUIRE(pDestination == nullptr);

		pConstDestination = any.GetConstPointer<uint64>();
		REQUIRE(pConstDestination);
		if (pConstDestination) { REQUIRE(*pConstDestination == source); }

		any.Destruct();

		++source;
		pConstSource = static_cast<const void*>(&source);

		// void AssignValue(const void* pValue, const ITypeDesc& srcTypeDesc, bool isConst = false);
		any.AssignValue(pConstSource, *m_pTypeDescUint64, false);
		pDestination = any.GetPointer<uint64>();
		REQUIRE(pDestination);
		if (pDestination) { REQUIRE(*pDestination == source); }

		pConstDestination = any.GetConstPointer<uint64>();
		REQUIRE(pConstDestination);
		if (pConstDestination) { REQUIRE(*pConstDestination == source); }

		any.Destruct();

		++source;
		pConstSource = static_cast<const void*>(&source);

		any.AssignValue(pConstSource, *m_pTypeDescUint64, true);
		pDestination = any.GetPointer<uint64>();
		REQUIRE(pDestination == nullptr);

		pConstDestination = any.GetConstPointer<uint64>();
		REQUIRE(pConstDestination);
		if (pConstDestination) { REQUIRE(*pConstDestination == source); }
	}
}

TEST_F(CReflectionUnitTest, CAnyAssignPointer)
{
	if (m_pTypeDescUint64)
	{
		const Cry::Reflection::TypeIndex index = m_pTypeDescUint64->GetIndex();

		uint64 source = 0xeeffc0;
		Cry::Reflection::CAny any;

		// template<typename VALUE_TYPE> void AssignPointer(VALUE_TYPE* pPointer);
		any.AssignPointer(&source);
		REQUIRE(any.IsConst() == false);
		REQUIRE(any.GetPointer<uint64>() == &source);
		REQUIRE(any.GetPointer() == static_cast<void*>(&source));
		REQUIRE(any.GetConstPointer<uint64>() == static_cast<const uint64*>(&source));
		REQUIRE(any.GetConstPointer() == static_cast<const void*>(&source));
		++source;

		any.AssignPointer(static_cast<const uint64*>(&source));
		REQUIRE(any.IsConst());
		REQUIRE(any.GetPointer<uint64>() == nullptr);
		REQUIRE(any.GetPointer() == nullptr);
		REQUIRE(any.GetConstPointer<uint64>() == static_cast<const uint64*>(&source));
		REQUIRE(any.GetConstPointer() == static_cast<const void*>(&source));
		++source;

		//  template<typename VALUE_TYPE> void AssignPointer(VALUE_TYPE* pPointer, TypeIndex typeIndex);
		any.AssignPointer(&source, index);
		REQUIRE(any.IsConst() == false);
		REQUIRE(any.GetPointer<uint64>() == &source);
		REQUIRE(any.GetPointer() == static_cast<void*>(&source));
		REQUIRE(any.GetConstPointer<uint64>() == static_cast<const uint64*>(&source));
		REQUIRE(any.GetConstPointer() == static_cast<const void*>(&source));
		++source;

		any.AssignPointer(static_cast<const uint64*>(&source), index);
		REQUIRE(any.IsConst());
		REQUIRE(any.GetPointer<uint64>() == nullptr);
		REQUIRE(any.GetPointer() == nullptr);
		REQUIRE(any.GetConstPointer<uint64>() == static_cast<const uint64*>(&source));
		REQUIRE(any.GetConstPointer() == static_cast<const void*>(&source));
		++source;

		//  template<typename VALUE_TYPE> void AssignPointer(VALUE_TYPE* pPointer, const ITypeDesc& srcTypeDesc);
		any.AssignPointer(&source, *m_pTypeDescUint64);
		REQUIRE(any.IsConst() == false);
		REQUIRE(any.GetPointer<uint64>() == &source);
		REQUIRE(any.GetPointer() == static_cast<void*>(&source));
		REQUIRE(any.GetConstPointer<uint64>() == static_cast<const uint64*>(&source));
		REQUIRE(any.GetConstPointer() == static_cast<const void*>(&source));
		++source;

		any.AssignPointer(static_cast<const uint64*>(&source), *m_pTypeDescUint64);
		REQUIRE(any.IsConst());
		REQUIRE(any.GetPointer<uint64>() == nullptr);
		REQUIRE(any.GetPointer() == nullptr);
		REQUIRE(any.GetConstPointer<uint64>() == static_cast<const uint64*>(&source));
		REQUIRE(any.GetConstPointer() == static_cast<const void*>(&source));
		++source;

		// void AssignPointer(void* pPointer, TypeIndex typeIndex);
		any.AssignPointer(static_cast<void*>(&source), index);
		REQUIRE(any.IsConst() == false);
		REQUIRE(any.GetPointer<uint64>() == &source);
		REQUIRE(any.GetPointer() == static_cast<void*>(&source));
		REQUIRE(any.GetConstPointer<uint64>() == static_cast<const uint64*>(&source));
		REQUIRE(any.GetConstPointer() == static_cast<const void*>(&source));
		++source;

		// void AssignPointer(void* pPointer, const ITypeDesc& srcTypeDesc);
		any.AssignPointer(static_cast<void*>(&source), *m_pTypeDescUint64);
		REQUIRE(any.IsConst() == false);
		REQUIRE(any.GetPointer<uint64>() == &source);
		REQUIRE(any.GetPointer() == static_cast<void*>(&source));
		REQUIRE(any.GetConstPointer<uint64>() == static_cast<const uint64*>(&source));
		REQUIRE(any.GetConstPointer() == static_cast<const void*>(&source));
		++source;

		// void AssignPointer(const void* pPointer, TypeIndex typeIndex);
		any.AssignPointer(static_cast<const void*>(&source), index);
		REQUIRE(any.IsConst());
		REQUIRE(any.GetPointer<uint64>() == nullptr);
		REQUIRE(any.GetPointer() == nullptr);
		REQUIRE(any.GetConstPointer<uint64>() == static_cast<const uint64*>(&source));
		REQUIRE(any.GetConstPointer() == static_cast<const void*>(&source));
		++source;

		// void AssignPointer(const void* pPointer, const ITypeDesc& srcTypeDesc);
		any.AssignPointer(static_cast<const void*>(&source), *m_pTypeDescUint64);
		REQUIRE(any.IsConst());
		REQUIRE(any.GetPointer<uint64>() == nullptr);
		REQUIRE(any.GetPointer() == nullptr);
		REQUIRE(any.GetConstPointer<uint64>() == static_cast<const uint64*>(&source));
		REQUIRE(any.GetConstPointer() == static_cast<const void*>(&source));
	}
}

// TODO: Check conversion operators
TEST_F(CReflectionUnitTest, CAnyCopyValue)
{
	if (m_pTypeDescUint64)
	{
		const Cry::Reflection::TypeIndex index = m_pTypeDescUint64->GetIndex();

		const uint64 source = 0xc0ffee;
		uint64 destination = 0;
		Cry::Reflection::CAny any(source);

		// template <typename VALUE_TYPE> bool CopyValue(VALUE_TYPE& destination) const;
		any.CopyValue(destination);
		REQUIRE(destination == source);
		destination = 0;

		// bool CopyValue(void* pDestination, TypeIndex typeIndex) const;
		any.CopyValue(static_cast<void*>(&destination), index);
		REQUIRE(destination == source);
		destination = 0;

		// bool CopyValue(void* pDestination, const ITypeDesc& dstTypeDesc) const;
		any.CopyValue(static_cast<void*>(&destination), *m_pTypeDescUint64);
		REQUIRE(destination == source);
	}
}
// ~TODO

TEST_F(CReflectionUnitTest, CAnyAssignToPointer)
{
	uint64 source = 0xeeffc0;
	uint64 destination = 0;
	Cry::Reflection::CAny any(&destination);

	// template<typename VALUE_TYPE> void AssignToPointer(const VALUE_TYPE& source);
	any.AssignToPointer(source);
	REQUIRE(destination == source);
	destination = 0;

	if (m_pTypeDescUint64)
	{
		// template<typename VALUE_TYPE> void AssignToPointer(const VALUE_TYPE& source, TypeIndex typeIndex);
		any.AssignToPointer(source, m_pTypeDescUint64->GetIndex());
		REQUIRE(destination == source);
		destination = 0;

		// template<typename VALUE_TYPE> void AssignToPointer(const VALUE_TYPE& source, const ITypeDesc& srcTypeDesc);
		any.AssignToPointer(source, *m_pTypeDescUint64);
		REQUIRE(destination == source);
		destination = 0;

		// void AssignToPointer(void* pSource, TypeIndex typeIndex);
		void* pSource = static_cast<void*>(&source);
		any.AssignToPointer(pSource, m_pTypeDescUint64->GetIndex());
		REQUIRE(destination == source);
		destination = 0;

		// void AssignToPointer(void* pSource, const ITypeDesc& srcTypeDesc);
		any.AssignToPointer(pSource, *m_pTypeDescUint64);
		REQUIRE(destination == source);
		destination = 0;

		// void AssignToPointer(const void* pSource, TypeIndex typeIndex);
		const void* pConstSource = static_cast<const void*>(&source);
		any.AssignToPointer(pConstSource, m_pTypeDescUint64->GetIndex());
		REQUIRE(destination == source);
		destination = 0;

		// void AssignToPointer(const void* pSource, const ITypeDesc& srcTypeDesc);
		any.AssignToPointer(pConstSource, *m_pTypeDescUint64);
		REQUIRE(destination == source);
	}
}

TEST_F(CReflectionUnitTest, CAnyIsEqual)
{
	const uint64 source = 0xeeffc0;

	Cry::Reflection::CAny any(source);
	Cry::Reflection::CAny other(source);

	// template<typename VALUE_TYPE> bool IsEqual(const VALUE_TYPE& other) const;
	REQUIRE(any.IsEqual(source));
	REQUIRE(any.IsEqual((uint64)0xefbe) == false);

	// bool IsEqual(const CAny& other) const;
	REQUIRE(any.IsEqual(other));
	other.Destruct();
	REQUIRE(any.IsEqual(other) == false);

	// bool IsEqualPointer(const void* pOther) const;
	REQUIRE(any.IsEqualPointer(other.GetPointer()) == false);
	any.Destruct();
	any.AssignPointer(&source);
	other.AssignPointer(&source);
	REQUIRE(any.IsEqualPointer(other.GetConstPointer()));
}
// ~TODO

TEST_F(CReflectionUnitTest, CAnySerialize)
{
	REQUIRE(m_pArchiveHost);
	if (m_pArchiveHost)
	{
		DynArray<char> buffer;

		Cry::Reflection::CAny anyTarget;
		Cry::Reflection::CAny anySource((uint64)0xeeffc0);
		m_pArchiveHost->SaveBinaryBuffer(buffer, Serialization::SStruct(anySource));
		m_pArchiveHost->LoadBinaryBuffer(Serialization::SStruct(anyTarget), buffer.data(), buffer.size());

		REQUIRE(anySource.IsPointer() == anyTarget.IsPointer());
		REQUIRE(anySource.IsConst() == anyTarget.IsConst());
		REQUIRE(anySource.GetTypeIndex() == anyTarget.GetTypeIndex());
		REQUIRE(anySource.GetConstPointer<uint64>());
		REQUIRE(anyTarget.GetConstPointer<uint64>());
		REQUIRE(*anySource.GetConstPointer<uint64>() == *anyTarget.GetConstPointer<uint64>());

		anySource.Destruct();
		anyTarget.Destruct();

		anySource.AssignValue(CVault());
		m_pArchiveHost->SaveBinaryBuffer(buffer, Serialization::SStruct(anySource));
		m_pArchiveHost->LoadBinaryBuffer(Serialization::SStruct(anyTarget), buffer.data(), buffer.size());

		REQUIRE(anySource.IsPointer() == anyTarget.IsPointer());
		REQUIRE(anySource.IsConst() == anyTarget.IsConst());
		REQUIRE(anySource.GetTypeIndex() == anyTarget.GetTypeIndex());

		const CVault* pSource = anySource.GetConstPointer<CVault>();
		const CVault* pTarget = anyTarget.GetConstPointer<CVault>();
		REQUIRE(pSource);
		REQUIRE(pTarget);

		REQUIRE(pSource->m_public == pTarget->m_public);
	}
}

TEST_F(CReflectionUnitTest, CAnyToString)
{
	if (m_pTypeDescCBank)
	{
		CBank bank;
		bank.SetAccount(0xeeffc0);

		Cry::Reflection::CAny any(bank);
		any.SetConst(true);

		string sourceString;
		sourceString.Format("{ \"m_pData\": %p, \"m_typeIndex\": %u, \"m_isSmallValue\": %u, \"m_isPointer\": %u, \"m_isConst\": %u }",
		                    any.GetConstPointer(), m_pTypeDescCBank->GetIndex().ToValueType(), 0, 0, 1);
		string targetString = any.ToString();

		REQUIRE(targetString == sourceString);
	}
}

TEST_F(CReflectionUnitTest, CAnyArrayCreate)
{
	if (m_pTypeDescUint64)
	{
		Cry::Reflection::CAnyArray anyArray;
		REQUIRE(anyArray.GetTypeIndex() == Cry::Reflection::TypeIndex::Invalid);
		REQUIRE(anyArray.GetSize() == 0);
		REQUIRE(anyArray.GetCapacity() == 0);

		anyArray.SetType(m_pTypeDescUint64->GetIndex(), true);
		anyArray.SetConst(true);
		REQUIRE(anyArray.IsConst());
		REQUIRE(anyArray.IsPointer());

		anyArray.DestructAll();
		REQUIRE(anyArray.GetTypeIndex() == Cry::Reflection::TypeIndex::Invalid);
		REQUIRE(anyArray.GetSize() == 0);
		REQUIRE(anyArray.GetCapacity() == 0);
	}
}

TEST_F(CReflectionUnitTest, CAnyArrayClear)
{
	if (m_pTypeDescCBank)
	{
		CBank* pBank = new CBank();
		pBank->m_type = eBankType_Buisness;

		Cry::Reflection::CAnyArray anyArray(*m_pTypeDescCBank, true);
		anyArray.AddPointer(pBank);
		anyArray.ClearAll();

		REQUIRE(anyArray.GetSize() == 0);
		REQUIRE(pBank->m_type == eBankType_Buisness);

		delete pBank;
	}
}

TEST_F(CReflectionUnitTest, CAnyArrayAlignment)
{
	if (m_pTypeDescUint64)
	{
		uint64 reference[] = { 1ull, 2ull, 3ull, 4ull };
		const uint32 refCount = static_cast<uint32>(CRY_ARRAY_COUNT(reference));

		Cry::Reflection::CAnyArray anyArray(*m_pTypeDescUint64);
		anyArray.Resize(refCount);

		for (uint32 i = 0; i < refCount-1; ++i)
		{
			ptrdiff_t sourceOffset = reinterpret_cast<uintptr_t>(&reference[i + 1]) - reinterpret_cast<uintptr_t>(&reference[i]);
			void* pT1 = anyArray.GetPointer(i);
			void* pT2 = anyArray.GetPointer(i + 1);
			ptrdiff_t targetOffset = reinterpret_cast<uintptr_t>(pT2) - reinterpret_cast<uintptr_t>(pT1);
			REQUIRE(sourceOffset == targetOffset);
		}
	}

	if (m_pTypeDescCVault)
	{
		CVault reference[] = { CVault(), CVault(), CVault(), CVault() };
		const uint32 refCount = static_cast<uint32>(CRY_ARRAY_COUNT(reference));

		Cry::Reflection::CAnyArray anyArray(*m_pTypeDescCVault);
		anyArray.Resize(refCount);

		for (uint32 i = 0; i < refCount - 1; ++i)
		{
			ptrdiff_t sourceOffset = reinterpret_cast<uintptr_t>(&reference[i + 1]) - reinterpret_cast<uintptr_t>(&reference[i]);
			void* pT1 = anyArray.GetPointer(i);
			void* pT2 = anyArray.GetPointer(i + 1);
			ptrdiff_t targetOffset = reinterpret_cast<uintptr_t>(pT2) - reinterpret_cast<uintptr_t>(pT1);
			REQUIRE(sourceOffset == targetOffset);
		}
	}
}

TEST_F(CReflectionUnitTest, CAnyArrayResize)
{
	if (m_pTypeDescUint64)
	{
		Cry::Reflection::CAnyArray anyArray(*m_pTypeDescUint64, false);
		REQUIRE(anyArray.GetSize() == 0);
		REQUIRE(anyArray.GetCapacity() == 0);

		anyArray.Reserve(2);
		REQUIRE(anyArray.GetSize() == 0);
		REQUIRE(anyArray.GetCapacity() == 2);

		anyArray.Resize(1);
		REQUIRE(anyArray.GetSize() == 1);
		REQUIRE(anyArray.GetCapacity() == 2);

		anyArray.Resize(3);
		REQUIRE(anyArray.GetSize() == 3);
		REQUIRE(anyArray.GetCapacity() == 3);

		anyArray.Resize(2);
		REQUIRE(anyArray.GetSize() == 2);
		REQUIRE(anyArray.GetCapacity() == 3);

		anyArray.Resize(0);
		REQUIRE(anyArray.GetSize() == 0);
		REQUIRE(anyArray.GetCapacity() == 3);
	}
}

TEST_F(CReflectionUnitTest, CAnyArrayAddValue)
{
	if (m_pTypeDescUint64)
	{
		uint32 index = 0;
		uint32 exptectedSize = 0;

		uint64 source = 0xeeffc0;
		Cry::Reflection::CAnyArray anyArray(*m_pTypeDescUint64, false);

		// This should not work
		EXPECT_ASSERT_FAILURE(anyArray.AddPointer(&source), "Can't add pointer 'pPointer' to value array.");
		REQUIRE(anyArray.GetSize() == exptectedSize);

		// template<typename VALUE_TYPE> void AddValue(const VALUE_TYPE& source);
		anyArray.AddValue(source);

		REQUIRE(anyArray.GetSize() == ++exptectedSize);
		uint64* pTarget = anyArray.GetPointer<uint64>(index++);
		REQUIRE(pTarget);
		REQUIRE(source == *pTarget);

		// void AddValue(const CAny& source);
		Cry::Reflection::CAny sourceAny(source);
		anyArray.AddValue(sourceAny);
		REQUIRE(anyArray.GetSize() == ++exptectedSize);
		pTarget = anyArray.GetPointer<uint64>(index++);
		REQUIRE(pTarget);
		REQUIRE(source == *pTarget);

		// void AddValue(const void* pSource, TypeIndex typeIndex);
		anyArray.AddValue(static_cast<const void*>(&source), m_pTypeDescUint64->GetIndex());
		REQUIRE(anyArray.GetSize() == ++exptectedSize);
		pTarget = anyArray.GetPointer<uint64>(index++);
		REQUIRE(pTarget);
		REQUIRE(source == *pTarget);

		// void AddValue(const void* pSource, const ITypeDesc& typeDesc);
		anyArray.AddValue(static_cast<const void*>(&source), *m_pTypeDescUint64);
		REQUIRE(anyArray.GetSize() == ++exptectedSize);
		pTarget = anyArray.GetPointer<uint64>(index++);
		REQUIRE(pTarget);
		REQUIRE(source == *pTarget);
	}
}

TEST_F(CReflectionUnitTest, CAnyArrayAddPointer)
{
	if (m_pTypeDescUint64)
	{
		uint32 index = 0;
		uint32 exptectedSize = 0;

		uint64 source = 0xeeffc0;
		Cry::Reflection::CAnyArray anyArray(*m_pTypeDescUint64, true);

		// This should not work
		EXPECT_ASSERT_FAILURE(anyArray.AddValue(source), "Can't add value type 'source' to pointer array.");
		REQUIRE(anyArray.GetSize() == exptectedSize);

		// template<typename VALUE_TYPE> void AddPointer(VALUE_TYPE* pPointer);
		anyArray.AddPointer(&source);

		REQUIRE(anyArray.GetSize() == ++exptectedSize);
		uint64* pTarget = anyArray.GetPointer<uint64>(index++);
		REQUIRE(pTarget);
		REQUIRE(&source == pTarget);

		// void AddPointer(const CAny& source);
		Cry::Reflection::CAny any(&source);
		anyArray.AddPointer(any);

		REQUIRE(anyArray.GetSize() == ++exptectedSize);
		pTarget = anyArray.GetPointer<uint64>(index++);
		REQUIRE(pTarget);
		REQUIRE(&source == pTarget);

		// void AddPointer(const void* pPointer, TypeIndex typeIndex);
		anyArray.AddPointer(static_cast<const void*>(&source), m_pTypeDescUint64->GetIndex());

		REQUIRE(anyArray.GetSize() == ++exptectedSize);
		pTarget = anyArray.GetPointer<uint64>(index++);
		REQUIRE(pTarget);
		REQUIRE(&source == pTarget);

		// void AddPointer(const void* pPointer, const ITypeDesc& typeDesc);
		anyArray.AddPointer(static_cast<const void*>(&source), *m_pTypeDescUint64);

		REQUIRE(anyArray.GetSize() == ++exptectedSize);
		pTarget = anyArray.GetPointer<uint64>(index++);
		REQUIRE(pTarget);
		REQUIRE(&source == pTarget);
	}
}

TEST_F(CReflectionUnitTest, CAnyArrayInsertValueAt)
{
	if (m_pTypeDescUint64)
	{
		const uint64 source = 0xeeffc0;
		const uint64 marker = 0xefbe;
		Cry::Reflection::CAnyArray anyArray(*m_pTypeDescUint64);

		anyArray.AddValue(marker);

		// template<typename VALUE_TYPE> void InsertValueAt(uint32 index, const VALUE_TYPE& source);
		anyArray.InsertValueAt(0, source);
		REQUIRE(anyArray.GetSize() == 2);
		uint64* pTarget = anyArray.GetPointer<uint64>(0);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == source);
		}
		pTarget = anyArray.GetPointer<uint64>(1);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == marker);
		}

		// void InsertValueAt(uint32 index, CAny& source);
		Cry::Reflection::CAny any(source);
		anyArray.InsertValueAt(1, any);
		REQUIRE(anyArray.GetSize() == 3);
		pTarget = anyArray.GetPointer<uint64>(1);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == source);
		}
		pTarget = anyArray.GetPointer<uint64>(2);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == marker);
		}

		// void InsertValueAt(uint32 index, const void* pSource, TypeIndex typeIndex);
		anyArray.InsertValueAt(1, static_cast<const void*>(&marker), m_pTypeDescUint64->GetIndex());
		REQUIRE(anyArray.GetSize() == 4);
		pTarget = anyArray.GetPointer<uint64>(1);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == marker);
		}
		pTarget = anyArray.GetPointer<uint64>(3);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == marker);
		}

		// void InsertValueAt(uint32 index, const void* pSource, const ITypeDesc& typeDesc);
		anyArray.InsertValueAt(3, static_cast<const void*>(&source), *m_pTypeDescUint64);
		REQUIRE(anyArray.GetSize() == 5);
		uint64 expectedValues[5] = { source, marker, source, source, marker };
		for (int i = 0; i < 5; ++i)
		{
			pTarget = anyArray.GetPointer<uint64>(i);
			REQUIRE(pTarget);
			if (pTarget)
			{
				REQUIRE(*pTarget == expectedValues[i]);
			}
		}
	}
}

TEST_F(CReflectionUnitTest, CAnyArrayInsertPointerAt)
{
	if (m_pTypeDescUint64)
	{
		uint64 source = 0xeeffc0;
		uint64 marker = 0xefbe;
		Cry::Reflection::CAnyArray anyArray(*m_pTypeDescUint64, true);

		// This shouldn't work
		EXPECT_ASSERT_FAILURE(anyArray.InsertPointerAt(0, source), "Trying to add value type 'source' to pointer array.");
		REQUIRE(anyArray.GetSize() == 0);

		anyArray.AddPointer(&marker);

		// void InsertPointerAt(uint32 index, const VALUE_TYPE* pPointer);
		anyArray.InsertPointerAt(0, &source);
		REQUIRE(anyArray.GetSize() == 2);
		uint64* pTarget = anyArray.GetPointer<uint64>(0);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == source);
		}
		pTarget = anyArray.GetPointer<uint64>(1);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == marker);
		}

		// void InsertPointerAt(uint32 index, const CAny& source);
		Cry::Reflection::CAny any(&source);
		anyArray.InsertPointerAt(1, any);
		REQUIRE(anyArray.GetSize() == 3);
		pTarget = anyArray.GetPointer<uint64>(1);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == source);
		}
		pTarget = anyArray.GetPointer<uint64>(2);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == marker);
		}

		// void InsertPointerAt(uint32 index, const void* pSource, TypeIndex typeIndex);
		anyArray.InsertPointerAt(1, static_cast<const void*>(&marker), m_pTypeDescUint64->GetIndex());
		REQUIRE(anyArray.GetSize() == 4);
		pTarget = anyArray.GetPointer<uint64>(1);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == marker);
		}
		pTarget = anyArray.GetPointer<uint64>(3);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == marker);
		}

		// void InsertPointerAt(uint32 index, const void* pSource, const ITypeDesc& typeDesc);
		anyArray.InsertPointerAt(3, static_cast<const void*>(&source), *m_pTypeDescUint64);
		REQUIRE(anyArray.GetSize() == 5);
		const uint64 expectedValues[5] = { source, marker, source, source, marker };
		for (int i = 0; i < 5; ++i)
		{
			pTarget = anyArray.GetPointer<uint64>(i);
			REQUIRE(pTarget);
			if (pTarget)
			{
				REQUIRE(*pTarget == expectedValues[i]);
			}
		}
	}
}

TEST_F(CReflectionUnitTest, CAnyArrayAssignCopyValueAt)
{
	if (m_pTypeDescUint64)
	{
		uint32 index = 0;

		const uint64 source = 0xeeffc0;
		Cry::Reflection::CAnyArray anyArray;
		anyArray.SetType(*m_pTypeDescUint64, false);
		anyArray.Resize(4);

		// template<typename VALUE_TYPE> void AssignCopyValueAt(uint32 index, const VALUE_TYPE& source);
		anyArray.AssignCopyValueAt(index, source);
		uint64* pTarget = anyArray.GetPointer<uint64>(index);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == source);
		}

		// void AssignCopyValueAt(uint32 index, const CAny& source);
		Cry::Reflection::CAny any(source);
		anyArray.AssignCopyValueAt(++index, any);
		pTarget = anyArray.GetPointer<uint64>(index);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == source);
		}

		// void AssignCopyValueAt(uint32 index, const void* pSource, TypeIndex typeIndex);
		anyArray.AssignCopyValueAt(++index, static_cast<const void*>(&source), m_pTypeDescUint64->GetIndex());
		pTarget = anyArray.GetPointer<uint64>(index);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == source);
		}

		// void AssignCopyValueAt(uint32 index, const void* pSource, const ITypeDesc& typeDesc);
		anyArray.AssignCopyValueAt(++index, static_cast<const void*>(&source), *m_pTypeDescUint64);
		pTarget = anyArray.GetPointer<uint64>(index);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == source);
		}
	}

	if (m_pTypeDescCBank)
	{
		CBank bank;
		bank.SetAccount(0xeeffc0);

		Cry::Reflection::CAnyArray anyArray;
		anyArray.SetType(*m_pTypeDescCBank, false);
		anyArray.Resize(1);

		// template<typename VALUE_TYPE> void AssignCopyValueAt(uint32 index, const VALUE_TYPE& source);
		anyArray.AssignCopyValueAt(0, bank);
		const CBank* pTarget = anyArray.GetConstPointer<CBank>(0);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(pTarget->GetAccount() == bank.GetAccount());
			REQUIRE(pTarget->m_type == bank.m_type);

			bank.SetAccount(0);
			REQUIRE(pTarget->GetAccount() != bank.GetAccount());
		}
	}
}

TEST_F(CReflectionUnitTest, CAnyArrayAssignMoveValueAt)
{
	if (m_pTypeDescUint64)
	{
		uint32 index = 0;

		uint64 source = 0xeeffc0;
		Cry::Reflection::CAnyArray anyArray;
		anyArray.SetType(*m_pTypeDescUint64, false);
		anyArray.Resize(4);

		// template<typename VALUE_TYPE> void AssignMoveValueAt(uint32 index, const VALUE_TYPE& source);
		anyArray.AssignMoveValueAt(index, source);
		uint64* pTarget = anyArray.GetPointer<uint64>(index);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == source);
		}

		// void AssignMoveValueAt(uint32 index, const CAny& source);
		Cry::Reflection::CAny any(source);
		anyArray.AssignMoveValueAt(++index, any);
		pTarget = anyArray.GetPointer<uint64>(index);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == source);
		}

		// void AssignMoveValueAt(uint32 index, void* pSource, TypeIndex typeIndex);
		anyArray.AssignMoveValueAt(++index, static_cast<void*>(&source), m_pTypeDescUint64->GetIndex());
		pTarget = anyArray.GetPointer<uint64>(index);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == source);
		}

		// void AssignMoveValueAt(uint32 index, void* pSource, const ITypeDesc& typeDesc);
		anyArray.AssignMoveValueAt(++index, static_cast<void*>(&source), *m_pTypeDescUint64);
		pTarget = anyArray.GetPointer<uint64>(index);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == source);
		}
	}

	if (m_pTypeDescCBank)
	{
		CBank bank;
		bank.SetAccount(0xeeffc0);
		CBank sourceBank(bank);

		Cry::Reflection::CAnyArray anyArray;
		anyArray.SetType(*m_pTypeDescCBank, false);
		anyArray.Resize(1);

		// template<typename VALUE_TYPE> void AssignMoveValueAt(uint32 index, const VALUE_TYPE& source);
		anyArray.AssignMoveValueAt(0, bank);
		const CBank* pTarget = anyArray.GetConstPointer<CBank>(0);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(pTarget->GetAccount() == sourceBank.GetAccount());
			REQUIRE(pTarget->m_type == sourceBank.m_type);
			REQUIRE(pTarget->GetAccount() != bank.GetAccount());

		}
	}
}

TEST_F(CReflectionUnitTest, CAnyArrayAssignPointerAt)
{
	if (m_pTypeDescUint64)
	{
		uint32 index = 0;

		const uint64 source = 0xeeffc0;
		Cry::Reflection::CAnyArray anyArray;
		anyArray.SetType(m_pTypeDescUint64->GetIndex(), true);
		anyArray.Resize(5);

		// template<typename VALUE_TYPE> void AssignPointerAt(uint32 index, const VALUE_TYPE* pPointer);
		anyArray.AssignPointerAt(index, &source);
		uint64* pTarget = anyArray.GetPointer<uint64>(index);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == source);
		}

		// void AssignPointerAt(uint32 index, const CAny& source);
		Cry::Reflection::CAny any(&source);
		anyArray.AssignPointerAt(++index, any);
		pTarget = anyArray.GetPointer<uint64>(index);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == source);
		}

		// void AssignPointerAt(uint32 index, const void* pPointer, TypeIndex typeIndex);
		anyArray.AssignPointerAt(++index, static_cast<const void*>(&source), m_pTypeDescUint64->GetIndex());
		pTarget = anyArray.GetPointer<uint64>(index);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == source);
		}

		// void AssignPointerAt(uint32 index, const void* pPointer, const ITypeDesc& typeDesc);
		anyArray.AssignPointerAt(++index, static_cast<const void*>(&source), *m_pTypeDescUint64);
		pTarget = anyArray.GetPointer<uint64>(index);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(*pTarget == source);
		}
	}

	if (m_pTypeDescCBank)
	{
		CBank* pBank = new CBank();
		Cry::Reflection::CAnyArray bankArray(*m_pTypeDescCBank, true);
		bankArray.Resize(2);

		// template<typename VALUE_TYPE> void AssignPointerAt(uint32 index, const VALUE_TYPE* pPointer);
		bankArray.AssignPointerAt(0, pBank);
		bankArray.AssignPointerAt(1, pBank);
		CBank* pTargetBank = bankArray.GetPointer<CBank>(0);
		REQUIRE(pTargetBank);
		if (pTargetBank)
		{
			REQUIRE(pTargetBank == pBank);
			void* pOtherBank = bankArray.GetPointer(1);
			REQUIRE(pOtherBank);
			if (pOtherBank)
			{
				REQUIRE(static_cast<void*>(pTargetBank) == bankArray.GetPointer(1));
			}
		}
		delete pBank;
	}
}

TEST_F(CReflectionUnitTest, CAnyArrayGetPointer)
{
	if (m_pTypeDescUint64)
	{
		Cry::Reflection::CAnyArray anyArray(*m_pTypeDescUint64, false);
		anyArray.SetConst(true);

		uint32 index = 0;
		EXPECT_ASSERT_FAILURE(REQUIRE(anyArray.GetPointer(index) == nullptr), "Const element can't be accessed as non-const pointer.");
		EXPECT_ASSERT_FAILURE(REQUIRE(anyArray.GetPointer<uint64>(index) == nullptr), "Const element can't be accessed as non-const pointer.");
		EXPECT_ASSERT_FAILURE(REQUIRE(anyArray.GetConstPointer(index) == nullptr), "'index' (0) is out of bounds (0).");
		EXPECT_ASSERT_FAILURE(REQUIRE(anyArray.GetConstPointer<uint64>(index) == nullptr), "'index' (0) is out of bounds (0).");

		anyArray.Resize(1);
		EXPECT_ASSERT_FAILURE(REQUIRE(anyArray.GetPointer(index) == nullptr), "Const element can't be accessed as non-const pointer.");
		EXPECT_ASSERT_FAILURE(REQUIRE(anyArray.GetPointer<uint64>(index) == nullptr), "Const element can't be accessed as non-const pointer.");
		REQUIRE(anyArray.GetConstPointer(index));
		REQUIRE(anyArray.GetConstPointer<uint64>(index));

		anyArray.SetConst(false);
		uint64* pPointer = anyArray.GetPointer<uint64>(index);
		REQUIRE(pPointer);
		if (pPointer)
		{
			*pPointer = 0xeeffc0;

			anyArray.SetConst(true);
			EXPECT_ASSERT_FAILURE(REQUIRE(anyArray.GetPointer<uint64>(index) == nullptr), "Const element can't be accessed as non-const pointer.");

			const void* pConstVoidPointer = anyArray.GetConstPointer(index);
			const uint64* pConstPointer = anyArray.GetConstPointer<uint64>(index);
			REQUIRE(pConstVoidPointer);
			REQUIRE(pConstPointer);

			if (pConstVoidPointer && pConstPointer)
			{
				REQUIRE(*static_cast<const uint64*>(pConstVoidPointer) == *pPointer);
			}
		}
	}
}

TEST_F(CReflectionUnitTest, CAnyArrayRemove)
{
	if (m_pTypeDescUint64)
	{
		Cry::Reflection::CAnyArray anyArray(*m_pTypeDescUint64, false);
		anyArray.Reserve(4);

		const uint64 source = 0xeeffc0;
		anyArray.AddValue(source);
		anyArray.AddValue(source);
		anyArray.AddValue(source);
		anyArray.AddValue(source);

		anyArray.Remove(3);
		REQUIRE(anyArray.GetSize() == 3);

		uint64* pValue = anyArray.GetPointer<uint64>(0);
		REQUIRE(pValue);
		REQUIRE(*pValue == source);

		pValue = anyArray.GetPointer<uint64>(1);
		REQUIRE(pValue);
		REQUIRE(*pValue == source);

		pValue = anyArray.GetPointer<uint64>(2);
		REQUIRE(pValue);
		REQUIRE(*pValue == source);

		anyArray.Remove(0);
		REQUIRE(anyArray.GetSize() == 2);

		pValue = anyArray.GetPointer<uint64>(0);
		REQUIRE(pValue);
		REQUIRE(*pValue == source);

		pValue = anyArray.GetPointer<uint64>(1);
		REQUIRE(pValue);
		REQUIRE(*pValue == source);
	}

	if (m_pTypeDescCBank)
	{
		CBank bank;
		bank.SetAccount(0xeeffc0);
		Cry::Reflection::CAnyArray anyArray(m_pTypeDescCBank->GetIndex(), false);
		anyArray.Reserve(2);
		anyArray.AddValue(bank);
		anyArray.AddValue(bank);

		anyArray.Remove(0);
		REQUIRE(anyArray.GetSize() == 1);
		const CBank* pTarget = anyArray.GetConstPointer<CBank>(0);
		REQUIRE(pTarget);
		if (pTarget)
		{
			REQUIRE(pTarget->GetAccount() == bank.GetAccount());
		}

		anyArray.Remove(0);
		REQUIRE(anyArray.GetSize() == 0);
		EXPECT_ASSERT_FAILURE(REQUIRE(anyArray.GetPointer<CBank>(0) == nullptr), "'index' (0) is out of bounds (0).");
	}
}

TEST_F(CReflectionUnitTest, CAnyArraySerialization)
{
	REQUIRE(m_pArchiveHost);
	if (m_pArchiveHost)
	{
		const Cry::Reflection::ITypeDesc* pTypeDesc = Cry::Reflection::CoreRegistry::GetTypeById(Cry::Type::IdOf<uint64>());
		REQUIRE(pTypeDesc);
		if (pTypeDesc)
		{
			DynArray<char> buffer;

			Cry::Reflection::CAnyArray anyTarget;
			Cry::Reflection::CAnyArray anySource(*pTypeDesc, false);
			anySource.Resize(4);

			const uint32 size = anySource.GetSize();
			REQUIRE(size == 4);
			for (uint32 i = 0; i < size; ++i)
			{
				uint64* pPointer = anySource.GetPointer<uint64>(i);
				REQUIRE(pPointer);
				if (pPointer)
				{
					*pPointer = i;
				}
			}

			m_pArchiveHost->SaveBinaryBuffer(buffer, Serialization::SStruct(anySource));
			m_pArchiveHost->LoadBinaryBuffer(Serialization::SStruct(anyTarget), buffer.data(), buffer.size());

			REQUIRE(anySource.GetTypeIndex() == anyTarget.GetTypeIndex());
			REQUIRE(anySource.IsPointer() == anyTarget.IsPointer());
			REQUIRE(anySource.IsConst() == anyTarget.IsConst());
			REQUIRE(anySource.GetSize() == anyTarget.GetSize());
			REQUIRE(anySource.GetCapacity() == anyTarget.GetCapacity());

			for (uint32 i = 0; i < size; ++i)
			{
				uint64* pSource = anySource.GetPointer<uint64>(i);
				uint64* pTarget = anyTarget.GetPointer<uint64>(i);
				REQUIRE(pSource);
				REQUIRE(pTarget);
				if (pSource && pTarget)
				{
					REQUIRE(*pSource == *pTarget);
				}
			}
		}
	}
}

TEST_F(CReflectionUnitTest, CAnyArrayToString)
{
	if (m_pTypeDescCBank)
	{
		CBank bank;
		CBank* pBank = new CBank();
		bank.SetAccount(0xeeffc0);

		Cry::Reflection::CAnyArray anyArray;
		anyArray.SetType(*m_pTypeDescCBank, true);
		anyArray.Reserve(2);
		anyArray.AddPointer(&bank);
		anyArray.AddPointer(pBank);
		anyArray.SetConst(true);

		string sourceString;
		sourceString.Format("{ \"m_pData\": %p, \"m_size\": %lu, \"m_capacity\": %lu, \"m_typeIndex\": %u, \"m_isPointer\": %u, \"m_isConst\": %u }",
		                    static_cast<void*>(anyArray.GetBuffer()), 2, 2, m_pTypeDescCBank->GetIndex().ToValueType(), 1, 1);
		string targetString = anyArray.ToString();

		REQUIRE(targetString == sourceString);
		delete pBank;
	}
}
