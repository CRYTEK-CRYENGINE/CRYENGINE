// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  splits a compression policy into two pieces, one for the
               witness, and another for all other clients
   -------------------------------------------------------------------------
   History:
   - 02/11/2006   12:34 : Created by Craig Tiller
*************************************************************************/

#ifndef __OWNCHANNELCOMPRESSIONPOLICY_H__
#define __OWNCHANNELCOMPRESSIONPOLICY_H__

#pragma once

#include "ICompressionPolicy.h"

class COwnChannelCompressionPolicy : public ICompressionPolicy
{
public:
	COwnChannelCompressionPolicy(uint32 key, ICompressionPolicyPtr pOwn, ICompressionPolicyPtr pOther) : ICompressionPolicy(key), m_pOwn(pOwn), m_pOther(pOther)
	{
	}

	virtual bool Load(XmlNodeRef node, const string& filename) { NET_ASSERT(!"should never be called"); return false; }

#if USE_ARITHSTREAM
	#define SERIALIZATION_TYPE(T)                                                                                                                                              \
	  virtual bool ReadValue(CCommInputStream & in, T & value, CArithModel * pModel, uint32 age, bool own, CByteInputStream * pCurState, CByteOutputStream * pNewState) const; \
	  virtual bool WriteValue(CCommOutputStream & out, T value, CArithModel * pModel, uint32 age, bool own, CByteInputStream * pCurState, CByteOutputStream * pNewState) const;
	#include <CryNetwork/SerializationTypes.h>
	#undef SERIALIZATION_TYPE
	virtual bool ReadValue(CCommInputStream& in, SSerializeString& value, CArithModel* pModel, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState) const;
	virtual bool WriteValue(CCommOutputStream& out, const SSerializeString& value, CArithModel* pModel, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState) const;
#else
	#define SERIALIZATION_TYPE(T)                                                                                                                              \
	  virtual bool ReadValue(CNetInputSerializeImpl * in, T & value, uint32 age, bool own, CByteInputStream * pCurState, CByteOutputStream * pNewState) const; \
	  virtual bool WriteValue(CNetOutputSerializeImpl * out, T value, uint32 age, bool own, CByteInputStream * pCurState, CByteOutputStream * pNewState) const;
	#include <CryNetwork/SerializationTypes.h>
	#undef SERIALIZATION_TYPE
	virtual bool ReadValue(CNetInputSerializeImpl* in, SSerializeString& value, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState) const;
	virtual bool WriteValue(CNetOutputSerializeImpl* out, const SSerializeString& value, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState) const;
#endif

	virtual void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "COwnChannelCompressionPolicy");
		pSizer->Add(*this);
		m_pOwn->GetMemoryStatistics(pSizer);
		m_pOther->GetMemoryStatistics(pSizer);
	}

#if NET_PROFILE_ENABLE
	#define SERIALIZATION_TYPE(T) \
	  virtual int GetBitCount(T value);
	#include <CryNetwork/SerializationTypes.h>
	#undef SERIALIZATION_TYPE
	virtual int GetBitCount(SSerializeString& value);
#endif

private:
	ILINE const ICompressionPolicyPtr Get(bool own) const { return own ? m_pOwn : m_pOther; }
	ILINE ICompressionPolicyPtr       Get(bool own)       { return own ? m_pOwn : m_pOther; }
	ICompressionPolicyPtr m_pOwn;
	ICompressionPolicyPtr m_pOther;
};

#endif
