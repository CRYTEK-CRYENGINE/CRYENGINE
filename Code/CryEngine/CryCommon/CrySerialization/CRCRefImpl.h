// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IArchive.h"
#include "Serializer.h"

template<typename TCRCRef>
class CRCRefSerializer
	: public Serialization::IString
{
public:
	CRCRefSerializer(TCRCRef& crcRef)
		: m_crcRef(crcRef)
	{
	}

	virtual void set(const char* value)
	{
		m_crcRef.SetByString(value);
	}

	virtual const char* get() const
	{
		return m_crcRef.c_str();
	}

	const void* handle() const
	{
		return &m_crcRef;
	}

	Serialization::TypeID type() const
	{
		return Serialization::TypeID::get<TCRCRef>();
	}

	TCRCRef& m_crcRef;
};

template<uint32 StoreStrings, typename THash>
class CCRCRefSerializerNoStrings
{
public:
	CCRCRefSerializerNoStrings(struct SCRCRef<StoreStrings, THash>& crcRef)
		: crc(crcRef.crc)
	{
	}

	bool Serialize(Serialization::IArchive& ar)
	{
		return ar(crc, "CRC", "CRC");
	}

	typedef typename THash::TInt TInt;
	TInt& crc;
};

template<uint32 StoreStrings, typename THash>
bool Serialize(Serialization::IArchive& ar, struct SCRCRef<StoreStrings, THash>& crcRef, const char* name, const char* label)
{
	if (StoreStrings == 0)
	{
		if (ar.isInput())
		{
			SCRCRef<StoreStrings, THash> crcCopy;
			ar(CCRCRefSerializerNoStrings<StoreStrings, THash>(crcCopy), name, label);
			if (crcCopy.crc != THash::INVALID)
			{
				crcRef = crcCopy;
				return true;
			}
		}
		else if (ar.isOutput())
		{
			return ar(CCRCRefSerializerNoStrings<StoreStrings, THash>(crcRef), name, label);
		}
	}

	CRCRefSerializer<SCRCRef<StoreStrings, THash>> crcRefSerializer(crcRef);
	return ar(static_cast<Serialization::IString&>(crcRefSerializer), name, label);
}
