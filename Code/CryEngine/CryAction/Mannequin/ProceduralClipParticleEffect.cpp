// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   History:
   - 14:12:2011   Created by Jean Geffroy
*************************************************************************/

#include "StdAfx.h"

#include <CryExtension/ClassWeaver.h>
#include <Mannequin/Serialization.h>

#include "ICryMannequin.h"
#include "ICryMannequinEditor.h"

#include <CryParticleSystem/ParticleParams.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct SPlayParticleEffectParams : public IProceduralParams
{
	SPlayParticleEffectParams()
		: posOffset(ZERO)
		, rotOffset(ZERO)
		, cloneAttachment(false)
		, killOnExit(false)
		, keepEmitterActive(false)
	{
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		ar(Serialization::ParticlePickerLegacy(effectName), "EffectName", "Effect Name");
		ar(Serialization::JointName(jointName), "JointName", "Joint Name");
		ar(Serialization::AttachmentName(attachmentName), "AttachmentName", "Attachment Name");
		ar(posOffset, "PosOffset", "Position Offset");
		ar(rotOffset, "RotOffset", "Rotation Offset");
		ar(cloneAttachment, "CloneAttachment", "Clone Attachment");
		ar(killOnExit, "KillOnExit", "Kill on Exit");
		ar(keepEmitterActive, "KeepEmitterActive", "Keep Emitter Active");
	}

	virtual void GetExtraDebugInfo(StringWrapper& extraInfoOut) const override
	{
		extraInfoOut = effectName.c_str();
	}

	virtual SEditorCreateTransformGizmoResult OnEditorCreateTransformGizmo(IEntity& entity) override
	{
		return SEditorCreateTransformGizmoResult(entity, QuatT(posOffset, Quat(rotOffset)), jointName.crc ? jointName.crc : attachmentName.crc);
	}

	virtual void OnEditorMovedTransformGizmo(const QuatT& gizmoLocation) override
	{
		posOffset = gizmoLocation.t;
		rotOffset = Ang3(gizmoLocation.q);
	}

	TProcClipString effectName;
	SProcDataCRC    jointName;
	SProcDataCRC    attachmentName;
	Vec3            posOffset;
	Ang3            rotOffset;
	bool            cloneAttachment;   //< Clone an attachment from the specified bone (so as to leave any existing attachment intact)
	bool            killOnExit;        //< Kills the emitter and particles when leaving the clip
	bool            keepEmitterActive; //< Keeps the emission of new particles when leaving the clip
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct SParticleEffectClipData
{
	SParticleEffectClipData()
		: pEmitter(NULL)
		, ownedAttachmentNameCRC(0)
		, slot(-1)
	{}

	_smart_ptr<IParticleEmitter> pEmitter;
	uint32                       ownedAttachmentNameCRC;
	int                          slot;
};

class CParticleEffectContext : public IProceduralContext
{
private:
	virtual ~CParticleEffectContext();

	typedef IProceduralContext BaseClass;

	struct SStopRequest
	{
		SStopRequest(SParticleEffectClipData& _data, EntityId _entityId, ICharacterInstance* _pCharInst)
			: pCharInst(_pCharInst)
			, data(_data)
			, entityId(_entityId)
			, isFinished(false)
		{}

		static bool HasFinished(const SStopRequest& input) { return input.isFinished; }

		_smart_ptr<ICharacterInstance> pCharInst;
		SParticleEffectClipData        data;
		EntityId                       entityId;   //< EntityId owning this request.
		bool                           isFinished; //< The request has already been processed.
	};

public:
	PROCEDURAL_CONTEXT(CParticleEffectContext, "ParticleEffectContext", "9ced3080-5f88-45a3-f527-229c51b542a8"_cry_guid);

	virtual void Update(float timePassed) override;
	void         StartEffect(SParticleEffectClipData& data, const SPlayParticleEffectParams& params, EntityId entityId, ICharacterInstance* pCharacterInstance, IActionController& pActionController);
	void         StopEffect(SParticleEffectClipData& data, const SPlayParticleEffectParams& params, EntityId entityId, ICharacterInstance* pCharacterInstance);

private:

	bool IsEffectAlive(const SStopRequest& request) const;
	void ShutdownEffect(SStopRequest& request);

private:

	typedef std::vector<SStopRequest> TStopRequestVector;
	TStopRequestVector m_stopRequests;
};

CRYREGISTER_CLASS(CParticleEffectContext);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CProceduralClipParticleEffect : public TProceduralContextualClip<CParticleEffectContext, SPlayParticleEffectParams>
{
public:
	CProceduralClipParticleEffect()
	{
	}

	virtual void OnEnter(float blendTime, float duration, const SPlayParticleEffectParams& params)
	{
		if (gEnv->IsEditor() && gEnv->pGameFramework->GetMannequinInterface().IsSilentPlaybackMode())
			return;

		if (gEnv->IsDedicated())
			return;

		m_context->StartEffect(m_data, params, m_scope->GetEntityId(), m_scope->GetCharInst(), m_scope->GetActionController());
	}

	virtual void OnExit(float blendTime)
	{
		if (m_data.pEmitter)
		{
			const SPlayParticleEffectParams& params = GetParams();
			if (params.killOnExit)
			{
				m_data.pEmitter->Kill();
			}
			else if (!params.keepEmitterActive)
			{
				m_data.pEmitter->Activate(false);
			}

			m_context->StopEffect(m_data, params, m_scope->GetEntityId(), m_scope->GetCharInst());
		}
	}

	virtual void Update(float timePassed)
	{
	}

private:
	SParticleEffectClipData m_data;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipParticleEffect, "ParticleEffect");

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Impl
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CParticleEffectContext::~CParticleEffectContext()
{
	//Destroy all requests as the context is over
	for (uint32 i = 0u, sz = m_stopRequests.size(); i < sz; ++i)
	{
		SStopRequest& request = m_stopRequests[i];
		if (IsEffectAlive(request))
		{
			request.data.pEmitter->Activate(false);
		}
		ShutdownEffect(request);
	}
}

void CParticleEffectContext::Update(float timePassed)
{
	for (uint32 i = 0u, sz = m_stopRequests.size(); i < sz; ++i)
	{
		SStopRequest& request = m_stopRequests[i];
		if (!IsEffectAlive(request))
		{
			ShutdownEffect(request);
		}
	}

	m_stopRequests.erase(std::remove_if(m_stopRequests.begin(), m_stopRequests.end(), SStopRequest::HasFinished), m_stopRequests.end());
}

void CParticleEffectContext::StartEffect(SParticleEffectClipData& data, const SPlayParticleEffectParams& params, EntityId entityId, ICharacterInstance* pCharacterInstance, IActionController& pActionController)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);

	if (!pEntity)
		return;

	const char* const szEffectName = params.effectName.c_str();
	if (IParticleEffect* const pEffect = gEnv->pParticleManager->FindEffect(szEffectName, "Particle.SpawnEffect"))
	{
		IAttachment* pAttachment = NULL;

		if (pCharacterInstance)
		{
			IAttachmentManager& attachmentManager = *pCharacterInstance->GetIAttachmentManager();
			const char* szNewAttachmentName = NULL;
			const uint32 jointCrc = params.jointName.crc;
			int32 attachmentJointId = jointCrc ? pCharacterInstance->GetIDefaultSkeleton().GetJointIDByCRC32(jointCrc) : -1;
			if (attachmentJointId >= 0)
			{
				// Found given joint
				szNewAttachmentName = pCharacterInstance->GetIDefaultSkeleton().GetJointNameByID(attachmentJointId);
			}
			else
			{
				// Joint not found (or not defined by the user), try and find an attachment interface
				pAttachment = attachmentManager.GetInterfaceByNameCRC(params.attachmentName.crc);
				if (params.cloneAttachment && pAttachment && pAttachment->GetType() == CA_BONE)
				{
					// Clone an already existing attachment interface
					attachmentJointId = pAttachment->GetJointID();
					szNewAttachmentName = pAttachment->GetName();
				}
			}

			if (szNewAttachmentName && attachmentJointId >= 0)
			{
				// Create new attachment interface with a unique name
				static uint16 s_clonedAttachmentCount = 0;
				++s_clonedAttachmentCount;
				CryFixedStringT<64> attachmentCloneName;
				attachmentCloneName.Format("%s%s%u", szNewAttachmentName, "FXClone", s_clonedAttachmentCount);

				const char* const pBoneName = pCharacterInstance->GetIDefaultSkeleton().GetJointNameByID(attachmentJointId);
				const IAttachment* const pOriginalAttachment = pAttachment;
				pAttachment = attachmentManager.CreateAttachment(attachmentCloneName.c_str(), CA_BONE, pBoneName);
				data.ownedAttachmentNameCRC = pAttachment->GetNameCRC();

				if (!pOriginalAttachment)
				{
					// Attachment newly created from a joint: clear the relative transform
					pAttachment->AlignJointAttachment();
				}
				else
				{
					// Cloning an attachment: copy original transforms
					CRY_ASSERT(params.cloneAttachment);
					pAttachment->SetAttAbsoluteDefault(pOriginalAttachment->GetAttAbsoluteDefault());
					pAttachment->SetAttRelativeDefault(pOriginalAttachment->GetAttRelativeDefault());
				}
			}

			if (pAttachment && (pAttachment->GetType() == CA_BONE))
			{
				const Matrix34 offsetMatrix(Vec3(1.0f), Quat::CreateRotationXYZ(params.rotOffset), params.posOffset);
				CEffectAttachment* const pEffectAttachment = new CEffectAttachment(pEffect, offsetMatrix.GetTranslation(), offsetMatrix.GetColumn1(), 1.0f);
				pAttachment->AddBinding(pEffectAttachment);
				pAttachment->UpdateAttModelRelative();
				pEffectAttachment->ProcessAttachment(pAttachment);
				data.pEmitter = pEffectAttachment->GetEmitter();

				if (data.pEmitter)
				{
					// Put the particle emitter in the entity's render proxy so that it can be updated based on visibility.
					data.slot = pEntity->SetParticleEmitter(-1, data.pEmitter.get());
				}
			}
		}

		if (!data.pEmitter && !pAttachment)
		{
			if (pEffect->GetParticleParams().eAttachType != GeomType_None)
			{
				data.slot = pEntity->LoadParticleEmitter(-1, pEffect);
				SEntitySlotInfo slotInfo;
				if (pEntity->GetSlotInfo(data.slot, slotInfo))
				{
					data.pEmitter = slotInfo.pParticleEmitter;
				}
			}
			else
			{
				const Matrix34& transform = pEntity->GetWorldTM();
				const Matrix34 localOffset(Vec3(1.0f), Quat(params.rotOffset), params.posOffset);
				data.pEmitter = pEffect->Spawn(transform * localOffset);
			}
		}

		if (data.pEmitter)
		{
			IMannequin& mannequinInterface = gEnv->pGameFramework->GetMannequinInterface();
			const uint32 numListeners = mannequinInterface.GetNumMannequinGameListeners();
			for (uint32 itListeners = 0; itListeners < numListeners; ++itListeners)
			{
				mannequinInterface.GetMannequinGameListener(itListeners)->OnSpawnParticleEmitter(data.pEmitter, pActionController);
			}
		}
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CParticleEffectContext: could not load requested effect %s", szEffectName);
	}
}

void CParticleEffectContext::StopEffect(SParticleEffectClipData& data, const SPlayParticleEffectParams& params, EntityId entityId, ICharacterInstance* pCharacterInstance)
{
	SStopRequest newRequest(data, entityId, pCharacterInstance);

	//overwrite the attachmentNameCRC if needed
	newRequest.data.ownedAttachmentNameCRC = newRequest.data.ownedAttachmentNameCRC ? newRequest.data.ownedAttachmentNameCRC : params.attachmentName.crc;

	//try immediate stop
	if (!IsEffectAlive(newRequest))
	{
		ShutdownEffect(newRequest);
	}

	//we need to defer the stop request for later.
	if (!newRequest.isFinished)
	{
		m_stopRequests.push_back(newRequest);
	}
}

bool CParticleEffectContext::IsEffectAlive(const SStopRequest& request) const
{
	return (request.data.pEmitter && request.data.pEmitter->IsAlive());
}

void CParticleEffectContext::ShutdownEffect(SStopRequest& request)
{
	//Kill the attachment,emitter and request
	if (request.data.pEmitter)
	{
		if (request.pCharInst)
		{
			IAttachmentManager& attachmentManager = *request.pCharInst->GetIAttachmentManager();
			if (IAttachment* pAttachment = attachmentManager.GetInterfaceByNameCRC(request.data.ownedAttachmentNameCRC))
			{
				IAttachmentObject* pAttachmentObject = pAttachment->GetIAttachmentObject();
				if (pAttachmentObject && (pAttachmentObject->GetAttachmentType() == IAttachmentObject::eAttachment_Effect))
				{
					CEffectAttachment* pEffectAttachment = static_cast<CEffectAttachment*>(pAttachmentObject);
					if (pEffectAttachment->GetEmitter() == request.data.pEmitter)
					{
						pAttachment->ClearBinding();
					}
				}
			}
		}

		//need to clear emitters from slot(if slotted)
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(request.entityId);
		if (request.data.slot != -1 && pEntity)
		{
			pEntity->FreeSlot(request.data.slot);
			request.data.slot = -1;
		}

		request.data.pEmitter.reset();
	}

	request.isFinished = true;
}
