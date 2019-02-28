// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameVolume_Water.h"

#include <CryGame/IGameVolumes.h>

//#pragma optimize("", off)
//#pragma inline_depth(0)

namespace
{
bool GetVolumeInfoForEntity(EntityId entityId, IGameVolumes::VolumeInfo& volumeInfo)
{
	IGameVolumes* pGameVolumesMgr = gEnv->pGameFramework->GetIGameVolumesManager();
	if (pGameVolumesMgr != NULL)
	{
		return pGameVolumesMgr->GetVolumeInfoForEntity(entityId, &volumeInfo);
	}

	return false;
}
}

namespace GVW
{
void RegisterEvents(IGameObjectExtension& goExt, IGameObject& gameObject)
{
	const int eventID = eGFE_ScriptEvent;
	gameObject.UnRegisterExtForEvents(&goExt, NULL, 0);
	gameObject.RegisterExtForEvents(&goExt, &eventID, 1);
}
}

CGameVolume_Water::CGameVolume_Water()
	: m_lastAwakeCheckPosition(ZERO)
	, m_volumeDepth(0.0f)
	, m_streamSpeed(0.0f)
	, m_awakeAreaWhenMoving(false)
	, m_isRiver(false)
{
	m_baseMatrix.SetIdentity();
	m_initialMatrix.SetIdentity();

	m_segments.resize(1);
}

CGameVolume_Water::~CGameVolume_Water()
{
	DestroyPhysicsAreas();

	if (gEnv->p3DEngine)
	{
		WaterSegments::iterator iter = m_segments.begin();
		WaterSegments::iterator end = m_segments.end();
		while (iter != end)
		{
			if (iter->m_pWaterRenderNode)
			{
				iter->m_pWaterRenderNode->ReleaseNode();
				iter->m_pWaterRenderNode = NULL;
			}

			++iter;
		}
	}
}

bool CGameVolume_Water::Init(IGameObject* pGameObject)
{
	SetGameObject(pGameObject);

	if (!GetGameObject()->BindToNetwork())
		return false;

	return true;
}

void CGameVolume_Water::PostInit(IGameObject* pGameObject)
{
	GVW::RegisterEvents(*this, *pGameObject);
	SetupVolume();

	//////////////////////////////////////////////////////////////////////////
	/// For debugging purposes
	if (gEnv->IsEditor())
	{
		GetGameObject()->EnableUpdateSlot(this, 0);
	}
}

bool CGameVolume_Water::ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params)
{
	ResetGameObject();

	GVW::RegisterEvents(*this, *pGameObject);

	CRY_ASSERT_MESSAGE(false, "CGameVolume_Water::ReloadExtension not implemented");

	return false;
}

void CGameVolume_Water::Release()
{
	delete this;
}

void CGameVolume_Water::Update(SEntityUpdateContext& ctx, int slot)
{
	if (gEnv->IsEditing())
	{
		DebugDrawVolume();
	}
}

void CGameVolume_Water::HandleEvent(const SGameObjectEvent& gameObjectEvent)
{
	if ((gameObjectEvent.event == eGFE_ScriptEvent) && (gameObjectEvent.param != NULL))
	{
		const char* eventName = static_cast<const char*>(gameObjectEvent.param);
		if (strcmp(eventName, "PhysicsEnable") == 0)
		{
			IGameVolumes::VolumeInfo volumeInfo;
			if (GetVolumeInfoForEntity(GetEntityId(), volumeInfo))
			{
				for (uint32 i = 0; i < m_segments.size(); ++i)
				{
					SWaterSegment& segment = m_segments[i];

					if ((segment.m_pWaterArea == NULL) && (segment.m_pWaterRenderNode != NULL))
					{
						if (!m_isRiver)
						{
							CreatePhysicsArea(i, m_baseMatrix, &volumeInfo.pVertices[0], volumeInfo.verticesCount, false, m_streamSpeed);
						}
						else
						{
							Vec3 vertices[4];
							FillOutRiverSegment(i, &volumeInfo.pVertices[0], volumeInfo.verticesCount, &vertices[0]);
							CreatePhysicsArea(i, m_baseMatrix, vertices, 4, true, m_streamSpeed);
						}

						segment.m_pWaterRenderNode->SetMatrix(m_baseMatrix);
					}
				}

				AwakeAreaIfRequired(true);

				m_lastAwakeCheckPosition.zero();
			}
		}
		else if (strcmp(eventName, "PhysicsDisable") == 0)
		{
			DestroyPhysicsAreas();
		}
	}
}

void CGameVolume_Water::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_EDITOR_PROPERTY_CHANGED:
		{
			SetupVolume();
		}
		break;

	case ENTITY_EVENT_RESET:
		{
			// Only when exiting game mode
			if ((gEnv->IsEditor()) && (event.nParam[0] == 0))
			{
				if (Matrix34::IsEquivalent(m_baseMatrix, m_initialMatrix) == false)
				{
					GetEntity()->SetWorldTM(m_initialMatrix);
				}
			}
		}
		break;

	case ENTITY_EVENT_XFORM:
		{
			// Rotation requires to setup again
			// Internally the shape vertices are projected to a plane, and rotating changes that plane
			// Only allow rotations in the editor, at run time can be expensive
			if ((gEnv->IsEditing()) && (event.nParam[0] & ENTITY_XFORM_ROT))
			{
				SetupVolume();
			}
			else if (event.nParam[0] & ENTITY_XFORM_POS)
			{
				const Matrix34 entityWorldTM = GetEntity()->GetWorldTM();

				m_baseMatrix.SetTranslation(entityWorldTM.GetTranslation());

				for (uint32 i = 0; i < m_segments.size(); ++i)
				{
					SWaterSegment& segment = m_segments[i];

					UpdateRenderNode(segment.m_pWaterRenderNode, m_baseMatrix);

					if (segment.m_pWaterArea)
					{
						const Vec3 newAreaPosition = m_baseMatrix.GetTranslation() + ((Quat(m_baseMatrix) * segment.m_physicsLocalAreaCenter) - segment.m_physicsLocalAreaCenter);

						pe_params_pos position;
						position.pos = newAreaPosition;
						segment.m_pWaterArea->SetParams(&position);

						pe_params_buoyancy pb;
						pb.waterPlane.origin = newAreaPosition;
						segment.m_pWaterArea->SetParams(&pb);

						//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere( areaPos.pos, 0.5f, ColorB(255, 0, 0));
					}
				}

				AwakeAreaIfRequired(false);
			}
		}
		break;

	case ENTITY_EVENT_HIDE:
		{
			AwakeAreaIfRequired(true);

			WaterSegments::iterator iter = m_segments.begin();
			WaterSegments::iterator end = m_segments.end();
			while (iter != end)
			{
				if (iter->m_pWaterRenderNode)
				{
					iter->m_pWaterRenderNode->Hide(true);
				}

				++iter;
			}

			DestroyPhysicsAreas();
		}
		break;
	case ENTITY_EVENT_UNHIDE:
		{
			IGameVolumes::VolumeInfo volumeInfo;
			if (GetVolumeInfoForEntity(GetEntityId(), volumeInfo))
			{
				for (uint32 i = 0; i < m_segments.size(); ++i)
				{
					SWaterSegment& segment = m_segments[i];
					if (segment.m_pWaterRenderNode)
					{
						segment.m_pWaterRenderNode->Hide(false);

						if (!m_isRiver)
						{
							CreatePhysicsArea(i, m_baseMatrix, &volumeInfo.pVertices[0], volumeInfo.verticesCount, false, m_streamSpeed);
						}
						else
						{
							Vec3 vertices[4];
							FillOutRiverSegment(i, &volumeInfo.pVertices[0], volumeInfo.verticesCount, &vertices[0]);
							CreatePhysicsArea(i, m_baseMatrix, vertices, 4, true, m_streamSpeed);
						}

						segment.m_pWaterRenderNode->SetMatrix(m_baseMatrix);
					}
				}

				AwakeAreaIfRequired(true);

				m_lastAwakeCheckPosition.zero(); //After unhide, next movement will awake the area
			}

		}
		break;
	}
}

uint64 CGameVolume_Water::GetEventMask() const
{
	return 
		ENTITY_EVENT_BIT(ENTITY_EVENT_EDITOR_PROPERTY_CHANGED) |
		ENTITY_EVENT_BIT(ENTITY_EVENT_RESET) |
		ENTITY_EVENT_BIT(ENTITY_EVENT_XFORM) |
		ENTITY_EVENT_BIT(ENTITY_EVENT_HIDE) |
		ENTITY_EVENT_BIT(ENTITY_EVENT_UNHIDE);
}

void CGameVolume_Water::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
}

void CGameVolume_Water::CreateWaterRenderNode(IWaterVolumeRenderNode*& pWaterRenderNode)
{
	if (pWaterRenderNode == NULL)
	{
		pWaterRenderNode = static_cast<IWaterVolumeRenderNode*>(gEnv->p3DEngine->CreateRenderNode(eERType_WaterVolume));
		pWaterRenderNode->SetAreaAttachedToEntity();
	}
}

void CGameVolume_Water::SetupVolume()
{
	IGameVolumes::VolumeInfo volumeInfo;
	if (GetVolumeInfoForEntity(GetEntityId(), volumeInfo) == false)
		return;

	if (volumeInfo.verticesCount < 4)
		return;

	WaterProperties waterProperties(GetEntity());

	if (waterProperties.isRiver)
	{
		if (volumeInfo.verticesCount % 2 != 0)
			return;

		int numSegments = (volumeInfo.verticesCount / 2) - 1;

		m_segments.resize(numSegments);

		for (int i = 0; i < numSegments; ++i)
		{
			SetupVolumeSegment(waterProperties, i, &volumeInfo.pVertices[0], volumeInfo.verticesCount);
		}
	}
	else
	{
		SetupVolumeSegment(waterProperties, 0, &volumeInfo.pVertices[0], volumeInfo.verticesCount);
	}

#if CRY_PLATFORM_WINDOWS && !defined(_RELEASE)
	{
		if (gEnv->IsEditor())
		{
			IEntity* pEnt = GetEntity();
			IMaterial* pMat = pEnt ? pEnt->GetMaterial() : 0;
			if (pMat)
			{
				const SShaderItem& si = pMat->GetShaderItem();
				if (si.m_pShader && si.m_pShader->GetShaderType() != eST_Water)
				{
					CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR, "Incorrect shader set for water / water fog volume \"%s\"!", pEnt->GetName());
				}
			}
		}
	}
#endif

	m_baseMatrix = GetEntity()->GetWorldTM();
	m_initialMatrix = m_baseMatrix;
	m_volumeDepth = waterProperties.depth;
	m_streamSpeed = waterProperties.streamSpeed;
	m_awakeAreaWhenMoving = waterProperties.awakeAreaWhenMoving;
	m_isRiver = waterProperties.isRiver;
}

void CGameVolume_Water::SetupVolumeSegment(const WaterProperties& waterProperties, const uint32 segmentIndex, const Vec3* pVertices, const uint32 vertexCount)
{
	SWaterSegment& segment = m_segments[segmentIndex];

	IWaterVolumeRenderNode*& pWaterRenderNode = segment.m_pWaterRenderNode;

	CreateWaterRenderNode(pWaterRenderNode);

	CRY_ASSERT(pWaterRenderNode != NULL);

	const Matrix34& entityWorldTM = GetEntity()->GetWorldTM();

	pWaterRenderNode->SetMinSpec(waterProperties.minSpec);
	pWaterRenderNode->SetMaterialLayers((uint8)waterProperties.materialLayerMask);
	pWaterRenderNode->SetViewDistRatio(waterProperties.viewDistanceRatio);

	Plane fogPlane;
	fogPlane.SetPlane(Vec3Constants<float>::fVec3_OneZ, pVertices[0]);

	pWaterRenderNode->SetFogDensity(waterProperties.fogDensity);
	pWaterRenderNode->SetFogColor(waterProperties.fogColor * max(waterProperties.fogColorMultiplier, 0.0f));
	pWaterRenderNode->SetFogColorAffectedBySun(waterProperties.fogColorAffectedBySun);
	pWaterRenderNode->SetFogShadowing(waterProperties.fogShadowing);
	pWaterRenderNode->SetCapFogAtVolumeDepth(waterProperties.capFogAtVolumeDepth);

	pWaterRenderNode->SetCaustics(waterProperties.caustics);
	pWaterRenderNode->SetCausticIntensity(waterProperties.causticIntensity);
	pWaterRenderNode->SetCausticTiling(waterProperties.causticTiling);
	pWaterRenderNode->SetCausticHeight(waterProperties.causticHeight);

	const Vec3* segmentVertices = pVertices;
	uint32 segmentVertexCount = vertexCount;

	Vec3 vertices[4];

	if (waterProperties.isRiver)
	{
		FillOutRiverSegment(segmentIndex, pVertices, vertexCount, &vertices[0]);

		segmentVertices = &vertices[0];
		segmentVertexCount = 4;
	}

	pWaterRenderNode->CreateArea(0, &segmentVertices[0], segmentVertexCount, Vec2(waterProperties.uScale, waterProperties.vScale), fogPlane, false);

	pWaterRenderNode->SetMaterial(GetEntity()->GetMaterial());
	pWaterRenderNode->SetVolumeDepth(waterProperties.depth);
	pWaterRenderNode->SetStreamSpeed(waterProperties.streamSpeed);

	CreatePhysicsArea(segmentIndex, entityWorldTM, segmentVertices, segmentVertexCount, waterProperties.isRiver, waterProperties.streamSpeed);

	// NOTE:
	// Set the matrix after everything has been setup in local space
	UpdateRenderNode(pWaterRenderNode, entityWorldTM);
}

void CGameVolume_Water::CreatePhysicsArea(const uint32 segmentIndex, const Matrix34& baseMatrix, const Vec3* pVertices, uint32 vertexCount, const bool isRiver, const float streamSpeed)
{
	//Destroy previous physics if any
	if (segmentIndex == 0)
	{
		DestroyPhysicsAreas();
	}

	SWaterSegment& segment = m_segments[segmentIndex];

	IWaterVolumeRenderNode* pWaterRenderNode = segment.m_pWaterRenderNode;
	Vec3 waterFlow(ZERO);

	CRY_ASSERT(segment.m_pWaterArea == NULL);
	CRY_ASSERT(pWaterRenderNode != NULL);

	pWaterRenderNode->SetMatrix(Matrix34::CreateIdentity());
	segment.m_pWaterArea = pWaterRenderNode->SetAndCreatePhysicsArea(&pVertices[0], vertexCount);

	IPhysicalEntity* pWaterArea = segment.m_pWaterArea;
	if (pWaterArea)
	{
		const Quat entityWorldRot = Quat(baseMatrix);

		pe_status_pos posStatus;
		pWaterArea->GetStatus(&posStatus);

		const Vec3 areaPosition = baseMatrix.GetTranslation() + ((entityWorldRot * posStatus.pos) - posStatus.pos);

		pe_params_pos position;
		position.pos = areaPosition;
		position.q = entityWorldRot;
		pWaterArea->SetParams(&position);

		pe_params_buoyancy pb;
		pb.waterPlane.n = entityWorldRot * Vec3(0, 0, 1);
		pb.waterPlane.origin = areaPosition;

		if (isRiver)
		{
			int i = segmentIndex;
			int j = vertexCount - 1 - segmentIndex;
			pb.waterFlow = ((pVertices[1] - pVertices[0]).GetNormalized() + (pVertices[2] - pVertices[3]).GetNormalized()) / 2.f * streamSpeed;
		}
		pWaterArea->SetParams(&pb);

		pe_params_foreign_data pfd;
		pfd.pForeignData = pWaterRenderNode;
		pfd.iForeignData = PHYS_FOREIGN_ID_WATERVOLUME;
		pfd.iForeignFlags = 0;
		pWaterArea->SetParams(&pfd);

		segment.m_physicsLocalAreaCenter = posStatus.pos;
	}
}

void CGameVolume_Water::DestroyPhysicsAreas()
{
	WaterSegments::iterator iter = m_segments.begin();
	WaterSegments::iterator end = m_segments.end();
	while (iter != end)
	{
		if (iter->m_pWaterArea)
		{
			if (gEnv->pPhysicalWorld)
			{
				gEnv->pPhysicalWorld->DestroyPhysicalEntity(iter->m_pWaterArea);
			}

			iter->m_pWaterArea = NULL;
		}

		++iter;
	}

	m_lastAwakeCheckPosition.zero();
}

void CGameVolume_Water::AwakeAreaIfRequired(bool forceAwake)
{
	if (gEnv->IsEditing())
		return;

	if ((m_segments[0].m_pWaterArea == NULL) || (m_awakeAreaWhenMoving == false))
		return;

	const float thresholdSqr = (2.0f * 2.0f);
	const float movedDistanceSqr = (m_baseMatrix.GetTranslation() - m_lastAwakeCheckPosition).len2();
	if ((forceAwake == false) && (movedDistanceSqr <= thresholdSqr))
		return;

	m_lastAwakeCheckPosition = m_baseMatrix.GetTranslation();

	WaterSegments::iterator iter = m_segments.begin();
	WaterSegments::iterator end = m_segments.end();
	while (iter != end)
	{
		CRY_ASSERT(iter->m_pWaterArea);
		pe_status_pos areaPos;
		iter->m_pWaterArea->GetStatus(&areaPos);

		const Vec3 areaBoxCenter = areaPos.pos;
		IPhysicalEntity** pEntityList = NULL;

		const int numberOfEntities = gEnv->pPhysicalWorld->GetEntitiesInBox(areaBoxCenter + areaPos.BBox[0], areaBoxCenter + areaPos.BBox[1], pEntityList, ent_sleeping_rigid | ent_rigid);

		pe_action_awake awake;
		awake.bAwake = true;

		for (int i = 0; i < numberOfEntities; ++i)
		{
			pEntityList[i]->Action(&awake);
		}

		++iter;
	}
}

void CGameVolume_Water::UpdateRenderNode(IWaterVolumeRenderNode* pWaterRenderNode, const Matrix34& newLocation)
{
	if (pWaterRenderNode)
	{
		gEnv->p3DEngine->FreeRenderNodeState(pWaterRenderNode);

		pWaterRenderNode->SetMatrix(newLocation);

		gEnv->p3DEngine->RegisterEntity(pWaterRenderNode);
	}
}

void CGameVolume_Water::FillOutRiverSegment(const uint32 segmentIndex, const Vec3* pVertices, const uint32 vertexCount, Vec3* pVerticesOut)
{
	int i = segmentIndex;
	int j = vertexCount - 1 - segmentIndex;

	pVerticesOut[0] = pVertices[i];
	pVerticesOut[1] = pVertices[i + 1];
	pVerticesOut[2] = pVertices[j - 1];
	pVerticesOut[3] = pVertices[j];
}

void CGameVolume_Water::DebugDrawVolume()
{
	IGameVolumes::VolumeInfo volumeInfo;
	if (GetVolumeInfoForEntity(GetEntityId(), volumeInfo) == false)
		return;

	if (volumeInfo.verticesCount < 3)
		return;

	const Matrix34 worldTM = GetEntity()->GetWorldTM();
	const Vec3 depthOffset = worldTM.GetColumn2().GetNormalized() * -m_volumeDepth;

	IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom();
	for (uint32 i = 0; i < volumeInfo.verticesCount - 1; ++i)
	{
		const Vec3 point1 = worldTM.TransformPoint(volumeInfo.pVertices[i]);
		const Vec3 point2 = worldTM.TransformPoint(volumeInfo.pVertices[i + 1]);

		pRenderAux->DrawLine(point1, Col_SlateBlue, point1 + depthOffset, Col_SlateBlue, 2.0f);
		pRenderAux->DrawLine(point1 + depthOffset, Col_SlateBlue, point2 + depthOffset, Col_SlateBlue, 2.0f);
	}

	const Vec3 firstPoint = worldTM.TransformPoint(volumeInfo.pVertices[0]);
	const Vec3 lastPoint = worldTM.TransformPoint(volumeInfo.pVertices[volumeInfo.verticesCount - 1]);

	pRenderAux->DrawLine(lastPoint, Col_SlateBlue, lastPoint + depthOffset, Col_SlateBlue, 2.0f);
	pRenderAux->DrawLine(lastPoint + depthOffset, Col_SlateBlue, firstPoint + depthOffset, Col_SlateBlue, 2.0f);
}
