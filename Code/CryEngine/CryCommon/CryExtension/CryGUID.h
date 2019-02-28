// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/IArchive.h>
#include <CrySerialization/TypeID.h>

#include <functional>

//! Globally Unique Identifier binary compatible with standard 128bit GUID
struct CryGUID
{
	uint64 hipart;
	uint64 lopart;

	//////////////////////////////////////////////////////////////////////////
	// METHODS
	//////////////////////////////////////////////////////////////////////////
	//remark: this custom constructor will turn CryGUID into a non-aggregate
	constexpr CryGUID() : hipart(0),lopart(0) {}

	constexpr CryGUID(const CryGUID& rhs) : hipart(rhs.hipart),lopart(rhs.lopart) {}

	constexpr CryGUID(const uint64& hipart_, const uint64& lopart_) : hipart(hipart_),lopart(lopart_) {}

	constexpr static CryGUID Construct(const uint64& hipart, const uint64& lopart)
	{
		return CryGUID{ hipart, lopart };
	}

	constexpr static CryGUID Construct(uint32 d1, uint16 d2, uint16 d3, uint8 d4[8])
	{
		return CryGUID(
			(((uint64)d3) << 48) | (((uint64)d2) << 32) | ((uint64)d1),  //high part
			*((uint64*)d4)  //low part
		 );
	}
	
	constexpr static CryGUID Construct(uint32 d1, uint16 d2, uint16 d3,
		uint8 d4_0, uint8 d4_1, uint8 d4_2, uint8 d4_3, uint8 d4_4, uint8 d4_5, uint8 d4_6, uint8 d4_7)
	{
		return CryGUID(
			(((uint64)d3) << 48) | (((uint64)d2) << 32) | ((uint64)d1),  //high part
			(((uint64)d4_7) << 56) | (((uint64)d4_6) << 48) | (((uint64)d4_5) << 40) | (((uint64)d4_4) << 32) | (((uint64)d4_3) << 24) | (((uint64)d4_2) << 16) | (((uint64)d4_1) << 8) | (uint64)d4_0   //low part
		);
	}

	inline static CryGUID Create();

	constexpr static CryGUID Null()
	{
		return CryGUID::Construct( 0, 0 );
	}

	constexpr bool IsNull() const { return hipart == 0 && lopart == 0; }

	constexpr bool operator==(const CryGUID& rhs) const { return hipart == rhs.hipart && lopart == rhs.lopart; }
	constexpr bool operator!=(const CryGUID& rhs) const { return hipart != rhs.hipart || lopart != rhs.lopart; }
	constexpr bool operator<(const CryGUID& rhs) const  { return hipart < rhs.hipart || (hipart == rhs.hipart) && lopart < rhs.lopart; }
	constexpr bool operator>(const CryGUID& rhs) const  { return hipart > rhs.hipart || (hipart == rhs.hipart) && lopart > rhs.lopart; }
	constexpr bool operator<=(const CryGUID& rhs) const { return hipart < rhs.hipart || (hipart == rhs.hipart) && lopart <= rhs.lopart; }
	constexpr bool operator>=(const CryGUID& rhs) const { return hipart > rhs.hipart || (hipart == rhs.hipart) && lopart >= rhs.lopart; }

	struct StringUtils
	{
		// Convert hexadecimal character to unsigned char.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		constexpr static uint8 HexCharToUInt8(char x) 
		{
			return x >= '0' && x <= '9' ? x - '0' :
				x >= 'a' && x <= 'f' ? x - 'a' + 10 :
				x >= 'A' && x <= 'F' ? x - 'A' + 10 : 0;
		}

		// Convert hexadecimal character to unsigned short.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		constexpr static uint16 HexCharToUInt16(char x)
		{
			return static_cast<uint16>(HexCharToUInt8(x));
		}

		// Convert hexadecimal character to unsigned long.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		constexpr static uint32 HexCharToUInt32(char x)
		{
			return static_cast<uint32>(HexCharToUInt8(x));
		}

		// Convert hexadecimal string to unsigned char.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		constexpr static uint8 HexStringToUInt8(const char* szInput)
		{
			return (HexCharToUInt8(szInput[0]) << 4) +
				HexCharToUInt8(szInput[1]);
		}

		// Convert hexadecimal string to unsigned short.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		constexpr static uint16 HexStringToUInt16(const char* szInput)
		{
			return (HexCharToUInt16(szInput[0]) << 12) +
				(HexCharToUInt16(szInput[1]) << 8) +
				(HexCharToUInt16(szInput[2]) << 4) +
				HexCharToUInt16(szInput[3]);
		}

		// Convert hexadecimal string to unsigned long.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		constexpr static uint32 HexStringToUInt32(const char* szInput)
		{
			return (HexCharToUInt32(szInput[0]) << 28) +
				(HexCharToUInt32(szInput[1]) << 24) +
				(HexCharToUInt32(szInput[2]) << 20) +
				(HexCharToUInt32(szInput[3]) << 16) +
				(HexCharToUInt32(szInput[4]) << 12) +
				(HexCharToUInt32(szInput[5]) << 8) +
				(HexCharToUInt32(szInput[6]) << 4) +
				HexCharToUInt32(szInput[7]);
		}
		struct SGuidStringSerializer : public Serialization::IString
		{
			char buffer[40];

			SGuidStringSerializer() { memset(buffer,0,sizeof(buffer)); }
			void        set(const char* value) { cry_strcpy(buffer,value); }
			const char* get() const { return buffer; }
			const void* handle() const { return &buffer; }
			Serialization::TypeID      type() const { return Serialization::TypeID::get<SGuidStringSerializer>(); }
		};
		struct GUID_mapped
		{
			uint32 Data1;
			uint16 Data2;
			uint16 Data3;
			uint8  Data4[8];
		};
	};

	//////////////////////////////////////////////////////////////////////////
	// Create GUID from string.
	// GUID must be in form of this example string (case insensitive): "296708CE-F570-4263-B067-C6D8B15990BD"
	////////////////////////////////////////////////////////////////////////////////////////////////////
	constexpr static CryGUID FromStringInternal(const char* szInput)
	{
		return CryGUID::Construct(
			StringUtils::HexStringToUInt32(szInput),        // uint32 data1
			StringUtils::HexStringToUInt16(szInput + 9),   // uint16 data2
			StringUtils::HexStringToUInt16(szInput + 14),  // uint16 data3
			StringUtils::HexStringToUInt8(szInput + 19),   // uint8  data4[8]
			StringUtils::HexStringToUInt8(szInput + 21),
			StringUtils::HexStringToUInt8(szInput + 24),
			StringUtils::HexStringToUInt8(szInput + 26),
			StringUtils::HexStringToUInt8(szInput + 28),
			StringUtils::HexStringToUInt8(szInput + 30),
			StringUtils::HexStringToUInt8(szInput + 32),
			StringUtils::HexStringToUInt8(szInput + 34)
		);
	};

	//! Write GUID to zero terminated character array.
	//! Require 36 bytes
	inline void ToString(char *output,size_t const output_size_in_bytes) const
	{
		static_assert(sizeof(StringUtils::GUID_mapped) == sizeof(CryGUID), "GUID_mapped and CryGUID must be the same size");

		const StringUtils::GUID_mapped* pMappedGUID = alias_cast<StringUtils::GUID_mapped*>(this);

		unsigned int Data1;
		unsigned int Data2;
		unsigned int Data3;
		unsigned int Data4[8];

		Data1 = static_cast<unsigned int>(pMappedGUID->Data1);
		Data2 = static_cast<unsigned int>(pMappedGUID->Data2);
		Data3 = static_cast<unsigned int>(pMappedGUID->Data3);

		Data4[0] = static_cast<unsigned int>(pMappedGUID->Data4[0]);
		Data4[1] = static_cast<unsigned int>(pMappedGUID->Data4[1]);
		Data4[2] = static_cast<unsigned int>(pMappedGUID->Data4[2]);
		Data4[3] = static_cast<unsigned int>(pMappedGUID->Data4[3]);
		Data4[4] = static_cast<unsigned int>(pMappedGUID->Data4[4]);
		Data4[5] = static_cast<unsigned int>(pMappedGUID->Data4[5]);
		Data4[6] = static_cast<unsigned int>(pMappedGUID->Data4[6]);
		Data4[7] = static_cast<unsigned int>(pMappedGUID->Data4[7]);

		cry_sprintf(output,output_size_in_bytes,"%.8x-%.4x-%.4x-%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x", Data1, Data2, Data3, Data4[0], Data4[1], Data4[2], Data4[3], Data4[4], Data4[5], Data4[6], Data4[7]);
	}

	//////////////////////////////////////////////////////////////////////////
	// Create GUID from string.
	// GUID must be in form of this example string (case insensitive): "296708CE-F570-4263-B067-C6D8B15990BD"
	// GUID can also be with curly brackets: "{296708CE-F570-4263-B067-C6D8B15990BD}"
	////////////////////////////////////////////////////////////////////////////////////////////////////
	static CryGUID FromString(const char* szGuidAsString)
	{
		CRY_ASSERT(szGuidAsString);
		CryGUID guid;
		if (szGuidAsString)
		{
			const size_t len = strlen(szGuidAsString);
			if (szGuidAsString[0] == '{' && len >= CRY_ARRAY_COUNT("{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}") - 1)
			{
				guid = FromStringInternal(szGuidAsString + 1);
			}
			else if (szGuidAsString[0] != '{' && len >= CRY_ARRAY_COUNT("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX") - 1)
			{
				guid = FromStringInternal(szGuidAsString);
			}
			else if (len <= sizeof(uint64) * 2 && std::all_of(szGuidAsString, szGuidAsString + len, ::isxdigit)) // Convert if it still coming from the old 64bit GUID system.
			{
				guid.hipart = std::strtoull(szGuidAsString, 0, 16);
			}
			else
			{
				CRY_ASSERT_MESSAGE(false, "GUID string is invalid: %s", szGuidAsString);
			}
		}
		return guid;
	};

	template<std::size_t N>
	void ToString(char(&ar)[N]) const
	{
		static_assert(N >= CRY_ARRAY_COUNT("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX") - 1, "GUID require buffer of at least 36 bytes");
		ToString((char*)ar, N);
	}
	
	string ToString() const
	{
		char temp[CRY_ARRAY_COUNT("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX")];
		ToString(temp);
		return string(temp);
	}

	//! Returns a character string used for Debugger Visualization or log messages.
	//! Do not use this method in runtime code.
	const char* ToDebugString() const
	{
		static char temp[CRY_ARRAY_COUNT("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX")];
		ToString(temp);
		return temp;
	}

	//! Serialize GUID as a two 64bit unsigned integers
	static bool SerializeAsNumber(Serialization::IArchive& ar,CryGUID &guid)
	{
		bool bResult = false;
		if (ar.isInput())
		{
			uint32 dwords[4];
			bResult = ar(dwords, "guid");
			guid.lopart = (((uint64)dwords[1]) << 32) | (uint64)dwords[0];
			guid.hipart = (((uint64)dwords[3]) << 32) | (uint64)dwords[2];
		}
		else
		{
			uint32 g[4] = {
				(uint32)(guid.lopart & 0xFFFFFFFF), (uint32)((guid.lopart >> 32) & 0xFFFFFFFF),
				(uint32)(guid.hipart & 0xFFFFFFFF), (uint32)((guid.hipart >> 32) & 0xFFFFFFFF)
			};
			bResult = ar(g, "guid");
		}
		return bResult;
	}

	//! Serialize GUID as a string in form XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
	static bool SerializeAsString(Serialization::IArchive& archive, CryGUID& guid, const char* szName, const char* szLabel)
	{
		StringUtils::SGuidStringSerializer strSerializer;
		if (archive.isInput())
		{
			if (archive(strSerializer, szName, szLabel))
			{
				guid = CryGUID::FromStringInternal(strSerializer.buffer);
			}
			else
			{
				guid = CryGUID::Null();
				return false;
			}
		}
		else if (archive.isOutput())
		{
			if (!guid.IsNull())
			{
				guid.ToString(strSerializer.buffer);
			}
			archive(strSerializer, szName, szLabel);
		}
		return true;
	}
};

//////////////////////////////////////////////////////////////////////////
//! GUID string literal.
//! Example: "e397e62c-5c7f-4fab-9195-12032f670c9f"_cry_guid
//! creates a valid GUID usable in static initialization
////////////////////////////////////////////////////////////////////////////////////////////////////
constexpr CryGUID operator"" _cry_guid(const char* szInput, size_t)
{
	// Can accept guids in both forms as 
	// BAC0332E-AB02-4276-9731-AB48865E0411 or as {BAC0332E-AB02-4276-9731-AB48865E0411}
	return (szInput[0] == '{') ? CryGUID::FromStringInternal(szInput+1) : CryGUID::FromStringInternal(szInput);
}

//! Global yasli serialization override for the CryGUID
inline bool Serialize(Serialization::IArchive& ar, CryGUID &guid,const char* szName, const char* szLabel = nullptr)
{
	return CryGUID::SerializeAsString(ar, guid, szName, szLabel);
}

namespace std
{
template<> struct hash<CryGUID>
{
public:
	size_t operator()(const CryGUID& guid) const
	{
		std::hash<uint64> hasher;
		return hasher(guid.lopart) ^ hasher(guid.hipart);
	}
};
}

inline bool Serialize(Serialization::IArchive& ar, CryGUID::StringUtils::SGuidStringSerializer& value, const char* name, const char* label)
{
	return ar(static_cast<Serialization::IString&>(value), name, label);
}
