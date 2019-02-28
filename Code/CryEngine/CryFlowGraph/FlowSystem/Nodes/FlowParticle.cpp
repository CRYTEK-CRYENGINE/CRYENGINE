// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryFlowGraph/IFlowBaseNode.h>



class CFlowParticleClass : public CFlowBaseNode<eNCT_Instanced>
{
public:
	IParticleEmitter* GetEmitter(SActivationInfo* pActInfo) const
	{
		if (!pActInfo->pEntity)
			return nullptr;
		return pActInfo->pEntity->GetParticleEmitter(0);
	}

private:
};

class CFlowNode_ParticleAttributeGet : public CFlowParticleClass
{
public:
	enum class EInputs
	{
		Activate,
		Name,
	};

public:
	CFlowNode_ParticleAttributeGet(SActivationInfo* pActInfo)
	{
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_ParticleAttributeGet(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inConfig[] =
		{
			InputPortConfig<SFlowSystemVoid>("Get",_HELP("")),
			InputPortConfig<string>("Attribute", _HELP(""), 0, _UICONFIG("ref_entity=entityId")),
			{ 0 }
		};

		static const SOutputPortConfig outConfig[] =
		{
			OutputPortConfig_AnyType("Value", _HELP("")),
			OutputPortConfig_AnyType("Error", _HELP("")),
			{ 0 }
		};

		config.sDescription = _HELP("");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inConfig;
		config.pOutputPorts = outConfig;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event != eFE_Activate)
			return;

		IParticleEmitter* pEmitter = GetEmitter(pActInfo);
		if (!pEmitter)
			return;

		const bool triggerPort = IsPortActive(pActInfo, int(EInputs::Activate));
		if (!triggerPort)
			return;

		const string inputPropertyName = GetPortString(pActInfo, int(EInputs::Name));
		IParticleAttributes& attributes = pEmitter->GetAttributes();
		auto attributeId = attributes.FindAttributeIdByName(inputPropertyName.c_str());
		if (attributeId == -1)
		{
			GameWarning(
				"[flow] CFlowNode_ParticleAttributeGet: Cannot resolve attribute '%s' in entity '%s'",
				inputPropertyName.c_str(), pActInfo->pEntity->GetName());
			ActivateOutput(pActInfo, 1, 0);
			return;
		}

		switch (attributes.GetAttributeType(attributeId))
		{
		case IParticleAttributes::ET_Integer:
			ActivateOutput(pActInfo, 0, attributes.GetAsInteger(attributeId, 0));
			break;
		case IParticleAttributes::ET_Float:
			ActivateOutput(pActInfo, 0, attributes.GetAsFloat(attributeId, 0.0f));
			break;
		case IParticleAttributes::ET_Boolean:
			ActivateOutput(pActInfo, 0, attributes.GetAsBoolean(attributeId, false));
			break;
		case IParticleAttributes::ET_Color:
			ActivateOutput(pActInfo, 0, attributes.GetAsColorF(attributeId, ColorF()).toVec3());
			break;
		}
	}
};

class CFlowNode_ParticleAttributeSet : public CFlowParticleClass
{
public:
	enum class EInputs
	{
		Activate,
		Name,
		Value,
	};

public:
	CFlowNode_ParticleAttributeSet(SActivationInfo* pActInfo)
	{
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_ParticleAttributeSet(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
	
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inConfig[] =
		{
			InputPortConfig<SFlowSystemVoid>("Set",_HELP("")),
			InputPortConfig<string>("Attribute", _HELP(""), 0, _UICONFIG("ref_entity=entityId")),
			InputPortConfig<string>("Value",_HELP("")),
			{ 0 }
		};

		static const SOutputPortConfig outConfig[] =
		{
			OutputPortConfig_AnyType("Error", _HELP("")),
			{ 0 }
		};

		config.sDescription = _HELP("");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inConfig;
		config.pOutputPorts = outConfig;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event != eFE_Activate)
			return;

		IParticleEmitter* pEmitter = GetEmitter(pActInfo);
		if (!pEmitter)
			return;

		const bool valuePort = IsPortActive(pActInfo, int(EInputs::Value));
		const bool triggerPort = IsPortActive(pActInfo, int(EInputs::Activate));
		if (!valuePort && !triggerPort)
			return;

		const string inputPropertyName = GetPortString(pActInfo, int(EInputs::Name));
		IParticleAttributes& attributes = pEmitter->GetAttributes();
		auto attributeId = attributes.FindAttributeIdByName(inputPropertyName.c_str());
		if (attributeId == -1)
		{
			GameWarning(
				"[flow] CFlowNode_ParticleAttributeSet: Cannot resolve attribute '%s' in entity '%s'",
				inputPropertyName.c_str(), pActInfo->pEntity->GetName());
			ActivateOutput(pActInfo, 0, 0);
			return;
		}

		switch (attributes.GetAttributeType(attributeId))
		{
		case IParticleAttributes::ET_Integer:
			attributes.SetAsInteger(attributeId, GetPortInt(pActInfo, int(int(EInputs::Value))));
			break;
		case IParticleAttributes::ET_Float:
			attributes.SetAsFloat(attributeId, GetPortFloat(pActInfo, int(int(EInputs::Value))));
			break;
		case IParticleAttributes::ET_Boolean:
			attributes.SetAsBoolean(attributeId, GetPortBool(pActInfo, int(int(EInputs::Value))));
			break;
		case IParticleAttributes::ET_Color:
			attributes.SetAsColor(attributeId, ColorF(GetPortVec3(pActInfo, int(int(EInputs::Value)))));
			break;
		}
	}
};


REGISTER_FLOW_NODE("Particle:AttributeGet", CFlowNode_ParticleAttributeGet)
REGISTER_FLOW_NODE("Particle:AttributeSet", CFlowNode_ParticleAttributeSet)
