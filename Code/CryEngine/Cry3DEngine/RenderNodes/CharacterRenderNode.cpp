// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CharacterRenderNode.h"

#include "ObjMan.h"
#include "3dEngine.h"
#include <CryAnimation/ICryAnimation.h>

//////////////////////////////////////////////////////////////////////////
static bool IsMatrixValid(const Matrix34& mat)
{
	Vec3 vScaleTest = mat.TransformVector(Vec3(0, 0, 1));
	float fDist = mat.GetTranslation().GetDistance(Vec3(0, 0, 0));

	if (vScaleTest.GetLength() > 1000.f || vScaleTest.GetLength() < 0.01f || fDist > 256000 ||
	    !_finite(vScaleTest.x) || !_finite(vScaleTest.y) || !_finite(vScaleTest.z))
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
CCharacterRenderNode::CCharacterRenderNode()
	: m_pCharacterInstance(nullptr)
	, m_pPhysicsEntity(nullptr)
{
	m_cachedBoundsWorld = AABB(0.0f);
	m_cachedBoundsLocal = AABB(0.0f);
	m_matrix.SetIdentity();

	GetInstCount(GetRenderNodeType())++;
}

//////////////////////////////////////////////////////////////////////////
CCharacterRenderNode::~CCharacterRenderNode()
{
	Dephysicalize();
	Get3DEngine()->FreeRenderNodeState(this);

	m_pCharacterInstance = nullptr;
	m_pPhysicsEntity = nullptr;

	if (m_pCameraSpacePos)
	{
		delete m_pCameraSpacePos;
		m_pCameraSpacePos = nullptr;
	}

	Cry3DEngineBase::GetInstCount(GetRenderNodeType())--;
}

//////////////////////////////////////////////////////////////////////////
const char* CCharacterRenderNode::GetName() const
{
	if (m_pCharacterInstance)
		return m_pCharacterInstance->GetFilePath();
	return "Unknown";
}

//////////////////////////////////////////////////////////////////////////
void CCharacterRenderNode::Render(const SRendParams& inputRendParams, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!m_pCharacterInstance || m_dwRndFlags & ERF_HIDDEN)
		return;

	// some parameters will be modified
	SRendParams rParms(inputRendParams);

	rParms.pRenderNode = this;
	rParms.nMaterialLayers = m_nMaterialLayers;
	rParms.pMatrix = &m_matrix;
	rParms.pMaterial = m_pMaterial;

	rParms.dwFObjFlags |= FOB_TRANS_MASK | FOB_DYNAMIC_OBJECT;
	rParms.dwFObjFlags |= (GetRndFlags() & ERF_FOB_RENDER_AFTER_POSTPROCESSING) ? FOB_RENDER_AFTER_POSTPROCESSING : 0;

	rParms.nHUDSilhouettesParams = m_nHUDSilhouettesParam;
	
	if (GetRndFlags() & ERF_FOB_NEAREST)
	{
		if (passInfo.IsRecursivePass()) // Nearest objects are not rendered in the recursive passes.
			return;

		rParms.dwFObjFlags |= FOB_NEAREST;

		// Nearest objects recalculate instance matrix every frame
		//m_bPermanentRenderObjectMatrixValid = false;
		
		auto nearestMatrix = m_matrix;	
		CalcNearestTransform(nearestMatrix, passInfo);
		rParms.pNearestMatrix = &nearestMatrix;

		m_pCharacterInstance->Render(rParms, passInfo);
	}
	else
	{
		m_pCharacterInstance->Render(rParms, passInfo);
	}
}

void CCharacterRenderNode::SetMatrix(const Matrix34& transform)
{
	if (!m_pCharacterInstance)
		return;

	if (!IsMatrixValid(transform))
	{
		const Vec3 pos = transform.GetTranslation();
		stack_string message;
		message.Format("Error: IRenderNode::SetMatrix: Invalid matrix ignored (name=\"%s\", position=[%.2f,%.2f,%.2f])", GetName(), pos.x, pos.y, pos.z);
		Warning(message.c_str());
		return;
	}

	if (m_matrix == transform)
		return;

	m_matrix = transform;

	m_cachedBoundsLocal = m_cachedBoundsWorld = AABB(0.0f);

	Get3DEngine()->UnRegisterEntityAsJob(this);
	Get3DEngine()->RegisterEntity(this);

	m_pCharacterInstance->SetAttachmentLocation_DEPRECATED(QuatTS(transform));
}

void CCharacterRenderNode::GetLocalBounds(AABB& bbox)
{
	if (!m_pCharacterInstance)
	{
		return;
	}

	bbox = m_pCharacterInstance->GetAABB();
}

const AABB CCharacterRenderNode::GetBBox() const
{
	if (!m_pCharacterInstance)
	{
		return AABB(AABB::RESET);
	}

	AABB boundsLocal = m_pCharacterInstance->GetAABB();

	if (IsEquivalent(boundsLocal, m_cachedBoundsLocal))
		return m_cachedBoundsWorld;

	m_cachedBoundsLocal = boundsLocal;
	m_cachedBoundsWorld = AABB::CreateTransformedAABB(m_matrix, boundsLocal);

	return m_cachedBoundsWorld;
}

//////////////////////////////////////////////////////////////////////////
void CCharacterRenderNode::Physicalize(bool bInstant)
{
	// Not implemented
	assert(0);
}

//////////////////////////////////////////////////////////////////////////
float CCharacterRenderNode::GetMaxViewDist()
{
	if (GetRndFlags() & ERF_FORCE_POST_3D_RENDER)
	{
		// Always want to render models in post 3d render (menus), whatever distance they are
		return FLT_MAX;
	}

	if (GetRndFlags() & ERF_CUSTOM_VIEW_DIST_RATIO)
	{
		const AABB boundsWorld = GetBBox();
		float s = max(max((boundsWorld.max.x - boundsWorld.min.x), (boundsWorld.max.y - boundsWorld.min.y)), (boundsWorld.max.z - boundsWorld.min.z));
		return max(GetCVars()->e_ViewDistMin, s * GetCVars()->e_ViewDistRatioCustom * GetViewDistRatioNormilized());
	}

	AABB boundsLocal;
	GetLocalBounds(boundsLocal);

	if (GetMinSpecFromRenderNodeFlags(m_dwRndFlags) == CONFIG_DETAIL_SPEC)
		return max(GetCVars()->e_ViewDistMin, min(GetFloatCVar(e_ViewDistCompMaxSize), boundsLocal.GetRadius()) * GetCVars()->e_ViewDistRatioDetail * GetViewDistRatioNormilized());

	return max(GetCVars()->e_ViewDistMin, min(GetFloatCVar(e_ViewDistCompMaxSize), boundsLocal.GetRadius()) * GetCVars()->e_ViewDistRatio * GetViewDistRatioNormilized());
}

//////////////////////////////////////////////////////////////////////////
CLodValue CCharacterRenderNode::ComputeLod(int wantedLod, const SRenderingPassInfo& passInfo)
{
	if (m_pCharacterInstance)
	{
		return m_pCharacterInstance->ComputeLod(wantedLod, passInfo);
	}

	return CLodValue(wantedLod);
}

//////////////////////////////////////////////////////////////////////////
bool CCharacterRenderNode::GetLodDistances(const SFrameLodInfo& frameLodInfo, float* distances) const
{
	const float fEntityLodRatio = GetLodRatioNormalized();
	if (fEntityLodRatio > 0.0f)
	{
		float fLodDistance = FLT_MAX;
		if (m_pCharacterInstance)
		{
			fLodDistance = sqrt(m_pCharacterInstance->ComputeGeometricMean().fGeometricMean);
		}
		const float fDistMultiplier = 1.0f / (fEntityLodRatio * frameLodInfo.fTargetSize);

		for (uint i = 0; i < SMeshLodInfo::s_nMaxLodCount; ++i)
		{
			distances[i] = fLodDistance * (i + 1) * fDistMultiplier;
		}
	}
	else
	{
		for (uint i = 0; i < SMeshLodInfo::s_nMaxLodCount; ++i)
		{
			distances[i] = FLT_MAX;
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CCharacterRenderNode::OnRenderNodeVisible( bool bBecomeVisible )
{
	if (bBecomeVisible)
	{
		if (m_pCharacterInstance)
		{
			m_pCharacterInstance->SetFlags(m_pCharacterInstance->GetFlags() | CS_FLAG_RENDER_NODE_VISIBLE);
		}
	}
	else
	{
		if (m_pCharacterInstance)
		{
			m_pCharacterInstance->SetFlags(m_pCharacterInstance->GetFlags() & ~CS_FLAG_RENDER_NODE_VISIBLE);
			m_pCharacterInstance->KillAllSkeletonEffects();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCharacterRenderNode::SetCameraSpacePos(Vec3* pCameraSpacePos)
{
	if (pCameraSpacePos)
	{
		if (!m_pCameraSpacePos)
			m_pCameraSpacePos = new Vec3;
		*m_pCameraSpacePos = *pCameraSpacePos;
	}
	else
	{
		delete m_pCameraSpacePos;
		m_pCameraSpacePos = nullptr;
	}
}
//////////////////////////////////////////////////////////////////////////
void CCharacterRenderNode::SetCharacter(ICharacterInstance* pCharacter)
{
	if (m_pCharacterInstance != pCharacter)
	{
		m_pCharacterInstance = pCharacter;
		InvalidatePermanentRenderObject();
	}
}

//////////////////////////////////////////////////////////////////////////
void CCharacterRenderNode::OffsetPosition(const Vec3& delta)
{
	if (const auto pTempData = m_pTempData.load()) pTempData->OffsetPosition(delta);
	m_matrix.SetTranslation(m_matrix.GetTranslation() + delta);
	m_cachedBoundsLocal = m_cachedBoundsWorld = AABB(0.0f);
}

//////////////////////////////////////////////////////////////////////////
void CCharacterRenderNode::CalcNearestTransform(Matrix34& transformMatrix, const SRenderingPassInfo& passInfo)
{
	// Camera space
	if (m_pCameraSpacePos)
	{
		// Use camera space relative position
		const Matrix33 cameraRotation = Matrix33(passInfo.GetCamera().GetViewMatrix());
		transformMatrix.SetTranslation(*m_pCameraSpacePos * cameraRotation);
	}
	else
	{
		// We don't have camera space relative position, so calculate it out from world space
		// (This will not have the precision advantages of camera space rendering)
		transformMatrix.AddTranslation(-passInfo.GetCamera().GetPosition());
	}
}

namespace
{
void CollectRenderMeshMaterials(IMaterial* pMaterial, IRenderMesh* pRenderMesh, std::vector<std::pair<IMaterial*, float>>& collectedMaterials)
{
	if (!pMaterial || !pRenderMesh)
	{
		return;
	}

	stl::push_back_unique(collectedMaterials, std::pair<IMaterial*, float>(pMaterial, 1.0f));

	TRenderChunkArray& chunks = pRenderMesh->GetChunks();
	const uint subMtlCount = pMaterial->GetSubMtlCount();

	const uint numChunks = chunks.size();
	for (uint i = 0; i < numChunks; ++i)
	{
		CRenderChunk& chunk = chunks[i];
		if (chunk.nNumIndices > 0 && chunk.nNumVerts > 0 && chunk.m_nMatID < subMtlCount)
		{
			stl::push_back_unique(collectedMaterials, std::pair<IMaterial*, float>(pMaterial->GetSubMtl(chunk.m_nMatID), chunk.m_texelAreaDensity));
		}
	}
}
}

void CCharacterRenderNode::PrecacheCharacterCollect(const float fImportance, ICharacterInstance* pCharacter, IMaterial* pSlotMat, const Matrix34& matParent,
                                                    const float fEntDistance, const float fScale, int nMaxDepth, bool bFullUpdate, bool bDrawNear, int nLod,
                                                    const int nRoundId, std::vector<std::pair<IMaterial*, float>>& collectedMaterials)
{
	if (!nMaxDepth || !pCharacter)
		return;
	nMaxDepth--;

	static ICVar* pSkipVertexAnimationLOD = gEnv->pConsole->GetCVar("ca_vaSkipVertexAnimationLOD");
	const bool bSkipVertexAnimationLOD = pSkipVertexAnimationLOD != NULL ? pSkipVertexAnimationLOD->GetIVal() != 0 : false;

	int minLod = 0;
	if (bSkipVertexAnimationLOD && pCharacter->HasVertexAnimation())
	{
		minLod = max(minLod, 1);
		nLod = 1;
	}

	SFrameLodInfo lodParam = gEnv->p3DEngine->GetFrameLodInfo();

	IAttachmentManager* pAttMan = pCharacter->GetIAttachmentManager();
	ICharacterInstance* pCharInstance = pAttMan->GetSkelInstance();
	if (pCharInstance && pCharInstance != pCharacter)
	{
		PrecacheCharacterCollect(fImportance, pCharInstance, pSlotMat, matParent, fEntDistance, fScale, nMaxDepth, bFullUpdate, bDrawNear, nLod, nRoundId, collectedMaterials);
	}

	int nCount = pAttMan->GetAttachmentCount();
	for (int i = 0; i < nCount; i++)
	{
		if (IAttachment* pAtt = pAttMan->GetInterfaceByIndex(i))
		{
			IAttachmentObject* pIAttachmentObject = pAtt->GetIAttachmentObject();
			if (pIAttachmentObject)
			{
				IAttachmentSkin* pIAttachmentSkin = pIAttachmentObject->GetIAttachmentSkin();
				if (pIAttachmentSkin)
				{
					ISkin* pISkin = pIAttachmentSkin->GetISkin();

					const int maxLod = (int)pISkin->GetNumLODs() - 1;

					const int minPrecacheLod = clamp_tpl(nLod - 1, minLod, maxLod);
					const int maxPrecacheLod = clamp_tpl(nLod + 1, minLod, maxLod);

					IMaterial* pAttMatOverride = (IMaterial*)pIAttachmentObject->GetReplacementMaterial();
					IMaterial* pAttMat = pAttMatOverride ? pAttMatOverride : (IMaterial*)pIAttachmentObject->GetBaseMaterial();

					for (int currentLod = minPrecacheLod; currentLod <= maxPrecacheLod; ++currentLod)
					{
						pISkin->PrecacheMesh(bFullUpdate, nRoundId, currentLod);

						IRenderMesh* pRenderMesh = pISkin->GetIRenderMesh(currentLod); //get the baseLOD
						IMaterial* pCharObjMat = pISkin->GetIMaterial(currentLod);

						CollectRenderMeshMaterials(pAttMat, pRenderMesh, collectedMaterials);
						if (pCharObjMat != pAttMat)
						{
							CollectRenderMeshMaterials(pCharObjMat, pRenderMesh, collectedMaterials);
						}
					}

					continue;
				}

				CStatObj* pStatObj = (CStatObj*)pIAttachmentObject->GetIStatObj();
				if (pStatObj)
				{
					if (!pStatObj || pStatObj->GetFlags() & STATIC_OBJECT_HIDDEN)
						continue;

					const int minLod = pStatObj->GetMinUsableLod();
					const int maxLod = (int)pStatObj->m_nMaxUsableLod;
					const int minPrecacheLod = clamp_tpl(nLod - 1, minLod, maxLod);
					const int maxPrecacheLod = clamp_tpl(nLod + 1, minLod, maxLod);

					const QuatT& q = pAtt->GetAttAbsoluteDefault();
					Matrix34A tm34 = matParent * Matrix34(q);

					for (int currentLod = minPrecacheLod; currentLod <= maxPrecacheLod; ++currentLod)
					{
						pStatObj->UpdateStreamableComponents(fImportance, tm34, bFullUpdate, currentLod);

						pStatObj = (CStatObj*)pStatObj->GetLodObject(currentLod, true);
						IMaterial* pAttMatOverride = (IMaterial*)pIAttachmentObject->GetReplacementMaterial();
						IMaterial* pAttMat = pAttMatOverride ? pAttMatOverride : (IMaterial*)pIAttachmentObject->GetBaseMaterial();
						IMaterial* pMaterial = pAttMat ? pAttMat : pStatObj->GetMaterial();

						CollectRenderMeshMaterials(pMaterial, pStatObj->GetRenderMesh(), collectedMaterials);
					}

					continue;
				}

				ICharacterInstance* pSkelInstance = pIAttachmentObject->GetICharacterInstance();
				if (pSkelInstance)
				{
					IMaterial* pAttMatOverride = (IMaterial*)pIAttachmentObject->GetReplacementMaterial();
					IMaterial* pAttMat = pAttMatOverride ? pAttMatOverride : (IMaterial*)pIAttachmentObject->GetBaseMaterial();

					PrecacheCharacterCollect(fImportance, pSkelInstance, pAttMat, matParent, fEntDistance, fScale, nMaxDepth, bFullUpdate, bDrawNear, nLod, nRoundId, collectedMaterials);
					continue;
				}
			}
		}
	}

	IMaterial* pCharObjMat = pCharacter->GetIMaterial();

	IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
	rIDefaultSkeleton.PrecacheMesh(bFullUpdate, nRoundId, nLod);

	IRenderMesh* pIRenderMesh = rIDefaultSkeleton.GetIRenderMesh(); //get the baseLOD
	CollectRenderMeshMaterials(pSlotMat, pIRenderMesh, collectedMaterials);
	if (pSlotMat != pCharObjMat)
	{
		CollectRenderMeshMaterials(pCharObjMat, pIRenderMesh, collectedMaterials);
	}

	if (pCharInstance->GetObjectType() == CGA)
	{
		// joints
		if (ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose())
		{
			uint32 numJoints = pCharacter->GetIDefaultSkeleton().GetJointCount();

			// check StatObj attachments
			for (uint32 i = 0; i < numJoints; i++)
			{
				CStatObj* pStatObj = (CStatObj*)pSkeletonPose->GetStatObjOnJoint(i);
				if (!pStatObj || pStatObj->GetFlags() & STATIC_OBJECT_HIDDEN)
					continue;

				const int minLod = pStatObj->GetMinUsableLod();
				const int maxLod = (int)pStatObj->m_nMaxUsableLod;
				const int minPrecacheLod = clamp_tpl(nLod - 1, minLod, maxLod);
				const int maxPrecacheLod = clamp_tpl(nLod + 1, minLod, maxLod);

				for (int currentLod = minPrecacheLod; currentLod <= maxPrecacheLod; ++currentLod)
				{
					Matrix34A tm34 = matParent * Matrix34(pSkeletonPose->GetAbsJointByID(i));
					pStatObj->UpdateStreamableComponents(fImportance, tm34, bFullUpdate, currentLod);

					IMaterial* pStatObjMat = pStatObj->GetMaterial();
					IStatObj* pStatObjLod = pStatObj->GetLodObject(currentLod, true);
					GetObjManager()->PrecacheStatObjMaterial(pStatObjMat ? pStatObjMat : pSlotMat, fEntDistance / fScale, pStatObjLod, bFullUpdate, bDrawNear);
				}
			}
		}
	}
}

void CCharacterRenderNode::PrecacheCharacter(const float fImportance, ICharacterInstance* pCharacter, IMaterial* pSlotMat, const Matrix34& matParent,
                                             const float fEntDistance, const float fScale, int nMaxDepth, bool bFullUpdate, bool bDrawNear, int nLod)
{
	const int nRoundId = bFullUpdate
	                     ? GetObjManager()->m_nUpdateStreamingPrioriryRoundIdFast
	                     : GetObjManager()->m_nUpdateStreamingPrioriryRoundId;

	static std::vector<std::pair<IMaterial*, float>> s_collectedMaterials;
	s_collectedMaterials.clear();
	PrecacheCharacterCollect(fImportance, pCharacter, pSlotMat, matParent, fEntDistance, fScale, nMaxDepth, bFullUpdate, bDrawNear, nLod, nRoundId, s_collectedMaterials);

	const uint numMaterials = s_collectedMaterials.size();
	if (numMaterials > 0)
	{
		int nFlags = 0;
		float scaledEntDistance = fEntDistance / fScale;

		if (bDrawNear)
		{
			nFlags |= FPR_HIGHPRIORITY;
		}
		else
		{
			scaledEntDistance = max(GetFloatCVar(e_StreamPredictionMinReportDistance), scaledEntDistance);
		}

		const float fMipFactor = scaledEntDistance * scaledEntDistance;

		for (uint i = 0; i < numMaterials; ++i)
		{
			const std::pair<IMaterial*, float>& collectedMaterial = s_collectedMaterials[i];
			CMatInfo* pMaterial = ((CMatInfo*)collectedMaterial.first);
			const float density = GetCVars()->e_StreamPredictionTexelDensity ? collectedMaterial.second : 1.0f;
			pMaterial->PrecacheTextures(fMipFactor * density, nFlags, bFullUpdate);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCharacterRenderNode::UpdateStreamingPriority(const SUpdateStreamingPriorityContext& streamingContext)
{
	if (!m_pCharacterInstance)
		return;
	if (!streamingContext.pPassInfo)
		return;

	bool bDrawNear = 0 != (GetRndFlags() & ERF_FOB_NEAREST);
	if ((m_pCharacterInstance->GetFlags() & CS_FLAG_STREAM_HIGH_PRIORITY) != 0)
	{
		bDrawNear = true;
	}

	CRY_PROFILE_REGION(PROFILE_3DENGINE, "UpdateObjectsStreamingPriority_PrecacheCharacter");

	const SRenderingPassInfo& passInfo = *streamingContext.pPassInfo;
	// If the object is in camera space, don't use the prediction position.
	const AABB boundsWorld = GetBBox();
	const float fApproximatePrecacheDistance = (bDrawNear)
	                                           ? sqrt_tpl(Distance::Point_AABBSq(passInfo.GetCamera().GetPosition(), boundsWorld))
	                                           : streamingContext.distance;
	float fObjScale = 1.0f;
	PrecacheCharacter(streamingContext.importance, m_pCharacterInstance, m_pMaterial, m_matrix, fApproximatePrecacheDistance, fObjScale, bDrawNear ? 4 : 2, streamingContext.bFullUpdate, bDrawNear, streamingContext.lod);
}
