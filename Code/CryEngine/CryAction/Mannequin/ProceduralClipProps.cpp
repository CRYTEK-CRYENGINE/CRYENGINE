// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "ICryMannequin.h"

#include <Mannequin/Serialization.h>

struct SProceduralClipAttachPropParams : public IProceduralParams
{
	virtual void Serialize(Serialization::IArchive& ar)
	{
		ar(Serialization::Decorators::ObjectFilename<TProcClipString>(objectFilename), "ObjectFilename", "Object Filename");
		ar(Serialization::Decorators::AttachmentName<SProcDataCRC>(attachmentName), "AttachmentName", "Attachment Name");
	}

	TProcClipString objectFilename;
	SProcDataCRC    attachmentName;
};

class CProceduralClipAttachProp : public TProceduralClip<SProceduralClipAttachPropParams>
{
public:
	CProceduralClipAttachProp()
		: m_attachmentCRC(0)
	{
	}

	virtual void OnEnter(float blendTime, float duration, const SProceduralClipAttachPropParams& params)
	{
		if (m_charInstance)
		{
			IAttachmentManager* pAttachmentManager = m_charInstance->GetIAttachmentManager();

			if (IAttachment* pAttachment = pAttachmentManager->GetInterfaceByNameCRC(params.attachmentName.crc))
			{
				m_attachmentCRC = params.attachmentName.crc;

				const char* fileExt = PathUtil::GetExt(params.objectFilename.c_str());

				if (0 == stricmp(fileExt, "cgf"))
				{
					if (m_pAttachedStatObj = gEnv->p3DEngine->LoadStatObj(params.objectFilename.c_str(), NULL, NULL, false))
					{
						CCGFAttachment* pCGFAttachment = new CCGFAttachment();
						pCGFAttachment->pObj = m_pAttachedStatObj;
						pAttachment->AddBinding(pCGFAttachment);
					}
				}
				else if (0 == stricmp(fileExt, "skin"))
				{
					if (IAttachment* pAttach = pAttachmentManager->GetInterfaceByNameCRC(params.attachmentName.crc))
					{
						if (IAttachmentSkin* pSkinAttach = pAttach->GetIAttachmentSkin())
						{
							if (ISkin* pSkin = gEnv->pCharacterManager->LoadModelSKIN(params.objectFilename.c_str(), 0))
							{
								CSKINAttachment* pSkinAttachment = new CSKINAttachment();
								pSkinAttachment->m_pIAttachmentSkin = pSkinAttach;
								pAttachment->AddBinding(pSkinAttachment, pSkin, 0);
							}
						}
					}
				}
				else if (m_pAttachedCharInst = gEnv->pCharacterManager->CreateInstance(params.objectFilename.c_str()))
				{
					CSKELAttachment* pChrAttachment = new CSKELAttachment();
					pChrAttachment->m_pCharInstance = m_pAttachedCharInst;
					pAttachment->AddBinding(pChrAttachment);
				}
			}
		}
	}

	virtual void OnExit(float blendTime)
	{
		if (m_pAttachedCharInst || m_pAttachedStatObj)
		{
			if (IAttachment* pAttachment = m_charInstance->GetIAttachmentManager()->GetInterfaceByNameCRC(m_attachmentCRC))
			{
				if (IAttachmentObject* pAttachmentObject = pAttachment->GetIAttachmentObject())
				{
					//ensure the correct model is still bound to the attachment before clearing
					if ((m_pAttachedCharInst && pAttachmentObject->GetICharacterInstance() == m_pAttachedCharInst)
					    || (m_pAttachedStatObj && pAttachmentObject->GetIStatObj() == m_pAttachedStatObj))
					{
						pAttachment->ClearBinding();
					}
				}
			}
		}

		m_pAttachedStatObj = NULL;
		m_pAttachedCharInst = NULL;
	}

	virtual void Update(float timePassed) {}

private:
	uint32                         m_attachmentCRC; //TODO: This should be available via m_pParams pointer but currently is cleared by the time OnExit is called. Remove once fixed by Tom
	_smart_ptr<IStatObj>           m_pAttachedStatObj;
	_smart_ptr<ICharacterInstance> m_pAttachedCharInst;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipAttachProp, "AttachProp");

struct SProceduralClipAttachEntityParams : public IProceduralParams
{
	virtual void Serialize(Serialization::IArchive& ar)
	{
		ar(Serialization::Decorators::AttachmentName<SProcDataCRC>(attachmentName), "AttachmentName", "Attachment Name");
		ar(entityIdParamName, "EntityIdParamName", "Param Name With EntityId");
	}

	SProcDataCRC    attachmentName;
	TProcClipString entityIdParamName;
};

class CProceduralClipAttachEntity : public TProceduralClip<SProceduralClipAttachEntityParams>
{
public:
	CProceduralClipAttachEntity();

	virtual void OnEnter(float blendTime, float duration, const SProceduralClipAttachEntityParams& params)
	{
		if (m_charInstance)
		{
			if (IAttachment* pAttachment = m_charInstance->GetIAttachmentManager()->GetInterfaceByNameCRC(params.attachmentName.crc))
			{
				m_attachedEntityId = 0;
				m_attachmentCRC = params.attachmentName.crc;
				IAttachmentObject* pNewAttachment = NULL;

				EntityId attachEntityId = 0;

				GetParam(params.entityIdParamName.c_str(), attachEntityId);

				if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(attachEntityId))
				{
					CEntityAttachment* pEntityAttachment = new CEntityAttachment();
					pEntityAttachment->SetEntityId(attachEntityId);

					pAttachment->AddBinding(pEntityAttachment);
					m_attachedEntityId = attachEntityId;

					if (IPhysicalEntity* pPhysics = pEntity->GetPhysics())
					{
						pe_action_add_constraint constraint;
						constraint.flags = constraint_inactive | constraint_ignore_buddy;
						constraint.pBuddy = m_scope->GetEntity().GetPhysics();
						constraint.pt[0].Set(0, 0, 0);
						m_attachedConstraintId = pPhysics->Action(&constraint);

						pe_params_part colltype;
						colltype.flagsOR = geom_no_coll_response;
						pPhysics->SetParams(&colltype);
					}
				}
			}
		}
	}

	virtual void OnExit(float blendTime)
	{
		if (m_attachedEntityId)
		{
			if (IAttachment* pAttachment = m_charInstance->GetIAttachmentManager()->GetInterfaceByNameCRC(m_attachmentCRC))
			{
				if (IAttachmentObject* pAttachmentObject = pAttachment->GetIAttachmentObject())
				{
					//ensure the correct entity is still bound to the attachment before clearing
					if (pAttachmentObject->GetAttachmentType() == IAttachmentObject::eAttachment_Entity)
					{
						if (static_cast<CEntityAttachment*>(pAttachmentObject)->GetEntityId() == m_attachedEntityId)
						{
							pAttachment->ClearBinding();

							if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_attachedEntityId))
							{
								if (IPhysicalEntity* pPhysics = pEntity->GetPhysics())
								{
									pe_action_update_constraint constraint;
									constraint.bRemove = true;
									constraint.idConstraint = m_attachedConstraintId;
									pPhysics->Action(&constraint);

									pe_params_part colltype;
									colltype.flagsAND = ~geom_no_coll_response;
									pPhysics->SetParams(&colltype);

									pe_action_awake action;
									action.bAwake = true;
									action.minAwakeTime = 0.5f;
									pPhysics->Action(&action);
								}
							}
						}
					}
				}
			}
		}

		m_attachedEntityId = 0;
		m_attachedConstraintId = 0;
	}

	virtual void Update(float timePassed) {}

private:
	uint32   m_attachmentCRC;
	EntityId m_attachedEntityId;
	uint     m_attachedConstraintId;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipAttachEntity, "AttachEntity")

CProceduralClipAttachEntity::CProceduralClipAttachEntity() : m_attachmentCRC(0), m_attachedEntityId(0), m_attachedConstraintId(0)
{
}

struct SSwapAttachmentParams : public IProceduralParams
{
	SProcDataCRC attachmentNameOld;
	SProcDataCRC attachmentNameNew;
	bool         resetOnExit;
	bool         clearOnExit;

	SSwapAttachmentParams()
		: resetOnExit(false)
		, clearOnExit(false)
	{
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		ar(Serialization::Decorators::AttachmentName<SProcDataCRC>(attachmentNameOld), "AttachmentNameOld", "Attachment Name Old");
		ar(Serialization::Decorators::AttachmentName<SProcDataCRC>(attachmentNameNew), "AttachmentNameNew", "Attachment Name New");
		ar(resetOnExit, "ResetOnExit", "Reset on Exit");
		ar(clearOnExit, "ClearOnExit", "Clear on Exit");
	}
};

class CProceduralClipSwapAttachment : public TProceduralClip<SSwapAttachmentParams>
{
public:
	CProceduralClipSwapAttachment()
		: m_newAttachmentCRC(0)
		, m_oldAttachmentCRC(0)
	{
	}

	virtual void OnEnter(float blendTime, float duration, const SSwapAttachmentParams& params)
	{
		m_newAttachmentCRC = 0;
		m_oldAttachmentCRC = 0;

		if (m_charInstance)
		{
			if (IAttachment* pNewAttachment = m_charInstance->GetIAttachmentManager()->GetInterfaceByNameCRC(params.attachmentNameNew.crc))
			{
				if (IAttachment* pOldAttachment = m_charInstance->GetIAttachmentManager()->GetInterfaceByNameCRC(params.attachmentNameOld.crc))
				{
					if (pNewAttachment->GetIAttachmentObject() == NULL && pOldAttachment->GetIAttachmentObject() != NULL)
					{
						if (params.resetOnExit)
						{
							m_oldAttachmentCRC = params.attachmentNameOld.crc;
							m_newAttachmentCRC = params.attachmentNameNew.crc;
						}
						else if (params.clearOnExit)
						{
							m_newAttachmentCRC = params.attachmentNameNew.crc;
						}

						pOldAttachment->SwapBinding(pNewAttachment);
					}
				}
			}
		}
	}

	virtual void OnExit(float blendTime)
	{
		if (m_newAttachmentCRC != 0)
		{
			if (IAttachment* pNewAttachment = m_charInstance->GetIAttachmentManager()->GetInterfaceByNameCRC(m_newAttachmentCRC))
			{
				if (m_oldAttachmentCRC != 0)
				{
					if (IAttachment* pOldAttachment = m_charInstance->GetIAttachmentManager()->GetInterfaceByNameCRC(m_oldAttachmentCRC))
					{
						if (pOldAttachment->GetIAttachmentObject() == NULL && pNewAttachment->GetIAttachmentObject() != NULL)
						{
							pNewAttachment->SwapBinding(pOldAttachment);
						}
					}
				}
				else
				{
					pNewAttachment->ClearBinding();
				}
			}
		}
	}

	virtual void Update(float timePassed) {}

private:
	uint32 m_newAttachmentCRC;
	uint32 m_oldAttachmentCRC;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipSwapAttachment, "SwapAttachment");

struct SProceduralClipHideAttachmentParams : public IProceduralParams
{
	SProcDataCRC attachmentName;

	virtual void Serialize(Serialization::IArchive& ar)
	{
		ar(Serialization::Decorators::AttachmentName<SProcDataCRC>(attachmentName), "AttachmentName", "Attachment Name");
	}
};

class CProceduralClipHideAttachment : public TProceduralClip<SProceduralClipHideAttachmentParams>
{
public:
	enum EHideFlags
	{
		eHideFlags_HiddenInMainPass  = BIT(0),
		eHideFlags_HiddenInShadow    = BIT(1),
		eHideFlags_HiddenInRecursion = BIT(2),
	};

	CProceduralClipHideAttachment()
		: m_attachmentCRC(0)
		, m_hiddenFlags(0)
	{
	}

	virtual void OnEnter(float blendTime, float duration, const SProceduralClipHideAttachmentParams& params)
	{
		m_attachmentCRC = 0;
		m_hiddenFlags = 0;

		if (m_charInstance)
		{
			if (IAttachment* pAttachment = m_charInstance->GetIAttachmentManager()->GetInterfaceByNameCRC(params.attachmentName.crc))
			{
				m_attachmentCRC = params.attachmentName.crc;

				if (pAttachment->IsAttachmentHidden())
				{
					m_hiddenFlags |= eHideFlags_HiddenInMainPass;
				}

				if (pAttachment->IsAttachmentHiddenInRecursion())
				{
					m_hiddenFlags |= eHideFlags_HiddenInRecursion;
				}

				if (pAttachment->IsAttachmentHiddenInShadow())
				{
					m_hiddenFlags |= eHideFlags_HiddenInShadow;
				}

				pAttachment->HideAttachment(1);
			}
		}
	}

	virtual void OnExit(float blendTime)
	{
		if (m_attachmentCRC != 0)
		{
			if (IAttachment* pAttachment = m_charInstance->GetIAttachmentManager()->GetInterfaceByNameCRC(m_attachmentCRC))
			{
				pAttachment->HideAttachment((m_hiddenFlags & eHideFlags_HiddenInMainPass));
				pAttachment->HideInRecursion((m_hiddenFlags & eHideFlags_HiddenInRecursion));
				pAttachment->HideInShadow((m_hiddenFlags & eHideFlags_HiddenInShadow));
			}
		}
	}

	virtual void Update(float timePassed) {}

private:
	uint32 m_attachmentCRC;
	uint32 m_hiddenFlags;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipHideAttachment, "HideAttachment");
