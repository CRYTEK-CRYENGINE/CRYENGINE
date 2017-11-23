#pragma once

#include "Legacy/Helpers/DesignerEntityComponent.h"

////////////////////////////////////////////////////////
// Entity that sends out trigger events based on entering / leaving linked areas
////////////////////////////////////////////////////////
class CTriggerEntity final
	: public CDesignerEntityComponent<>
	, public IEntityPropertyGroup
{
	CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS_GUID(CTriggerEntity, "TriggerEntity", "717fcf92-e048-4479-ab8d-6af20d94aa85"_cry_guid);

	CTriggerEntity();
	virtual ~CTriggerEntity() {}

public:
	enum EFlowgraphInputPorts
	{
		eInputPorts_Enable = 0,
		eInputPorts_Disable
	};

	enum EFlowgraphOutputPorts
	{
		eOutputPorts_Disabled = 0,
		eOutputPorts_Enabled,
		eOutputPorts_Enter,
		eOutputPorts_Leave,
	};

public:
	// ISimpleExtension
	virtual IEntityPropertyGroup* GetPropertyGroup() final { return this; }

	virtual void ProcessEvent(const SEntityEvent& event) override;
	// ~ISimpleExtension

	// IEntityPropertyGroup
	virtual const char* GetLabel() const override { return "TriggerEntity Properties"; }

	virtual void SerializeProperties(Serialization::IArchive& archive) override
	{
		archive(m_bActive, "Active", "Active");

		if (archive.isInput())
		{
			OnResetState();
		}
	}
	// ~IEntityPropertyGroup

	static void OnFlowgraphActivation(EntityId entityId, IFlowNode::SActivationInfo* pActInfo, const class CEntityFlowNode* pNode);

protected:
	bool m_bActive = true;
};
