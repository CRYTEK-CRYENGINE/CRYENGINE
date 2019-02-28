// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "PredictiveBase.h"
#include "TimePolicy.h"

class CTimePolicyWithDistribution
	: public CTimePolicy
{
public:
	void SetTimeValue(uint32 timeFraction32);
	bool Manage(CCompressionManager* pManager);

	void Init(CCompressionManager* pManager);
	bool Load(XmlNodeRef node, const string& filename);

#if USE_ARITHSTREAM
	bool ReadValue(CCommInputStream& in, CTimeValue& value, CArithModel* pModel, uint32 age) const;
	bool WriteValue(CCommOutputStream& out, CTimeValue value, CArithModel* pModel, uint32 age) const;

	template<class T>
	bool ReadValue(CCommInputStream& in, T& value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("CTimePolicyWithDistribution: not implemented for generic types");
		return false;
	}
	template<class T>
	bool WriteValue(CCommOutputStream& out, T value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("CTimePolicyWithDistribution: not implemented for generic types");
		return false;
	}
#endif

private:
	string m_name;

#if USE_ARITHSTREAM
	CPredictiveBase<int64> m_dist[2];
#endif
};
