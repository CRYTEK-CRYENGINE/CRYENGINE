// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Cry3DEngine/CGF/CGFContent.h>

class CControllerPQLog : public IController
{
public:

	/**
	 * Default constructor.
	 */
	CControllerPQLog();

	/**
	 * Assigns a data payload to this controller object.
	 */
	void SetControllerData(const DynArray<PQLogS>& arrKeys, const DynArray<int>& arrTimes)
	{
		m_arrKeys = arrKeys;
		m_arrTimes = arrTimes;
	}

	//////////////////////////////////////////////////////////
	// IController implementation
	virtual JointState GetOPS(const kTime& key, Quat& quat, Vec3& pos, Diag33& scl) const override;
	virtual JointState GetOP(const kTime& key, Quat& quat, Vec3& pos) const override;
	virtual JointState GetO(const kTime& key, Quat& quat) const override;
	virtual JointState GetP(const kTime& key, Vec3& pos) const override;
	virtual JointState GetS(const kTime& key, Diag33& scl) const override;
	virtual int32      GetO_numKey() const override;
	virtual int32      GetP_numKey() const override;
	virtual size_t     GetRotationKeysNum() const override;
	virtual size_t     GetPositionKeysNum() const override;
	virtual size_t     GetScaleKeysNum() const override;
	virtual size_t     SizeOfController() const override;
	virtual size_t     ApproximateSizeOfThis() const override;
	//////////////////////////////////////////////////////////

private:

	const QuatTNS& DecodeKey(const kTime& keyTime) const;

public:

	// TODO: make these members private
	DynArray<PQLogS> m_arrKeys;
	DynArray<int>    m_arrTimes;

private:

	mutable kTime   m_lastTime;
	mutable QuatTNS m_lastValue;
};

TYPEDEF_AUTOPTR(CControllerPQLog);

inline const QuatTNS& CControllerPQLog::DecodeKey(const kTime& keyTime) const
{
#ifdef DEFINE_PROFILER_FUNCTION
	DEFINE_PROFILER_FUNCTION();
#endif

	if (keyTime == m_lastTime)
	{
		return m_lastValue;
	}
	m_lastTime = keyTime;

	// Use local variables to spare checking the this pointer everytime
	const PQLogS* const __restrict arrKeys = &m_arrKeys[0];
	const int* const __restrict arrTimes = &m_arrTimes[0];
	const int nNumKeysPQ = m_arrKeys.size();

	assert(m_arrKeys.size() > 0);

	const int keytimeStart = arrTimes[0];
	if (keyTime < keytimeStart)
	{
		const PQLogS& value = arrKeys[0];
		m_lastValue = QuatTNS(!Quat::exp(value.rotationLog), value.position, value.scale);
		return m_lastValue;
	}

	const int keytimeEnd = arrTimes[nNumKeysPQ - 1];
	if (keyTime >= keytimeEnd)
	{
		const PQLogS& value = arrKeys[nNumKeysPQ - 1];
		m_lastValue = QuatTNS(!Quat::exp(value.rotationLog), value.position, value.scale);
		return m_lastValue;
	}

	assert(m_arrKeys.size() > 1);

	int nPos = nNumKeysPQ >> 1;
	int nStep = nNumKeysPQ >> 2;

	// use binary search
	while (nStep)
	{
		const int time = arrTimes[nPos];
		if (keyTime < time)
		{
			nPos = nPos - nStep;
		}
		else if (keyTime > time)
		{
			nPos = nPos + nStep;
		}
		else
		{
			break;
		}
		nStep = nStep >> 1;
	}

	// fine-tuning needed since time is not linear
	while (keyTime > arrTimes[nPos])
	{
		nPos++;
	}

	while (keyTime < arrTimes[nPos - 1])
	{
		nPos--;
	}

	assert(nPos > 0 && nPos < (int)nNumKeysPQ);

	PQLogS pKeys[2] = { arrKeys[nPos - 1], arrKeys[nPos] };

	const int32 k0 = arrTimes[nPos];
	const int32 k1 = arrTimes[nPos - 1];

	if (k0 == k1)
	{
		m_lastValue = QuatTNS(!Quat::exp(pKeys[1].rotationLog), pKeys[1].position, pKeys[1].scale);
		return m_lastValue;
	}

	AdjustLogRotations(pKeys[0].rotationLog, pKeys[1].rotationLog);

	PQLogS value;
	const f32 blendFactor = (f32(keyTime - k1)) / (k0 - k1);
	value.Blend(pKeys[0], pKeys[1], blendFactor);

	m_lastValue = QuatTNS(!Quat::exp(value.rotationLog), value.position, value.scale);
	return m_lastValue;
}
