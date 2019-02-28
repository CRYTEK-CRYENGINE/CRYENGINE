// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryNetwork/NetHelpers.h>
#include "CET_Server.h"
#include "Protocol/NetChannel.h"
#include "ContextView.h"
#include "ClientContextView.h"

class CCET_PostSpawnObjects : public CCET_Base
{
public:
	CCET_PostSpawnObjects() { ++g_objcnt.postSpawnObjects; }
	~CCET_PostSpawnObjects() { --g_objcnt.postSpawnObjects; }

	const char* GetName()
	{
		return "PostInitObjects";
	}

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		CNetChannel* pChannel = (CNetChannel*) state.pSender;
		if (pChannel)
		{
			// TODO: tidy up
			SCOPED_GLOBAL_LOCK;
			pChannel->GetContextView()->ContextState()->GC_SendPostSpawnEntities(pChannel->GetContextView());
			return eCETR_Ok;
		}
		return eCETR_Failed;
	}
};

void AddPostSpawnObjects(IContextEstablisher* pEst, EContextViewState state)
{
	pEst->AddTask(state, new CCET_PostSpawnObjects);
}
