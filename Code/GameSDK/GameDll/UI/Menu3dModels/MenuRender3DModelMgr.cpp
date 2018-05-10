// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Rendering of 3d models in menus.

// Includes
#include "StdAfx.h"
#include "MenuRender3DModelMgr.h"
#include "FrontEndModelCache.h"
#include "Graphics/2DRenderUtils.h"
#include "Utility/CryWatch.h"
#include <CryString/StringUtils.h>
#include "GameCVars.h"
#include "UI/UIManager.h"

// Defines
#if MP_FE_CACHE_LOG_FILE_ACCESSES
#define CACHE_LOG_FILE_ACCESSES_START		CFrontEndModelCache::SetReasonForLoggingFileOpens("CMenuRender3DModelMgr::AddModel");
#define CACHE_LOG_FILE_ACCESSES_END			CFrontEndModelCache::SetReasonForLoggingFileOpens(NULL);
#else
#define CACHE_LOG_FILE_ACCESSES_START
#define CACHE_LOG_FILE_ACCESSES_END
#endif

#define	MENU_LIGHT_COLOR_SCALE					1.0f
#define MENU_LIGHT_AMBIENT_ALPHA_SCALE	1.0f

// Statics
CMenuRender3DModelMgr*	CMenuRender3DModelMgr::s_pInstance = NULL;
CMenuRender3DModelMgr*	CMenuRender3DModelMgr::s_pFirstNonFreedModelMgr = NULL;
uint8										CMenuRender3DModelMgr::s_instanceCount = 0;

#if DEBUG_MENU_RENDER_3D_MODEL_MGR
	float									CMenuRender3DModelMgr::s_menuLightColorScale = MENU_LIGHT_COLOR_SCALE;
#endif

//#define MR3MM_TRACE(...) CryLogAlways(__VA_ARGS__)
#define MR3MM_TRACE(...)
//#pragma optimize("",off)

//--------------------------------------------------------------------------------------------------
// Name: CMenuRender3DModelMgr
// Desc: Constructor
//--------------------------------------------------------------------------------------------------
CMenuRender3DModelMgr::CMenuRender3DModelMgr()
{
	CRY_ASSERT_MESSAGE( IsMenu3dModelEngineSupportActive(), 
											"No 3d model engine support - Must have a FrontEndModelCache in the front end, check FrontEndModelCache.xml");

	CMenuRender3DModelMgr::Release();

	s_instanceCount++;
	if(s_instanceCount == 1)
	{
		gEnv->p3DEngine->SetPostEffectParam("Post3DRenderer_Active",1.0f,true);
	}

	CRY_ASSERT_MESSAGE(s_pInstance==NULL,"Some how there's still an instance of CMenuRender3DModelMgr");
	s_pInstance = this;

	m_groupCount = 0;
	m_bReleaseMe = false;
	m_framesUntilDestroy = 3;
	m_pNextNonFreedModelMgr = NULL;
	m_pPrevNonFreedModelMgr = NULL;

	m_bUserRotation = false;

	gEnv->pGameFramework->RegisterListener(this, "MenuRender3DModelMgr", FRAMEWORKLISTENERPRIORITY_MENU);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ~CMenuRender3DModelMgr
// Desc: Destructor
//--------------------------------------------------------------------------------------------------
CMenuRender3DModelMgr::~CMenuRender3DModelMgr()
{
	CRY_ASSERT_MESSAGE(s_instanceCount,"No instances left to delete in destructor");
	CRY_ASSERT_MESSAGE(m_bReleaseMe,"Deleting an instance which isn't marked for release");

	s_instanceCount--;
	if(s_instanceCount == 0)
	{
		CRY_ASSERT(m_pPrevNonFreedModelMgr == NULL);
		CRY_ASSERT(m_pNextNonFreedModelMgr == NULL);
		gEnv->p3DEngine->SetPostEffectParam("Post3DRenderer_Active",0.0f,true);
	}

	const uint renderEntityCount = m_renderEntityData.size();
	FE_LOG("[MenuRender3DModelMgr] Destroying instance %p (%d entities) THREAD=%u", this, renderEntityCount, CryGetCurrentThreadId());

	// Fix up linked list of instances
	if(m_pNextNonFreedModelMgr)
	{
		CRY_ASSERT(m_pNextNonFreedModelMgr->m_pPrevNonFreedModelMgr == this);
		m_pNextNonFreedModelMgr->m_pPrevNonFreedModelMgr = m_pPrevNonFreedModelMgr;
	}

	if (m_pPrevNonFreedModelMgr == NULL)
	{
		CRY_ASSERT(s_pFirstNonFreedModelMgr == this);
		s_pFirstNonFreedModelMgr = m_pNextNonFreedModelMgr;
	}
	else
	{
		CRY_ASSERT(s_pFirstNonFreedModelMgr != this);
		CRY_ASSERT(m_pPrevNonFreedModelMgr->m_pNextNonFreedModelMgr == this);
		m_pPrevNonFreedModelMgr->m_pNextNonFreedModelMgr = m_pNextNonFreedModelMgr;
	}

	// Remove entities
	const bool bForceRemoveNow = true;
	for(uint8 i=0; i<renderEntityCount; i++)
	{
		gEnv->pEntitySystem->RemoveEntity(m_renderEntityData[i].entityId, bForceRemoveNow);
	}

	// Release lights
	ReleaseLights();

	// Free containers
	stl::free_container(m_sceneSettings.lights);

	// Unregister listeners
	gEnv->pGameFramework->UnregisterListener(this);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: Release
// Desc: Releases CMenuRender3DModelMgr
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::Release(bool bImmediateDelete)
{
	FE_LOG("[MenuRender3DModelMgr] Release s_pInstance:%p immediateDelete:%d", s_pInstance, bImmediateDelete);

	if(s_pInstance)
	{
		s_pInstance->m_bUserRotation = false;

		CRY_ASSERT(!s_pInstance->m_bReleaseMe);
		s_pInstance->m_bReleaseMe = true;

		// Release lights
		s_pInstance->ReleaseLights();

		CRY_ASSERT(s_pInstance->m_pNextNonFreedModelMgr == NULL);
		CRY_ASSERT(s_pInstance->m_pPrevNonFreedModelMgr == NULL);

		s_pInstance->SetVisibilityOfAllEntities(false);

		// Linked list ptr management
		if(s_pFirstNonFreedModelMgr)
		{
			CRY_ASSERT(s_pFirstNonFreedModelMgr->m_pPrevNonFreedModelMgr == NULL);
			s_pFirstNonFreedModelMgr->m_pPrevNonFreedModelMgr = s_pInstance;
		}
		s_pInstance->m_pNextNonFreedModelMgr = s_pFirstNonFreedModelMgr;
		s_pFirstNonFreedModelMgr = s_pInstance;
		s_pInstance = NULL;
	}

	if(bImmediateDelete)
	{
		PREFAST_SUPPRESS_WARNING(6001);
		while(s_pFirstNonFreedModelMgr)
		{
			CRY_ASSERT(s_pFirstNonFreedModelMgr->m_pPrevNonFreedModelMgr == NULL);
			delete s_pFirstNonFreedModelMgr;
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ReleaseLights
// Desc: Releases lights
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::ReleaseLights()
{
	const int lightCount = m_sceneSettings.lights.size();
	for(int i=0; i<lightCount; i++)
	{
		SLightData& lightData = m_sceneSettings.lights[i];

		if(lightData.pLightSource)
		{
			gEnv->p3DEngine->UnRegisterEntityDirect(lightData.pLightSource);
			gEnv->p3DEngine->DeleteLightSource(lightData.pLightSource);
			lightData.pLightSource = NULL;
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: AddModel
// Desc: Adds a model to the manager
//--------------------------------------------------------------------------------------------------
CMenuRender3DModelMgr::TAddedModelIndex CMenuRender3DModelMgr::AddModel(const SModelParams& modelParams)
{
	TAddedModelIndex modelIndex = kAddedModelIndex_Invalid;

	if(IsMenu3dModelEngineSupportActive())
	{
		CACHE_LOG_FILE_ACCESSES_START;

		const uint renderEntityCount = m_renderEntityData.size();
		CRY_ASSERT(modelParams.pFilename);
		CRY_ASSERT_MESSAGE((renderEntityCount < kAddedModelIndex_MaxEntities),"CMenuRender3DModelMgr has no free slots left for new models, failed to add one");

		if((CFrontEndModelCache::Exists()==false) && (g_pGame->GetGameRules()==NULL))
		{
			GameWarning("Can't add '%s' over menu as game currently has no model cache and no game rules", modelParams.pFilename);
		}
		else if(!CFrontEndModelCache::IsAllowed3dFrontEndAssets())
		{
			GameWarning ("Not allowed to load 3d front end assets [g_pGame->IsAllowed3dFrontEndAssets==false], aborting");
		}
		else if(renderEntityCount < kAddedModelIndex_MaxEntities && g_pGameCVars->menu3D_enabled)
		{
			// Add model
			const bool bIsCharacter = IsCharacterFile(modelParams.pFilename);
			FE_LOG("[MenuRender3DModelMgr] Adding %s model='%s' material='%s' THREAD=%u", 
								bIsCharacter ? "character" : "prop", 
								modelParams.pFilename,
								modelParams.pMaterialOverrideFilename, 
								CryGetCurrentThreadId());

			Quat rotQuat;
			rotQuat.SetRotationXYZ(modelParams.rot);

			char uniqueEntityName[64];
			cry_sprintf(uniqueEntityName,sizeof(uniqueEntityName),"MenuObj[%s %d]",modelParams.pName,renderEntityCount);

			// Spawn entity
			SEntitySpawnParams entitySpawnParams;
			entitySpawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
			entitySpawnParams.sName = uniqueEntityName;
			entitySpawnParams.nFlags = ENTITY_FLAG_NO_PROXIMITY | ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_NO_SAVE | ENTITY_FLAG_UPDATE_HIDDEN;
			entitySpawnParams.vPosition = ZERO;
			entitySpawnParams.qRotation = rotQuat;
			IEntity* pNewEntity = gEnv->pEntitySystem->SpawnEntity(entitySpawnParams);
			CRY_ASSERT_MESSAGE(pNewEntity,"Failed to create entity");

			if(pNewEntity)
			{
				// Attempt to add model to entity
				bool bHasModel = false;
				if(modelParams.pFilename && modelParams.pFilename[0])
				{
					bHasModel = true;
					const int slotIndex = 0;
					if(bIsCharacter)
					{
						pNewEntity->LoadCharacter(slotIndex,modelParams.pFilename,IEntity::EF_NO_STREAMING);
						SetCharacterFlags(pNewEntity);
					}
					else
					{
						pNewEntity->LoadGeometry(slotIndex,modelParams.pFilename,NULL,IEntity::EF_NO_STREAMING);
					}
				}

				ApplyMaterialOverride(pNewEntity,modelParams.pMaterialOverrideFilename);

				// Fill out SRenderSingleEntityData
				SRenderSingleEntityData renderEntityData;
				renderEntityData.entityId = pNewEntity->GetId();
				renderEntityData.flags |= (bIsCharacter) ? eRSE_IsCharacter : 0;
				renderEntityData.pos = ZERO;
				renderEntityData.rot = modelParams.rot;
				renderEntityData.secondRot = modelParams.secondRot;
				renderEntityData.continuousRot = modelParams.continuousRot;
				renderEntityData.userRotScale = modelParams.userRotScale;
				renderEntityData.topParentIndex = renderEntityCount;
				renderEntityData.parentIndex = kAddedModelIndex_Invalid;
				renderEntityData.name = modelParams.pName;
				renderEntityData.flags |= (bHasModel) ? eRSE_HasModel : 0;
				renderEntityData.groupId = m_groupCount;
				renderEntityData.posOffset = modelParams.posOffset;
				renderEntityData.posSecondOffset = modelParams.posSecondOffset;
				renderEntityData.scale = max(modelParams.scale,0.0f);
				renderEntityData.secondScale = max(modelParams.secondScale,0.0f);
				renderEntityData.silhouetteColor.x = clamp_tpl<float>(modelParams.silhouetteColor.x,0.0f,1.0f);
				renderEntityData.silhouetteColor.y = clamp_tpl<float>(modelParams.silhouetteColor.y,0.0f,1.0f);
				renderEntityData.silhouetteColor.z = clamp_tpl<float>(modelParams.silhouetteColor.z,0.0f,1.0f);
				memcpy(renderEntityData.screenRect,modelParams.screenRect,sizeof(float)*4);

				PreCacheMaterial(renderEntityData, pNewEntity,bIsCharacter);

				CalcWorldPosFromScreenRect(renderEntityData);

				int nIdx = m_renderEntityData.size();
				m_renderEntityData.push_back(renderEntityData);

				// Start of hidden, then show when has fully streamed in
				HideModel(true,nIdx);

				SetAsPost3dRenderObject(pNewEntity,renderEntityData.groupId,renderEntityData.screenRect);
				m_groupCount++;

				modelIndex = renderEntityCount;
			}

			m_bUserRotation |= (modelParams.userRotScale > 0.f);
		}

		CACHE_LOG_FILE_ACCESSES_END;
	}

	return modelIndex;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: HideModel
// Desc: Hides model
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::HideModel(bool bHide,int nEntityIdx,bool bUpdateModelPos)
{
	SRenderSingleEntityData& renderEntityData = m_renderEntityData[nEntityIdx];

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(renderEntityData.entityId);
	if(pEntity)
	{
		// Set hidden status
		if(pEntity->IsHidden() != bHide)
		{
			pEntity->Hide(bHide);
		}

		// Reset alpha
		if(bHide)
		{
			pEntity->SetOpacity(0.0f);
			renderEntityData.alpha=0.0f;
		}
		else if((renderEntityData.flags & eRSE_HasModel) && bUpdateModelPos)
		{
			CalcWorldPosFromScreenRect(renderEntityData);
		}
	}

	// Recurse through children
	for (int i = 0, c = m_renderEntityData.size(); i != c; ++ i)
	{
		SRenderSingleEntityData& child = m_renderEntityData[i];
		if (child.parentIndex == nEntityIdx)
		{
			HideModel(bHide, i, false);
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ForceAnimationUpdate
// Desc: Forces the animation to update so it is correct for the 1st frame
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::ForceAnimationUpdate(IEntity* pEntity)
{
	// Force update on animation, so it will be correct on the 1st frame
	if(pEntity)
	{
		ICharacterInstance* pCharInst = pEntity->GetCharacter(0);
		if(pCharInst)
		{
			const QuatTS pose = (QuatTS)pEntity->GetWorldTM();
			const float fZoomAdjustedDistanceFromCamera = (GetISystem()->GetViewCamera().GetPosition()-pose.t).GetLength();
			SAnimationProcessParams animProcessParams;
			animProcessParams.locationAnimation = pose;
			animProcessParams.bOnRender = false;
			animProcessParams.zoomAdjustedDistanceFromCamera = fZoomAdjustedDistanceFromCamera;
			pCharInst->StartAnimationProcessing(animProcessParams);
			pCharInst->FinishAnimationComputations();
			IEntityRender* pIEntityRender = pEntity->GetRenderInterface();
			if(pIEntityRender)
			{
				pIEntityRender->InvalidateLocalBounds();
			}
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: IsCharacterFile
// Desc: Returns true if pFilename is a character file
//--------------------------------------------------------------------------------------------------
bool CMenuRender3DModelMgr::IsCharacterFile(const char* pFilename)
{
	bool bIsCharacter = false;
	if(pFilename)
	{
		const char* pFilenameExtension = PathUtil::GetExt(pFilename);
		bIsCharacter = (strcmp(pFilenameExtension,"cdf")==0);
	}
	return bIsCharacter;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SwapIfMinBiggerThanMax
// Desc: Swaps values if min is bigger than max
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::SwapIfMinBiggerThanMax(float& minValue,float& maxValue)
{
	const float minValueCopy = minValue;
	const float maxValueCopy = maxValue;
	minValue = min(minValueCopy,maxValueCopy);
	maxValue = max(minValueCopy,maxValueCopy);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: GetPostRenderCamera
// Desc: Gets post render camera
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::GetPostRenderCamera(CCamera& postRenderCamera)
{
	CRY_ASSERT_MESSAGE(m_sceneSettings.fovScale!=0.0f,"Scene Settings FOV hasn't been set yet - must SetSceneSettings before adding models");

	Matrix34 newCameraMatrix;
	newCameraMatrix.SetIdentity();
	postRenderCamera = gEnv->pSystem->GetViewCamera();
	const float newFov = DEFAULT_FOV * clamp_tpl<float>( m_sceneSettings.fovScale, 0.05f, 1.0f);
	const int viewSurfaceX = postRenderCamera.GetViewSurfaceX();
	const int viewSurfaceZ = postRenderCamera.GetViewSurfaceZ();
	const float pixelAspectRatio = postRenderCamera.GetPixelAspectRatio();
	postRenderCamera.SetMatrix(newCameraMatrix);
	postRenderCamera.SetFrustum(viewSurfaceX,viewSurfaceZ,newFov,DEFAULT_NEAR,DEFAULT_FAR,pixelAspectRatio);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: CalcWorldPosFromScreenRect
// Desc: Calculates model world position so that it will render within the screen rect provided
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::CalcWorldPosFromScreenRect(SRenderSingleEntityData& renderEntityData)
{
	if(!(renderEntityData.flags & eRSE_Attached)) // Only position non-attached entities
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(renderEntityData.entityId);
		if(pEntity)
		{
			float scaleToUse = (renderEntityData.bUseSecondOffset)? renderEntityData.secondScale : renderEntityData.scale;
			// Use local AABB and then rotate to same as model, this will then be in "post render camera space",
			// rather than are default camera space (which would then be wrong if in-game)
			AABB localAabb;
			pEntity->GetLocalBounds(localAabb);

			// Rotate local aabb so that it has the same rot as the model
			Quat rotQuatInv;
			rotQuatInv.SetRotationXYZ((renderEntityData.bUseSecondOffset)? renderEntityData.secondRot : renderEntityData.rot);
			rotQuatInv.Invert();
			const Vec3 localAabbCentre = localAabb.GetCenter();
			localAabb.min = localAabb.min * rotQuatInv;
			localAabb.max = localAabb.max * rotQuatInv;

			// Update min's and max's to be correct
			AABB aabbCopy = localAabb;
			SwapIfMinBiggerThanMax(localAabb.min.x,localAabb.max.x);
			SwapIfMinBiggerThanMax(localAabb.min.y,localAabb.max.y);
			SwapIfMinBiggerThanMax(localAabb.min.z,localAabb.max.z);

			Vec3 aabbSize = localAabb.GetSize();

			// Get post render camera
			CCamera postRenderCamera;
			GetPostRenderCamera(postRenderCamera);

			// Calc destination width and height
			const float doubleScale = scaleToUse * 2.0f;
			float destScreenWidth = (renderEntityData.screenRect[2] - renderEntityData.screenRect[0]) * doubleScale;
			float destScreenHeight = (renderEntityData.screenRect[3] - renderEntityData.screenRect[1]) * doubleScale;

			if((destScreenWidth>0.0f) && (destScreenHeight>0.0f))
			{
				if((destScreenWidth > scaleToUse) || (destScreenHeight > scaleToUse))
				{
					if(destScreenWidth >= destScreenHeight)
					{
						destScreenHeight *= (scaleToUse / destScreenWidth);
						destScreenWidth = scaleToUse;
					}
					else
					{
						destScreenWidth *= (scaleToUse / destScreenHeight);
						destScreenHeight = scaleToUse;
					}
				}

				// Calc depth model needs to render at
				const float horizontalSize = tan(0.5f*postRenderCamera.GetHorizontalFov());
				const float verticalSize = tan(0.5f*postRenderCamera.GetFov());
				const float horizontalDepth = ((aabbSize.x*0.5f)/destScreenWidth) / horizontalSize;
				const float verticalDepth = ((aabbSize.z*0.5f)/destScreenHeight) / verticalSize;
				const float maxDepth = max(horizontalDepth,verticalDepth);

				renderEntityData.pos = Vec3(0.0f,maxDepth+(aabbSize.y*0.5f),0.0f);

				// Character pivot points are at their feet, not centre, so remove offset
				if(renderEntityData.flags & eRSE_IsCharacter)
				{
					const Vec3 centreOffset = localAabbCentre * rotQuatInv;
					renderEntityData.pos -= centreOffset;
				}

				// Override depth if less than near plane
				renderEntityData.pos.y = max(DEFAULT_NEAR + aabbSize.y,renderEntityData.pos.y);
			}
		}
	}
}//-------------------------------------------------------------------------------------------------

bool CMenuRender3DModelMgr::HasAttachmentStreamedIn(SRenderSingleEntityData& rd, IAttachment* pAttachment)
{
	bool bHasStreamed = true;

	IAttachmentObject* pAttachmentObj = pAttachment->GetIAttachmentObject();
	if(pAttachmentObj)
	{
		if (IStatObj* pStatObj = pAttachmentObj->GetIStatObj())
		{
			if (!HasStatObjStreamedIn(rd, pStatObj))
			{
				MR3MM_TRACE("[MR3MM] (%p) Missing stat obj '%s'", &rd, pStatObj->GetFilePath());
				bHasStreamed = false;
			}
		}

		if (pAttachmentObj->GetAttachmentType() == IAttachmentObject::eAttachment_Entity)
		{
			CEntityAttachment* pEntAttachment = static_cast<CEntityAttachment*>(pAttachmentObj);
			if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(pEntAttachment->GetEntityId()))
			{
				if (!HasEntityStreamedIn(rd, pEntity))
				{
					MR3MM_TRACE("[MR3MM] (%p) Missing entity attachment '%s'", &rd, pEntity->GetName());
					bHasStreamed = false;
				}
			}
		}

		if (ICharacterInstance* pCharInst = pAttachmentObj->GetICharacterInstance())
		{
			if (!HasCharacterStreamedIn(rd, pCharInst))
			{
				MR3MM_TRACE("[MR3MM] (%p) Missing character attachment", &rd);
				bHasStreamed = false;
			}
		}

		if (IAttachmentSkin* pASkin = pAttachmentObj->GetIAttachmentSkin())
		{
			if (ISkin* pSkin = pASkin->GetISkin())
			{
				if (!pSkin->GetIRenderMesh(0))
				{
					MR3MM_TRACE("[MR3MM] (%p) Missing skin attachment '%s'", &rd, pSkin->GetModelFilePath());
					bHasStreamed = false;

					if (IMaterial* pMaterial = pSkin->GetIMaterial(0))
					{
						if (!pMaterial->IsStreamedIn(rd.firstPrecacheRoundIds, pSkin->GetIRenderMesh(0)))
						{
							MR3MM_TRACE("[MR3MM] (%p) Missing skin attachment '%s' material '%s'", &rd, pSkin->GetModelFilePath(), pMaterial->GetName());
							bHasStreamed = false;
						}
					}
				}
			}
		}
	}

	return bHasStreamed;
}

bool CMenuRender3DModelMgr::HasCharacterStreamedIn(SRenderSingleEntityData& rd, ICharacterInstance* pCharacter)
{
	bool bHasStreamed = true;

	IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
	{
		if (!rIDefaultSkeleton.GetIRenderMesh())
		{
			MR3MM_TRACE("[MR3MM] (%p) Missing character model render mesh '%s'", &rd, rIDefaultSkeleton.GetModelFilePath());
			bHasStreamed = false;
		}
	}

	if (IMaterial* pMat = pCharacter->GetIMaterial())
	{
		if (!pMat->IsStreamedIn(rd.firstPrecacheRoundIds))
		{
			MR3MM_TRACE("[MR3MM] (%p) Missing character material '%s'", &rd, pMat->GetName());
			bHasStreamed = false;
		}
	}

	if(IAttachmentManager* pAttachmentMgr = pCharacter->GetIAttachmentManager())
	{
		const int32 attachmentCount = pAttachmentMgr->GetAttachmentCount();
		for(int32 i=0; i<attachmentCount; i++)
		{
			IAttachment* pAttachment = pAttachmentMgr->GetInterfaceByIndex(i);
			if(pAttachment)
			{
				if (!HasAttachmentStreamedIn(rd, pAttachment))
				{
					MR3MM_TRACE("[MR3MM] (%p) Missing attachment", &rd);
					bHasStreamed = false;
					break;
				}
			}
		}
	}

	return bHasStreamed;
}

bool CMenuRender3DModelMgr::HasStatObjStreamedIn(SRenderSingleEntityData& rd, IStatObj* pStatObj)
{
	int nFlags = pStatObj->GetFlags();

	if ((nFlags & STATIC_OBJECT_COMPOUND) && pStatObj->GetSubObjectCount())
	{
		for(int s=0,num = pStatObj->GetSubObjectCount(); s<num; s++)
		{
			IStatObj::SSubObject &subObj = *pStatObj->GetSubObject(s);

			if (subObj.pStatObj && subObj.nType == STATIC_SUB_OBJECT_MESH && !subObj.bShadowProxy)
			{
				IStatObj *pSubStatObj = subObj.pStatObj;

				if (IStatObj * pLod = (IStatObj *)pSubStatObj->GetLodObject(0, true))
				{
					if (!HasStatObjStreamedIn(rd, pLod))
					{
						MR3MM_TRACE("[MR3MM] (%p) Missing rendermesh for '%s'", &rd, pLod->GetFilePath());
						return false;
					}

					if (IMaterial* pMat = pLod->GetMaterial())
					{
						if (!pMat->IsStreamedIn(rd.firstPrecacheRoundIds, pLod->GetRenderMesh()))
						{
							MR3MM_TRACE("[MR3MM] (%p) Missing material '%s' for '%s'", &rd, pMat->GetName(), pLod->GetFilePath());
							return false;
						}
					}
				}
			}
		}
	}
	else
	{
		if(IStatObj * pLod = pStatObj->GetLodObject(0, true))
		{
			if (!pLod->GetRenderMesh())
			{
				MR3MM_TRACE("[MR3MM] (%p) Missing rendermesh for '%s'", &rd, pLod->GetFilePath());
				return false;
			}

			if (IMaterial* pMat = pLod->GetMaterial())
			{
				if (!pMat->IsStreamedIn(rd.firstPrecacheRoundIds, pLod->GetRenderMesh()))
				{
					MR3MM_TRACE("[MR3MM] (%p) Missing material '%s' for '%s'", &rd, pMat->GetName(), pLod->GetFilePath());
					return false;
				}
			}
		}
	}

	return true;
}

//--------------------------------------------------------------------------------------------------
// Name: HasEntityStreamedIn
// Desc: Returns true if entity has streamed in
//--------------------------------------------------------------------------------------------------
bool CMenuRender3DModelMgr::HasEntityStreamedIn(SRenderSingleEntityData& rd, IEntity* pEntity)
{
	bool bHasStreamed = true;
	if(pEntity)
	{
		SEntitySlotInfo slotInfo;
		const int slotIndex = 0;
		bool bGotSlotInfo = pEntity->GetSlotInfo(slotIndex,slotInfo);
		if(bGotSlotInfo)
		{
			if(!slotInfo.pCharacter && !slotInfo.pStatObj)
			{
				// No char or statObj yet
				MR3MM_TRACE("[MR3MM] (%p) Slot is empty on entity '%s'", &rd, pEntity->GetName());
				bHasStreamed = false;
			}
			else if(slotInfo.pCharacter)
			{
				if (!HasCharacterStreamedIn(rd, slotInfo.pCharacter))
				{
					MR3MM_TRACE("[MR3MM] (%p) Character missing on entity '%s'", &rd, pEntity->GetName());
					bHasStreamed = false;
				}
			}
			else
			{
				// Stat obj
				if (!HasStatObjStreamedIn(rd, slotInfo.pStatObj))
				{
					MR3MM_TRACE("[MR3MM] (%p) Stat obj missing on entity '%s'", &rd, pEntity->GetName());
					bHasStreamed = false;
				}
			}
		}
	}
	else
	{
		// No entity, so not streamed in
		bHasStreamed = false;
	}

	return bHasStreamed;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: PreCacheMaterial
// Desc: Pre-caches material
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::PreCacheMaterial(SRenderSingleEntityData& rd, IEntity* pEntity,bool bIsCharacter) 
{
	// Precache material -> This forces the material to have a high streaming priority
	const float distance = 0.0f;
	IRenderMesh* pRenderMesh = NULL;
	const bool bFullUpdate = true;
	const bool bDrawNear = true;

	IEntityRender* pIEntityRender = pEntity->GetRenderInterface();
	if (pIEntityRender && pIEntityRender->GetRenderNode())
		gEnv->p3DEngine->PrecacheRenderNode(pIEntityRender->GetRenderNode(), 0.0f);

	if (!rd.firstPrecacheRoundIds[0])
	{
		gEnv->p3DEngine->GetPrecacheRoundIds(rd.firstPrecacheRoundIds);

		// We only precached the near zone, so we don't care about when the far zone was last precached.
		rd.firstPrecacheRoundIds[1] = -1;
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: AddModelData
// Desc: Adds model data
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::AddModelData(const PodArray<CMenuRender3DModelMgr::SModelData>& modelData)
{
	if(IsMenu3dModelEngineSupportActive())
	{
		const uint modelCount = modelData.Size();
		for(uint i=0; i<modelCount; i++)
		{
			const SModelData& currentModelData = modelData[i];

			SModelParams modelParams;
			modelParams.pName = currentModelData.name.c_str();
			modelParams.pFilename = currentModelData.filename.c_str();
			modelParams.pMaterialOverrideFilename = currentModelData.materialOverrideFilename.c_str();
			modelParams.rot = currentModelData.rot;
			modelParams.secondRot = currentModelData.secondRot;
			modelParams.userRotScale = currentModelData.userRotScale;
			modelParams.posOffset = currentModelData.posOffset;
			modelParams.posSecondOffset = currentModelData.posSecondOffset;
			modelParams.scale = currentModelData.scale;
			modelParams.secondScale = currentModelData.secondScale;
			modelParams.silhouetteColor = currentModelData.silhouetteColor;
			memcpy(modelParams.screenRect,currentModelData.screenRect,sizeof(float)*4);

			// Add model
			const TAddedModelIndex modelIndex = AddModel(	modelParams );

			if(modelIndex != kAddedModelIndex_Invalid)
			{
				SRenderSingleEntityData& renderEntityData = m_renderEntityData[modelIndex];

				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(renderEntityData.entityId);
				if(pEntity)
				{
					SetAsPost3dRenderObject(pEntity,renderEntityData.groupId,renderEntityData.screenRect);
				}

				// Set animation
				const char* pAnimName = currentModelData.animName.c_str();
				if(pAnimName && pAnimName[0])
				{
					UpdateAnim(modelIndex,pAnimName,currentModelData.animSpeed);
				}

				// Set attachment
				const char* pAttachmentParent = currentModelData.attachmentParent.c_str();
				if(pAttachmentParent && pAttachmentParent[0])
				{
					CMenuRender3DModelMgr::TAddedModelIndex parentModelIndex = 0;
					if(GetModelIndexFromName(pAttachmentParent,parentModelIndex))
					{
						AttachChildToParent(modelIndex,parentModelIndex,currentModelData.attachmentLocation.c_str());
					}
				}
			}
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SetSilhouette
// Desc: Sets model silhouette
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::SetSilhouette(TAddedModelIndex modelIndex, const Vec3& silhouetteColor)
{
	const uint renderEntityCount = m_renderEntityData.size();
	if(modelIndex < renderEntityCount)
	{
		SRenderSingleEntityData& renderEntityData =  m_renderEntityData[modelIndex];
		renderEntityData.silhouetteColor.x = clamp_tpl<float>(silhouetteColor.x,0.0f,1.0f);
		renderEntityData.silhouetteColor.y = clamp_tpl<float>(silhouetteColor.y,0.0f,1.0f);
		renderEntityData.silhouetteColor.z = clamp_tpl<float>(silhouetteColor.z,0.0f,1.0f);
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SetAlpha
// Desc: Sets model alpha
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::SetAlpha(TAddedModelIndex modelIndex,float alpha)
{
	const uint renderEntityCount = m_renderEntityData.size();
	if(modelIndex < renderEntityCount)
	{
		SRenderSingleEntityData& renderEntityData =  m_renderEntityData[modelIndex];
		renderEntityData.maxAlpha = clamp_tpl<float>(alpha,0.0f,1.0f);
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: UseSecondOffset
// Desc: Enables or disabled usage of the second offset values
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::UseSecondOffset(TAddedModelIndex modelIndex, bool enabled)
{
	const uint renderEntityCount = m_renderEntityData.size();
	if(modelIndex < renderEntityCount)
	{
		SRenderSingleEntityData& renderEntityData =  m_renderEntityData[modelIndex];
		renderEntityData.bUseSecondOffset = enabled;
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: GetModelIndexFromName
// Desc: Gets a model index from name, returns true if found model
//--------------------------------------------------------------------------------------------------
bool CMenuRender3DModelMgr::GetModelIndexFromName(const char* pName,TAddedModelIndex& outModelIndex) const
{
	const uint renderEntityCount = m_renderEntityData.size();
	for(uint i=0; i<renderEntityCount; i++)
	{
		if(strcmp(m_renderEntityData[i].name.c_str(),pName)==0)
		{
			outModelIndex = i;
			return true;
		}
	}
	return false;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: UpdateModel
// Desc: Updates model with new data
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::UpdateModel(TAddedModelIndex modelIndex,const char* pFilename, const char* pMaterialOverrideFilename, bool bUpdateModelPos)
{
	if(IsMenu3dModelEngineSupportActive())
	{
		const uint renderEntityCount = m_renderEntityData.size();
		if(modelIndex < renderEntityCount)
		{
			const bool bIsNewModelCharacter = IsCharacterFile(pFilename);

			SRenderSingleEntityData& renderEntityData = m_renderEntityData[modelIndex];

			IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
			if(pEntitySystem)
			{
				IEntity* pEntity = pEntitySystem->GetEntity(renderEntityData.entityId);
				if(pEntity)
				{
					const int slotIndex = 0;
					float animNormalizedTime = 0.0f;
					const bool bHasNewModel = (pFilename && pFilename[0]) ? true : false;
					const bool bIsOldModelCharacter = (renderEntityData.flags & eRSE_IsCharacter) ? true : false;

					// If previous model wasn't streamed in, then it can't rely on previous model pos
					if(!(renderEntityData.flags & eRSE_StreamedIn) && !(renderEntityData.flags & eRSE_Attached))
					{
						bUpdateModelPos = true;
					}

					if(bIsNewModelCharacter)
					{
						renderEntityData.flags |= eRSE_IsCharacter;
					}
					else
					{
						renderEntityData.flags &=~ eRSE_IsCharacter;
					}

					// UnHide model, then reHide after loading new model, otherwise will get out of sync
					const bool bPrevHidden = pEntity->IsHidden();
					HideModel(false,modelIndex,bUpdateModelPos);

					if(bIsNewModelCharacter)
					{
						// Get animNormaizedTime so we can resume animation playback from same place for new char
						ICharacterInstance *pCharInst = pEntity->GetCharacter(slotIndex);
						if(pCharInst)
						{
							const uint32 animationLayer = 0;
							ISkeletonAnim* pSkelAnim = pCharInst->GetISkeletonAnim();
							if(pSkelAnim)
							{
								const int animationsInLayer = pSkelAnim->GetNumAnimsInFIFO(animationLayer);
								const int lastAnimationIndex = animationsInLayer - 1;
								CAnimation& animation = pSkelAnim->GetAnimFromFIFO(animationLayer,lastAnimationIndex);
								animNormalizedTime = pSkelAnim->GetAnimationNormalizedTime(&animation);
							}
						}

						// Free old char and load new char
						pEntity->FreeSlot(slotIndex);

						if(bHasNewModel)
						{
							pEntity->LoadCharacter(slotIndex, pFilename,IEntity::EF_NO_STREAMING);
							SetCharacterFlags(pEntity);
						}
					}
					else
					{
						// Free old obj and load new obj
						pEntity->FreeSlot(slotIndex);
						if(bHasNewModel)
						{
							pEntity->LoadGeometry(0, pFilename,NULL,IEntity::EF_NO_STREAMING);
						}
					}

					if(bHasNewModel)
					{
						renderEntityData.flags |= eRSE_HasModel;
					}
					else
					{
						renderEntityData.flags &=~ eRSE_HasModel;
					}

					ApplyMaterialOverride(pEntity,pMaterialOverrideFilename);
					PreCacheMaterial(renderEntityData, pEntity,bIsNewModelCharacter);

					SetAsPost3dRenderObject(pEntity,renderEntityData.groupId,renderEntityData.screenRect);

					// Apply existing animation
					if(bHasNewModel)
					{
						const char* pAnimName = renderEntityData.animName.c_str();
						if(pAnimName && pAnimName[0])
						{
							float speedMultiplier = 0.5f;
							int layer = 0;

							ICharacterInstance *pCharInst = pEntity->GetCharacter(slotIndex);
							if(pCharInst)
							{
								CryCharAnimationParams animParams;
								animParams.m_nLayerID = layer;
								animParams.m_nFlags |= CA_LOOP_ANIMATION | CA_FORCE_SKELETON_UPDATE;
								animParams.m_fPlaybackSpeed = speedMultiplier;
								animParams.m_fTransTime = 0.0f;
								ISkeletonAnim* pSkelAnim = pCharInst->GetISkeletonAnim();
								if(pSkelAnim)
								{
									pSkelAnim->StartAnimation(pAnimName,animParams);

									const uint32 animationLayer = 0;
									const int animationsInLayer = pSkelAnim->GetNumAnimsInFIFO(animationLayer);
									const int lastAnimationIndex = animationsInLayer - 1;
									CAnimation& animation = pSkelAnim->GetAnimFromFIFO(animationLayer,lastAnimationIndex);
									pSkelAnim->SetAnimationNormalizedTime(&animation,animNormalizedTime);
								}
							}
						}
					}

					// If model is already streamed in then calc new world pos
					if(!pEntity->IsHidden() && bUpdateModelPos)
					{
						CalcWorldPosFromScreenRect(renderEntityData);
					}

					// Fix attachments
					for(uint8 i=0; i<renderEntityCount; i++)
					{
						SRenderSingleEntityData& childRenderEntityData = m_renderEntityData[i];
						if(childRenderEntityData.parentIndex == modelIndex)
						{
							AttachChildToParent(i,modelIndex,childRenderEntityData.attachmentLocation.c_str());
						}
					}

					renderEntityData.flags |= eRSE_Invalid;

					// If attached then don't fade in, but set alpha the same as parent
					if(renderEntityData.flags & eRSE_Attached)
					{
						SRenderSingleEntityData& parentRenderEntityData = m_renderEntityData[renderEntityData.parentIndex];
						renderEntityData.alpha = parentRenderEntityData.alpha;
						parentRenderEntityData.flags |= eRSE_Invalid;
					}

					HideModel(bPrevHidden,modelIndex,bUpdateModelPos);
				}
			}
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SetAsPost3dRenderObject
// Desc: Sets model to render as post 3d render object
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::SetAsPost3dRenderObject(IEntity* pEntity,uint8 groupId,float* pScreenRect)
{
	if(pEntity)
	{
		auto nodeParams = pEntity->GetRenderNodeParams();
		nodeParams.additionalRenderNodeFlags |= ERF_FORCE_POST_3D_RENDER|ERF_RENDER_ALWAYS|ERF_HUD; // Set render node to always render
		nodeParams.lodRatio = 0; // Ignore LODs
		pEntity->SetRenderNodeParams(nodeParams);
		//pIEntityRender->SetAsPost3dRenderObject(true,groupId,pScreenRect);
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ApplyMaterialOverride
// Desc: Applies an override material to a model
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::ApplyMaterialOverride(IEntity* pEntity,const char* pOverrideMaterialFilename)
{
	if(pEntity && pOverrideMaterialFilename && pOverrideMaterialFilename[0])
	{
		IMaterialManager* pMaterialManager = gEnv->p3DEngine->GetMaterialManager();
		if(pMaterialManager)
		{
			IMaterial* pMaterialOverride = pMaterialManager->FindMaterial(pOverrideMaterialFilename);
			if(!pMaterialOverride)
			{
				const bool bMakeIfNotFound = false;
				pMaterialOverride = pMaterialManager->LoadMaterial(pOverrideMaterialFilename,bMakeIfNotFound);

				if(!pMaterialOverride)
				{
					FE_LOG("Material not found '%s'", pOverrideMaterialFilename);
				}
			}
			if(pMaterialOverride)
			{
				pEntity->SetSlotMaterial(0,pMaterialOverride);
			}
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SetCharacterFlags
// Desc: Sets character flags
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::SetCharacterFlags(IEntity* pEntity)
{
	if(pEntity)
	{
		const int slotIndex = 0;
		SEntitySlotInfo slotInfo;
		const bool bGotSlotInfo = pEntity->GetSlotInfo(slotIndex, slotInfo);
		if(bGotSlotInfo && slotInfo.pCharacter)
		{
			slotInfo.pCharacter->SetFlags(slotInfo.pCharacter->GetFlags()|CS_FLAG_UPDATE|CS_FLAG_UPDATE_ALWAYS);
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SetSceneSettings
// Desc: Sets scene settings
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::SetSceneSettings(const SSceneSettings& sceneSettings)
{
	// Set scene settings
	m_sceneSettings.ambientLight = sceneSettings.ambientLight;
	m_sceneSettings.ambientLight.w *= MENU_LIGHT_AMBIENT_ALPHA_SCALE;
	m_sceneSettings.fadeInSpeed = sceneSettings.fadeInSpeed;
	m_sceneSettings.fovScale = sceneSettings.fovScale;
	m_sceneSettings.flashEdgeFadeScale = sceneSettings.flashEdgeFadeScale;

	ReleaseLights();

	const int lightCount = sceneSettings.lights.size();
	m_sceneSettings.lights.resize(lightCount);
	for(int i=0; i<lightCount; i++)
	{
		// Copy light settings
		SLightData& lightData = m_sceneSettings.lights[i];
		memcpy(&lightData,&sceneSettings.lights[i],sizeof(CMenuRender3DModelMgr::SLightData));

#if !DEBUG_MENU_RENDER_3D_MODEL_MGR
		lightData.color *= MENU_LIGHT_COLOR_SCALE;
#endif

		// Create light source
		lightData.pLightSource = gEnv->p3DEngine->CreateLightSource();
		if(lightData.pLightSource)
		{
			const float frameTime = 0.0f;
			UpdateLight(i,frameTime);	//Needs to update before Register so it has a valid AABB
			gEnv->p3DEngine->RegisterEntity(lightData.pLightSource);
			lightData.pLightSource->SetRndFlags(ERF_RENDER_ALWAYS,true);
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: UpdateAnim
// Desc: Updates animation with new data
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::UpdateAnim(TAddedModelIndex modelIndex, const char* animName,float speedMultiplier,int layer)
{
	if(IsMenu3dModelEngineSupportActive())
	{
		if(animName && animName[0])
		{
			CACHE_LOG_FILE_ACCESSES_START;

			const uint renderEntityCount = m_renderEntityData.size();
			if(modelIndex < renderEntityCount)
			{
				SRenderSingleEntityData& renderEntityData = m_renderEntityData[modelIndex];

				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(renderEntityData.entityId);
				if(pEntity)
				{
					renderEntityData.animName = animName;

					const int slotIndex = 0;
					ICharacterInstance* pCharInst = pEntity->GetCharacter(slotIndex);
					if(pCharInst)
					{
						CryCharAnimationParams animParams;
						animParams.m_nLayerID = layer;
						animParams.m_nFlags |= CA_LOOP_ANIMATION | CA_FORCE_SKELETON_UPDATE;
						animParams.m_fPlaybackSpeed = speedMultiplier;
						animParams.m_fTransTime = 0.0f;
						pCharInst->GetISkeletonAnim()->StartAnimation(animName,animParams);
					}
				}
			}

			CACHE_LOG_FILE_ACCESSES_END;
		}
	}
}//-------------------------------------------------------------------------------------------------

bool CMenuRender3DModelMgr::IsModelStreamed(TAddedModelIndex modelIndex) const
{
	if (!(m_renderEntityData[modelIndex].flags & eRSE_HasModel))
		return true;

	if (m_renderEntityData[modelIndex].flags & eRSE_Invalid)
		return false;

	if (!(m_renderEntityData[modelIndex].flags & eRSE_StreamedIn))
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------
// Name: AttachChildToParent
// Desc: Attaches child to parent
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::AttachChildToParent(TAddedModelIndex childIndex,TAddedModelIndex parentIndex, const char* pAttachPointName)
{
	if(pAttachPointName && pAttachPointName[0])
	{
		const uint renderEntityCount = m_renderEntityData.size();
		if(parentIndex < renderEntityCount && childIndex < renderEntityCount)
		{
			SRenderSingleEntityData& parentRenderData =  m_renderEntityData[parentIndex];
			SRenderSingleEntityData& childRenderData =  m_renderEntityData[childIndex];

			// Store top level parent index, so all linked models can have a common piece of data
			TAddedModelIndex topParentIndex = parentIndex;
			SRenderSingleEntityData* pTopParentRenderData = &m_renderEntityData[topParentIndex];
			while(pTopParentRenderData->topParentIndex != topParentIndex)
			{
				topParentIndex = pTopParentRenderData->topParentIndex;
				pTopParentRenderData = &m_renderEntityData[topParentIndex];
			}

			if(parentRenderData.flags & eRSE_IsCharacter)
			{
				AttachToCharacter(childIndex,parentIndex,pAttachPointName);
			}
			else
			{
				AttachToStaticObject(childIndex,parentIndex,pAttachPointName);
			}

			// Copy data after attachment change, so can release previous attachment
			childRenderData.topParentIndex = topParentIndex;
			childRenderData.parentIndex = parentIndex;
			childRenderData.attachmentLocation = pAttachPointName;
			memcpy(childRenderData.screenRect,pTopParentRenderData->screenRect,sizeof(float)*4);
			childRenderData.pos = ZERO;

			IEntity* pChildEntity = gEnv->pEntitySystem->GetEntity(childRenderData.entityId);
			if(pChildEntity)
			{
				const uint8 topParentGroupId = pTopParentRenderData->groupId;
				const uint8 groupIdToFree = childRenderData.groupId;
				childRenderData.groupId = topParentGroupId;
				SetAsPost3dRenderObject(pChildEntity,topParentGroupId,pTopParentRenderData->screenRect);
			
				if(groupIdToFree != topParentGroupId)
				{
					FreeUnusedGroup(groupIdToFree);
				}
			}
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: FreeUnusedGroup
// Desc: Frees an unused group
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::FreeUnusedGroup(uint8 unusedGroupId)
{
	const uint8 endGroupIndex = m_groupCount-1;
	if(unusedGroupId < endGroupIndex)
	{
		// Not last group, so swap with last group
		const uint renderEntityCount = m_renderEntityData.size();
		for(uint8 i=0; i<renderEntityCount; i++)
		{
			SRenderSingleEntityData& renderEntityData = m_renderEntityData[i];
			if(renderEntityData.groupId == endGroupIndex)
			{
				renderEntityData.groupId = unusedGroupId;
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(renderEntityData.entityId);
				if(pEntity)
				{
					SRenderSingleEntityData& topParentRenderEntityData = m_renderEntityData[renderEntityData.topParentIndex];
					SetAsPost3dRenderObject(pEntity,unusedGroupId,topParentRenderEntityData.screenRect);
				}
			}
		}
	}
	m_groupCount--;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: AttachToStaticObject
// Desc: Attaches a model to a static object
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::AttachToStaticObject(TAddedModelIndex childIndex, TAddedModelIndex parentIndex, const char* pMountPointName)
{
	if(pMountPointName && pMountPointName[0])
	{
		const uint renderEntityCount = m_renderEntityData.size();
		if((parentIndex < renderEntityCount) && (childIndex < renderEntityCount))
		{
			SRenderSingleEntityData& childRenderEntityData = m_renderEntityData[childIndex];
			childRenderEntityData.flags |= eRSE_Attached;

			IEntity* pParentEntity = gEnv->pEntitySystem->GetEntity(m_renderEntityData[parentIndex].entityId);
			IEntity* pChildEntity = gEnv->pEntitySystem->GetEntity(childRenderEntityData.entityId);

			if(pParentEntity && pChildEntity)
			{
				const int slotIndex = 0;
				IStatObj* pStatObject = pParentEntity->GetStatObj(slotIndex);
				if(pStatObject)
				{
					const bool bRelativeToParent = false;
					const Vec3 rawPosition = pStatObject->GetHelperPos(pMountPointName);
					const Vec3 transformedPosition = pChildEntity->GetSlotLocalTM(slotIndex,bRelativeToParent).TransformPoint(rawPosition);

					pParentEntity->AttachChild(pChildEntity);

					Matrix34 transformMatrix;
					transformMatrix.SetIdentity();
					transformMatrix.SetTranslation(transformedPosition);
					pChildEntity->SetLocalTM(transformMatrix);
				}
			}
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: AttachToCharacter
// Desc: Attaches a model to a character
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::AttachToCharacter(TAddedModelIndex childIndex, TAddedModelIndex parentIndex, const char* pAttachPointName)
{
	CACHE_LOG_FILE_ACCESSES_START;

	if(pAttachPointName && pAttachPointName[0])
	{
		const uint renderEntityCount = m_renderEntityData.size();
		if((parentIndex < renderEntityCount) && (childIndex < renderEntityCount))
		{
			SRenderSingleEntityData& parentRenderData = m_renderEntityData[parentIndex];
			IEntity* pParentEntity = gEnv->pEntitySystem->GetEntity(parentRenderData.entityId);
			if(pParentEntity)
			{
				const int slotIndex = 0;
				ICharacterInstance* pCharInst = pParentEntity->GetCharacter(slotIndex);
				if(pCharInst)
				{
					IAttachmentManager* pAttachMgr = pCharInst->GetIAttachmentManager();
					if(pAttachMgr)
					{
						SRenderSingleEntityData& childRenderEntityData = m_renderEntityData[childIndex];

						// Clear previous attachment binding
						const char* pPrevAttachmentName = childRenderEntityData.attachmentLocation.c_str();
						if(pPrevAttachmentName && pPrevAttachmentName[0])
						{
							IAttachment* pPrevAttachment = pAttachMgr->GetInterfaceByName(pPrevAttachmentName);
							if(pPrevAttachment)
							{
								pPrevAttachment->ClearBinding();
							}
						}

						// Add new attachment binding
						IAttachment* pAttachment = pAttachMgr->GetInterfaceByName(pAttachPointName);
						if(pAttachment)
						{
							CEntityAttachment* pEntityAttachment = new CEntityAttachment();

							childRenderEntityData.flags |= eRSE_Attached;
							pEntityAttachment->SetEntityId(childRenderEntityData.entityId);

							pAttachment->AddBinding(pEntityAttachment);

							IEntity* pChildEntity = gEnv->pEntitySystem->GetEntity(childRenderEntityData.entityId);
							if(pChildEntity)
							{
								pAttachment->HideAttachment(pChildEntity->IsHidden());
							}
						}
					}
				}
			}
		}
	}

	CACHE_LOG_FILE_ACCESSES_END;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: UpdateLight
// Desc: Updates light
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::UpdateLight(int lightIndex,float frameTime)
{
	const int lightCount = m_sceneSettings.lights.size();
	if(lightIndex < lightCount)
	{
		SLightData& lightData = m_sceneSettings.lights[lightIndex];

		if(lightData.pLightSource)
		{
			SRenderLight menuLight;

			const Vec3 lightPos = lightData.pos;
			Vec3 lightDataColor = lightData.color;

#if DEBUG_MENU_RENDER_3D_MODEL_MGR
			lightDataColor *= s_menuLightColorScale;
#endif

			ColorF menuLightColor(lightDataColor,0.0);

			menuLight.m_Flags |= DLF_POINT|DLF_POST_3D_RENDERER;
			menuLight.SetPosition(lightPos);
			menuLight.SetLightColor(menuLightColor);
			menuLight.SetSpecularMult(lightData.specular);
			menuLight.SetRadius(lightData.radius);

			char uniqueLightName[16];
			cry_sprintf(uniqueLightName,sizeof(uniqueLightName),"Menu light %d",lightIndex);
			menuLight.m_sName = uniqueLightName;

			lightData.pLightSource->SetLightProperties(menuLight);

			Vec3 halfBBSize(menuLight.m_fRadius,menuLight.m_fRadius,menuLight.m_fRadius);
			lightData.pLightSource->SetBBox(AABB(lightPos-halfBBSize,lightPos+halfBBSize));
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: UpdateLightSettings
// Desc: Updates light settings
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::UpdateLightSettings(float frameTime)
{
	const int lightCount = m_sceneSettings.lights.size();
	for(int i=0; i<lightCount; i++)
	{
		UpdateLight(i,frameTime);
	}
	const bool bForceValue = true;
	gEnv->p3DEngine->SetPostEffectParamVec4("Post3DRenderer_Ambient", m_sceneSettings.ambientLight, bForceValue);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: UpdateStreaming
// Desc: Updates streaming
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::UpdateStreaming(bool bUpdateModelPos)
{
	const uint renderEntityCount = m_renderEntityData.size();
	for(uint8 i=0; i<renderEntityCount; i++)
	{
		SRenderSingleEntityData& renderEntityData = m_renderEntityData[i];
		const bool bIsCharacter = (renderEntityData.flags&eRSE_IsCharacter) ? true : false;
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(renderEntityData.entityId);
		if(pEntity)
		{
			PreCacheMaterial(renderEntityData, pEntity,bIsCharacter);

			if (renderEntityData.flags & eRSE_Invalid)
				renderEntityData.flags &= ~(eRSE_StreamedIn | eRSE_Invalid);

			const bool bIsStreamedIn = (renderEntityData.flags&eRSE_StreamedIn) ? true : false;
			if(!bIsStreamedIn || pEntity->IsHidden())
			{
				bool bHasStreamedIn = false;
				bool bHasAnimStreamedIn = true;
				const bool bHasModel = (renderEntityData.flags&eRSE_HasModel) ? true : false;

				if(bHasModel)
				{
					// Has entity streamed in...
					bHasStreamedIn = HasEntityStreamedIn(renderEntityData, pEntity);

					// Has animation streamed in...
					const char* pAnimName = renderEntityData.animName.c_str();
					if(bIsCharacter && pAnimName && pAnimName[0])
					{
						bHasAnimStreamedIn = false;
						const int slotIndex = 0;
						ICharacterInstance* pCharInst = pEntity->GetCharacter(slotIndex);
						if(pCharInst)
						{
							IAnimationSet* pAnimSet = pCharInst->GetIAnimationSet();
							if(pAnimSet)
							{
								const int animId = pAnimSet->GetAnimIDByName(pAnimName);
								bHasAnimStreamedIn = pAnimSet->IsAnimLoaded(animId);
							}
						}
					}
				}

				// Update entity hidden status if changed
				if(bHasStreamedIn && bHasAnimStreamedIn)
				{
					bool bRenderThisEntity = true;
					if(bHasModel)
					{
						renderEntityData.flags |= eRSE_StreamedIn;

						// Only render this entity if attached entities have streamed in too
						for(uint8 j=0; j<renderEntityCount; j++)
						{
							SRenderSingleEntityData& otherAttachedData = m_renderEntityData[j];
							if(otherAttachedData.topParentIndex == renderEntityData.topParentIndex)
							{
								if(!(otherAttachedData.flags&eRSE_StreamedIn) && (otherAttachedData.flags&eRSE_HasModel))
								{
									bRenderThisEntity = false;
									break;
								}
							}
						}
					}

					// Set new hidden status of all entities linked to this one
					bUpdateModelPos &= (renderEntityData.topParentIndex == i); //Entities other than the top parent one should not update anyone's position
					if(bRenderThisEntity)
					{
						for(uint8 j=0; j<renderEntityCount; j++)
						{
							SRenderSingleEntityData& otherAttachedData = m_renderEntityData[j];
							if(otherAttachedData.topParentIndex == renderEntityData.topParentIndex)
							{
								if(otherAttachedData.flags&eRSE_HasModel)
								{
									HideModel(!bRenderThisEntity,j,bUpdateModelPos);
								}
							}
						}
					}
					else if (bUpdateModelPos)
					{
						// Force animation updates until we are ready to render, otherwise the 1st couple of 
						// frames of rendering will show character model animation incorrectly
						ForceAnimationUpdate(pEntity);
					}
				}
				else
				{
					// Entity not streamed in, so hide
					renderEntityData.flags &=~ eRSE_StreamedIn;
					HideModel(true,i,bUpdateModelPos);
				}
			}
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: UpdateEntities
// Desc: Updates entities
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::UpdateEntities()
{
	const uint renderEntityCount = m_renderEntityData.size();
	const float frameTime = gEnv->pTimer->GetFrameTime(ITimer::ETIMER_UI);
	const float keepOldAlphaFraction = powf(m_sceneSettings.fadeInSpeed,frameTime);
	const float invKeepOldAlphaFraction = 1.0f - keepOldAlphaFraction;

	for(uint8 i=0; i<renderEntityCount; i++)
	{
		SRenderSingleEntityData& renderEntityData = m_renderEntityData[i];
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(renderEntityData.entityId);
		if(pEntity)
		{
			{
				// Set silhouettes
				const Vec3& silhouetteColor = renderEntityData.silhouetteColor;
				//pIEntityRender->SetHUDSilhouettesParams(silhouetteColor.x,silhouetteColor.y,silhouetteColor.z,0.0f);
			
				// Update alpha
				if(!pEntity->IsHidden())
				{
					renderEntityData.alpha = (renderEntityData.maxAlpha*invKeepOldAlphaFraction)+(renderEntityData.alpha*keepOldAlphaFraction);
					pEntity->SetOpacity(renderEntityData.alpha);
				}
			}

			// Update entities 
			if(!(renderEntityData.flags & eRSE_Attached)) // Attached objects get updated from their parents
			{
				Vec3 &posOffsetToUse = (renderEntityData.bUseSecondOffset)? renderEntityData.posSecondOffset : renderEntityData.posOffset;
				const Vec3 renderEntityPos = (renderEntityData.pos+posOffsetToUse);

				QuatT newTran;
				renderEntityData.rotationOverTime += renderEntityData.continuousRot * frameTime;
			//	Ang3 userRot(0.0f, m_userControlledRotationY*renderEntityData.userRotScale,m_userControlledRotationX*renderEntityData.userRotScale);
				Ang3 userRot = renderEntityData.rotationOverTime;
				Ang3 &rotToUse = (renderEntityData.bUseSecondOffset)? renderEntityData.secondRot : renderEntityData.rot;
				newTran.q.SetRotationXYZ(( rotToUse + userRot));
				newTran.t = renderEntityPos;

				// Center non-character objects (Needed for user rotation etc)
				if(!(renderEntityData.flags&eRSE_IsCharacter))
				{
					AABB localAabb;
					pEntity->GetLocalBounds(localAabb);
					const Vec3 localAabbCentre = localAabb.GetCenter();
					newTran.t -= localAabbCentre * newTran.q.GetInverted();
				}
				pEntity->SetPos(newTran.t);
				pEntity->SetRotation(newTran.q);
			}
		}
	}

	const bool bForceValue = true;
	gEnv->p3DEngine->SetPostEffectParam("Post3DRenderer_FOVScale", m_sceneSettings.fovScale, bForceValue);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: OnPreRender
// Desc: On pre-render
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::OnPreRender()
{
	if(!m_bReleaseMe)
	{
		UpdateStreaming();
		UpdateEntities();

		const bool bForceValue = true;
		gEnv->p3DEngine->SetPostEffectParam("Post3DRenderer_EdgeFadeScale", m_sceneSettings.flashEdgeFadeScale, true);
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SetVisibilityOfAllEntities
// Desc: Sets visibility of all entities
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::SetVisibilityOfAllEntities(bool bVisible)
{
	const uint renderEntityCount = m_renderEntityData.size();
	for(uint i=0; i<renderEntityCount; i++)
	{
		HideModel(!bVisible,i);
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: OnPostUpdate
// Desc: On post update
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::OnPostUpdate(float frameTime)
{
	if(m_bReleaseMe)
	{
		m_framesUntilDestroy--;
		if(m_framesUntilDestroy == 0)
		{
			delete this;
		}
	}
	else
	{
		UpdateContinuousRot(frameTime);
		UpdateLightSettings(frameTime);

#if DEBUG_MENU_RENDER_3D_MODEL_MGR
		DebugDraw();
#endif
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: UpdateContinuousRot
// Desc: Updates rotation of models over time
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::UpdateContinuousRot(float frameTime)
{

}//-------------------------------------------------------------------------------------------------

#if DEBUG_MENU_RENDER_3D_MODEL_MGR
//--------------------------------------------------------------------------------------------------
// Name: DebugDraw
// Desc: Draws debug view
//--------------------------------------------------------------------------------------------------
void CMenuRender3DModelMgr::DebugDraw()
{
	if(g_pGameCVars->g_post3DRendererDebug == 2)
	{
		float scale = m_fDebugScale;
		IRenderAuxGeom* pAuxRenderer = gEnv->pRenderer->GetIRenderAuxGeom();
		if(pAuxRenderer)
		{
			// Set debug render state
			SAuxGeomRenderFlags renderFlags;
			renderFlags.SetDepthTestFlag(e_DepthTestOff);
			renderFlags.SetCullMode(e_CullModeNone);
			renderFlags.SetAlphaBlendMode(e_AlphaBlended);
			pAuxRenderer->SetRenderFlags(renderFlags);

			static ColorB hiddenColor(255,0,0,255);
			static ColorB finishedStreamingColor(255,255,0,255);
			static ColorB renderingColor(0,255,0,255);

			const uint renderEntityCount = m_renderEntityData.size();
			for(uint8 i=0; i<renderEntityCount; i++)
			{
				SRenderSingleEntityData& renderEntityData = m_renderEntityData[i];
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(renderEntityData.entityId);
				if(pEntity)
				{
					// Draw aabb's for each model, the color represents stream-in progress
					AABB aabb;
					pEntity->GetWorldBounds(aabb);

					ColorB aabbColor = hiddenColor;
					
					if(renderEntityData.flags & eRSE_StreamedIn)
					{
						aabbColor = (pEntity->IsHidden()) ? finishedStreamingColor : renderingColor;
					}

					pAuxRenderer->DrawAABB(aabb,false,aabbColor,eBBD_Faceted);
				}
			}

			// Draw canvas + RT
			SAuxGeomRenderFlags flags( e_Def2DPublicRenderflags );
			flags.SetDepthTestFlag( e_DepthTestOff );
			flags.SetDepthWriteFlag( e_DepthWriteOff );
			flags.SetCullMode( e_CullModeNone );
			pAuxRenderer->SetRenderFlags(flags);

			ITexture* tex  = NULL;

			if(gEnv->pConsole->GetCVar("r_UsePersistentRTForModelHUD")->GetIVal() > 0)
				tex =	gEnv->pRenderer->EF_GetTextureByName("$ModelHUD");
			else
				tex =	gEnv->pRenderer->EF_GetTextureByName("$BackBuffer");

			IRenderAuxImage::Draw2dImage(0, 0, 800*scale, 600*scale, -1, 0.0f,0.0f,1.0f,1.0f,	0.f,
				0.0f, 0.0f, 0.0f, 1.0f, 0.f);
			IRenderAuxImage::Draw2dImage(0.0f,0.0f, 800*scale, 600*scale, tex->GetTextureID(), 0.0f, 1.0f, 1.0f, 0.0f);

			pAuxRenderer->DrawLine(Vec3(0,0,0), ColorB(255,255,255),Vec3(0,1*scale,0), ColorB(255,255,255), 1.0f);
			pAuxRenderer->DrawLine(Vec3(0,0,0), ColorB(255,255,255),Vec3(1*scale,0,0), ColorB(255,255,255), 1.0f);
			pAuxRenderer->DrawLine(Vec3(0,1*scale,0), ColorB(255,255,255),Vec3(1*scale, 1*scale,0), ColorB(255,255,255), 1.0f);
			pAuxRenderer->DrawLine(Vec3(1*scale,0,0), ColorB(255,255,255),Vec3(1*scale,1*scale,0), ColorB(255,255,255), 1.0f);

			// Draw uv-mapping
			for(uint8 i=0; i<renderEntityCount; i++)
			{
				SRenderSingleEntityData& renderEntityData = m_renderEntityData[i];
				pAuxRenderer->DrawLine(Vec3(renderEntityData.screenRect[0] * scale, renderEntityData.screenRect[1] * scale, 0), ColorB(255,255,255),Vec3(renderEntityData.screenRect[2] * scale, renderEntityData.screenRect[1] * scale, 0), ColorB(255,255,255), 1.5f);
				pAuxRenderer->DrawLine(Vec3(renderEntityData.screenRect[0] * scale, renderEntityData.screenRect[1] * scale, 0), ColorB(255,255,255),Vec3(renderEntityData.screenRect[0] * scale, renderEntityData.screenRect[3] * scale, 0), ColorB(255,255,255), 1.5f);
				pAuxRenderer->DrawLine(Vec3(renderEntityData.screenRect[0] * scale, renderEntityData.screenRect[3] * scale, 0), ColorB(255,255,255),Vec3(renderEntityData.screenRect[2] * scale, renderEntityData.screenRect[3] * scale, 0), ColorB(255,255,255), 1.5f);
				pAuxRenderer->DrawLine(Vec3(renderEntityData.screenRect[2] * scale, renderEntityData.screenRect[1] * scale, 0), ColorB(255,255,255),Vec3(renderEntityData.screenRect[2] * scale, renderEntityData.screenRect[3] * scale, 0), ColorB(255,255,255), 1.5f);
			}
		}
	}
}//-------------------------------------------------------------------------------------------------
#endif
