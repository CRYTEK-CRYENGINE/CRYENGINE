// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Custom nodes relating to general entity features

#include "StdAfx.h"
#include "FlowEntityCustomNodes.h"

#include <CryFlowGraph/IFlowBaseNode.h>
#include <CryAnimation/IAttachment.h>

class CEntityAttachmentControlNode : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		eIP_CharacterSlot = 0,
		eIP_AttachmentName,
		eIP_Show,
		eIP_Hide,
		eIP_Replace,
		eIP_ObjectName,
		eIP_MaterialName,
	};

	enum OUTPUTS
	{
		eOP_Done = 0,
		eOP_Failed,
	};

public:
	CEntityAttachmentControlNode(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config) override
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<int>("CharacterSlot",     _HELP("Slot in which the character is loaded or -1 for all loaded characters.")),
			InputPortConfig<string>("AttachmentName", _HELP("Name of the attachment as specified in the .cdf")),
			InputPortConfig_Void("Show",              _HELP("Show the specified attachment")),
			InputPortConfig_Void("Hide",              _HELP("Hide the specified attachment")),
			InputPortConfig_Void("Replace",           _HELP("Replace the specified attachment with the specified skin and/or replace the skin's material with the specified one")),
			InputPortConfig<string>("ObjectName",     _HELP("Cgf or Skin file to be used on that attachment")),
			InputPortConfig<string>("MaterialName",   _HELP("Material to be used on the attachment")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Done",   _HELP("Triggered when at least partially succeeded.")),
			OutputPortConfig_AnyType("Failed", _HELP("Triggered when at least partially failed.")),
			{ 0 }
		};
		config.nFlags = EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("This node lets you easily switch a skin attachment and/or its current material and control its visibility.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	template<typename TCustomArgs, bool(CEntityAttachmentControlNode::* TFunc)(int slotId, ICharacterInstance*, const string&, const TCustomArgs&)>
	void ForRequiredCharacters(SActivationInfo* const pActInfo, const string& attachmentName, const TCustomArgs& customArguments)
	{
		const int characterSlot = GetPortInt(pActInfo, eIP_CharacterSlot);
		if (characterSlot >= 0)
		{
			ICharacterInstance* pCharacterInstance = pActInfo->pEntity->GetCharacter(characterSlot);
			bool bResult = pCharacterInstance && (this->*TFunc)(characterSlot, pCharacterInstance, attachmentName, customArguments);
			UpdateOutput(pActInfo, bResult);
		}
		else
		{
			const int slotCount = pActInfo->pEntity->GetSlotCount();
			for (int slotId = 0; slotId != slotCount; ++slotId)
			{
				ICharacterInstance* pCharacterInstance = pActInfo->pEntity->GetCharacter(characterSlot);
				if (pCharacterInstance)
				{
					bool bResult = (this->*TFunc)(slotId, pCharacterInstance, attachmentName, customArguments);
					UpdateOutput(pActInfo, bResult);
				}
			}
		}
	}

	void UpdateOutput(SActivationInfo* pActInfo, const bool bSuccess)
	{
		if (bSuccess)
		{
			ActivateOutput(pActInfo, eOP_Done, true);
		}
		else
		{
			ActivateOutput(pActInfo, eOP_Failed, true);
		}
	}

	typedef VectorMap<int, IAttachmentObject*> TSlotAttachmentCache;
	struct SUpdateAttachmentObjectArgs
	{
		SUpdateAttachmentObjectArgs(const string& objectName, TSlotAttachmentCache& slotAttachmentCache)
			: objectName(objectName)
			, slotAttachmentCache(slotAttachmentCache)
		{
		}
		const string&         objectName;
		TSlotAttachmentCache& slotAttachmentCache;
	};

	struct SUpdateAttachmentMaterialArgs
	{
		SUpdateAttachmentMaterialArgs(const string& materialName, const TSlotAttachmentCache& slotAttachmentCache)
			: materialName(materialName)
			, slotAttachmentCache(slotAttachmentCache)
		{
		}
		const string&               materialName;
		const TSlotAttachmentCache& slotAttachmentCache;
	};

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (pActInfo->pEntity)
				{
					if (IsPortActive(pActInfo, eIP_Replace))
					{
						const string& attachmentName = GetPortString(pActInfo, eIP_AttachmentName);
						if (!attachmentName.empty())
						{
							const string& objectName = GetPortString(pActInfo, eIP_ObjectName);
							const string& materialName = GetPortString(pActInfo, eIP_MaterialName);
							TSlotAttachmentCache slotAttachmentCache;

							if (!objectName.empty())
							{
								SUpdateAttachmentObjectArgs args(objectName, slotAttachmentCache);
								ForRequiredCharacters<SUpdateAttachmentObjectArgs, & CEntityAttachmentControlNode::UpdateAttachmentObject>(pActInfo, attachmentName, args);
							}

							if (!materialName.empty())
							{
								SUpdateAttachmentMaterialArgs args(materialName, slotAttachmentCache);
								ForRequiredCharacters<SUpdateAttachmentMaterialArgs, & CEntityAttachmentControlNode::UpdateAttachmentMaterial>(pActInfo, attachmentName, args);
							}
						}
					}

					if (IsPortActive(pActInfo, eIP_Show) || IsPortActive(pActInfo, eIP_Hide))
					{
						bool bVisible = IsPortActive(pActInfo, eIP_Show);

						const string& attachmentName = GetPortString(pActInfo, eIP_AttachmentName);
						if (!attachmentName.empty())
						{
							ForRequiredCharacters<bool, & CEntityAttachmentControlNode::ChangeAttachmentVisibility>(pActInfo, attachmentName, bVisible);
						}
					}
				}
			}
		}
	}

	bool UpdateAttachmentObject(int slotId, ICharacterInstance* pCharacterInstance, const string& attachmentName, const SUpdateAttachmentObjectArgs& args)
	{
		bool bSuccess = false;
		IAttachmentManager* const pAttachmentManager = pCharacterInstance->GetIAttachmentManager();
		IAttachment* const pAttachment = pAttachmentManager ? pAttachmentManager->GetInterfaceByName(attachmentName.c_str()) : nullptr;
		if (pAttachment)
		{
			switch (pAttachment->GetType())
			{
			case CA_BONE:
				{
					stack_string tempName = args.objectName;
					tempName.MakeLower();
					bool bIsCdf = (tempName.find(".cdf") != tempName.npos);

					if (bIsCdf)
					{
						if (const _smart_ptr<ICharacterInstance> pCharacterInstance = gEnv->pCharacterManager->CreateInstance(args.objectName.c_str()))
						{
							CSKELAttachment* pChrAttachment = new CSKELAttachment();
							pChrAttachment->m_pCharInstance = pCharacterInstance;
							pAttachment->AddBinding(pChrAttachment);

							args.slotAttachmentCache[slotId] = pChrAttachment;
							bSuccess = true;
						}
						else
						{
							GameWarning("[CharacterAttachmentControl] Trying to match an invalid character with a bone attachment :\n\tCharacterName=%s\n\tAttachmentName=%s", args.objectName.c_str(), attachmentName.c_str());
						}
					}
					else
					{
						if (const _smart_ptr<IStatObj> pStatObj = gEnv->p3DEngine->LoadStatObj(args.objectName.c_str(), nullptr, nullptr, false))
						{
							CCGFAttachment* pCGFAttachment = new CCGFAttachment();
							pCGFAttachment->pObj = pStatObj;
							pAttachment->AddBinding(pCGFAttachment);

							args.slotAttachmentCache[slotId] = pCGFAttachment;
							bSuccess = true;
						}
						else
						{
							GameWarning("[CharacterAttachmentControl] Trying to match an invalid static object with a bone attachment :\n\tObjectName=%s\n\tAttachmentName=%s", args.objectName.c_str(), attachmentName.c_str());
						}
					}
				}
				break;
			case CA_SKIN:
				{
					ISkin* pSkin = gEnv->pCharacterManager->LoadModelSKIN(args.objectName.c_str(), 0);
					if (pSkin)
					{
						CSKINAttachment* pSkinAttachment = new CSKINAttachment();
						pSkinAttachment->m_pIAttachmentSkin = pAttachment->GetIAttachmentSkin();
						pAttachment->AddBinding(pSkinAttachment, pSkin);

						args.slotAttachmentCache[slotId] = pSkinAttachment;
						bSuccess = true;
					}
					else
					{
						GameWarning("[CharacterAttachmentControl] Trying to match an invalid skin with a skin attachment :\n\tSkinName=%s\n\tAttachmentName=%s", args.objectName.c_str(), attachmentName.c_str());
					}
				}
				break;
			default:
				GameWarning("[CharacterAttachmentControl] Unsupported attachment type can't be replaced");
				break;
			}
		}
		else if (pAttachmentManager)
		{
			GameWarning("[CharacterAttachmentControl] At least one character instance didn't have the binding we were looking for :\n\tAttachmentName=%s", attachmentName.c_str());
		}
		return bSuccess;
	}

	bool UpdateAttachmentMaterial(int slotId, ICharacterInstance* pCharacterInstance, const string& attachmentName, const SUpdateAttachmentMaterialArgs& args)
	{
		bool bSuccess = false;
		IAttachmentObject* pAttachmentObject = nullptr;
		const TSlotAttachmentCache::const_iterator cacheIt = args.slotAttachmentCache.find(slotId);
		if (cacheIt != args.slotAttachmentCache.end())
		{
			pAttachmentObject = cacheIt->second;
		}

		if (!pAttachmentObject)
		{
			IAttachmentManager* const pAttachmentManager = pCharacterInstance->GetIAttachmentManager();
			IAttachment* const pAttachment = pAttachmentManager ? pAttachmentManager->GetInterfaceByName(attachmentName.c_str()) : nullptr;
			pAttachmentObject = pAttachment ? pAttachment->GetIAttachmentObject() : nullptr;
			if (pAttachmentManager && !pAttachmentObject)
			{
				GameWarning("[CharacterAttachmentControl] At least one character instance didn't have the binding we were looking for :\n\tAttachmentName=%s", attachmentName.c_str());
			}
		}

		if (pAttachmentObject)
		{
			IMaterialManager* const pMaterialManager = gEnv->p3DEngine->GetMaterialManager();
			IMaterial* const pMaterial = pMaterialManager->LoadMaterial(args.materialName.c_str());
			if (pMaterial)
			{
				pAttachmentObject->SetReplacementMaterial(pMaterial);
				bSuccess = true;
			}
		}
		return bSuccess;
	}

	bool ChangeAttachmentVisibility(int slotId, ICharacterInstance* pCharacterInstance, const string& attachmentName, const bool& bVisible)
	{
		bool bSuccess = false;
		IAttachmentManager* const pAttachmentManager = pCharacterInstance->GetIAttachmentManager();
		IAttachment* const pAttachment = pAttachmentManager ? pAttachmentManager->GetInterfaceByName(attachmentName.c_str()) : nullptr;
		if (pAttachment)
		{
			pAttachment->HideAttachment(bVisible ? 0 : 1);
			bSuccess = true;
		}
		else if (pAttachmentManager)
		{
			GameWarning("[CharacterAttachmentControl] At least one character instance didn't have the binding we were looking for :\n\tAttachmentName=%s", attachmentName.c_str());
		}
		return bSuccess;
	}
};

// Attachment registry implementation

CEntityAttachmentExNodeRegistry::CEntityAttachmentExNodeRegistry()
{
	if (gEnv->IsEditor())
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CEntityAttachmentExNodeRegistry");
	}
}

CEntityAttachmentExNodeRegistry::~CEntityAttachmentExNodeRegistry()
{
	if (gEnv->IsEditor())
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
	}
}

bool CEntityAttachmentExNodeRegistry::OwnsAttachment(EntityId id, const string& attachmentName) const
{
	bool bFound = false;
	TEntityAttachments::const_iterator it = m_entityAttachments.find(id);
	if (it != m_entityAttachments.end())
	{
		const TAttachmentNames& names = it->second;
		const TAttachmentNames::const_iterator nameIt = stl::binary_find(names.begin(), names.end(), attachmentName);
		if (nameIt != names.end())
		{
			bFound = true;
		}
	}
	return bFound;
}

bool CEntityAttachmentExNodeRegistry::DeleteAttachment(EntityId id, const string& attachmentName)
{
	bool bOwnedAttachment = false;
	TEntityAttachments::iterator it = m_entityAttachments.find(id);
	if (it != m_entityAttachments.end())
	{
		if (stl::binary_erase(it->second, attachmentName))
		{
			bOwnedAttachment = true;
			if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id))
			{
				if (ICharacterInstance* pCharacter = pEntity->GetCharacter(0))
				{
					if (IAttachmentManager* pAttachmentManager = pCharacter->GetIAttachmentManager())
					{
						pAttachmentManager->RemoveAttachmentByName(attachmentName.c_str());
					}
				}
			}
		}
	}
	return bOwnedAttachment;
}

void CEntityAttachmentExNodeRegistry::DeleteAllAttachments(EntityId id)
{
	const TAttachmentNames* pNames = GetOwnedAttachmentNames(id);
	if (pNames)
	{
		DestroyAllListedAttachments(id, *pNames);
		m_entityAttachments.erase(id);
	}
}

void CEntityAttachmentExNodeRegistry::CleanAll()
{
	for (const TEntityAttachments::value_type& entityAttachmentPair : m_entityAttachments)
	{
		DestroyAllListedAttachments(entityAttachmentPair.first, entityAttachmentPair.second);
	}
	TEntityAttachments().swap(m_entityAttachments);
}

void CEntityAttachmentExNodeRegistry::DestroyAllListedAttachments(EntityId id, const TAttachmentNames& names)
{
	if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id))
	{
		if (ICharacterInstance* pCharacter = pEntity->GetCharacter(0))
		{
			if (IAttachmentManager* pAttachmentManager = pCharacter->GetIAttachmentManager())
			{
				for (const string& attachmentName : names)
				{
					pAttachmentManager->RemoveAttachmentByName(attachmentName.c_str());
				}
			}
		}
	}
}

void CEntityAttachmentExNodeRegistry::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_LEVEL_LOAD_START:
	case ESYSTEM_EVENT_LEVEL_UNLOAD:
		{
			CleanAll();
		}
		break;
	case ESYSTEM_EVENT_EDITOR_GAME_MODE_CHANGED:
	case ESYSTEM_EVENT_EDITOR_SIMULATION_MODE_CHANGED:
		{
			if (wparam == 0)
			{
				CleanAll();
			}
		}
		break;
	}
}

// ~AttachmentRegistry implementation

class CEntityAttachmentExNode : public CFlowBaseNode<eNCT_Instanced>
{
	enum INPUTS
	{
		eIP_TargetId = 0,
		eIP_AttachmentName,
		eIP_JointName,
		eIP_JointId,
		eIP_TranslationOffset,
		eIP_RotationOffset,
		eIP_Attach,
		eIP_Detach,
		eIP_DetachAllOwned,
		eIP_Show,
		eIP_Hide,
	};

	enum OUTPUTS
	{
		eOP_Done = 0,
		eOP_Failed,
		eOP_Attached,
		eOP_Detached,
		eOP_Hidden,
		eOP_Shown,
		eOP_AttachmentName,
	};

public:
	CEntityAttachmentExNode(SActivationInfo* pActInfo)
		: m_actInfo(*pActInfo)
		, m_id(++s_instanceCounter)
	{
	}

	CEntityAttachmentExNodeRegistry& GetRegistry()
	{
		CEntityAttachmentExNodeRegistry* pRegistry = nullptr;
		if (CCryAction* pCryAction = CCryAction::GetCryAction())
		{
			pRegistry = &pCryAction->GetEntityAttachmentExNodeRegistry();
		}

		if (!pRegistry)
		{
			static CEntityAttachmentExNodeRegistry s_registry;
			GameWarning("[CEntityAttachmentExNode] Trying to access the attachment registry but it is not available at this point. Some attachments might be leaked");
			pRegistry = &s_registry;
		}

		return *pRegistry;
	}

	template<bool(CEntityAttachmentExNode::* TFunc)(ICharacterInstance*, IAttachmentManager*, IAttachment*)>
	bool ForRequiredCharacter()
	{
		bool bSuccess = false;
		ICharacterInstance* const pCharacterInstance = m_actInfo.pEntity->GetCharacter(0);
		if (pCharacterInstance)
		{
			IAttachmentManager* const pAttachmentManager = pCharacterInstance->GetIAttachmentManager();
			if (pAttachmentManager)
			{
				IAttachment* pAttachment = pAttachmentManager->GetInterfaceByName(m_currentAttachmentName.c_str());
				bSuccess = (this->*TFunc)(pCharacterInstance, pAttachmentManager, pAttachment);
			}
			else
			{
				GameWarning("%s \"%s\".", "[CEntityAttachExNode] No available attachment manager on entity for node with attachment name ");
			}
		}
		else
		{
			GameWarning("%s \"%s\".", "[CEntityAttachExNode] No available character on entity for node with attachment name ");
		}
		return bSuccess;
	}

	~CEntityAttachmentExNode()
	{
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) override
	{
		return new CEntityAttachmentExNode(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config) override
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<EntityId>("TargetId",      _HELP("Entity to be attached.")),
			InputPortConfig<string>("AttachmentName",  _HELP("Desired name of the attachment. Leave empty for using an automatically generated name. Don't change at runtime.")),
			InputPortConfig<string>("bone_JointName",  _HELP("Name of the joint to attach to.")),
			InputPortConfig<int>("JointId",            INVALID_JOINT_ID,                                                                                                                                                             _HELP("Id of the joint to attach to (only used if the joint name is left empty).")),
			InputPortConfig<Vec3>("TranslationOffset", _HELP("Translation offset in joint space.")),
			InputPortConfig<Vec3>("RotationOffset",    _HELP("Rotation offset in joint space. Angles are specified in degrees")),
			InputPortConfig_Void("Attach",             _HELP("Attach the specified entity to the specified joint with the specified attachment name (if names is already used, previous attachment with that name will be broken)")),
			InputPortConfig_Void("Detach",             _HELP("Detach the entity")),
			InputPortConfig_Void("DetachAllOwned",     _HELP("Detaches all attachment created through this node from the selected entity.")),
			InputPortConfig_Void("Show",               _HELP("Show the specified attachment")),
			InputPortConfig_Void("Hide",               _HELP("Hide the specified attachment")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Done",           _HELP("Triggered everytime an operation is done.")),
			OutputPortConfig_AnyType("Failed",         _HELP("Triggered everytime an operation failed.")),
			OutputPortConfig_AnyType("Attached",       _HELP("Triggered when an entity was successfully attached.")),
			OutputPortConfig_AnyType("Detached",       _HELP("Triggered when an entity was successfully detached.")),
			OutputPortConfig_AnyType("Shown",          _HELP("Triggered when an entity attachment was successfully shown.")),
			OutputPortConfig_AnyType("Hidden",         _HELP("Triggered when an entity attachment was successfully hidden.")),
			OutputPortConfig<string>("AttachmentName", _HELP("Triggered when the attachment name changes and on initialization.")),
			{ 0 }
		};
		config.nFlags = EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("This node lets you attach entities to other entities without requiring declaration of the attachment joint in a .cdf file.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_actInfo = *pActInfo;
				UpdateAttachmentName();
			}
			break;
		case eFE_SetEntityId:
			{
				m_actInfo = *pActInfo;
			}
			break;
		case eFE_Activate:
			{
				if (pActInfo->pEntity)
				{
					m_actInfo = *pActInfo;

					if (IsPortActive(pActInfo, eIP_AttachmentName))
					{
						UpdateAttachmentName();
					}

					bool bDone = false;
					bool bFailed = false;

					if (IsPortActive(pActInfo, eIP_Attach))
					{
						bFailed = !ForRequiredCharacter<& CEntityAttachmentExNode::Attach>() || bFailed;
						if (!bFailed)
						{
							ActivateOutput(&m_actInfo, eOP_Attached, true);
						}
						bDone = true;
					}

					if (IsPortActive(pActInfo, eIP_Detach))
					{
						bFailed = !ForRequiredCharacter<& CEntityAttachmentExNode::Detach>() || bFailed;
						if (!bFailed)
						{
							ActivateOutput(&m_actInfo, eOP_Detached, true);
						}
						bDone = true;
					}

					if (IsPortActive(pActInfo, eIP_DetachAllOwned))
					{
						GetRegistry().DeleteAllAttachments(pActInfo->pEntity->GetId());
						bDone = true;
					}

					if (IsPortActive(pActInfo, eIP_Show))
					{
						bFailed = !ForRequiredCharacter<& CEntityAttachmentExNode::Show>() || bFailed;
						if (!bFailed)
						{
							ActivateOutput(&m_actInfo, eOP_Shown, true);
						}
						bDone = true;
					}

					if (IsPortActive(pActInfo, eIP_Hide))
					{
						bFailed = !ForRequiredCharacter<& CEntityAttachmentExNode::Hide>() || bFailed;
						if (!bFailed)
						{
							ActivateOutput(&m_actInfo, eOP_Hidden, true);
						}
						bDone = true;
					}

					if (IsPortActive(pActInfo, eIP_TranslationOffset) || IsPortActive(pActInfo, eIP_RotationOffset))
					{
						ForRequiredCharacter<& CEntityAttachmentExNode::UpdateOffset>();
						bDone = true;
					}

					if (bDone)   ActivateOutput(&m_actInfo, eOP_Done, true);
					if (bFailed) ActivateOutput(&m_actInfo, eOP_Failed, true);
				}
			}
		}
	}

	void UpdateAttachmentName()
	{
		const string& inputAttachmentName = GetPortString(&m_actInfo, eIP_AttachmentName);
		if (m_currentAttachmentName.empty() || (inputAttachmentName != m_currentAttachmentName))
		{
			if (inputAttachmentName.empty())
			{
				m_currentAttachmentName.Format("EntityAttachEx_%u", m_id);
			}
			else
			{
				m_currentAttachmentName = inputAttachmentName;
			}
			ActivateOutput(&m_actInfo, eOP_AttachmentName, m_currentAttachmentName);
		}
	}

	static int32 GetDesiredJointId(SActivationInfo* pActInfo, ICharacterInstance* pCharacterInstance)
	{
		const string& jointName = GetPortString(pActInfo, eIP_JointName);
		if (!jointName.empty())
		{
			const IDefaultSkeleton& defaultSkeleton = pCharacterInstance->GetIDefaultSkeleton();
			return defaultSkeleton.GetJointIDByName(jointName.c_str());
		}
		else
		{
			return GetPortInt(pActInfo, eIP_JointId);
		}
	}

	static string GetDesiredJointName(SActivationInfo* pActInfo, ICharacterInstance* pCharacterInstance)
	{
		const string& currentJointName = GetPortString(pActInfo, eIP_JointName);
		if (!currentJointName.empty())
		{
			return currentJointName;
		}
		else
		{
			int32 jointId = GetPortInt(pActInfo, eIP_JointId);
			if (jointId >= 0)
			{
				const IDefaultSkeleton& defaultSkeleton = pCharacterInstance->GetIDefaultSkeleton();
				return defaultSkeleton.GetJointNameByID(jointId);
			}
			else
			{
				return "";
			}
		}
	}

	bool Attach(ICharacterInstance* pCharacterInstance, IAttachmentManager* pAttachmentManager, IAttachment* pOldAttachment)
	{
		bool bSuccess = false;
		EntityId targetId = GetPortEntityId(&m_actInfo, eIP_TargetId);
		if (targetId != INVALID_ENTITYID)
		{
			IAttachment* pFinalAttachment = pOldAttachment;
			CEntityAttachmentExNodeRegistry& attachmentRegistry = GetRegistry();

			bool bAbort = false;
			if (pFinalAttachment)
			{
				// If an attachment exists, make sure it's on the right joint
				const IDefaultSkeleton& defaultSkeleton = pCharacterInstance->GetIDefaultSkeleton();
				const int32 oldJointId = static_cast<int32>(pFinalAttachment->GetJointID());
				const int32 desiredJointId = GetDesiredJointId(&m_actInfo, pCharacterInstance);
				if (oldJointId != desiredJointId)
				{
					// If it's not on the right joint and we created it, then we can destroy it.
					EntityId currentEntityId = m_actInfo.pEntity->GetId();
					const string& currentAttachmentName = m_currentAttachmentName;
					if (attachmentRegistry.OwnsAttachment(m_actInfo.pEntity->GetId(), currentAttachmentName))
					{
						pAttachmentManager->RemoveAttachmentByInterface(pFinalAttachment);
						pFinalAttachment = nullptr;
					}
					else
					{
						const string& jointName = GetDesiredJointName(&m_actInfo, pCharacterInstance);
						GameWarning("[CEntityAttachExNode] Found another attachment with name \"%s\" on joint \"%s\" when we try to attach to joint (name=\"%s\", id=%d)."
						            , m_currentAttachmentName.c_str(), defaultSkeleton.GetJointNameByID(oldJointId), jointName.c_str(), desiredJointId);
						bAbort = true;
					}
				}
			}

			if (!bAbort)
			{
				// Create a new attachment if necessary
				if (!pFinalAttachment)
				{
					const string& desiredJointName = GetDesiredJointName(&m_actInfo, pCharacterInstance).c_str();
					if (pFinalAttachment = pAttachmentManager->CreateAttachment(m_currentAttachmentName.c_str(), CA_BONE, desiredJointName))
					{
						pFinalAttachment->AlignJointAttachment();
						attachmentRegistry.RegisterAttachmentOwnership(m_actInfo.pEntity->GetId(), m_currentAttachmentName);
					}
				}

				// Attach the entity to the existing attachment
				if (pFinalAttachment && (CA_BONE == pFinalAttachment->GetType()))
				{
					CEntityAttachment* pEntityAttachment = new CEntityAttachment();
					pEntityAttachment->SetEntityId(targetId);

					pFinalAttachment->AddBinding(pEntityAttachment);
					pFinalAttachment->UpdateAttModelRelative();
					pEntityAttachment->ProcessAttachment(pFinalAttachment);

					UpdateOffset(pCharacterInstance, pAttachmentManager, pFinalAttachment);

					bSuccess = true;
				}
				else
				{
					GameWarning("[CEntityAttachExNode] Cannot create Bone/Joint attachment with that name (probably an attachment with the same name \"%s\" but a different type already exists).", m_currentAttachmentName.c_str());
				}
			}
		}
		else
		{
			GameWarning("[CEntityAttachExNode] no entity to attach !");
		}
		return bSuccess;
	}

	bool Detach(ICharacterInstance* pCharacterInstance, IAttachmentManager* pAttachmentManager, IAttachment* pAttachment)
	{
		bool bSuccess = false;
		if (pAttachment && (CA_BONE == pAttachment->GetType()))
		{
			EntityId currentEntityId = m_actInfo.pEntity->GetId();
			const string& currentAttachmentName = m_currentAttachmentName;

			CEntityAttachmentExNodeRegistry& attachmentRegistry = GetRegistry();
			const bool bDestructionSuccessful = attachmentRegistry.DeleteAttachment(currentEntityId, currentAttachmentName);

			if (!bDestructionSuccessful)
			{
				// We didn't own that attachment, release the target gracefully.
				CEntityAttachment* pEntityAttachment = new CEntityAttachment();
				pAttachment->AddBinding(pEntityAttachment);
			}
			bSuccess = true;
		}
		return bSuccess;
	}

	bool Show(ICharacterInstance* pCharacterInstance, IAttachmentManager* pAttachmentManager, IAttachment* pAttachment)
	{
		bool bSuccess = false;
		if (pAttachment)
		{
			pAttachment->HideAttachment(false);
			bSuccess = true;
		}
		return bSuccess;
	}

	bool Hide(ICharacterInstance* pCharacterInstance, IAttachmentManager* pAttachmentManager, IAttachment* pAttachment)
	{
		bool bSuccess = false;
		if (pAttachment)
		{
			pAttachment->HideAttachment(true);
			bSuccess = true;
		}
		return bSuccess;
	}

	bool UpdateOffset(ICharacterInstance* pCharacterInstance, IAttachmentManager* pAttachmentManager, IAttachment* pAttachment)
	{
		bool bSuccess = false;
		if (pAttachment)
		{
			const Vec3& rotationOffsetEuler = GetPortVec3(&m_actInfo, eIP_RotationOffset);
			QuatT offset = QuatT::CreateRotationXYZ(Ang3(DEG2RAD(rotationOffsetEuler)));
			offset.t = GetPortVec3(&m_actInfo, eIP_TranslationOffset);
			pAttachment->SetAttRelativeDefault(offset);
			bSuccess = true;
		}
		return bSuccess;
	}

private:
	SActivationInfo m_actInfo;
	const uint32    m_id;
	string          m_currentAttachmentName;

	static uint32   s_instanceCounter;
};

uint32 CEntityAttachmentExNode::s_instanceCounter = 0;

class CEntityLinksGetNode : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		eIP_GetCount = 0,
		eIP_GetByIndex,
		eIP_Index,
		eIP_GetByName,
		eIP_Name,
		eIP_NameIndex,
		eIP_GetNameCount,
	};

	enum OUTPUTS
	{
		eOP_Failed = 0,
		eOP_Succeeded,
		eOP_LinkCount,
		eOP_LinkName,
		eOP_LinkTarget,
	};

public:
	CEntityLinksGetNode(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config) override
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("GetCount",       _HELP("Outputs the number of links available on the selected entity.")),
			InputPortConfig_Void("GetLinkByIndex", _HELP("Outputs the details (name and target) of the link at the specified index on the selected entity.")),
			InputPortConfig<int>("LinkIndex",      _HELP("Index of the link you want the details of. Should be in the range [0, LinkCount[")),
			InputPortConfig_Void("GetLinkByName",  _HELP("Outputs the details (name and target) of the first link found with the specified name on the selected entity.")),
			InputPortConfig<string>("LinkName",    _HELP("Name of the link you want details from (beware : it is case sensitive).")),
			InputPortConfig<int>("NameIndex",      0,                                                                                                                      _HELP("When multiple links have the same name, you can access the ones after the first one by increasing this value.")),
			InputPortConfig_Void("GetNameCount",   _HELP("Outputs the number of links with that name to LinkCount.")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Failed",          _HELP("Triggered when getting the data you asked for failed (invalid entity selected or invalid index for that entity).")),
			OutputPortConfig_Void("Succeeded",       _HELP("Triggered when succeeding to get the desired data.")),
			OutputPortConfig<int>("LinkCount",       _HELP("Number of links available on the selected entity.")),
			OutputPortConfig<string>("LinkName",     _HELP("Name of the link at the specified index.")),
			OutputPortConfig<EntityId>("LinkTarget", _HELP("Target of the link at the specified index.")),
			{ 0 }
		};
		config.nFlags = EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("This node lets you inspect the configuration of entity links.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_GAME);
		switch (event)
		{
		case eFE_Activate:
			{
				const bool bGetCount = IsPortActive(pActInfo, eIP_GetCount);
				const bool bGetByIndex = IsPortActive(pActInfo, eIP_GetByIndex);
				const bool bGetByName = IsPortActive(pActInfo, eIP_GetByName);
				const bool bGetNameCount = IsPortActive(pActInfo, eIP_GetNameCount);

				if (bGetCount || bGetByIndex || bGetByName || bGetNameCount)
				{
					bool bFailed = false;
					bool bSucceeded = false;
					if (pActInfo->pEntity)
					{
						const IEntityLink* const pFirstLink = pActInfo->pEntity->GetEntityLinks();
						const string& linkName = GetPortString(pActInfo, eIP_Name);

						if (bGetCount)
						{
							int linkCount = 0;

							const IEntityLink* pLink = pFirstLink;
							while (pLink)
							{
								++linkCount;
								pLink = pLink->next;
							}

							ActivateOutput(pActInfo, eOP_LinkCount, linkCount);
							bSucceeded = true;
						}

						if (bGetNameCount)
						{
							int linkCount = 0;

							const IEntityLink* pLink = pFirstLink;
							while (pLink)
							{
								if (linkName == pLink->name)
								{
									++linkCount;
								}
								pLink = pLink->next;
							}

							ActivateOutput(pActInfo, eOP_LinkCount, linkCount);
							bSucceeded = true;
						}

						if (bGetByIndex)
						{
							int index = GetPortInt(pActInfo, eIP_Index);

							const IEntityLink* pLink = pFirstLink;
							while (pLink && index > 0)
							{
								--index;
								pLink = pLink->next;
							}

							if (pLink)
							{
								ActivateOutput(pActInfo, eOP_LinkName, string(pLink->name));
								ActivateOutput(pActInfo, eOP_LinkTarget, pLink->entityId);
								bSucceeded = true;
							}
							else
							{
								bFailed = true;
							}
						}

						if (bGetByName)
						{
							int nameIndex = GetPortInt(pActInfo, eIP_NameIndex);
							bool bFound = false;
							const IEntityLink* pLink = pFirstLink;
							while (pLink)
							{
								if (linkName == pLink->name)
								{
									if (--nameIndex < 0)
									{
										bFound = true;
										break;
									}
								}
								pLink = pLink->next;
							}

							if (bFound)
							{
								ActivateOutput(pActInfo, eOP_LinkName, string(pLink->name));
								ActivateOutput(pActInfo, eOP_LinkTarget, pLink->entityId);
								bSucceeded = true;
							}
							else
							{
								bFailed = true;
							}
						}
					}
					else
					{
						bFailed = true;
					}

					if (bFailed)
					{
						ActivateOutput(pActInfo, eOP_Failed, true);
					}
					if (bSucceeded)
					{
						ActivateOutput(pActInfo, eOP_Succeeded, true);
					}
				}
			}
		}
	}

};

class CEntityLinksSetNode : public CFlowBaseNode<eNCT_Singleton>, private ISystemEventListener
{
	enum INPUTS
	{
		eIP_LinkName = 0,
		eIP_Index,
		eIP_Target,
		eIP_Add,
		eIP_Remove,
		eIP_Reset,
		eIP_Rename,
		eIP_NewName,
	};

	enum OUTPUTS
	{
		eOP_Failed = 0,
		eOP_Succeeded,
	};

	struct SEntityLinkDesc
	{
		SEntityLinkDesc() {}
		SEntityLinkDesc(const string& linkName, EntityId targetId, EntityGUID targetGuid)
			: linkName(linkName)
			, targetId(targetId)
			, targetGuid(targetGuid)
		{
		}
		string     linkName;
		EntityId   targetId;
		EntityGUID targetGuid;
	};

	class CEntityOriginalLinks
	{
	public:
		void StoreCurrentEntityState(IEntity& entity);
		void ApplyStoredStateToEntity(IEntity& entity) const;
	private:
		typedef std::vector<SEntityLinkDesc> TLinkDescs;
		TLinkDescs m_linkDescs;
	};

	typedef std::map<EntityId, CEntityOriginalLinks> TOriginalStateMap;

public:
	CEntityLinksSetNode(SActivationInfo* pActInfo)
	{
		if (gEnv->IsEditor())
		{
			gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CEntityLinksSetNode");
		}
	}

	~CEntityLinksSetNode()
	{
		if (gEnv->IsEditor())
		{
			gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config) override
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<string>("Name",     _HELP("Name of the link you want details from (beware : it is case sensitive). An empty name means the name is not relevant when removing or renaming. A name is mandatory for adding.")),
			InputPortConfig<int>("Index",       0,                                                                                                                                                                                        _HELP("When multiple links have the same name, you can access the ones after the first one by increasing this value. This is only used for Remove and Rename commands. -1 means all of them.")),
			InputPortConfig<EntityId>("Target", _HELP("Target Entity for Adding and Replacing a link.")),
			InputPortConfig_Void("Add",         _HELP("Adds a new entity link from the selected entity to the target with the specified link name.")),
			InputPortConfig_Void("Remove",      _HELP("Remove the link identified by its name, target and index.")),
			InputPortConfig_Void("Reset",       _HELP("Restore all of the selected entity's links to their original state.")),
			InputPortConfig_Void("Rename",      _HELP("Rename the link identified by its name, target and index.")),
			InputPortConfig<string>("NewName",  "",                                                                                                                                                                                       _HELP("New name to apply to the link when renaming."), _HELP("New Name"),NULL),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Failed",    _HELP("Triggered when getting the data you asked for failed (invalid entity selected or invalid index for that entity).")),
			OutputPortConfig_Void("Succeeded", _HELP("Triggered when succeeding to get the desired data.")),
			{ 0 }
		};
		config.nFlags = EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("This node lets you inspect the configuration of entity links.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_GAME);
		switch (event)
		{
		case eFE_Activate:
			{
				if (IEntity* pEntity = pActInfo->pEntity)
				{
					const bool bAdd = IsPortActive(pActInfo, eIP_Add);
					const bool bRemove = IsPortActive(pActInfo, eIP_Remove);
					const bool bRename = IsPortActive(pActInfo, eIP_Rename);
					const bool bReset = IsPortActive(pActInfo, eIP_Reset);

					bool bSucceeded = false;
					bool bFailed = false;

					if (bAdd || bRemove || bRename)
					{
						StoreOriginalState(*pEntity);

						const string& linkName = GetPortString(pActInfo, eIP_LinkName);
						const int linkIndex = GetPortInt(pActInfo, eIP_Index);
						const EntityId linkTargetId = GetPortEntityId(pActInfo, eIP_Target);

						if (bRemove)
						{
							if (DoRemove(*pEntity, linkName, linkIndex, linkTargetId))
							{
								bSucceeded = true;
							}
							else
							{
								bFailed = true;
							}
						}

						if (bAdd)
						{
							if ((linkTargetId != INVALID_ENTITYID) && !linkName.empty())
							{
								if (IEntity* pTargetEntity = gEnv->pEntitySystem->GetEntity(linkTargetId))
								{
									pEntity->AddEntityLink(linkName.c_str(), linkTargetId, pTargetEntity->GetGuid());
									bSucceeded = true;
								}
								else
								{
									bFailed = true;
								}
							}
							else
							{
								bFailed = true;
							}
						}

						if (bRename && !bRemove)
						{
							if (DoRename(*pEntity, linkName, linkIndex, linkTargetId, GetPortString(pActInfo, eIP_NewName)))
							{
								bSucceeded = true;
							}
							else
							{
								bFailed = true;
							}
						}
					}

					if (bReset)
					{
						if (ResetOriginalState(*pEntity))
						{
							bSucceeded = true;
						}
						else
						{
							bFailed = true;
						}
					}

					if (bSucceeded)
					{
						ActivateOutput(pActInfo, eOP_Succeeded, true);
					}

					if (bFailed)
					{
						ActivateOutput(pActInfo, eOP_Failed, true);
					}
				}
			}
		}
	}

	static bool DoRename(IEntity& entity, const string& linkName, int linkIndex, EntityId linkTargetId, const string& newLinkName)
	{
		bool bResult = false;
		if (linkIndex >= 0)
		{
			if (IEntityLink* pLink = FindLink(entity, linkName, linkIndex, linkTargetId))
			{
				entity.RenameEntityLink(pLink, newLinkName.c_str());
				bResult = true;
			}
		}
		else
		{
			while (IEntityLink* pLink = FindLink(entity, linkName, 0, linkTargetId))
			{
				entity.RenameEntityLink(pLink, newLinkName.c_str());
			}
			bResult = true;
		}

		return bResult;
	}

	static bool DoRemove(IEntity& entity, const string& linkName, int linkIndex, EntityId linkTargetId)
	{
		bool bResult = false;
		if (linkIndex >= 0)
		{
			if (IEntityLink* pLink = FindLink(entity, linkName, linkIndex, linkTargetId))
			{
				entity.RemoveEntityLink(pLink);
				bResult = true;
			}
		}
		else
		{
			while (IEntityLink* pLink = FindLink(entity, linkName, 0, linkTargetId))
			{
				entity.RemoveEntityLink(pLink);
			}
			bResult = true;
		}

		return bResult;
	}

	static IEntityLink* FindLink(IEntity& entity, const string& linkName, const int linkIndex, EntityId linkTargetId)
	{
		IEntityLink* pLink = entity.GetEntityLinks();
		int nameIndex = linkIndex;
		while (pLink)
		{
			if ((linkName.empty() || linkName == pLink->name)
			    && ((linkTargetId == INVALID_ENTITYID) || (linkTargetId == pLink->entityId)))
			{
				if (--nameIndex < 0)
				{
					break;
				}
			}
			pLink = pLink->next;
		}
		return pLink;
	}

	void StoreOriginalState(IEntity& entity)
	{
		EntityId entityId = entity.GetId();
		TOriginalStateMap::iterator it = m_originalStateMap.find(entityId);
		if (it == m_originalStateMap.end())
		{
			m_originalStateMap[entityId].StoreCurrentEntityState(entity);
		}
	}

	bool ResetOriginalState(IEntity& entity)
	{
		bool bSuccess = false;
		EntityId entityId = entity.GetId();
		TOriginalStateMap::iterator it = m_originalStateMap.find(entityId);
		if (it != m_originalStateMap.end())
		{
			it->second.ApplyStoredStateToEntity(entity);
			bSuccess = true;
		}
		return bSuccess;
	}

private:
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override
	{
		switch (event)
		{
		case ESYSTEM_EVENT_EDITOR_GAME_MODE_CHANGED:
		case ESYSTEM_EVENT_EDITOR_SIMULATION_MODE_CHANGED:
			{
				IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
				for (const TOriginalStateMap::value_type& originalState : m_originalStateMap)
				{
					if (IEntity* pEntity = pEntitySystem->GetEntity(originalState.first))
					{
						originalState.second.ApplyStoredStateToEntity(*pEntity);
					}
				}
				m_originalStateMap.clear();
			}
			break;
		}
	}

	// These members are intended for this class to be a singleton.
	TOriginalStateMap m_originalStateMap;
};

void CEntityLinksSetNode::CEntityOriginalLinks::StoreCurrentEntityState(IEntity& entity)
{
	assert(m_linkDescs.empty()); // Storing the same entity twice ? Sounds like links could have been meddled with between these two storages.
	m_linkDescs.clear();
	for (IEntityLink* pLink = entity.GetEntityLinks(); pLink; pLink = pLink->next)
	{
		m_linkDescs.emplace_back(pLink->name, pLink->entityId, pLink->entityGuid);
	}
}

void CEntityLinksSetNode::CEntityOriginalLinks::ApplyStoredStateToEntity(IEntity& entity) const
{
	entity.RemoveAllEntityLinks();
	for (const SEntityLinkDesc& linkDesc : m_linkDescs)
	{
		entity.AddEntityLink(linkDesc.linkName, linkDesc.targetId, linkDesc.targetGuid);
	}
}

class CEntityLayerSwitchListenerNode final
	: public CFlowBaseNode<eNCT_Instanced>
	  , private IEntityEventListener
	  , private ISystemEventListener
{
	enum OUTPUTS
	{
		eOP_HiddenByLayer = 0,
		eOP_UnhiddenByLayer,
	};

public:
	CEntityLayerSwitchListenerNode(SActivationInfo* pActInfo)
		: m_actInfo(*pActInfo)
		, m_currentlyRegisteredTo(INVALID_ENTITYID)
	{
		if (gEnv->IsEditor())
		{
			gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CEntityLayerSwitchListenerNode");
		}
	}

	~CEntityLayerSwitchListenerNode()
	{
		UnregisterFromCurrentlyRegistered();
		if (gEnv->IsEditor())
		{
			gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) override final
	{
		return new CEntityLayerSwitchListenerNode(pActInfo);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config) override
	{
		static const SInputPortConfig in_config[] =
		{
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("HiddenByLayer",   _HELP("Triggered when the entity is hidden by its layer.")),
			OutputPortConfig_Void("UnhiddenByLayer", _HELP("Triggered when the entity is unhidden by its layer.")),
			{ 0 }
		};
		config.nFlags = EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("This node lets you listen to layer hiding/unhiding events for a given entity.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_GAME);
		switch (event)
		{
		case eFE_SetEntityId:
		case eFE_Initialize:
			{
				m_actInfo = *pActInfo;
				RefreshListener();
			}
			break;
		}
	}

	void UnregisterFromCurrentlyRegistered()
	{
		if (m_currentlyRegisteredTo != INVALID_ENTITYID)
		{
			IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
			pEntitySystem->RemoveEntityEventListener(m_currentlyRegisteredTo, ENTITY_EVENT_LAYER_HIDE, this);
			pEntitySystem->RemoveEntityEventListener(m_currentlyRegisteredTo, ENTITY_EVENT_LAYER_UNHIDE, this);
			pEntitySystem->RemoveEntityEventListener(m_currentlyRegisteredTo, ENTITY_EVENT_DONE, this);

			m_currentlyRegisteredTo = INVALID_ENTITYID;
		}
	}

	void RegisterToActInfo()
	{
		if (m_actInfo.pEntity)
		{
			IEntitySystem* pEntitySystem = gEnv->pEntitySystem;

			m_currentlyRegisteredTo = m_actInfo.pEntity->GetId();

			pEntitySystem->AddEntityEventListener(m_currentlyRegisteredTo, ENTITY_EVENT_LAYER_HIDE, this);
			pEntitySystem->AddEntityEventListener(m_currentlyRegisteredTo, ENTITY_EVENT_LAYER_UNHIDE, this);
			pEntitySystem->AddEntityEventListener(m_currentlyRegisteredTo, ENTITY_EVENT_DONE, this);
		}
	}

	void RefreshListener()
	{
		const bool bRegisteringToSame = (m_actInfo.pEntity && (m_actInfo.pEntity->GetId() == m_currentlyRegisteredTo))
		                                || (!m_actInfo.pEntity && (m_currentlyRegisteredTo == INVALID_ENTITYID));

		if (!bRegisteringToSame)
		{
			UnregisterFromCurrentlyRegistered();
			RegisterToActInfo();
		}
	}

	virtual void OnEntityEvent(IEntity* pEntity, const SEntityEvent& evt) override
	{
		switch (evt.event)
		{
		case ENTITY_EVENT_LAYER_HIDE:
		case ENTITY_EVENT_LAYER_UNHIDE:
			ActivateOutput(&m_actInfo, evt.event == ENTITY_EVENT_LAYER_HIDE ? eOP_HiddenByLayer : eOP_UnhiddenByLayer, true);
			break;
		case ENTITY_EVENT_DONE:
			// That's a bit fishy (having FG referencing a deleted entity), but let's make sure we don't keep a dangling pointer to the it.
			m_actInfo.pEntity = nullptr;
			UnregisterFromCurrentlyRegistered();
			break;
		}
	}

private:
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override
	{
		switch (event)
		{
		case ESYSTEM_EVENT_EDITOR_GAME_MODE_CHANGED:
		case ESYSTEM_EVENT_EDITOR_SIMULATION_MODE_CHANGED:
			{
				if (wparam == 0)
				{
					UnregisterFromCurrentlyRegistered();
				}
			}
			break;
		}
	}

	SActivationInfo m_actInfo;
	EntityId        m_currentlyRegisteredTo;
};

class CEntityGlobalLayerSwitchListenerNode final
	: public CFlowBaseNode<eNCT_Instanced>
	  , private ISystemEventListener
	  , private IEntityLayerListener
{
	enum INPUTS
	{
		eIP_LayerName = 0,
		eIP_ForceGet,
	};

	enum OUTPUTS
	{
		eOP_LayerDeactivated = 0,
		eOP_LayerActivated,
	};

public:
	CEntityGlobalLayerSwitchListenerNode(SActivationInfo* pActInfo)
		: m_actInfo(*pActInfo)
		, m_currentlyRegisteredTo()
	{
		if (gEnv->IsEditor())
		{
			gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CEntityGlobalLayerSwitchListenerNode");
		}
	}

	~CEntityGlobalLayerSwitchListenerNode()
	{
		UnregisterFromCurrentlyRegistered();
		if (gEnv->IsEditor())
		{
			gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) override final
	{
		return new CEntityGlobalLayerSwitchListenerNode(pActInfo);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config) override
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<string>("LayerName", _HELP("Name of the layer you want to know about.")),
			InputPortConfig_Void("ForceGet",     _HELP("Force get the state of the layer (usually not necesary as the output should trigger automatically when the layer's activation state changes).")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Hidden",   _HELP("Triggered when the layer is hidden.")),
			OutputPortConfig_Void("Unhidden", _HELP("Triggered when the layer is unhidden.")),
			{ 0 }
		};
		config.sDescription = _HELP("This node lets you listen to layer hiding/unhiding events.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_GAME);
		switch (event)
		{
		case eFE_Initialize:
			{
				m_actInfo = *pActInfo;
				RefreshListener();
			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIP_ForceGet))
				{
					const string& layerName = GetPortString(pActInfo, eIP_LayerName);
					gEnv->pEntitySystem->IsLayerEnabled(layerName.c_str(), true, false);
				}
			}
			break;
		}
	}

	void UnregisterFromCurrentlyRegistered()
	{
		if (!m_currentlyRegisteredTo.empty())
		{
			IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
			pEntitySystem->RemoveEntityLayerListener(m_currentlyRegisteredTo.c_str(), this, false);
			m_currentlyRegisteredTo.clear();
		}
	}

	void RegisterToActInfo()
	{
		const string& layerName = GetPortString(&m_actInfo, eIP_LayerName);
		if (!layerName.empty())
		{
			IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
			pEntitySystem->AddEntityLayerListener(layerName.c_str(), this, false);
			m_currentlyRegisteredTo = layerName;
		}
	}

	void RefreshListener()
	{
		const string& layerName = GetPortString(&m_actInfo, eIP_LayerName);
		const bool bRegisteringToSame = (0 == stricmp(m_currentlyRegisteredTo.c_str(), layerName.c_str()));

		if (!bRegisteringToSame)
		{
			UnregisterFromCurrentlyRegistered();
			RegisterToActInfo();
		}
	}

	virtual void LayerEnabled(bool bActivated) override
	{
		ActivateOutput(&m_actInfo, bActivated ? eOP_LayerActivated : eOP_LayerDeactivated, true);
	}

private:
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override
	{
		switch (event)
		{
		case ESYSTEM_EVENT_EDITOR_GAME_MODE_CHANGED:
		case ESYSTEM_EVENT_EDITOR_SIMULATION_MODE_CHANGED:
			{
				if (wparam == 0)
				{
					UnregisterFromCurrentlyRegistered();
				}
			}
			break;
		}
	}

	SActivationInfo m_actInfo;
	string          m_currentlyRegisteredTo;
};

REGISTER_FLOW_NODE("Entity:CharacterAttachmentControl", CEntityAttachmentControlNode);
REGISTER_FLOW_NODE("Entity:AttachmentEx", CEntityAttachmentExNode);
REGISTER_FLOW_NODE("Entity:LinksGet", CEntityLinksGetNode);
REGISTER_FLOW_NODE("Entity:LinksSet", CEntityLinksSetNode);
REGISTER_FLOW_NODE("Entity:LayerSwitchListener", CEntityLayerSwitchListenerNode);
REGISTER_FLOW_NODE("Entity:GlobalLayerListener", CEntityGlobalLayerSwitchListenerNode);
