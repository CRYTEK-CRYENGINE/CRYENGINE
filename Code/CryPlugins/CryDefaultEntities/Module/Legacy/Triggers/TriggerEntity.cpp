#include "StdAfx.h"
#include "TriggerEntity.h"

#include "Legacy/Helpers/EntityFlowNode.h"

class CTriggerEntityRegistrator
	: public IEntityRegistrator
{
	virtual void Register() override
	{
		if (gEnv->pEntitySystem->GetClassRegistry()->FindClass("AreaTrigger") != nullptr)
		{
			// Skip registration of default engine class if the game has overridden it
			CryLog("Skipping registration of default engine entity class AreaTrigger, overridden by game");
			return;
		}

		RegisterEntityWithDefaultComponent<CTriggerEntity>("AreaTrigger", "Triggers", "AreaTrigger.bmp");
		
		// Register flow node
		// Factory will be destroyed by flowsystem during shutdown
		pFlowNodeFactory = new CEntityFlowNodeFactory("entity:AreaTrigger");

		pFlowNodeFactory->m_inputs.push_back(InputPortConfig<bool>("Enable", ""));
		pFlowNodeFactory->m_inputs.push_back(InputPortConfig_Void("Disable", ""));

		pFlowNodeFactory->m_activateCallback = CTriggerEntity::OnFlowgraphActivation;

		pFlowNodeFactory->m_outputs.push_back(OutputPortConfig_Void("Disabled"));
		pFlowNodeFactory->m_outputs.push_back(OutputPortConfig_Void("Enabled"));
		pFlowNodeFactory->m_outputs.push_back(OutputPortConfig<EntityId>("Enter"));
		pFlowNodeFactory->m_outputs.push_back(OutputPortConfig<EntityId>("Leave"));

		pFlowNodeFactory->Close();
	}

public:
	~CTriggerEntityRegistrator()
	{
		if (pFlowNodeFactory)
			pFlowNodeFactory->UnregisterFactory();
		pFlowNodeFactory = nullptr;
	}

protected:
	_smart_ptr<CEntityFlowNodeFactory> pFlowNodeFactory = nullptr;
};

CTriggerEntityRegistrator g_triggerEntityRegistrator;

CRYREGISTER_CLASS(CTriggerEntity);

CTriggerEntity::CTriggerEntity()
	: m_bActive(true)
{
}

void CTriggerEntity::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
		case ENTITY_EVENT_ENTERAREA:
			{
				if (!m_bActive)
					return;

				ActivateFlowNodeOutput(eOutputPorts_Enter, TFlowInputData((EntityId)event.nParam[0]));
			}
			break;
		case ENTITY_EVENT_LEAVEAREA:
			{
				if (!m_bActive)
					return;

				ActivateFlowNodeOutput(eOutputPorts_Leave, TFlowInputData((EntityId)event.nParam[0]));
			}
			break;
	}
}

void CTriggerEntity::OnFlowgraphActivation(EntityId entityId, IFlowNode::SActivationInfo* pActInfo, const class CEntityFlowNode* pNode)
{
	auto* pEntity = gEnv->pEntitySystem->GetEntity(entityId);
	auto* pTriggerEntity = pEntity->GetComponent<CTriggerEntity>();

	if (IsPortActive(pActInfo, eInputPorts_Enable))
	{
		pTriggerEntity->m_bActive = GetPortBool(pActInfo, eInputPorts_Enable);

		if (pTriggerEntity->m_bActive)
		{
			pTriggerEntity->ActivateFlowNodeOutput(eOutputPorts_Enabled, TFlowInputData());
		}
		else
		{
			pTriggerEntity->ActivateFlowNodeOutput(eOutputPorts_Disabled, TFlowInputData());
		}
	}
	else if (IsPortActive(pActInfo, eInputPorts_Disable))
	{
		pTriggerEntity->m_bActive = false;

		pTriggerEntity->ActivateFlowNodeOutput(eOutputPorts_Disabled, TFlowInputData());
	}
}