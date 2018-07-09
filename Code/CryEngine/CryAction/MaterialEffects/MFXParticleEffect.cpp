// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryParticleSystem/ParticleParams.h>
#ifndef _LIB
	#include <CryParticleSystem/ParticleParams_TypeInfo.h>
#endif
#include "MFXParticleEffect.h"
#include "MaterialEffectsCVars.h"
#include "IActorSystem.h"

IParticleAttributes::EType GetAttributeType(const char* szType)
{
	if(!cry_stricmp(szType, "bool"))
	{ 
		return IParticleAttributes::ET_Boolean;
	}
	else if(!cry_stricmp(szType, "int"))
	{
		return IParticleAttributes::ET_Integer;
	}
	else if (!cry_stricmp(szType, "float"))
	{
		return IParticleAttributes::ET_Float;
	}
	else if (!cry_stricmp(szType, "color"))
	{
		return IParticleAttributes::ET_Color;
	}

	return IParticleAttributes::ET_Count;
}

ColorF ColorBToColorF(const ColorB& colorB)
{
	return ColorF(colorB.r / 255.0f, colorB.g / 255.0f, colorB.b / 255.0f, colorB.a / 255.0f);
}

CMFXParticleEffect::CMFXParticleEffect()
	: CMFXEffectBase(eMFXPF_Particles)
	, m_particleParams()
{
}

CMFXParticleEffect::~CMFXParticleEffect()
{
}

void CMFXParticleEffect::Execute(const SMFXRunTimeEffectParams& params)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	Vec3 pos = params.pos;
	Vec3 dir = ZERO;
	Vec3 inDir = params.dir[0];
	Vec3 reverso = inDir * -1.0f;
	switch (m_particleParams.directionType)
	{
	case SMFXParticleParams::eDT_Normal:
		dir = params.normal;
		break;
	case SMFXParticleParams::eDT_Ricochet:
		dir = reverso.GetRotated(params.normal, gf_PI).normalize();
		break;
	case SMFXParticleParams::eDT_ProjectileDir:
		dir = -inDir;
		break;
	default:
		dir = params.normal;
		break;
	}

	bool tryToAttachEffect = (CMaterialEffectsCVars::Get().mfx_EnableAttachedEffects != 0);
	float distToPlayer = 0.f;
	IActor* pClientActor = gEnv->pGameFramework->GetClientActor();
	if (pClientActor)
	{
		distToPlayer = (pClientActor->GetEntity()->GetWorldPos() - params.pos).GetLength();
		tryToAttachEffect = tryToAttachEffect && (pClientActor->GetEntityId() != params.trg);
	}

	SMFXParticleEntries::const_iterator end = m_particleParams.m_entries.end();
	for (SMFXParticleEntries::const_iterator it = m_particleParams.m_entries.begin(); it != end; ++it)
	{
		// choose effect based on distance
		if ((it->maxdist == 0.f) || (distToPlayer <= it->maxdist) && !it->name.empty())
		{
			IParticleEffect* pParticle = gEnv->pParticleManager->FindEffect(it->name.c_str());

			if (pParticle)
			{
				const float pfx_minscale = (it->minscale != 0.f) ? it->minscale : CMaterialEffectsCVars::Get().mfx_pfx_minScale;
				const float pfx_maxscale = (it->maxscale != 0.f) ? it->maxscale : CMaterialEffectsCVars::Get().mfx_pfx_maxScale;
				const float pfx_maxdist = (it->maxscaledist != 0.f) ? it->maxscaledist : CMaterialEffectsCVars::Get().mfx_pfx_maxDist;

				const float truscale = pfx_minscale + ((pfx_maxscale - pfx_minscale) * (distToPlayer != 0.f ? min(1.0f, distToPlayer / pfx_maxdist) : 1.f));

				bool particleSpawnedAndAttached = tryToAttachEffect ? AttachToTarget(*it, params, pParticle, dir, truscale) : false;

				// If not attached, just spawn the particle
				if (particleSpawnedAndAttached == false)
				{
					if (IParticleEmitter* pEmitter = pParticle->Spawn(true, IParticleEffect::ParticleLoc(pos, dir, truscale)))
					{
						IParticleAttributes& particleAttributes = pEmitter->GetAttributes();
						for (const SMFXEmitterParameter& emitterParameter : it->parameters)
						{
							int paramIdx = particleAttributes.FindAttributeIdByName(emitterParameter.name.c_str());
							if (paramIdx != -1)
							{
								const SMFXEmitterParameter* paramToUse = &emitterParameter;

								// Check if we have runtime param set and use it instead of the static one in the XML
								for (const SMFXEmitterParameter& runtimeParam : params.particleParams)
								{
									if (runtimeParam == emitterParameter)
									{
										paramToUse = &runtimeParam;
										break;
									}
								}

								particleAttributes.SetValue(paramIdx, paramToUse->value);
							}
						}
					}
				}
			}

			break;
		}
	}
}

bool CMFXParticleEffect::AttachToTarget(const SMFXParticleEntry& particleParams, const SMFXRunTimeEffectParams& params, IParticleEffect* pParticleEffect, const Vec3& dir, float scale)
{
	bool shouldTryToAttach = particleParams.attachToTarget && (params.trg != 0);
	if (!shouldTryToAttach)
	{
		return false;
	}

	IEntity* pTargetEntity = gEnv->pEntitySystem->GetEntity(params.trg);
	if (pTargetEntity)
	{
		//Try to figure out if it's a character using physics type
		IPhysicalEntity* pTargetPhysics = pTargetEntity->GetPhysics();
		int physicsType = pTargetPhysics ? pTargetPhysics->GetType() : PE_NONE;

		bool isCharacter = (physicsType == PE_LIVING) || (physicsType == PE_ARTICULATED);

		if (isCharacter)
		{
			return AttachToCharacter(*pTargetEntity, particleParams, params, dir, scale);
		}
		//else
		//{
		//return AttachToEntity(*pTargetEntity, particleParams, params, pParticleEffect, dir, scale);
		//}
	}

	return false;
}

bool CMFXParticleEffect::AttachToCharacter(IEntity& targetEntity, const SMFXParticleEntry& particleParams, const SMFXRunTimeEffectParams& params, const Vec3& dir, float scale)
{
	if (params.partID >= 0)
	{
		//Assume character is loaded in first slot
		//We could iterate through all available slots, but first one should be good enough
		ICharacterInstance* pCharacterInstace = targetEntity.GetCharacter(0);
		ISkeletonPose* pSkeletonPose = pCharacterInstace ? pCharacterInstace->GetISkeletonPose() : NULL;
		if (pSkeletonPose)
		{
			IDefaultSkeleton& rIDefaultSkeleton = pCharacterInstace->GetIDefaultSkeleton();
			//It hit the character, but probably in a physicalized attached part, like armor plates, etc
			if (params.partID >= rIDefaultSkeleton.GetJointCount())
			{
				return false;
			}

			//It hit some valid joint, create an attachment
			const char* boneName = rIDefaultSkeleton.GetJointNameByID(params.partID);
			TAttachmentName attachmentName;
			GetNextCharacterAttachmentName(attachmentName);

			IAttachmentManager* pAttachmentManager = pCharacterInstace->GetIAttachmentManager();
			CRY_ASSERT(pAttachmentManager);

			//Remove the attachment first (in case was created before)
			pAttachmentManager->RemoveAttachmentByName(attachmentName.c_str());

			//Create attachment on nearest hit bone
			IAttachment* pAttachment = pAttachmentManager->CreateAttachment(attachmentName.c_str(), CA_BONE, boneName, false);
			if (pAttachment)
			{
				//Apply relative offsets
				const QuatT boneLocation = pSkeletonPose->GetAbsJointByID(params.partID);
				Matrix34 inverseJointTM = targetEntity.GetWorldTM() * Matrix34(boneLocation);
				inverseJointTM.Invert();
				Vec3 attachmentOffsetPosition = inverseJointTM * params.pos;
				Quat attachmentOffsetRotation = Quat(inverseJointTM) * targetEntity.GetRotation();

				CRY_ASSERT(attachmentOffsetPosition.IsValid());
				//CRY_ASSERT(attachmentOffsetRotation.IsUnit());

				pAttachment->SetAttRelativeDefault(QuatT(attachmentOffsetRotation, attachmentOffsetPosition));

				//Finally attach the effect
				CEffectAttachment* pEffectAttachment = new CEffectAttachment(particleParams.name.c_str(), Vec3(0, 0, 0), dir, scale);
				pAttachment->AddBinding(pEffectAttachment);

				return true;
			}
		}
	}

	return false;
}

bool CMFXParticleEffect::AttachToEntity(IEntity& targetEntity, const SMFXParticleEntry& particleParams, const SMFXRunTimeEffectParams& params, IParticleEffect* pParticleEffect, const Vec3& dir, float scale)
{
	if (pParticleEffect)
	{
		int effectSlot = targetEntity.LoadParticleEmitter(-1, pParticleEffect);
		if (effectSlot >= 0)
		{
			Matrix34 hitTM;
			hitTM.Set(Vec3(1.0f, 1.0f, 1.0f), Quat::CreateRotationVDir(dir), params.pos);

			Matrix34 localEffectTM = targetEntity.GetWorldTM().GetInverted() * hitTM;
			localEffectTM.ScaleColumn(Vec3(scale, scale, scale));

			CRY_ASSERT(localEffectTM.IsValid());

			targetEntity.SetSlotLocalTM(effectSlot, localEffectTM);

			return true;
		}
	}

	return false;
}

void CMFXParticleEffect::GetNextCharacterAttachmentName(TAttachmentName& attachmentName)
{
	static int nextId = 0;
	const int maxAttachmentIds = 6;

	attachmentName.Format("Mfx_Particle_Attachment%d", nextId);

	nextId = (nextId < maxAttachmentIds) ? (nextId + 1) : 0;
}

void CMFXParticleEffect::LoadParamsFromXml(const XmlNodeRef& paramsNode)
{
	// Xml data format
	/*
	   <Particle>
	   <Name userdata="..." scale="..." maxdist="..." minscale="..." maxscale="..." maxscaledist="..." attach="...">particle.name
	     <Attribute name="..." type="bool|float|int|color" value="..."/>
	   </Name>
	   <Direction>DirectionType</Direction>
	   </Particle>
	 */

	for (int i = 0; i < paramsNode->getChildCount(); ++i)
	{
		XmlNodeRef child = paramsNode->getChild(i);
		if (!strcmp(child->getTag(), "Name"))
		{
			SMFXParticleEntry entry;
			entry.name = child->getContent();

			if (child->haveAttr("userdata"))
				entry.userdata = child->getAttr("userdata");

			if (child->haveAttr("scale"))
				child->getAttr("scale", entry.scale);

			if (child->haveAttr("maxdist"))
				child->getAttr("maxdist", entry.maxdist);

			if (child->haveAttr("minscale"))
				child->getAttr("minscale", entry.minscale);

			if (child->haveAttr("maxscale"))
				child->getAttr("maxscale", entry.maxscale);

			if (child->haveAttr("maxscaledist"))
				child->getAttr("maxscaledist", entry.maxscaledist);

			if (child->haveAttr("attach"))
				child->getAttr("attach", entry.attachToTarget);

			entry.parameters.reserve(child->getChildCount());
			for (int j = 0; j < child->getChildCount(); ++j)
			{
				XmlNodeRef attribute = child->getChild(j);
				if (!strcmp(attribute->getTag(), "Attribute"))
				{
					SMFXEmitterParameter parameter;
					if (attribute->haveAttr("name") && attribute->haveAttr("type") && attribute->haveAttr("value"))
					{
						parameter.name = attribute->getAttr("name");
						IParticleAttributes::EType type = GetAttributeType(attribute->getAttr("type"));
						DO_FOR_ATTRIBUTE_TYPE(type, T,
						{
							T val;
							attribute->getAttr("value", val);
							parameter.value = val;
						});
						entry.parameters.emplace_back(parameter);
					}
				}
			}

			m_particleParams.m_entries.push_back(entry);
		}
	}

	SMFXParticleParams::EDirectionType directionType = SMFXParticleParams::eDT_Normal;
	XmlNodeRef dirType = paramsNode->findChild("Direction");
	if (dirType)
	{
		const char* val = dirType->getContent();
		if (!strcmp(val, "Normal"))
		{
			directionType = SMFXParticleParams::eDT_Normal;
		}
		else if (!strcmp(val, "Ricochet"))
		{
			directionType = SMFXParticleParams::eDT_Ricochet;
		}
		else if (!strcmp(val, "ProjectileDir"))
		{
			directionType = SMFXParticleParams::eDT_ProjectileDir;
		}
	}
	m_particleParams.directionType = directionType;

}

void CMFXParticleEffect::GetResources(SMFXResourceList& resourceList) const
{
	SMFXParticleListNode* listNode = SMFXParticleListNode::Create();

	if (!m_particleParams.m_entries.empty())
	{
		const SMFXParticleEntry& entry = m_particleParams.m_entries.back();
		listNode->m_particleParams.name = entry.name.c_str();
		listNode->m_particleParams.userdata = entry.userdata.c_str();
		listNode->m_particleParams.scale = entry.scale;
	}

	SMFXParticleListNode* next = resourceList.m_particleList;

	if (!next)
		resourceList.m_particleList = listNode;
	else
	{
		while (next->pNext)
			next = next->pNext;

		next->pNext = listNode;
	}
}

void CMFXParticleEffect::PreLoadAssets()
{
	LOADING_TIME_PROFILE_SECTION;

	SMFXParticleEntries::iterator it = m_particleParams.m_entries.begin();
	while (it != m_particleParams.m_entries.end())
	{
		if (gEnv->pParticleManager->FindEffect(it->name.c_str()) == NULL)
		{
			CryLog("MFXParticleEffect: Unable to find effect <%s>; Removing from list", it->name.c_str());
			it = m_particleParams.m_entries.erase(it);
		}
		else
		{
			++it;
		}

		SLICE_AND_SLEEP();
	}
}

void CMFXParticleEffect::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_particleParams.m_entries);
	for (size_t i = 0; i < m_particleParams.m_entries.size(); i++)
	{
		pSizer->AddObject(m_particleParams.m_entries[i].name);
		pSizer->AddObject(m_particleParams.m_entries[i].userdata);
	}
}
