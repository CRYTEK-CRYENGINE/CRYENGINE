// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowEntityNode.h"

class CFlowNode_Attachment : public CFlowEntityNodeBase
{
private:
	enum ECharacterSlot { eCharacterSlot_Invalid = -1 };

protected:
	int    m_characterSlot;
	string m_boneName;

public:
	CFlowNode_Attachment(SActivationInfo* pActInfo)
		: m_characterSlot(eCharacterSlot_Invalid)
	{
		m_event = ENTITY_EVENT_RESET;
	}

	enum EInPorts
	{
		eIP_EntityId,
		eIP_BoneName,
		eIP_CharacterSlot,
		eIP_Attach,
		eIP_Detach,
		eIP_Hide,
		eIP_UnHide,
		eIP_RotationOffset,
		eIP_TranslationOffset
	};

	enum EOutPorts
	{
		eOP_Attached,
		eOP_Detached
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<EntityId>("Item",          _HELP("Entity to be linked")),
			InputPortConfig<string>("BoneName",        _HELP("Attachment bone")),
			InputPortConfig<int>("CharacterSlot",      0,                                                               _HELP("Host character slot")),
			InputPortConfig<SFlowSystemVoid>("Attach", _HELP("Attach")),
			InputPortConfig<SFlowSystemVoid>("Detach", _HELP("Detach any entity currently attached to the given bone")),
			InputPortConfig_Void("Hide",               _HELP("Hides attachment")),
			InputPortConfig_Void("UnHide",             _HELP("Unhides attachment")),
			InputPortConfig<Vec3>("RotationOffset",    _HELP("Rotation offset")),
			InputPortConfig<Vec3>("TranslationOffset", _HELP("Translation offset")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<SFlowSystemVoid>("Attached"),
			OutputPortConfig<SFlowSystemVoid>("Detached"),
			{ 0 }
		};
		config.sDescription = _HELP("Attach/Detach an entity to another one.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED | EFLN_TARGET_ENTITY);
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_Attachment(pActInfo); }

	virtual void         Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		CFlowEntityNodeBase::ProcessEvent(event, pActInfo);
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIP_CharacterSlot))
				{
					m_characterSlot = GetPortInt(pActInfo, eIP_CharacterSlot);
				}
				if (IsPortActive(pActInfo, eIP_BoneName))
				{
					m_boneName = GetPortString(pActInfo, eIP_BoneName);
				}
				if (IsPortActive(pActInfo, eIP_Attach))
				{
					AttachObject(pActInfo);
				}
				if (IsPortActive(pActInfo, eIP_Detach))
				{
					DetachObject(pActInfo);
				}

				if (IsPortActive(pActInfo, eIP_Hide))
				{
					HideAttachment(pActInfo);
				}
				else if (IsPortActive(pActInfo, eIP_UnHide))
				{
					UnHideAttachment(pActInfo);
				}

				if (IsPortActive(pActInfo, eIP_RotationOffset) || IsPortActive(pActInfo, eIP_TranslationOffset))
				{
					UpdateOffset(pActInfo);
				}
				break;
			}
		case eFE_Initialize:
			{
				m_characterSlot = GetPortInt(pActInfo, eIP_CharacterSlot);
				m_boneName = GetPortString(pActInfo, eIP_BoneName);

				if (gEnv->IsEditor() && m_entityId)
				{
					UnregisterEvent(m_event);
					RegisterEvent(m_event);
				}
				break;
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	IAttachment* GetAttachment(IEntity* pEntity)
	{
		IAttachment* pAttachment = nullptr;
		if (pEntity && !m_boneName.empty() && m_characterSlot > eCharacterSlot_Invalid)
		{
			if (ICharacterInstance* pCharacterInstance = pEntity->GetCharacter(m_characterSlot))
			{
				if (IAttachmentManager* pAttachmentManager = pCharacterInstance->GetIAttachmentManager())
				{
					pAttachment = pAttachmentManager->GetInterfaceByName(m_boneName);
				}
			}
		}
		return pAttachment;
	}

	void AttachObject(SActivationInfo* pActInfo)
	{
		if (pActInfo->pEntity)
		{
			EntityId entityId = GetPortEntityId(pActInfo, eIP_EntityId);
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);
			if (pEntity)
			{
				IAttachment* pAttachment = GetAttachment(pActInfo->pEntity);
				if (pAttachment)
				{
					CEntityAttachment* pEntityAttachment = new CEntityAttachment;
					pEntityAttachment->SetEntityId(entityId);
					pAttachment->AddBinding(pEntityAttachment);

					UpdateOffset(pActInfo);
					ActivateOutput(pActInfo, eOP_Attached, 0);
				}
			}
		}
	}

	void DetachObject(SActivationInfo* pActInfo)
	{
		IAttachment* pAttachment = GetAttachment(pActInfo->pEntity);
		if (pAttachment)
		{
			pAttachment->ClearBinding();
			ActivateOutput(pActInfo, eOP_Detached, 0);
		}
	}

	void HideAttachment(SActivationInfo* pActInfo)
	{
		IAttachment* pAttachment = GetAttachment(pActInfo->pEntity);
		if (pAttachment)
		{
			pAttachment->HideAttachment(1);

			IAttachmentObject* pAttachmentObject = pAttachment->GetIAttachmentObject();
			if ((pAttachmentObject != NULL) && (pAttachmentObject->GetAttachmentType() == IAttachmentObject::eAttachment_Entity))
			{
				IEntity* pAttachedEntity = gEnv->pEntitySystem->GetEntity(static_cast<CEntityAttachment*>(pAttachmentObject)->GetEntityId());
				if (pAttachedEntity)
				{
					pAttachedEntity->Hide(true);
				}
			}
		}
	}

	void UnHideAttachment(SActivationInfo* pActInfo)
	{
		IAttachment* pAttachment = GetAttachment(pActInfo->pEntity);
		if (pAttachment)
		{
			pAttachment->HideAttachment(0);

			IAttachmentObject* pAttachmentObject = pAttachment->GetIAttachmentObject();
			if ((pAttachmentObject != NULL) && (pAttachmentObject->GetAttachmentType() == IAttachmentObject::eAttachment_Entity))
			{
				IEntity* pAttachedEntity = gEnv->pEntitySystem->GetEntity(static_cast<CEntityAttachment*>(pAttachmentObject)->GetEntityId());
				if (pAttachedEntity)
				{
					pAttachedEntity->Hide(false);
				}
			}
		}
	}

	void UpdateOffset(SActivationInfo* pActInfo)
	{
		IAttachment* pAttachment = GetAttachment(pActInfo->pEntity);
		if (pAttachment)
		{
			const Vec3& rotationOffsetEuler = GetPortVec3(pActInfo, eIP_RotationOffset);
			QuatT offset = QuatT::CreateRotationXYZ(Ang3(DEG2RAD(rotationOffsetEuler)));
			offset.t = GetPortVec3(pActInfo, eIP_TranslationOffset);
			pAttachment->SetAttRelativeDefault(offset);
		}
	}

	void OnEntityEvent(IEntity* pEntity, const SEntityEvent& event)
	{
		if (event.event == m_event)
		{
			if (IAttachment* pAttachment = GetAttachment(pEntity))
			{
				pAttachment->ClearBinding();
			}
		}
	}
};

REGISTER_FLOW_NODE("Entity:Attachment", CFlowNode_Attachment);
