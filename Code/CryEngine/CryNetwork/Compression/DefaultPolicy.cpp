// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ICompressionPolicy.h"
#include "ArithModel.h"
#include "Protocol/Serialize.h"

class CDefaultPolicy
{
public:
	bool Load(XmlNodeRef node, const string& filename)
	{
		return true;
	}

#if USE_MEMENTO_PREDICTORS
	bool ReadMemento(CByteInputStream& in) const
	{
		return true;
	}

	bool WriteMemento(CByteOutputStream& out) const
	{
		return true;
	}

	void NoMemento() const
	{
	}
#endif

#if USE_ARITHSTREAM
	bool ReadValue(CCommInputStream& in, bool& value, CArithModel* pModel, uint32 age) const
	{
		value = in.ReadBits(1) != 0;
		return true;
	}
	bool WriteValue(CCommOutputStream& out, bool value, CArithModel* pModel, uint32 age) const
	{
		out.WriteBits(value ? 1 : 0, 1);
		return true;
	}

	template<class T>
	bool ReadValue(CCommInputStream& in, T& value, CArithModel* pModel, uint32 age) const
	{
		if (!ReadBytes(in, &value, sizeof(value)))
			return false;
		SwapEndian(value);
		return true;
	}
	template<class T>
	bool WriteValue(CCommOutputStream& out, T value, CArithModel* pModel, uint32 age) const
	{
		SwapEndian(value);
		return WriteBytes(out, &value, sizeof(value));
	}

	bool ReadValue(CCommInputStream& in, CTimeValue& value, CArithModel* pModel, uint32 age) const
	{
		value = pModel->ReadTime(in, eTS_Network);
		return true;
	}
	bool WriteValue(CCommOutputStream& out, CTimeValue value, CArithModel* pModel, uint32 age) const
	{
		pModel->WriteTime(out, eTS_Network, value);
		return true;
	}

	bool ReadValue(CCommInputStream& in, ScriptAnyValue& value, CArithModel* pModel, uint32 age) const
	{
		NET_ASSERT(!"script values not supported");
		NetWarning("Network serialization of script types is not supported");
		return false;
	}
	bool WriteValue(CCommOutputStream& out, const ScriptAnyValue& value, CArithModel* pModel, uint32 age) const
	{
		NET_ASSERT(!"script values not supported");
		NetWarning("Network serialization of script types is not supported");
		return false;
	}

	bool ReadValue(CCommInputStream& in, XmlNodeRef& value, CArithModel* pModel, uint32 age) const
	{
		NET_ASSERT(!"XmlNodeRef not supported");
		NetWarning("Network serialization of script types is not supported");
		return false;
	}
	bool WriteValue(CCommOutputStream& out, const XmlNodeRef& value, CArithModel* pModel, uint32 age) const
	{
		NET_ASSERT(!"XmlNodeRef not supported");
		NetWarning("Network serialization of script types is not supported");
		return false;
	}

	bool ReadValue(CCommInputStream& in, SSerializeString& value, CArithModel* pModel, uint32 age) const
	{
		string s;
		bool bRes = pModel->ReadString(in, s);
		if (bRes)
			value = s.c_str();
		return bRes;
	}
	bool WriteValue(CCommOutputStream& out, const SSerializeString& value, CArithModel* pModel, uint32 age) const
	{
		pModel->WriteString(out, string(value.c_str()));
		return true;
	}
#else
	bool ReadValue(CNetInputSerializeImpl* in, bool& value, uint32 age) const
	{
		value = in->ReadBits(1) != 0;
		return true;
	}
	bool WriteValue(CNetOutputSerializeImpl* out, bool value, uint32 age) const
	{
		out->WriteBits(value ? 1 : 0, 1);
		return true;
	}

	template<class T>
	bool ReadValue(CNetInputSerializeImpl* in, T& value, uint32 age) const
	{
		if (!ReadBytes(in, &value, sizeof(value)))
			return false;
		SwapEndian(value);
		return true;
	}
	template<class T>
	bool WriteValue(CNetOutputSerializeImpl* out, T value, uint32 age) const
	{
		SwapEndian(value);
		return WriteBytes(out, &value, sizeof(value));
	}

	bool ReadValue(CNetInputSerializeImpl* in, CTimeValue& value, uint32 age) const
	{
		value = in->ReadTime(eTS_Network);
		return true;
	}
	bool WriteValue(CNetOutputSerializeImpl* out, CTimeValue value, uint32 age) const
	{
		out->WriteTime(eTS_Network, value);
		return true;
	}

	bool ReadValue(CNetInputSerializeImpl* in, ScriptAnyValue& value, uint32 age) const
	{
		NET_ASSERT(!"script values not supported");
		NetWarning("Network serialization of script types is not supported");
		return false;
	}
	bool WriteValue(CNetOutputSerializeImpl* out, const ScriptAnyValue& value, uint32 age) const
	{
		NET_ASSERT(!"script values not supported");
		NetWarning("Network serialization of script types is not supported");
		return false;
	}

	bool ReadValue(CNetInputSerializeImpl* in, XmlNodeRef& value, uint32 age) const
	{
		NET_ASSERT(!"XmlNodeRef not supported");
		NetWarning("Network serialization of script types is not supported");
		return false;
	}
	bool WriteValue(CNetOutputSerializeImpl* out, const XmlNodeRef& value, uint32 age) const
	{
		NET_ASSERT(!"XmlNodeRef not supported");
		NetWarning("Network serialization of script types is not supported");
		return false;
	}

	bool ReadValue(CNetInputSerializeImpl* in, SSerializeString& value, uint32 age) const
	{
		in->ReadString(&value);
		return true;
	}
	bool WriteValue(CNetOutputSerializeImpl* out, const SSerializeString& value, uint32 age) const
	{
		out->WriteString(&value);
		return true;
	}
#endif

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CDefaultPolicy");
		pSizer->Add(*this);
	}
#if NET_PROFILE_ENABLE
	int GetBitCount(bool value)
	{
		return BITCOUNT_BOOL;
	}

	template<class T>
	int GetBitCount(T value)
	{
		return sizeof(value) * 8;
	}

	int GetBitCount(CTimeValue value)
	{
		return BITCOUNT_TIME;
	}

	int GetBitCount(ScriptAnyValue value)
	{
		return 0;
	}

	int GetBitCount(SSerializeString& value)
	{
		return BITCOUNT_STRINGID;
	}
#endif
private:
#if USE_ARITHSTREAM
	bool ReadBytes(CCommInputStream& in, void* pValue, size_t nBytes) const
	{
		uint8* pAry = (uint8*) pValue;
		for (size_t i = 0; i < nBytes; i++)
			pAry[i] = in.ReadBits(8);
		return true;
	}
	bool WriteBytes(CCommOutputStream& out, const void* pValue, size_t nBytes) const
	{
		const uint8* pAry = (const uint8*) pValue;
		for (size_t i = 0; i < nBytes; i++)
			out.WriteBits(pAry[i], 8);
		return true;
	}
#else
	bool ReadBytes(CNetInputSerializeImpl* in, void* pValue, size_t nBytes) const
	{
		uint8* pAry = (uint8*) pValue;

		for (size_t i = 0; i < nBytes; i++)
		{
			pAry[i] = in->ReadBits(8);
		}

		return true;
	}
	bool WriteBytes(CNetOutputSerializeImpl* out, const void* pValue, size_t nBytes) const
	{
		const uint8* pAry = (const uint8*) pValue;

		for (size_t i = 0; i < nBytes; i++)
		{
			out->WriteBits(pAry[i], 8);
		}

		return true;
	}
#endif
};

REGISTER_COMPRESSION_POLICY(CDefaultPolicy, "Default");

#ifndef _LIB
	#include <CryCore/Common_TypeInfo.h>
#endif

#include <CryCore/TypeInfo_impl.h>
#include <CryNetwork/ISerialize_info.h>
