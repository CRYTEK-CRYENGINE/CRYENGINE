// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SurfaceInfoPicker.h"
#include "Material/MaterialManager.h"
#include "Terrain/TerrainManager.h"
#include "Terrain/SurfaceType.h"
#include "Vegetation/VegetationMap.h"
#include "Vegetation/VegetationObject.h"
#include "Objects/EntityObject.h"
#include "Viewport.h"

static const float kEnoughFarDistance(5000.0f);

CSurfaceInfoPicker::CSurfaceInfoPicker() :
	m_pObjects(NULL),
	m_pSetObjects(NULL),
	m_PickOption(0)
{
}

bool CSurfaceInfoPicker::PickObject(const CPoint& point,
                                    SRayHitInfo& outHitInfo,
                                    CBaseObject* pObject)
{
	IDisplayViewport* view = GetIEditorImpl()->GetActiveView();
	if (!view)
	{
		return false;
	}

	Vec3 vWorldRaySrc, vWorldRayDir;
	view->ViewToWorldRay(point, vWorldRaySrc, vWorldRayDir);
	vWorldRaySrc = vWorldRaySrc + vWorldRayDir * 0.1f;
	vWorldRayDir = vWorldRayDir * kEnoughFarDistance;

	return PickObject(vWorldRaySrc, vWorldRayDir, outHitInfo, pObject);
}

bool CSurfaceInfoPicker::PickObject(const Vec3& vWorldRaySrc,
                                    const Vec3& vWorldRayDir,
                                    SRayHitInfo& outHitInfo,
                                    CBaseObject* pObject)
{
	if (pObject == NULL)
		return false;
	if (pObject->IsHidden() || IsFrozen(pObject))
		return false;

	Vec3 vDir = vWorldRayDir.GetNormalized() * kEnoughFarDistance;
	memset(&outHitInfo, 0, sizeof(outHitInfo));
	outHitInfo.fDistance = kEnoughFarDistance;

	AABB boundbox;
	pObject->GetBoundBox(boundbox);
	IDisplayViewport* view = GetIEditorImpl()->GetActiveView();
	if (!(view && view->IsBoundsVisible(boundbox)))
		return false;

	ObjectType objType(pObject->GetType());

	if (objType == OBJTYPE_SOLID || objType == OBJTYPE_VOLUMESOLID)
	{
		_smart_ptr<IStatObj> pStatObj = pObject->GetIStatObj();
		if (!pStatObj)
			return false;
		return RayIntersection(vWorldRaySrc, vWorldRayDir, NULL, NULL, pStatObj, pObject->GetWorldTM(), outHitInfo, NULL);
	}
	else if (objType == OBJTYPE_BRUSH)
	{
		return RayIntersection(vWorldRaySrc, vWorldRayDir, pObject, NULL, outHitInfo);
	}
	else if (objType == OBJTYPE_PREFAB)
	{
		for (int k = 0, nChildCount(pObject->GetChildCount()); k < nChildCount; ++k)
		{
			CBaseObject* childObject(pObject->GetChild(k));
			if (childObject == NULL)
				continue;
			if (childObject->IsHidden() || IsFrozen(childObject))
				continue;
			if (RayIntersection(vWorldRaySrc, vWorldRayDir, pObject->GetChild(k), NULL, outHitInfo))
				return true;
		}
		return false;
	}
	else if (objType == OBJTYPE_ENTITY)
	{
		CEntityObject* pEntity((CEntityObject*)pObject);
		return RayIntersection(vWorldRaySrc, vWorldRayDir, NULL, pEntity->GetIEntity(), NULL, pObject->GetWorldTM(), outHitInfo, NULL);
	}

	return false;
}

bool CSurfaceInfoPicker::PickByAABB(const CPoint& point, int nFlag, IDisplayViewport* pView, CExcludedObjects* pExcludedObjects, std::vector<CBaseObjectPtr>* pOutObjects)
{
	GetIEditorImpl()->GetObjectManager()->GetObjects(m_objects);

	Vec3 vWorldRaySrc, vWorldRayDir;
	pView->ViewToWorldRay(point, vWorldRaySrc, vWorldRayDir);
	vWorldRaySrc = vWorldRaySrc + vWorldRayDir * 0.1f;
	vWorldRayDir = vWorldRayDir * kEnoughFarDistance;

	bool bPicked = false;

	for (int i = 0, iCount(m_objects.size()); i < iCount; ++i)
	{
		if (pExcludedObjects && pExcludedObjects->Contains(m_objects[i]))
			continue;

		AABB worldObjAABB;
		m_objects[i]->GetBoundBox(worldObjAABB);

		if (pView)
		{
			float fScreenFactor = pView->GetScreenScaleFactor(m_objects[i]->GetPos());
			worldObjAABB.Expand(0.01f * Vec3(fScreenFactor, fScreenFactor, fScreenFactor));
		}

		Vec3 vHitPos;
		if (Intersect::Ray_AABB(vWorldRaySrc, vWorldRayDir, worldObjAABB, vHitPos))
		{
			if ((vHitPos - vWorldRaySrc).GetNormalized().Dot(vWorldRayDir) > 0 || worldObjAABB.IsContainPoint(vHitPos))
			{
				if (pOutObjects)
					pOutObjects->push_back(m_objects[i]);
				bPicked = true;
			}
		}
	}

	return bPicked;
}

bool CSurfaceInfoPicker::PickImpl(const CPoint& point,
                                  IMaterial** ppOutLastMaterial,
                                  SRayHitInfo& outHitInfo,
                                  CExcludedObjects* pExcludedObjects,
                                  int nFlag)
{
	Vec3 vWorldRaySrc;
	Vec3 vWorldRayDir;

	IDisplayViewport* view = GetIEditorImpl()->GetActiveView();
	if (!view)
	{
		return false;
	}

	view->ViewToWorldRay(point, vWorldRaySrc, vWorldRayDir);
	vWorldRaySrc = vWorldRaySrc + vWorldRayDir * 0.1f;
	vWorldRayDir = vWorldRayDir * kEnoughFarDistance;

	return PickImpl(vWorldRaySrc, vWorldRayDir, ppOutLastMaterial, outHitInfo, pExcludedObjects, nFlag);
}

bool CSurfaceInfoPicker::PickImpl(const Vec3& vWorldRaySrc, const Vec3& vWorldRayDir,
                                  IMaterial** ppOutLastMaterial,
                                  SRayHitInfo& outHitInfo,
                                  CExcludedObjects* pExcludedObjects,
                                  int nFlag)
{
	memset(&outHitInfo, 0, sizeof(outHitInfo));
	outHitInfo.fDistance = kEnoughFarDistance;

	if (m_pSetObjects)
	{
		m_pObjects = m_pSetObjects;
	}
	else
	{
		GetIEditorImpl()->GetObjectManager()->GetObjects(m_objects);
		m_pObjects = &m_objects;
	}

	if (pExcludedObjects)
		m_ExcludedObjects = *pExcludedObjects;
	else
		m_ExcludedObjects.Clear();

	m_pPickedObject = NULL;

	if (nFlag & ePOG_Terrain)
		FindNearestInfoFromTerrain(vWorldRaySrc, vWorldRayDir, ppOutLastMaterial, outHitInfo);
	if (nFlag & ePOG_BrushObject)
		FindNearestInfoFromBrushObjects(vWorldRaySrc, vWorldRayDir, ppOutLastMaterial, outHitInfo);
	if (nFlag & ePOG_Entity)
		FindNearestInfoFromEntities(vWorldRaySrc, vWorldRayDir, ppOutLastMaterial, outHitInfo);
	if (nFlag & ePOG_Vegetation)
		FindNearestInfoFromVegetations(vWorldRaySrc, vWorldRayDir, ppOutLastMaterial, outHitInfo);
	if (nFlag & ePOG_Prefabs)
		FindNearestInfoFromPrefabs(vWorldRaySrc, vWorldRayDir, ppOutLastMaterial, outHitInfo);
	if ((nFlag & ePOG_Solid) || (nFlag & ePOG_DesignerObject))
		FindNearestInfoFromSolids(nFlag, vWorldRaySrc, vWorldRayDir, ppOutLastMaterial, outHitInfo);

	if (!m_pSetObjects)
		m_objects.clear();

	m_ExcludedObjects.Clear();

	return outHitInfo.fDistance < kEnoughFarDistance;
}

void CSurfaceInfoPicker::FindNearestInfoFromBrushObjects(
  const Vec3& vWorldRaySrc,
  const Vec3& vWorldRayDir,
  IMaterial** pOutLastMaterial,
  SRayHitInfo& outHitInfo) const
{
	for (size_t i = 0; i < m_pObjects->size(); ++i)
	{
		CBaseObject* object((*m_pObjects)[i]);
		ObjectType objType(object->GetType());
		if (object == NULL)
			continue;
		if (objType != OBJTYPE_BRUSH)
			continue;
		if (object->IsHidden() || IsFrozen(object))
			continue;
		if (m_ExcludedObjects.Contains(object))
			continue;
		if (RayIntersection(vWorldRaySrc, vWorldRayDir, object, pOutLastMaterial, outHitInfo))
		{
			if (pOutLastMaterial)
				AssignObjectMaterial(object, outHitInfo, pOutLastMaterial);
			m_pPickedObject = object;
		}
	}
}

void CSurfaceInfoPicker::FindNearestInfoFromPrefabs(
  const Vec3& vWorldRaySrc,
  const Vec3& vWorldRayDir,
  IMaterial** pOutLastMaterial,
  SRayHitInfo& outHitInfo) const
{
	for (size_t i = 0; i < m_pObjects->size(); ++i)
	{
		CBaseObject* object((*m_pObjects)[i]);
		if (object == NULL)
			continue;
		ObjectType objType(object->GetType());
		if (objType != OBJTYPE_PREFAB)
			continue;
		if (object->IsHidden() || IsFrozen(object))
			continue;
		if (m_ExcludedObjects.Contains(object))
			continue;
		for (int k = 0, nChildCount(object->GetChildCount()); k < nChildCount; ++k)
		{
			CBaseObject* childObject(object->GetChild(k));
			if (childObject == NULL)
				continue;
			if (childObject->IsHidden() || IsFrozen(childObject))
				continue;
			if (RayIntersection(vWorldRaySrc, vWorldRayDir, object->GetChild(k), pOutLastMaterial, outHitInfo))
			{
				if (pOutLastMaterial)
					AssignObjectMaterial(childObject, outHitInfo, pOutLastMaterial);
				m_pPickedObject = childObject;
			}
		}
	}
}

void CSurfaceInfoPicker::FindNearestInfoFromSolids(int nFlags,
                                                   const Vec3& vWorldRaySrc,
                                                   const Vec3& vWorldRayDir,
                                                   IMaterial** pOutLastMaterial,
                                                   SRayHitInfo& outHitInfo) const
{
	for (size_t i = 0; i < m_pObjects->size(); ++i)
	{
		CBaseObject* object((*m_pObjects)[i]);
		if (object == NULL)
			continue;
		if (object->GetType() != OBJTYPE_SOLID)
			continue;
		if (!(nFlags & ePOG_DesignerObject) && object->GetType() == OBJTYPE_SOLID)
			continue;
		if (object->IsHidden() || IsFrozen(object))
			continue;
		if (m_ExcludedObjects.Contains(object))
			continue;
		_smart_ptr<IStatObj> pStatObj = object->GetIStatObj();
		if (!pStatObj)
			continue;

		Matrix34A worldTM(object->GetWorldTM());
		if (object->GetMaterial() && pOutLastMaterial)
		{
			if (!IsMaterialValid((CMaterial*)object->GetMaterial()))
				continue;
		}

		if (RayIntersection(vWorldRaySrc, vWorldRayDir, NULL, NULL, pStatObj, worldTM, outHitInfo, pOutLastMaterial))
		{
			if (pOutLastMaterial)
				AssignObjectMaterial(object, outHitInfo, pOutLastMaterial);
			m_pPickedObject = object;
		}
	}
}

void CSurfaceInfoPicker::FindNearestInfoFromDecals(
  const Vec3& vWorldRaySrc,
  const Vec3& vWorldRayDir,
  IMaterial** pOutLastMaterial,
  SRayHitInfo& outHitInfo) const
{
	for (size_t i = 0; i < m_pObjects->size(); ++i)
	{
		CBaseObject* object((*m_pObjects)[i]);

		if (object->GetType() != OBJTYPE_DECAL)
			continue;

		if (object->IsHidden() || IsFrozen(object))
			continue;

		if (m_ExcludedObjects.Contains(object))
			continue;

		IRenderNode* pRenderNode(object->GetEngineNode());
		AABB localBoundBox;
		pRenderNode->GetLocalBounds(localBoundBox);

		Matrix34 worldTM(object->GetWorldTM());
		Matrix34 invWorldTM = worldTM.GetInverted();

		Vec3 localRaySrc = invWorldTM.TransformPoint(vWorldRaySrc);
		Vec3 localRayDir = invWorldTM.TransformVector(vWorldRayDir);

		SRayHitInfo hitInfo;
		Vec3 localRayOut;
		if (Intersect::Ray_AABB(localRaySrc, localRayDir, localBoundBox, localRayOut))
		{
			float distance(localRaySrc.GetDistance(localRayOut));
			if (distance < outHitInfo.fDistance)
			{
				if (pOutLastMaterial)
					*pOutLastMaterial = object->GetMaterial()->GetMatInfo();
				outHitInfo.fDistance = distance;
				outHitInfo.vHitPos = worldTM.TransformPoint(localRayOut);
				outHitInfo.vHitNormal = worldTM.GetColumn2().GetNormalized();
				outHitInfo.nHitMatID = -1;
				m_pPickedObject = NULL;
			}
		}
	}
}

void CSurfaceInfoPicker::FindNearestInfoFromEntities(
  const Vec3& vWorldRaySrc,
  const Vec3& vWorldRayDir,
  IMaterial** pOutLastMaterial,
  SRayHitInfo& outHitInfo) const
{
	for (size_t i = 0; i < m_pObjects->size(); ++i)
	{
		CBaseObject* object((*m_pObjects)[i]);
		if (object == NULL)
			continue;
		if (!object->IsKindOf(RUNTIME_CLASS(CEntityObject)))
			continue;
		if (object->IsHidden() || IsFrozen(object))
			continue;
		if (m_ExcludedObjects.Contains(object))
			continue;
		CEntityObject* entity = static_cast<CEntityObject*>(object);

		if (RayIntersection(vWorldRaySrc, vWorldRayDir, NULL, entity->GetIEntity(), NULL, object->GetWorldTM(), outHitInfo, pOutLastMaterial))
		{
			AssignObjectMaterial(object, outHitInfo, pOutLastMaterial);
			m_pPickedObject = object;
		}
	}
}

void CSurfaceInfoPicker::FindNearestInfoFromVegetations(
  const Vec3& vWorldRaySrc,
  const Vec3& vWorldRayDir,
  IMaterial** ppOutLastMaterial,
  SRayHitInfo& outHitInfo) const
{
	CVegetationMap* pVegetationMap = GetIEditorImpl()->GetVegetationMap();
	if (!pVegetationMap)
		return;
	std::vector<CVegetationInstance*> vegetations;
	pVegetationMap->GetAllInstances(vegetations);

	for (size_t i = 0; i < vegetations.size(); ++i)
	{
		CVegetationInstance* vegetation(vegetations[i]);
		if (vegetation == NULL)
			continue;
		if (vegetation->object == NULL)
			continue;
		if (vegetation->object->IsHidden())
			continue;
		Matrix34A worldTM;

		float fAngle = DEG2RAD(360.0f / 255.0f * vegetation->angle);
		worldTM.SetIdentity();
		worldTM.SetRotationZ(fAngle);
		worldTM.SetTranslation(vegetation->pos);

		IStatObj* pStatObj = vegetation->object->GetObject();
		if (pStatObj == NULL)
			continue;

		if (RayIntersection(vWorldRaySrc, vWorldRayDir, NULL, NULL, pStatObj, worldTM, outHitInfo, ppOutLastMaterial))
		{
			CMaterial* pUserDefMaterial = vegetation->object->GetMaterial();
			if (pUserDefMaterial && ppOutLastMaterial)
			{
				*ppOutLastMaterial = pUserDefMaterial->GetMatInfo();
			}

			m_pPickedObject = NULL;
		}
	}
}

void CSurfaceInfoPicker::FindNearestInfoFromTerrain(
  const Vec3& vWorldRaySrc,
  const Vec3& vWorldRayDir,
  IMaterial** pOutLastMaterial,
  SRayHitInfo& outHitInfo) const
{
	int objTypes = ent_terrain;
	int flags = (geom_colltype_ray | geom_colltype14) << rwi_colltype_bit | rwi_colltype_any | rwi_ignore_terrain_holes | rwi_stop_at_pierceable;
	ray_hit hit;
	hit.pCollider = 0;
	int col = gEnv->pPhysicalWorld->RayWorldIntersection(vWorldRaySrc, vWorldRayDir, objTypes, flags, &hit, 1);
	if (col > 0 && hit.dist < outHitInfo.fDistance && hit.bTerrain)
	{
		outHitInfo.nHitSurfaceID = hit.surface_idx;
		outHitInfo.vHitNormal = hit.n;
		outHitInfo.vHitPos = hit.pt;
		int nTerrainSurfaceType = hit.idmatOrg;
		if (nTerrainSurfaceType >= 0 && nTerrainSurfaceType < GetIEditorImpl()->GetTerrainManager()->GetSurfaceTypeCount())
		{
			CSurfaceType* pSurfaceType = GetIEditorImpl()->GetTerrainManager()->GetSurfaceTypePtr(nTerrainSurfaceType);
			string mtlName = pSurfaceType ? pSurfaceType->GetMaterial() : "";
			if (!mtlName.IsEmpty())
			{
				if (pOutLastMaterial)
					*pOutLastMaterial = GetIEditorImpl()->Get3DEngine()->GetMaterialManager()->FindMaterial(mtlName);
				outHitInfo.fDistance = hit.dist;
				outHitInfo.nHitMatID = -1;
			}
		}
	}
}

bool CSurfaceInfoPicker::RayIntersection(
  const Vec3& vWorldRaySrc,
  const Vec3& vWorldRayDir,
  CBaseObject* pBaseObject,
  IMaterial** pOutLastMaterial,
  SRayHitInfo& outHitInfo)
{
	if (pBaseObject == NULL)
		return false;
	IRenderNode* pRenderNode(pBaseObject->GetEngineNode());
	if (pRenderNode == NULL)
		return false;
	IStatObj* pStatObj(pRenderNode->GetEntityStatObj());
	if (pStatObj == NULL)
		return false;
	return RayIntersection(vWorldRaySrc, vWorldRayDir, pRenderNode, NULL, pStatObj, pBaseObject->GetWorldTM(), outHitInfo, pOutLastMaterial);
}

bool CSurfaceInfoPicker::RayIntersection(
  const Vec3& vWorldRaySrc,
  const Vec3& vWorldRayDir,
  IRenderNode* pRenderNode,
  IEntity* pEntity,
  IStatObj* pStatObj,
  const Matrix34A& WorldTM,
  SRayHitInfo& outHitInfo,
  IMaterial** ppOutLastMaterial)
{
	SRayHitInfo hitInfo;
	bool bRayIntersection = false;
	IMaterial* pMaterial(NULL);

	bRayIntersection = RayIntersection(vWorldRaySrc, vWorldRayDir, pEntity, &pMaterial, WorldTM, hitInfo);
	if (!bRayIntersection)
	{
		bRayIntersection = RayIntersection(vWorldRaySrc, vWorldRayDir, pStatObj, &pMaterial, WorldTM, hitInfo);
		if (!bRayIntersection)
			bRayIntersection = RayIntersection(vWorldRaySrc, vWorldRayDir, pRenderNode, &pMaterial, WorldTM, hitInfo);
	}

	if (bRayIntersection)
	{
		hitInfo.fDistance = vWorldRaySrc.GetDistance(hitInfo.vHitPos);
		if (hitInfo.fDistance < outHitInfo.fDistance)
		{
			if (ppOutLastMaterial)
				*ppOutLastMaterial = pMaterial;
			memcpy(&outHitInfo, &hitInfo, sizeof(SRayHitInfo));
			outHitInfo.vHitNormal.Normalize();
			return true;
		}
	}
	return false;
}

bool CSurfaceInfoPicker::RayIntersection(
  const Vec3& vWorldRaySrc,
  const Vec3& vWorldRayDir,
  IStatObj* pStatObj,
  IMaterial** ppOutLastMaterial,
  const Matrix34A& worldTM,
  SRayHitInfo& outHitInfo)
{
	if (pStatObj == NULL)
		return false;

	Vec3 localRaySrc;
	Vec3 localRayDir;
	if (!RayWorldToLocal(worldTM, vWorldRaySrc, vWorldRayDir, localRaySrc, localRayDir))
		return false;

	memset(&outHitInfo, 0, sizeof(outHitInfo));
	outHitInfo.inReferencePoint = localRaySrc;
	outHitInfo.inRay = Ray(localRaySrc, localRayDir);
	outHitInfo.bInFirstHit = false;
	outHitInfo.bUseCache = false;

	Vec3 hitPosOnAABB;
	if (Intersect::Ray_AABB(outHitInfo.inRay, pStatObj->GetAABB(), hitPosOnAABB) == 0x00)
		return false;

	if (pStatObj->RayIntersection(outHitInfo))
	{
		if (outHitInfo.fDistance < 0)
			return false;
		outHitInfo.vHitPos = worldTM.TransformPoint(outHitInfo.vHitPos);
		outHitInfo.vHitNormal = worldTM.GetTransposed().GetInverted().TransformVector(outHitInfo.vHitNormal);
		if (ppOutLastMaterial)
		{
			IMaterial* pMaterial = pStatObj->GetMaterial();
			if (pMaterial)
			{
				*ppOutLastMaterial = pMaterial;
				if (outHitInfo.nHitMatID >= 0)
				{
					if (pMaterial->GetSubMtlCount() > 0 && outHitInfo.nHitMatID < pMaterial->GetSubMtlCount())
						*ppOutLastMaterial = pMaterial->GetSubMtl(outHitInfo.nHitMatID);
				}
			}
		}
		return true;
	}

	if (*ppOutLastMaterial)
		outHitInfo.nHitSurfaceID = (*ppOutLastMaterial)->GetSurfaceTypeId();

	return outHitInfo.fDistance < kEnoughFarDistance;
}

#if defined(USE_GEOM_CACHES)
bool CSurfaceInfoPicker::RayIntersection(
  const Vec3& vWorldRaySrc,
  const Vec3& vWorldRayDir,
  IGeomCacheRenderNode* pGeomCacheRenderNode,
  IMaterial** ppOutLastMaterial,
  const Matrix34A& worldTM,
  SRayHitInfo& outHitInfo)
{
	if (!pGeomCacheRenderNode)
		return false;

	outHitInfo.inReferencePoint = vWorldRaySrc;
	outHitInfo.inRay = Ray(vWorldRaySrc, vWorldRayDir);
	outHitInfo.bInFirstHit = false;
	outHitInfo.bUseCache = false;

	if (pGeomCacheRenderNode->RayIntersection(outHitInfo))
	{
		if (outHitInfo.fDistance < 0)
			return false;

		if (ppOutLastMaterial)
		{
			IMaterial* pMaterial = pGeomCacheRenderNode->GetMaterial();
			if (pMaterial)
			{
				*ppOutLastMaterial = pMaterial;
				if (outHitInfo.nHitMatID >= 0)
				{
					if (pMaterial->GetSubMtlCount() > 0 && outHitInfo.nHitMatID < pMaterial->GetSubMtlCount())
						*ppOutLastMaterial = pMaterial->GetSubMtl(outHitInfo.nHitMatID);
				}
			}
		}
		return true;
	}

	return outHitInfo.fDistance < kEnoughFarDistance;
}
#endif

bool CSurfaceInfoPicker::RayIntersection(
  const Vec3& vWorldRaySrc,
  const Vec3& vWorldRayDir,
  IRenderNode* pRenderNode,
  IMaterial** pOutLastMaterial,
  const Matrix34A& WorldTM,
  SRayHitInfo& outHitInfo)
{
	if (pRenderNode == NULL)
		return false;

	IPhysicalEntity* physics = 0;
	physics = pRenderNode->GetPhysics();
	if (physics == NULL)
		return false;

	if (physics->GetStatus(&pe_status_nparts()) == 0)
		return false;

	ray_hit hit;
	int col = GetIEditorImpl()->GetSystem()->GetIPhysicalWorld()->RayTraceEntity(physics, vWorldRaySrc, vWorldRayDir, &hit, 0, geom_collides);
	if (col <= 0)
		return false;

	outHitInfo.vHitPos = hit.pt;
	outHitInfo.vHitNormal = hit.n;
	outHitInfo.fDistance = hit.dist;
	if (outHitInfo.fDistance < 0)
		return false;
	outHitInfo.nHitMatID = -1;
	outHitInfo.nHitSurfaceID = hit.surface_idx;
	IMaterial* pMaterial(pRenderNode->GetMaterial());

	if (pMaterial && pOutLastMaterial)
	{
		*pOutLastMaterial = pMaterial;
		if (pMaterial->GetSubMtlCount() > 0 && hit.idmatOrg >= 0 && hit.idmatOrg < pMaterial->GetSubMtlCount())
		{
			outHitInfo.nHitMatID = hit.idmatOrg;
			*pOutLastMaterial = pMaterial->GetSubMtl(hit.idmatOrg);
		}
	}

	return true;
}

bool CSurfaceInfoPicker::RayIntersection(
  const Vec3& vWorldRaySrc,
  const Vec3& vWorldRayDir,
  IEntity* pEntity,
  IMaterial** pOutLastMaterial,
  const Matrix34A& WorldTM,
  SRayHitInfo& outHitInfo)
{
	if (pEntity == NULL)
		return false;

	IPhysicalWorld* pPhysWorld = GetIEditorImpl()->GetSystem()->GetIPhysicalWorld();
	IPhysicalEntity* pPhysics = 0;
	bool bHit = false;

	IMaterial* pMaterial = NULL;
	int nSlotCount = pEntity->GetSlotCount();
	int nCurSlot = 0;

	for (int i = 0; i < 2; )
	{
		if (i == 0)
		{
			if (nCurSlot >= nSlotCount)
			{
				++i;
				continue;
			}

			SEntitySlotInfo slotInfo;
			if (pEntity->GetSlotInfo(nCurSlot++, slotInfo) == false)
				continue;

			if (slotInfo.pCharacter)
			{
				pPhysics = slotInfo.pCharacter->GetISkeletonPose()->GetCharacterPhysics();
				pMaterial = slotInfo.pCharacter->GetIMaterial();
			}
			else if (slotInfo.pStatObj)
			{
				if (RayIntersection(vWorldRaySrc, vWorldRayDir, slotInfo.pStatObj, pOutLastMaterial, WorldTM * (*slotInfo.pLocalTM), outHitInfo))
					return true;
			}
#if defined(USE_GEOM_CACHES)
			else if (slotInfo.pGeomCacheRenderNode)
			{
				if (RayIntersection(vWorldRaySrc, vWorldRayDir, slotInfo.pGeomCacheRenderNode, pOutLastMaterial, WorldTM * (*slotInfo.pLocalTM), outHitInfo))
					return true;
			}
#endif
			else
			{
				continue;
			}
		}
		else
		{
			pPhysics = pEntity->GetPhysics();
			++i;
		}
		if (pPhysics == NULL)
			continue;
		int type = pPhysics->GetType();
		if (type == PE_NONE || type == PE_PARTICLE || type == PE_ROPE || type == PE_SOFT)
			continue;
		else if (pPhysics->GetStatus(&pe_status_nparts()) == 0)
			continue;
		ray_hit hit;
		bHit = (pPhysWorld->RayTraceEntity(pPhysics, vWorldRaySrc, vWorldRayDir, &hit) > 0);
		if (!bHit)
			continue;
		outHitInfo.vHitPos = hit.pt;
		outHitInfo.vHitNormal = hit.n;
		outHitInfo.fDistance = hit.dist;
		if (outHitInfo.fDistance < 0)
		{
			bHit = false;
			continue;
		}
		outHitInfo.nHitSurfaceID = hit.surface_idx;
		outHitInfo.nHitMatID = hit.idmatOrg;
		break;
	}

	if (!bHit)
		return false;

	if (pOutLastMaterial)
	{
		*pOutLastMaterial = pMaterial;
		if (pMaterial)
		{
			if (outHitInfo.nHitMatID >= 0 && pMaterial->GetSubMtlCount() > 0 && outHitInfo.nHitMatID < pMaterial->GetSubMtlCount())
				*pOutLastMaterial = pMaterial->GetSubMtl(outHitInfo.nHitMatID);
		}
	}

	return true;
}

bool CSurfaceInfoPicker::RayWorldToLocal(
  const Matrix34A& WorldTM,
  const Vec3& vWorldRaySrc,
  const Vec3& vWorldRayDir,
  Vec3& outRaySrc,
  Vec3& outRayDir)
{
	if (!WorldTM.IsValid())
		return false;
	Matrix34A invertedM(WorldTM.GetInverted());
	if (!invertedM.IsValid())
		return false;
	outRaySrc = invertedM.TransformPoint(vWorldRaySrc);
	outRayDir = invertedM.TransformVector(vWorldRayDir).GetNormalized();
	return true;
}

bool CSurfaceInfoPicker::IsMaterialValid(CMaterial* pMaterial)
{
	if (pMaterial == NULL)
		return false;
	return !(pMaterial->GetMatInfo()->GetFlags() & MTL_FLAG_NODRAW);
}

void CSurfaceInfoPicker::AssignObjectMaterial(CBaseObject* pObject, const SRayHitInfo& outHitInfo, IMaterial** pOutMaterial)
{
	if (pObject->GetMaterial())
	{
		if (pObject->GetMaterial()->GetMatInfo())
		{
			if (pOutMaterial)
			{
				*pOutMaterial = pObject->GetMaterial()->GetMatInfo();
				if (*pOutMaterial)
				{
					if (outHitInfo.nHitMatID >= 0 && (*pOutMaterial)->GetSubMtlCount() > 0 && outHitInfo.nHitMatID < (*pOutMaterial)->GetSubMtlCount())
						*pOutMaterial = (*pOutMaterial)->GetSubMtl(outHitInfo.nHitMatID);
				}
			}
		}
	}
}
