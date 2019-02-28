// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "TimePolicyWithDistribution.h"
#include "CompressionPolicyTime.h"
#include "Protocol/Serialize.h"

void CTimePolicyWithDistribution::SetTimeValue(uint32 timeFraction32)
{//we use time from arith model
}

bool CTimePolicyWithDistribution::Manage(CCompressionManager* pManager)
{
#if USE_ARITHSTREAM
	bool ret = false;

	for (int k = 0; k < 2; k++)
	{
		if (m_dist[k].Manage(pManager, m_name.c_str(), k))
			ret = true;
	}

	return ret;
#else
	return false;
#endif
}

void CTimePolicyWithDistribution::Init(CCompressionManager* pManager)
{
#if USE_ARITHSTREAM
	for (int k = 0; k < 2; k++)
		m_dist[k].Init(m_name.c_str(), k, pManager->GetUseDirectory().c_str(), pManager->GetAccDirectory().c_str());
#endif
}

bool CTimePolicyWithDistribution::Load(XmlNodeRef node, const string& filename)
{
	m_name = node->getAttr("name");

	bool ret = CTimePolicy::Load(node, filename);

#if USE_ARITHSTREAM
	XmlNodeRef params = node->findChild("Params");
	if (params != nullptr)
	{
		m_dist[0].Load(params, 1.f);
		m_dist[1].Load(params, 1.f);
	}
#endif

	return ret;
}

#if USE_ARITHSTREAM
bool CTimePolicyWithDistribution::ReadValue(CCommInputStream& in, CTimeValue& value, CArithModel* pModel, uint32 age) const
{
	auto dist = pModel->IsTimeInFrame(m_stream) ? &m_dist[0] : &m_dist[1];

	int32 error32 = 0;
	int64 error = 0;

	if (dist->GetReadDistribution()->ReadValue(error32, &in, false))
		error = error32;
	else
		error = in.ReadBitsLarge(64);

	value = pModel->ReadTimeWithDelta(in, m_stream, error);
	return true;
}

bool CTimePolicyWithDistribution::WriteValue(CCommOutputStream& out, CTimeValue value, CArithModel* pModel, uint32 age) const
{
	auto dist = pModel->IsTimeInFrame(m_stream) ? &m_dist[0] : &m_dist[1];

	int64 error = pModel->WriteGetTimeDelta(m_stream, value, 0);

	bool errorToBig = false;
	if (error > (int64)INT_MAX - 1 || error < (int64)(INT_MIN + 1))
		errorToBig = true;

	if (!errorToBig)
		dist->Count(error);

	bool writeBits = true;

	if (errorToBig)
	{
		dist->GetWriteDistribution()->WriteOutOfRange(&out);
		dist->GetWriteDistribution()->WriteBitOutOfRange(&out);
		writeBits = false;
	}
	else if (dist->GetWriteDistribution()->WriteValue((int32)error, &out, false))
		writeBits = false;

	if (writeBits)
		out.WriteBitsLarge(error, 64);

	return true;
}
#endif

REGISTER_COMPRESSION_POLICY_TIME(CTimePolicyWithDistribution, "TimeDist");
