// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <CryCore/Platform/platform.h>
#include <CryReflection/Framework.h>
#include "ReflectionBase.h"

// Fundamental Types
void UnitTest_ReflectBool(Cry::Reflection::ITypeDesc& typeDesc)
{
	typeDesc.SetLabel("bool");
	typeDesc.SetDescription("A 1-byte integral type that can have one of the two values true or false.");
}
CRY_REFLECT_TYPE(bool, "{BB67FE0B-744E-4253-AD72-03D80CD1734B}"_cry_guid, UnitTest_ReflectBool);

void UnitTest_ReflectChar(Cry::Reflection::ITypeDesc& typeDesc)
{
	typeDesc.SetLabel("char");
	typeDesc.SetDescription("A 1-byte integral type that usually contains members of the basic execution character set.");
}
CRY_REFLECT_TYPE(char, "{CC1A3C72-4819-46F6-B8F6-992D9F45CCFB}"_cry_guid, UnitTest_ReflectChar);

void UnitTest_ReflectFloat(Cry::Reflection::ITypeDesc& typeDesc)
{
	typeDesc.SetLabel("float");
	typeDesc.SetDescription("A 4-byte floating point type.");
}
CRY_REFLECT_TYPE(float, "{FF1355EC-96DA-4D56-87BB-D36A2A2CDAD3}"_cry_guid, UnitTest_ReflectFloat);

void UnitTest_ReflectUInt64(Cry::Reflection::ITypeDesc& typeDesc)
{
	typeDesc.SetLabel("uint64");
	typeDesc.SetDescription("A 8-byte unsigned integral type.");
}
CRY_REFLECT_TYPE(uint64, "{64BE4B59-A9CD-46B3-8944-77349FD2F456}"_cry_guid, UnitTest_ReflectUInt64);

void UnitTest_ReflectUInt32(Cry::Reflection::ITypeDesc& typeDesc)
{
	typeDesc.SetLabel("uint32");
	typeDesc.SetDescription("A 4-byte unsigned integral type.");
}
CRY_REFLECT_TYPE(uint32, "{3202DE0D-FD01-4B75-B536-F27B4C305E14}"_cry_guid, UnitTest_ReflectUInt32);

void UnitTest_ReflectUInt16(Cry::Reflection::ITypeDesc& typeDesc)
{
	typeDesc.SetLabel("uint16");
	typeDesc.SetDescription("A 2-byte unsigned integral type.");
}
CRY_REFLECT_TYPE(uint16, "{163AA70B-7DEA-435A-B726-BFDAFB6AA2F7}"_cry_guid, UnitTest_ReflectUInt16);

void UnitTest_ReflectUInt8(Cry::Reflection::ITypeDesc& typeDesc)
{
	typeDesc.SetLabel("uint8");
	typeDesc.SetDescription("A 1-byte unsigned integral type.");
}
CRY_REFLECT_TYPE(uint8, "{82EA5C18-75D3-4333-8E95-B6BE1A79EFB3}"_cry_guid, UnitTest_ReflectUInt8);

void UnitTest_ReflectInt64(Cry::Reflection::ITypeDesc& typeDesc)
{
	typeDesc.SetLabel("int64");
	typeDesc.SetDescription("A 8-byte signed integral type.");
}
CRY_REFLECT_TYPE(int64, "{64E9AB2C-E182-45B2-822C-3E2BD70B96FE}"_cry_guid, UnitTest_ReflectInt64);

void UnitTest_ReflectInt32(Cry::Reflection::ITypeDesc& typeDesc)
{
	typeDesc.SetLabel("int32");
	typeDesc.SetDescription("A 4-byte signed integral type.");
}
CRY_REFLECT_TYPE(int32, "{32AA5FA9-5F45-42D7-9F9A-F919567BE0C7}"_cry_guid, UnitTest_ReflectInt32);

void UnitTest_ReflectInt16(Cry::Reflection::ITypeDesc& typeDesc)
{
	typeDesc.SetLabel("int16");
	typeDesc.SetDescription("A 2-byte signed integral type.");
}
CRY_REFLECT_TYPE(int16, "{16EF4C9A-9BAB-4B5D-94B1-20D8C0DA75DA}"_cry_guid, UnitTest_ReflectInt16);

void UnitTest_ReflectInt8(Cry::Reflection::ITypeDesc& typeDesc)
{
	typeDesc.SetLabel("int8");
	typeDesc.SetDescription("A 1-byte unsigned integral type.");
}
CRY_REFLECT_TYPE(int8, "{8BD5825E-1878-4D11-9AE9-911109CFDCAE}"_cry_guid, UnitTest_ReflectInt8);

void UnitTest_ReflectBankType(Cry::Reflection::ITypeDesc& typeDesc)
{
	typeDesc.SetLabel("Bank Type");
	typeDesc.SetDescription("Enumeration of different bank types.");

	CRY_REFLECT_ENUM_VALUE(typeDesc, "Retail", eBankType_Retail, "Deals directily with consumers and small buisness owners.");
	CRY_REFLECT_ENUM_VALUE(typeDesc, "Buisness", eBankType_Buisness, "Provides services to medium sized buinsesses and other organizations.");
	CRY_REFLECT_ENUM_VALUE(typeDesc, "Investment", eBankType_Investment, "Provides services related to financial markets.");
}
CRY_REFLECT_TYPE(EBankType, "{6487CCAC-BF2A-430F-8A35-5D24D8A36BE4}"_cry_guid, UnitTest_ReflectBankType);

// Class Type
void CVault::ReflectVault(Cry::Reflection::ITypeDesc& typeDesc)
{
	typeDesc.SetLabel("Vault");
	typeDesc.SetDescription("A Vault to store all your secrets.");

	CRY_REFLECT_MEMBER_VARIABLE(typeDesc, CVault::m_public, "PublicNumber", "Not a secret.", "{F4310AC9-CC2E-4C96-8F96-015D845A1711}"_cry_guid, true);
	CRY_REFLECT_MEMBER_VARIABLE(typeDesc, CVault::m_secret, "SecretNumber", "This is not public.", "{D63DC617-114D-408C-AB01-766FFB3EA51E}"_cry_guid, false);
}

CVault::CVault()
	: m_public(42)
	, m_secret(1337)
	, m_legacy("s3cr3tANDs3cur3!")
	, m_garbage("bckdor")
{
}

CVault::~CVault()
{
	m_public = 42;
}

CVault::CVault(const CVault& other)
	: m_public(other.m_public)
	, m_secret(other.m_secret)
{
	cry_strcpy(m_legacy, other.m_legacy, CRY_ARRAY_COUNT(m_legacy));
	cry_strcpy(m_garbage, other.m_garbage, CRY_ARRAY_COUNT(m_garbage));
}

bool CVault::Serialize(Serialization::IArchive& archive)
{
	archive(m_legacy, "legacy");
	archive(m_public, "public");
	archive(m_garbage, "garbage");
	archive(m_secret, "secret");
	return true;
}

string CVault::ToString() const
{
	string str;
	str.Format("{ \"m_legacy\": %.17s, \"m_public\": %llu, \"m_garbage\": %.7s, \"m_secret\": %llu }",
		m_legacy, m_public, m_garbage, m_secret);
	return str;
}

CVault& CVault::operator=(const CVault& other)
{
	if (this != &other)
	{
		m_public = other.m_public;
		cry_strcpy(m_legacy, other.m_legacy, CRY_ARRAY_COUNT(m_legacy));
		cry_strcpy(m_garbage, other.m_garbage, CRY_ARRAY_COUNT(m_garbage));
		m_secret = other.m_secret;
	}
	return *this;
}

CVault& CVault::operator=(CVault&& other)
{
	if (this != &other)
	{
		m_public = other.m_public;
		cry_strcpy(m_legacy, other.m_legacy, CRY_ARRAY_COUNT(m_legacy));
		cry_strcpy(m_garbage, other.m_garbage, CRY_ARRAY_COUNT(m_garbage));
		m_secret = other.m_secret;

		other.m_public = 42;
	}
	return *this;
}

bool CVault::operator==(const CVault& other) const
{
	bool areEqual = true;
	areEqual &= m_public == other.m_public;
	areEqual &= m_secret == other.m_secret;

	for (int i = 0; i < CRY_ARRAY_COUNT(m_legacy); ++i)
	{
		areEqual &= m_legacy[i] == other.m_legacy[i];
	}
	for (int i = 0; i < CRY_ARRAY_COUNT(m_garbage); ++i)
	{
		areEqual &= m_garbage[i] == other.m_garbage[i];
	}

	return areEqual;
}
CRY_REFLECT_TYPE(CVault, "{6F6C65AA-0551-4DDB-8DC2-B873A8A72052}"_cry_guid, CVault::ReflectVault);

void CBank::ReflectBank(Cry::Reflection::ITypeDesc& typeDesc)
{
	typeDesc.SetLabel("Bank");
	typeDesc.SetDescription("This Bank needs better management.");

	CRY_REFLECT_BASE(typeDesc, CBank, CVault);
	CRY_REFLECT_MEMBER_VARIABLE(typeDesc, CBank::m_account, "Account", "Stores debts and profits.", "{A89799C3-F359-4FCF-902F-BF686AAD1FE8}"_cry_guid, false);
	CRY_REFLECT_MEMBER_VARIABLE(typeDesc, CBank::m_type, "Type", "Enum that specifies bank type.", "{2F3F3D99-A3A2-450A-9AB0-62C94080523B}"_cry_guid, true);
	CRY_REFLECT_MEMBER_FUNCTION(typeDesc, CBank::SetAccount, "SetAccount", "Set account to the given amount.", "{0D2313E4-575A-4BC1-B403-8CFD54290145}"_cry_guid);
	CRY_REFLECT_MEMBER_FUNCTION(typeDesc, CBank::GetAccount, "GetAccount", "Retrieve balance of account.", "{39FC35A1-5377-4429-8DB6-9F7E032F064A}"_cry_guid);
}

CBank::CBank()
	: m_type(eBankType_Investment)
	, m_account(-1)
{
	for (size_t i = 0; i < CBank::s_documentCount; ++i)
	{
		m_documents[i] = 's';
	}
}

CBank::~CBank()
{
	m_type = eBankType_Investment;
	m_account = -1;
}

CBank::CBank(const CBank& other)
{
	m_type = other.m_type;
	m_account = other.m_account;
}

CBank::CBank(CBank&& other)
{
	m_type = other.m_type;
	m_account = other.m_account;

	other.m_type = eBankType_Investment;
	other.m_account = -1;
}

bool CBank::Serialize(Serialization::IArchive& archive)
{
	archive(m_type, "type");
	archive(m_account, "account");

	return true;
}

string CBank::ToString() const
{
	string str;
	str.Format("{ \"m_type\": %i, \"m_account\": %li }", m_type, m_account);
	return str;
}

CBank& CBank::operator=(const CBank& other)
{
	if (this != &other)
	{
		m_type = other.m_type;
		m_account = other.m_account;
	}
	return *this;
}

CBank& CBank::operator=(CBank&& other)
{
	if (this != &other)
	{
		m_type = other.m_type;
		m_account = other.m_account;

		other.m_type = eBankType_Investment;
		other.m_account = -1;
	}
	return *this;
}

bool CBank::operator==(const CBank& other) const
{
	return (m_type == other.m_type) && (m_account == other.m_account);
}

CRY_REFLECT_TYPE(CBank, "{BA9ECBE3-FA87-4972-96B6-70B3AF6DE72B}"_cry_guid, CBank::ReflectBank);
