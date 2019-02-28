// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ModelMesh.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include "Model.h"

#ifdef EDITOR_PCDEBUGCODE
extern float g_YLine;

//--------------------------------------------------------------------
//---              hex-dump of model                               ---
//--------------------------------------------------------------------
void CModelMesh::ExportModel(IRenderMesh* pIRenderMesh)
{
	#if 0
	DynArray<Vec3> arrSrcPositions;
	DynArray<Quat> arrSrcQTangents;
	DynArray<Vec2> arrSrcUV;
	DynArray<BoneIndices8> arrSrcBoneIDs;
	DynArray<BoneWeights8> arrSrcVWeights;

	uint32 numExtIndices = pIRenderMesh->GetIndicesCount();
	uint32 numExtVertices = pIRenderMesh->GetVerticesCount();
	uint32 vsize = m_arrSrcPositions.size();
	if (vsize < numExtVertices)
	{
		arrSrcPositions.resize(numExtVertices);
		arrSrcQTangents.resize(numExtVertices);
		arrSrcVWeights.resize(numExtVertices);
		arrSrcUV.resize(numExtVertices);
	}

	pIRenderMesh->LockForThreadAccess();
	vtx_idx* pIndices = pIRenderMesh->GetIndexPtr(FSL_READ);
	int32 nPositionStride;
	uint8* pPositions = pIRenderMesh->GetPosPtr(nPositionStride, FSL_READ);
	if (pPositions == 0)
		return;
	int32 nQTangentStride;
	uint8* pQTangents = pIRenderMesh->GetQTangentPtr(nQTangentStride, FSL_READ);
	if (pQTangents == 0)
		return;
	int32 nUVStride;
	uint8* pUV = pIRenderMesh->GetUVPtr(nUVStride, FSL_READ);
	if (pUV == 0)
		return;
	int32 nSkinningStride;
	uint8* pSkinningInfo = pIRenderMesh->GetHWSkinPtr(nSkinningStride, FSL_READ);    //pointer to weights and bone-id
	if (pSkinningInfo == 0)
		return;
	++m_iThreadMeshAccessCounter;

	//convert hw-buffer into sw-buffer
	for (uint32 i = 0; i < numExtVertices; ++i)
	{
		Vec3 wpos = *((Vec3*) (pPositions + i * nPositionStride));
		SPipQTangents QTangent = *(SPipQTangents*)(pQTangents + i * nQTangentStride);
		Vec2 uv = *((Vec2*) (pUV + i * nUVStride));
		ColorB weights = *(ColorB*)&((SVF_W4B_I4B*)(pSkinningInfo + i * nSkinningStride))->weights;

		arrSrcPositions[i] = wpos;

		arrSrcQTangents[i] = QTangent.GetQ();
		assert(m_arrSrcQTangents[i].IsUnit());

		arrSrcVWeights[i].w0 = weights[0] / 255.0f;
		arrSrcVWeights[i].w1 = weights[1] / 255.0f;
		arrSrcVWeights[i].w2 = weights[2] / 255.0f;
		arrSrcVWeights[i].w3 = weights[3] / 255.0f;

		arrSrcUV[i].x = uv.x;
		arrSrcUV[i].y = uv.y;
	}

	//initialize bone indices
	if (m_arrSrcBoneIDs.size() != numExtVertices)
	{
		m_arrSrcBoneIDs.resize(numExtVertices);
		memset(&m_arrSrcBoneIDs[0], 0, numExtVertices * sizeof(BoneIndices8));

		uint32 numSubsets = m_arrRenderChunks.size();
		for (uint32 s = 0; s < numSubsets; s++)
		{
			uint32 startIndex = m_arrRenderChunks[s].m_nFirstIndexId;
			uint32 endIndex = m_arrRenderChunks[s].m_nFirstIndexId + m_arrRenderChunks[s].m_nNumIndices;
			for (uint32 idx = startIndex; idx < endIndex; ++idx)
			{
				uint32 e = pIndices[idx];
				ColorB hwBoneIDs = *(ColorB*)&((SVF_W4B_I4B*)(pSkinningInfo + e * nSkinningStride))->indices;
				ColorB hwWeights = *(ColorB*)&((SVF_W4B_I4B*)(pSkinningInfo + e * nSkinningStride))->weights;
				uint32 _id0 = hwBoneIDs[0];
				uint32 _id1 = hwBoneIDs[1];
				uint32 _id2 = hwBoneIDs[2];
				uint32 _id3 = hwBoneIDs[3];
				if (hwWeights[0])
					arrSrcBoneIDs[e].idx0 = _id0;
				if (hwWeights[1])
					arrSrcBoneIDs[e].idx1 = _id1;
				if (hwWeights[2])
					arrSrcBoneIDs[e].idx2 = _id2;
				if (hwWeights[3])
					arrSrcBoneIDs[e].idx3 = _id3;
			}
		}
	}

	//--------------------------------------------------------------------------------
	//-- dump all vertices
	//--------------------------------------------------------------------------------
	FILE* vstream = fopen("c:\\VBuffer.txt", "w+b");
	for (uint32 v = 0; v < numExtVertices; v++)
	{
		fprintf(vstream, " {%15.10ff,%15.10ff,%15.10ff,   %15.10ff,%15.10ff,%15.10ff,%15.10ff,   %15.10ff,%15.10ff }, //%04x", arrSrcPositions[v].x, arrSrcPositions[v].y, arrSrcPositions[v].z, arrSrcQTangents[v].v.x, arrSrcQTangents[v].v.y, arrSrcQTangents[v].v.z, arrSrcQTangents[v].w, arrSrcUV[v].x, arrSrcUV[v].y, v);
		fprintf(vstream, "\r\n");
	}
	fprintf(vstream, "\r\n\r\n");
	fclose(vstream);

	//--------------------------------------------------------------------------------
	//--   dump all indices
	//--------------------------------------------------------------------------------
	FILE* istream = fopen("c:\\IBuffer.txt", "w+b");
	for (uint32 f = 0; f < numExtIndices; f = f + 3)
	{
		fprintf(istream, "0x%04x,0x%04x,0x%04x, //0x%08x", pIndices[f + 0], pIndices[f + 1], pIndices[f + 2], f / 3);
		fprintf(istream, "\r\n");
	}
	fprintf(istream, "\r\n\r\n");
	fclose(istream);

	//--------------------------------------------------------------------------------
	//--   dump all subsets
	//--------------------------------------------------------------------------------
	/*uint32 numSubsets = m_arrSubsets.size();
	   FILE* sstream = fopen( "c:\\Subsets.txt", "w+b" );
	   for (uint32 s=0; s<numSubsets; s++)
	   {
	   fprintf(sstream, "0x%08x,0x%08x, 0x%08x,0x%08x,  0x%04x, //0x%08x",  m_arrSubsets[s].nFirstVertId,m_arrSubsets[s].nNumVerts, m_arrSubsets[s].nFirstIndexId,m_arrSubsets[s].nNumIndices/3,   m_arrSubsets[s].nMatID, s);
	   fprintf(vstream, "\r\n");
	   }
	   fprintf(sstream, "\r\n\r\n");
	   fclose(sstream);*/

	pIRenderMesh->UnLockForThreadAccess();
	--m_iThreadMeshAccessCounter;
	if (m_iThreadMeshAccessCounter == 0)
	{
		pIRenderMesh->UnlockStream(VSF_GENERAL);
		pIRenderMesh->UnlockStream(VSF_TANGENTS);
		pIRenderMesh->UnlockStream(VSF_HWSKIN_INFO);
	}
	#endif
}

void CModelMesh::DrawWireframeStatic(const Matrix34& rRenderMat34, uint32 color)
{
	if (m_pIRenderMesh == 0)
		return;

	uint32 numExtIndices = m_pIRenderMesh->GetIndicesCount();
	uint32 numExtVertices = m_pIRenderMesh->GetVerticesCount();
	assert(numExtVertices);

	static std::vector<Vec3> arrExtSkinnedStream;
	uint32 vsize = arrExtSkinnedStream.size();
	if (vsize < numExtVertices)
		arrExtSkinnedStream.resize(numExtVertices);

	m_pIRenderMesh->LockForThreadAccess();
	uint32 numIndices = m_pIRenderMesh->GetIndicesCount();
	vtx_idx* pIndices = m_pIRenderMesh->GetIndexPtr(FSL_READ);
	int32 nPositionStride;
	uint8* pPositions = m_pIRenderMesh->GetPosPtr(nPositionStride, FSL_READ);
	if (pPositions == 0)
		return;
	++m_iThreadMeshAccessCounter;

	for (uint32 e = 0; e < numExtVertices; e++)
	{
		Vec3 v = *(Vec3*)(pPositions + e * nPositionStride);
		arrExtSkinnedStream[e] = rRenderMat34 * (v + m_vRenderMeshOffset);
	}

	m_pIRenderMesh->UnLockForThreadAccess();
	--m_iThreadMeshAccessCounter;
	if (m_iThreadMeshAccessCounter == 0)
	{
		m_pIRenderMesh->UnlockStream(VSF_GENERAL);
		m_pIRenderMesh->UnlockStream(VSF_TANGENTS);
		m_pIRenderMesh->UnlockStream(VSF_HWSKIN_INFO);
	}

	SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
	renderFlags.SetFillMode(e_FillModeWireframe);
	renderFlags.SetDrawInFrontMode(e_DrawInFrontOn);
	renderFlags.SetAlphaBlendMode(e_AlphaAdditive);
	g_pAuxGeom->SetRenderFlags(renderFlags);
	g_pAuxGeom->DrawTriangles(&arrExtSkinnedStream[0], numExtVertices, pIndices, numExtIndices, color);
}

#endif

void CModelMesh::DrawDebugInfo(CDefaultSkeleton* pCSkel, int nLOD, const Matrix34& rRenderMat34, int DebugMode, IMaterial* pMaterial, CRenderObject* pObj, const SRendParams& RendParams, bool isGeneralPass, IRenderNode* pRenderNode, const AABB& aabb,const SRenderingPassInfo &passInfo)
{
	if (m_pIRenderMesh == 0)
		return;

	const float cvar_e_debugDrawMaxDistance = gEnv->pConsole->GetCVar("e_DebugDrawMaxDistance")->GetFVal();
	if (pObj->m_fDistance > cvar_e_debugDrawMaxDistance)
		return;

	bool bNoText = DebugMode < 0;

	const SFrameLodInfo frameLodInfo = gEnv->p3DEngine->GetFrameLodInfo();

	int32 numLODs = frameLodInfo.nMaxLod;
	int index = 0;
	float color[4] = { 1, 1, 1, 1 };

	int nTris = m_pIRenderMesh->GetVerticesCount();
	int nMats = m_pIRenderMesh->GetChunks().size();
	int nRenderMats = 0;

	string shortName = PathUtil::GetFile(pCSkel->GetModelFilePath());

	IRenderAuxGeom* pAuxGeom = g_pIRenderer->GetIRenderAuxGeom();

	// Convert "camera space" to "world space"
	Matrix34 tm = rRenderMat34;	
	if (pObj->m_ObjFlags & FOB_NEAREST)
	{
		tm.AddTranslation(passInfo.GetCamera().GetPosition());
	}
	Vec3 trans = tm.GetTranslation();

	if (nMats)
	{
		for (int i = 0; i < nMats; ++i)
		{
			CRenderChunk& rc = m_pIRenderMesh->GetChunks()[i];
			if (rc.pRE && rc.nNumIndices && rc.nNumVerts && ((rc.m_nMatFlags & MTL_FLAG_NODRAW) == 0))
				++nRenderMats;
		}
	}

	switch (DebugMode)
	{
	case 1:
		{
			IRenderAuxText::DrawLabelExF(trans, 1.3f, color, true, true, "%s\n%d LOD(%i\\%i)", shortName.c_str(), nTris, nLOD + 1, numLODs);
			pAuxGeom->DrawAABB(pCSkel->m_ModelAABB, rRenderMat34, false, ColorB(0, 255, 255, 128), eBBD_Faceted);
		}
		break;
	case 2:
		{

			IMaterialManager* pMatMan = g_pI3DEngine->GetMaterialManager();
			int fMult = 1;
			//int nTris = m_pDefaultSkeleton->GetRenderMesh(nLOD)->GetSysVertCount();
			ColorB clr = ColorB(255, 255, 255, 255);
			if (nTris >= 20000 * fMult)
				clr = ColorB(255, 0, 0, 255);
			else if (nTris >= 10000 * fMult)
				clr = ColorB(255, 255, 0, 255);
			else if (nTris >= 5000 * fMult)
				clr = ColorB(0, 255, 0, 255);
			else if (nTris >= 2500 * fMult)
				clr = ColorB(0, 255, 255, 255);
			else if (nTris > 1250 * fMult)
				clr = ColorB(0, 0, 255, 255);
			else
				clr = ColorB(nTris / 10, nTris / 10, nTris / 10, 255);

			pObj->m_nMaterialLayers = 0;
			pObj->m_ObjFlags |= FOB_SELECTED;
			pObj->m_data.m_nHUDSilhouetteParams = RGBA8(255.0f, clr.b, clr.g, clr.r);

			if (!bNoText)
				IRenderAuxText::DrawLabelExF(trans, 1.3f, color, true, true, "%d", nTris);

		}
		break;
	case 3:
		// Do nothing here. 
		// Environment and character debug info has been split.
		// 24 is for characters
		break;

	case 4:
		if (m_pIRenderMesh)
		{
			int nTexMemUsage = m_pIRenderMesh->GetTextureMemoryUsage(pMaterial);
			IRenderAuxText::DrawLabelExF(trans, 1.3f, color, true, true, "%d", nTexMemUsage / 1024);
		}

		break;

	case 5:
		{
			ColorB clr;
			if (nRenderMats == 1)
				clr = ColorB(0, 0, 255, 255);
			else if (nRenderMats == 2)
				clr = ColorB(0, 255, 255, 255);
			else if (nRenderMats == 3)
				clr = ColorB(0, 255, 0, 255);
			else if (nRenderMats == 4)
				clr = ColorB(255, 0, 255, 255);
			else if (nRenderMats == 5)
				clr = ColorB(255, 255, 0, 255);
			else if (nRenderMats >= 6)
				clr = ColorB(255, 0, 0, 255);
			else if (nRenderMats >= 11)
				clr = ColorB(255, 255, 255, 255);

			pObj->m_nMaterialLayers = 0;
			pObj->m_ObjFlags |= FOB_SELECTED;
			pObj->m_data.m_nHUDSilhouetteParams = RGBA8(127.0f, clr.b, clr.g, clr.r);


			if (!bNoText)
				IRenderAuxText::DrawLabelExF(trans, 1.3f, color, true, true, "%d", nRenderMats);
		}
		break;

	case 6:
		{
		IRenderAuxText::DrawLabelExF(trans, 1.3f, color, true, true, "%d,%d,%d,%d",
			                          (int)(RendParams.AmbientColor.r * 255.0f), (int)(RendParams.AmbientColor.g * 255.0f), (int)(RendParams.AmbientColor.b * 255.0f), (int)(RendParams.AmbientColor.a * 255.0f)
			                          );
		}
		break;

	case 7:
		if (m_pIRenderMesh)
		{
			int nTexMemUsage = m_pIRenderMesh->GetTextureMemoryUsage(pMaterial);
			IRenderAuxText::DrawLabelExF(trans, 1.3f, color, true, true, "%d,%d,%d", nTris, nRenderMats, nTexMemUsage / 1024);
		}
		break;
	case 21:
		if (m_pIRenderMesh)
		{
			AABB bbox;
			m_pIRenderMesh->GetBBox(bbox.min, bbox.max);
			bbox.SetTransformedAABB(rRenderMat34, bbox);
			trans = (bbox.max + bbox.min) / 2;
			Vec3 pos = g_pI3DEngine->GetRenderingCamera().GetPosition();
			float fEntDistance = sqrt_tpl(Distance::Point_AABBSq(pos, bbox)); // activate objects before they get really visible
			IRenderAuxText::DrawLabelExF(trans, 1.3f, color, true, true, "%.2f", fEntDistance);
		}
		break;

	case -21:
		if (m_pIRenderMesh)
		{
			AABB bbox;
			m_pIRenderMesh->GetBBox(bbox.min, bbox.max);
			bbox.SetTransformedAABB(rRenderMat34, bbox);
			trans = (bbox.max + bbox.min) / 2;
			Vec3 pos = g_pI3DEngine->GetRenderingCamera().GetPosition();
			float fEntDistance = sqrt_tpl(Distance::Point_AABBSq(pos, bbox)); // activate objects before they get really visible
			IRenderAuxText::DrawLabelExF(trans, 1.3f, color, true, true, "%.2f (%s)", fEntDistance, m_pIRenderMesh->GetSourceName());
		}
		break;

	case 10:
		if (m_pIRenderMesh)
		{
			SGeometryDebugDrawInfo dd;
			dd.tm = *RendParams.pMatrix;
			m_pIRenderMesh->DebugDraw(dd);
		}
		break;

	case 19:  // Displays the triangle count of physic proxies.
		if (!bNoText)
		{
			int nPhysTrisCount = 0;
			const phys_geometry* pgeom;
			for (int i = pCSkel->GetJointCount() - 1; i >= 0; --i)
			{
				if (pgeom = pCSkel->GetJointPhysGeom((uint32)i))
					nPhysTrisCount += pgeom->pGeom->GetPrimitiveCount();
			}

			if (nPhysTrisCount == 0)
				color[3] = 0.1f;

			IRenderAuxText::DrawLabelExF(trans, 1.3f, color, true, true, "%d", nPhysTrisCount);
		}
		break;

	case 24:
	{
		//////////////////////////////////////////////////////////////////////////
		// Show Lods for characters
		//////////////////////////////////////////////////////////////////////////
		
		if (!bNoText && isGeneralPass)
		{
			bool bLodFaceArea = false;
			float distances[SMeshLodInfo::s_nMaxLodCount];
			stack_string dist;
			if (pRenderNode)
			{
				bLodFaceArea = gEnv->pConsole->GetCVar("e_LodFaceArea")->GetIVal() != 0;
				if (bLodFaceArea)
				{
					bLodFaceArea = pRenderNode->GetLodDistances(frameLodInfo, distances);
				}
				if (bLodFaceArea)
				{
					for (uint32 i = 0; i < SMeshLodInfo::s_nMaxLodCount; ++i)
					{
						dist += stack_string().Format(",%d:%.1f", i, distances[i]);
					}
				}
			}

			// Change color based on LOD
			ColorB clr;
			switch (nLOD)
			{
			case 1:  { clr = ColorF(1.0f, 0.0f, 0.0f, 1.0f); } break;
			case 2:  { clr = ColorF(0.0f, 1.0f, 0.0f, 1.0f); } break;
			case 3:  { clr = ColorF(0.0f, 0.0f, 1.0f, 1.0f); } break;
			case 4:  { clr = ColorF(0.0f, 1.0f, 1.0f, 1.0f); } break;
			default: { clr = ColorF(0.8f, 0.8f, 0.8f, 1.0f); } break;
			}
			
			pObj->m_nMaterialLayers = 0;
			pObj->m_ObjFlags |= FOB_SELECTED;
			pObj->m_data.m_nHUDSilhouetteParams = RGBA8(255.0f, clr.b, clr.g, clr.r);

			IRenderNode* pRenderNode = RendParams.pRenderNode;
			const float lodRatioNorm = pRenderNode ? pRenderNode->GetLodRatioNormalized() : -1.0f;
			const float lodRatio = pRenderNode ? pRenderNode->GetLodRatio() : -1.0f;
			const int   viewDistRatio = pRenderNode ? pRenderNode->GetViewDistRatioVal() : -1;

			pAuxGeom->DrawAABB(pCSkel->m_ModelAABB, rRenderMat34, false, ColorB(0, 255, 255, 128), eBBD_Faceted);

			float fEntDistance = -1.f;
			string sourceName = "<Unknown>";
			if (m_pIRenderMesh)
			{
				AABB bbox;
				m_pIRenderMesh->GetBBox(bbox.min, bbox.max);
				bbox.SetTransformedAABB(rRenderMat34, bbox);
				trans = bbox.GetCenter();
				const Vec3 vCamPos = g_pI3DEngine->GetRenderingCamera().GetPosition();
				fEntDistance = sqrt_tpl(Distance::Point_AABBSq(vCamPos, bbox));

				sourceName = PathUtil::GetFile(m_pIRenderMesh->GetSourceName());
			}
			
			if (lodRatio >= 0.0f)
				IRenderAuxText::DrawLabelExF(trans, 1.3f, clr, true, true, "%s\nLod[%d]vdr[%d]lr[%.1f/%.1f]\nD[%s]\nCamDist:%.2f / %.2f", sourceName.c_str(), nLOD, viewDistRatio, lodRatio, lodRatioNorm, !dist.empty() ? dist.c_str() + 1 : "-", fEntDistance, RendParams.fDistance);
			else
				IRenderAuxText::DrawLabelExF(trans, 1.3f, clr, true, true, "%s\nLod[%d]vdr[%d] LodRatio=0", sourceName.c_str(), nLOD, viewDistRatio);
		}
	}
	break;

	default:
		break;
	}

#ifndef _RELEASE
	if (isGeneralPass && gEnv->p3DEngine->IsDebugDrawListEnabled())
	{
		I3DEngine::SObjectInfoToAddToDebugDrawList objectInfo;
		objectInfo.pFileName = pCSkel->GetModelFilePath();
		objectInfo.pName = pRenderNode ? pRenderNode->GetName() : "";
		objectInfo.pClassName = pRenderNode ? pRenderNode->GetEntityClassName() : "";
		objectInfo.texMemory = m_pIRenderMesh->GetTextureMemoryUsage(pMaterial);
		objectInfo.numTris = nTris;
		objectInfo.numVerts = nTris;
		objectInfo.meshMemory = m_pIRenderMesh->GetMemoryUsage(NULL, IRenderMesh::MEM_USAGE_COMBINED);
		objectInfo.pBox = &aabb;

		objectInfo.pMat = &rRenderMat34;
		objectInfo.type = I3DEngine::DLOT_CHARACTER;
		objectInfo.pRenderNode = pRenderNode;
		gEnv->p3DEngine->AddObjToDebugDrawList(objectInfo);
	}
#endif
}
