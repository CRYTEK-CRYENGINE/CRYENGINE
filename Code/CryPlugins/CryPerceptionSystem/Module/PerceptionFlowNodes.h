// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryFlowGraph/IFlowBaseNode.h>

struct SAIStimulusParams;

class CFlowNode_PerceptionState : public CFlowBaseNode<eNCT_Instanced>
{
public:
	enum EInputs
	{
		IN_ENABLE,
		IN_DISABLE,
		IN_RESET,
	};
	CFlowNode_PerceptionState(SActivationInfo* pActInfo) {};

	virtual IFlowNodePtr Clone(SActivationInfo * pActInfo) override { return new CFlowNode_PerceptionState(pActInfo); }

	virtual void GetConfiguration(SFlowNodeConfig& config) override
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType("Enable",         _HELP("Enable actor's perception")),
			InputPortConfig_AnyType("Disable",        _HELP("Disable actor's perception")),
			InputPortConfig_AnyType("Reset",          _HELP("Reset current perception state")),
			{ 0 }
		};
		config.sDescription = _HELP("Enabling/Disabling or Resetting AI actor's perception.");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override;

	virtual void GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
	}
};

class CFlowNode_AIEventListener : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowNode_AIEventListener(SActivationInfo* pActInfo) :
		m_pos(0, 0, 0),
		m_radius(0.0f),
		m_thresholdSound(0.5f),
		m_thresholdCollision(0.5f),
		m_thresholdBullet(0.5f),
		m_thresholdExplosion(0.5f),
		m_nodeID(pActInfo->myID),
		m_pGraph(pActInfo->pGraph)
	{}

	virtual ~CFlowNode_AIEventListener() override;

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) override { return new CFlowNode_AIEventListener(pActInfo); }
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser) override;

	virtual void GetConfiguration(SFlowNodeConfig& config) override;

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override;
	virtual void GetMemoryUsage(ICrySizer* s) const override;

	void OnAIEvent(const SAIStimulusParams& params);

private:
	Vec3 m_pos;
	float m_radius;
	float m_thresholdSound;
	float m_thresholdCollision;
	float m_thresholdBullet;
	float m_thresholdExplosion;
	IFlowGraph* m_pGraph;
	TFlowNodeId m_nodeID;
};