// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>

class CMovieManager;

class CMovieInstance
{
public:
	void AddRef() { ++m_nRefs; }
	void Release();

private:
	int            m_nRefs;
	CMovieManager* m_pManager;
};

class CMovieManager
{
public:
	void ReleaseInstance(CMovieInstance* pInstance)
	{
	}
};

void CMovieInstance::Release()
{
	if (0 == --m_nRefs)
	{
		m_pManager->ReleaseInstance(this);
	}
}

class CFlowNode_Movie : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Movie(SActivationInfo* pActInfo)
	{
	}

	enum EInputPorts
	{
		eIP_TimeOfDay = 0,
	};

	enum EOutputPorts
	{
		eOP_SunDirection = 0,
	};

	void GetConfiguration(SFlowNodeConfig& config)
	{
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
	void Reload(const char* filename);

};

// REGISTER_FLOW_NODE("Environment:Movie", CFlowNode_Movie);
