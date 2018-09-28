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
	CActionWait(const CTimeValue& time) : m_minTimeToWait(time), m_maxTimeToWait(time) {}
	CActionWait(const CTimeValue& minTime, const CTimeValue& maxTime) : m_minTimeToWait(minTime), m_maxTimeToWait(maxTime) {}

	//////////////////////////////////////////////////////////
	// IResponseAction implementation
	virtual DRS::IResponseActionInstanceUniquePtr Execute(DRS::IResponseInstance* pResponseInstance) override;
	virtual string                                GetVerboseInfo() const override;
	virtual void                                  Serialize(Serialization::IArchive& ar) override;
	virtual const char*                           GetType() const override { return "Do Nothing"; }
	//////////////////////////////////////////////////////////

private:
	CTimeValue m_minTimeToWait = "0.5"; //seconds
	CTimeValue m_maxTimeToWait = 0;     //seconds
};

//////////////////////////////////////////////////////////////////////////

class CActionWaitInstance final : public DRS::IResponseActionInstance
{
public:
	CActionWaitInstance(const CTimeValue& timeToWait);

	//////////////////////////////////////////////////////////
	// IResponseActionInstance implementation
	virtual eCurrentState Update() override;
	virtual void          Cancel() override { m_finishTime.SetSeconds(0); }
	//////////////////////////////////////////////////////////

private:
	CTimeValue m_finishTime;
};

} // namespace CryDRS
