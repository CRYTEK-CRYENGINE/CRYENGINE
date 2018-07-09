// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ICompressionPolicy.h"
#include "ArithModel.h"
#include "Protocol/Serialize.h"

class CStringTablePolicy
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
	bool ReadValue(CCommInputStream& in, SSerializeString& value, CArithModel* pModel, uint32 age) const
	{
		string s;
		pModel->TableReadString(in, s);
		value = s.c_str();
		NetLogPacketDebug("CStringTablePolicy::ReadValue %s (%f)", value.c_str(), in.GetBitSize());
		return true;
	}
	bool WriteValue(CCommOutputStream& out, const SSerializeString& value, CArithModel* pModel, uint32 age) const
	{
		pModel->TableWriteString(out, string(value.c_str()));
		return true;
	}

	template<class T>
	bool ReadValue(CCommInputStream& in, T& value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("StringTable does not support arbitrary types (only strings)");
		return false;
	}
	template<class T>
	bool WriteValue(CCommOutputStream& out, T value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("StringTable does not support arbitrary types (only strings)");
		return false;
	}
#else
	bool ReadValue(CNetInputSerializeImpl* in, SSerializeString& value, uint32 age) const
	{
		in->ReadString(&value);
		NetLogPacketDebug("CStringTablePolicy::ReadValue %s (%f)", value.c_str(), in->GetBitSize());
		return true;
	}

	bool WriteValue(CNetOutputSerializeImpl* out, const SSerializeString& value, uint32 age) const
	{
		out->WriteString(&value);
		return true;
	}

	template<class T>
	bool ReadValue(CNetInputSerializeImpl* in, T& value, uint32 age) const
	{
		NetWarning("StringTable does not support arbitrary types (only strings)");
		return false;
	}

	template<class T>
	bool WriteValue(CNetOutputSerializeImpl* out, T value, uint32 age) const
	{
		NetWarning("StringTable does not support arbitrary types (only strings)");
		return false;
	}
#endif

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CStringTablePolicy");
		pSizer->Add(*this);
	}
#if NET_PROFILE_ENABLE
	int GetBitCount(SSerializeString& value)
	{
		return BITCOUNT_STRINGID;
	}

	template<class T>
	int GetBitCount(T value)
	{
		return 0;
	}
#endif
};

REGISTER_COMPRESSION_POLICY(CStringTablePolicy, "StringTable");
