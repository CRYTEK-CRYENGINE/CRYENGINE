// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowItemAnimation.h"


namespace
{


	enum ItemActionInputsIds
	{
		IAI_Activate,
		IAI_ItemId,
		IAI_Action,
		IAI_Busy,
		IAI_Loop,
	};

	const SInputPortConfig itemActionInputs[] = {
		InputPortConfig_Void("Activate", _HELP("Play the action.")),
		InputPortConfig<EntityId>("ItemId", _HELP("Set item that will play the action.")),
		InputPortConfig<string>("Action", _HELP("Set the action name to be played.")),
		InputPortConfig<bool>("Busy", false, _HELP("Set item as busy while the action is playing.")),
		InputPortConfig<bool>("Loop", false, _HELP("Make the action to loop.")),
		{0}
	};



	enum ItemActionOutputIds
	{
		IAO_Done,
	};

	const SOutputPortConfig itemActionOutputs[] = {
		OutputPortConfig_Void("Done", _HELP("Activated when action finishes. Does not work for looped actions.")),
		{0}
	};

}



CFlowItemAction::CFlowItemAction(SActivationInfo * pActInfo)
	:	m_timerCountDown(0.0f)
{
}



IFlowNodePtr CFlowItemAction::Clone(SActivationInfo * pActInfo)
{
	return new CFlowItemAction(pActInfo);
}



void CFlowItemAction::GetConfiguration(SFlowNodeConfig& config)
{
	config.pInputPorts = itemActionInputs;
	config.pOutputPorts = itemActionOutputs;
	config.sDescription = _HELP("Plays an action on an item.");
	config.SetCategory(EFLN_APPROVED);
}



void CFlowItemAction::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event == eFE_Activate && IsPortActive(pActInfo, IAI_Activate))
		Activate(pActInfo);
	else if (event == eFE_Update)
		Update(pActInfo);
}



void CFlowItemAction::GetMemoryUsage(ICrySizer * s) const
{
	s->Add(*this);
}



IItem* CFlowItemAction::GetItem(SActivationInfo* pActInfo) const
{
	EntityId itemId = GetPortEntityId(pActInfo, IAI_ItemId);
	IItem* pItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(itemId);
	return pItem;
}



void CFlowItemAction::Activate(SActivationInfo* pActInfo)
{
	IItem* pItem = GetItem(pActInfo);
	if (!pItem)
		return;

	CItem* pItemImpl = static_cast<CItem*>(pItem);
	bool busy = GetPortBool(pActInfo, IAI_Busy);
	bool loop = GetPortBool(pActInfo, IAI_Loop);
	const string& actionName = GetPortString(pActInfo, IAI_Action);

	FragmentID fragID = pItemImpl->GetFragmentID(actionName);
	pItemImpl->PlayAction(fragID, 0, loop);

	if (busy)
	{
		pItem->SetBusy(true);
	}
	if (busy || !loop)
	{
		uint ownerAnimationtime = pItemImpl->GetCurrentAnimationTime(eIGS_Owner);
		StartDoneCountDown(pActInfo, ownerAnimationtime / (float)1000.0f);
	}
}



void CFlowItemAction::StartDoneCountDown(SActivationInfo* pActInfo, float time)
{
	pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
	m_timerCountDown = time;
}



void CFlowItemAction::Update(SActivationInfo* pActInfo)
{
	m_timerCountDown -= gEnv->pTimer->GetFrameTime();
	if (m_timerCountDown > 0.0f)
		return;

	IItem* pItem = GetItem(pActInfo);
	if (pItem)
		pItem->SetBusy(false);

	pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
	ActivateOutput(pActInfo, IAO_Done, true);
}


REGISTER_FLOW_NODE("Weapon:ItemAction", CFlowItemAction);
