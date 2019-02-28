// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Cry3DEngine/CGF/CGFContent.h>
#include "Controller.h"


// old motion format cry bone controller
class CControllerPQLog : public IController
{
public:

	CControllerPQLog()
		: m_nControllerId(0)
	{
	}

	uint32 GetID() const
	{
		return m_nControllerId;
	}

	QuatTNS GetValue3(f32 fTime);

	Status4 GetOPS(f32 realtime, Quat& quat, Vec3& pos, Diag33& scale);
	Status4 GetOP(f32 realtime, Quat& quat, Vec3& pos);
	uint32 GetO(f32 realtime, Quat& quat);
	uint32 GetP(f32 realtime, Vec3& pos);
	uint32 GetS(f32 realtime, Diag33& pos);

	QuatTNS GetValueByKey(uint32 key) const;

	int32 GetO_numKey()
	{
		return m_arrKeys.size();
	}

	int32 GetP_numKey()
	{
		return m_arrKeys.size();
	}

	int32 GetS_numKey()
	{
		return m_arrKeys.size();
	}

	// returns the start time
	f32 GetTimeStart()
	{
		return f32(m_arrTimes[0]);
	}

	// returns the end time
	f32 GetTimeEnd()
	{
		assert(!m_arrTimes.empty());
		return f32(m_arrTimes.back());
	}


	size_t SizeOfThis() const;


	CInfo GetControllerInfo() const
	{
		// TODO: what about scaling?
		CInfo info;

		info.numKeys  = m_arrKeys.size();
		info.quat     = !Quat::exp(m_arrKeys[info.numKeys - 1].rotationLog);
		info.pos      = m_arrKeys[info.numKeys - 1].position;

		info.stime    = m_arrTimes[0];
		info.etime    = m_arrTimes[info.numKeys - 1];
		info.realkeys = (info.etime - info.stime) / TICKS_PER_FRAME + 1;

		return info;
	}

	void SetControllerData(const std::vector<PQLogS>& arrKeys, const std::vector<int>& arrTimes)
	{
		m_arrKeys = arrKeys;
		m_arrTimes = arrTimes;
	}

public:

	std::vector<PQLogS> m_arrKeys;
	std::vector<int> m_arrTimes;

	unsigned m_nControllerId;
};


TYPEDEF_AUTOPTR(CControllerPQLog);
