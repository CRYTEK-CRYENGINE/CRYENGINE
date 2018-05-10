// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AIAssignmentNodes.h"

//////////////////////////////////////////////////////////////////////////

void AssignmentFlowNodeBase::DispatchAssignment( Assignment assignment, IEntity* entity)
{
	assert(entity);
	SmartScriptTable entityScript = entity->GetScriptTable();
	Script::CallMethod(entityScript, "SetAssignment", assignment.type, assignment.data);
}

void AssignmentFlowNodeBase::ProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	if(!pActInfo->pEntity)
		return;

	if(event == eFE_Activate && IsPortActive(pActInfo, GetSetPortNumber()))
	{
		DispatchAssignment(SpecifyAssignment(pActInfo), pActInfo->pEntity);
	}
}

//////////////////////////////////////////////////////////////////////////

void ClearAssignmentFlowNode::GetConfiguration( SFlowNodeConfig& config )
{
	static const SInputPortConfig inputPortConfig[] = 
	{
		InputPortConfig_Void("Set"),
		{0}
	};

	config.pInputPorts = inputPortConfig;
	config.sDescription = _HELP("Removes the active assignment. Usable by: All");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}

Assignment ClearAssignmentFlowNode::SpecifyAssignment( SActivationInfo* pActInfo )
{
	return Assignment(Assignment::NoAssignment, NULL);
}

//////////////////////////////////////////////////////////////////////////

void DefendAreaAssignmentFlowNode::GetConfiguration( SFlowNodeConfig& config )
{
	static const SInputPortConfig inputPortConfig[] = 
	{
		InputPortConfig_Void("Set"),
		InputPortConfig<Vec3>("Position"),
		InputPortConfig<int>("Radius", 15),
		{0}
	};

	config.pInputPorts = inputPortConfig;
	config.sDescription = _HELP("Assigns the entity/wave to defend an area. With this assignment, the agents will try to remain inside the specified perimeter and will not advance towards the targets. Usable by: Human, Grunt, Heavy");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}

Assignment DefendAreaAssignmentFlowNode::SpecifyAssignment( SActivationInfo* pActInfo )
{
	SmartScriptTable data(gEnv->pScriptSystem);
	data->SetValue("position", GetPortVec3(pActInfo, InputPort_Position));
	data->SetValue("radius", GetPortInt(pActInfo, InputPort_Radius));
	return Assignment(Assignment::DefendArea, data);
}

//////////////////////////////////////////////////////////////////////////

void HoldPositionAssignmentFlowNode::GetConfiguration( SFlowNodeConfig& config )
{
	static const SInputPortConfig inputPortConfig[] = 
	{
		InputPortConfig_Void("Set"),
		InputPortConfig<Vec3>("Position"),
		InputPortConfig<int>("Radius", 3),
		InputPortConfig<Vec3>("Direction"),
		InputPortConfig<bool>("UseCover", true),
		{0}
	};

	config.pInputPorts = inputPortConfig;
	config.sDescription = _HELP("Assigns the entity/wave to stay in a specific position. Usable by: Human, Grunt, Heavy");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}

Assignment HoldPositionAssignmentFlowNode::SpecifyAssignment( SActivationInfo* pActInfo )
{
	SmartScriptTable data(gEnv->pScriptSystem);
	data->SetValue("position", GetPortVec3(pActInfo, InputPort_Position));
	data->SetValue("radius", GetPortInt(pActInfo, InputPort_Radius));
	data->SetValue("direction", GetPortVec3(pActInfo, InputPort_Direction));
	data->SetValue("useCover", GetPortBool(pActInfo, InputPort_UseCover));
	return Assignment(Assignment::HoldPosition, data);
}

//////////////////////////////////////////////////////////////////////////

void CombatMoveAssignmentFlowNode::GetConfiguration( SFlowNodeConfig& config )
{
	static const SInputPortConfig inputPortConfig[] = 
	{
		InputPortConfig_Void("Set"),
		InputPortConfig<Vec3>("Position"),
		InputPortConfig<bool>("UseCover", true),
		{0}
	};

	config.pInputPorts = inputPortConfig;
	config.sDescription = _HELP("Assigns the entity/wave to relocate and keep engaging the targets at the same time. The assignment will end when the agent reaches the destination. Usable by: Human, Grunt");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}

Assignment CombatMoveAssignmentFlowNode::SpecifyAssignment( SActivationInfo* pActInfo )
{
	SmartScriptTable data(gEnv->pScriptSystem);
	data->SetValue("position", GetPortVec3(pActInfo, InputPort_Position));
	data->SetValue("useCover", GetPortBool(pActInfo, InputPort_UseCover));
	return Assignment(Assignment::CombatMove, data);
}

//////////////////////////////////////////////////////////////////////////
REGISTER_FLOW_NODE("AIAssignments:Clear", ClearAssignmentFlowNode);
REGISTER_FLOW_NODE("AIAssignments:DefendArea", DefendAreaAssignmentFlowNode);
REGISTER_FLOW_NODE("AIAssignments:HoldPosition", HoldPositionAssignmentFlowNode);
REGISTER_FLOW_NODE("AIAssignments:CombatMove", CombatMoveAssignmentFlowNode);