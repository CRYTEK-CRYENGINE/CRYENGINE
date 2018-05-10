// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowStartNode.h"


// if this is defined we use the old behaviour.
// means: In Editor switching from Game mode back to Editor
//        the node outputs an 'Output event (which is bogus...)
#define VS2_SAFE_PLAY_USE_OLD_BEHAVIOUR
#undef  VS2_SAFE_PLAY_USE_OLD_BEHAVIOUR  // default: use new behaviour

CFlowStartNode::CFlowStartNode(SActivationInfo* pActInfo)
{
	m_bActivated = true;
	SetActivation(pActInfo, false);
}

IFlowNodePtr CFlowStartNode::Clone(SActivationInfo* pActInfo)
{
	CFlowStartNode* pClone = new CFlowStartNode(pActInfo);
	pClone->SetActivation(pActInfo, m_bActivated);
	return pClone;
}

void CFlowStartNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputs[] = {
		InputPortConfig<bool>("InGame",   true, _HELP("Start will Trigger in PureGameMode")),
		InputPortConfig<bool>("InEditor", true, _HELP("Start will Trigger in EditorGameMode")),
		{ 0 }
	};
	static const SOutputPortConfig outputs[] = {
		OutputPortConfig<bool>("output"),
		{ 0 }
	};

	config.pInputPorts = inputs;
	config.pOutputPorts = outputs;
	config.SetCategory(EFLN_APPROVED);
}

bool CFlowStartNode::MayTrigger(SActivationInfo* pActInfo)
{
	const bool isEditor = gEnv->IsEditor();
	const bool canTriggerInGame = GetPortBool(pActInfo, 0);
	const bool canTriggerInEditor = GetPortBool(pActInfo, 1);
	const bool canTrigger = (isEditor && canTriggerInEditor) || (!isEditor && canTriggerInGame);
	return canTrigger;
}

void CFlowStartNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	SFlowAddress addr(pActInfo->myID, 0, true);

#ifdef VS2_SAFE_PLAY_USE_OLD_BEHAVIOUR
	switch (event)
	{
	case eFE_Update:
		{
			if (!CCryAction::GetCryAction()->IsGameStarted())
				return;

			pActInfo->pGraph->ActivatePort(addr, true);
			SetActivation(pActInfo, true);
		}
		break;
	case eFE_Initialize:
		pActInfo->pGraph->ActivatePort(addr, false);
		SetActivation(pActInfo, false);
		break;
	}
#else   // new behaviour
	switch (event)
	{
	case eFE_Update:
		{
			// when we're in pure game mode
			if (!gEnv->IsEditor())
			{
				if (!gEnv->pGameFramework->IsGameStarted())
					return; // not yet started
			}
			// else: in editor mode or game has been started
			// in editor mode we regard the game as initialized as soon as
			// we receive the first update...
			if (MayTrigger(pActInfo))
				pActInfo->pGraph->ActivatePort(addr, true);
			SetActivation(pActInfo, true);
		}
		break;
	case eFE_Initialize:
		if (MayTrigger(pActInfo))
			pActInfo->pGraph->ActivatePort(addr, false);
		if (!gEnv->IsEditor())
		{
			//check whether this Start node is in a deactivated entity (and should not trigger)
			//this is necessary for deactivated layers, because the flowgraphs are reset and should not run
			bool bSkipActivation = false;
			EntityId graphEntity = pActInfo->pGraph->GetGraphEntity(0);
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(graphEntity);
			if (pEntity && gEnv->pEntitySystem->ShouldSerializedEntity(pEntity) == false)
				bSkipActivation = true;

			if (bSkipActivation)
			{
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			}
			else
			{
				// we're in pure game mode
				SetActivation(pActInfo, false);
			}
		}
		else
		{
			// we're in editor mode and we're currently editing,
			// (-> not in editor/game mode), so we schedule ourself to be updated
			if (gEnv->pGameFramework->IsEditing() == true)
				SetActivation(pActInfo, false);
		}
		break;
	}
#endif
}

bool CFlowStartNode::SerializeXML(SActivationInfo* pActInfo, const XmlNodeRef& root, bool reading)
{
	if (reading)
	{
		bool activated;
		if (!root->getAttr("ACTIVATED", activated))
			return false;
		SetActivation(pActInfo, activated);
	}
	else
	{
		root->setAttr("ACTIVATED", m_bActivated);
	}
	return true;
}

void CFlowStartNode::Serialize(SActivationInfo* pActInfo, TSerialize ser)
{
	if (ser.IsWriting())
		ser.Value("activated", m_bActivated);
	else
	{
		bool activated;
		ser.Value("activated", activated);
		SetActivation(pActInfo, activated);
	}
}

void CFlowStartNode::SetActivation(SActivationInfo* pActInfo, bool value)
{
	if (value == m_bActivated)
		return;

	m_bActivated = value;
	pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, !m_bActivated);
}
