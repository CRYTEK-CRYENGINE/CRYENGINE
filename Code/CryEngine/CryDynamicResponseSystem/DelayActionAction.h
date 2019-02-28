// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/************************************************************************

   This action will delay the execution of the wrapped action.

   /************************************************************************/

#pragma once

#include <CryDynamicResponseSystem/IDynamicResponseAction.h>

namespace CryDRS
{
class CResponseInstance;

class DelayActionActionInstance final : public DRS::IResponseActionInstance
{
public:
	virtual ~DelayActionActionInstance() override;
	DelayActionActionInstance(float timeToDelay, DRS::IResponseActionSharedPtr pActionToDelay, CResponseInstance* pResponseInstance);

	//////////////////////////////////////////////////////////
	// IResponseActionInstance implementation
	virtual eCurrentState Update() override;
	virtual void          Cancel() override;
	//////////////////////////////////////////////////////////

private:
	CTimeValue                            m_delayFinishTime;
	DRS::IResponseActionSharedPtr         m_pDelayedAction;
	CResponseInstance*                    m_pResponseInstance;

	DRS::IResponseActionInstanceUniquePtr m_RunningInstance;    //this is the instance of the action that we have delayed, once started, we forward all update/cancel/Release calls
};
} // namespace CryDRS
