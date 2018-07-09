// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/************************************************************************

   This action will simply wait for the specified time, and therefore delay
   the execution of all following further actions.

   /************************************************************************/

#pragma once

#include <CryDynamicResponseSystem/IDynamicResponseAction.h>

namespace CryDRS
{
class CActionWait final : public DRS::IResponseAction
{
public:
	CActionWait() = default;
	CActionWait(float time) : m_minTimeToWait(time), m_maxTimeToWait(time) {}
	CActionWait(float minTime, float maxTime) : m_minTimeToWait(minTime), m_maxTimeToWait(maxTime) {}

	//////////////////////////////////////////////////////////
	// IResponseAction implementation
	virtual DRS::IResponseActionInstanceUniquePtr Execute(DRS::IResponseInstance* pResponseInstance) override;
	virtual string                                GetVerboseInfo() const override;
	virtual void                                  Serialize(Serialization::IArchive& ar) override;
	virtual const char*                           GetType() const override { return "Do Nothing"; }
	//////////////////////////////////////////////////////////

private:
	float m_minTimeToWait = 0.5f;    //seconds
	float m_maxTimeToWait = 0.0f;    //seconds
};

//////////////////////////////////////////////////////////////////////////

class CActionWaitInstance final : public DRS::IResponseActionInstance
{
public:
	CActionWaitInstance(float timeToWait);

	//////////////////////////////////////////////////////////
	// IResponseActionInstance implementation
	virtual eCurrentState Update() override;
	virtual void          Cancel() override { m_finishTime = 0.0f; }
	//////////////////////////////////////////////////////////

private:
	float m_finishTime;
};

} // namespace CryDRS
