// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DriverD3D.h"
#include <Cry3DEngine/I3DEngine.h>
#include <CryAnimation/ICryAnimation.h>
#include <Cry3DEngine/IIndexedMesh.h>
#include <CryRenderer/IShader.h>
#include "Common/RenderView.h"

//#define CREATE_RENDERDOC_CAPTURE

#if !CRY_PLATFORM_CONSOLE && !CRY_PLATFORM_MOBILE
class CREBaker : public CRenderElement
{
private:
	CRenderElement*                    m_pSrc;
	std::vector<IIndexedMesh*>       m_pDst;
	CMesh*                           m_pSrcMesh;
	int                              m_nPhase;
	bool                             m_bSmoothNormals;
	const SMeshBakingMaterialParams* m_params;
	int                              m_numParams;

public:
	CREBaker(CRenderElement* pSrc, CMesh* pSrcMesh, std::vector<IIndexedMesh*> pDst, int nPhase, const SMeshBakingMaterialParams* params, int numParams, bool bSmoothNormals)
		: m_pSrc(pSrc)
		, m_pDst(pDst)
		, m_pSrcMesh(pSrcMesh)
		, m_nPhase(nPhase)
		, m_bSmoothNormals(bSmoothNormals)
		, m_params(params)
		, m_numParams(numParams)
	{}

	virtual ~CREBaker() {}
	inline uint32              mfGetFlags(void)                                                                        { return m_pSrc->mfGetFlags(); }
	inline void                mfSetFlags(uint32 fl)                                                                   { m_pSrc->mfSetFlags(fl); }
	inline void                mfUpdateFlags(uint32 fl)                                                                { m_pSrc->mfUpdateFlags(fl); }
	inline void                mfClearFlags(uint32 fl)                                                                 { m_pSrc->mfClearFlags(fl); }
	inline bool                mfCheckUpdate(InputLayoutHandle eVertFormat, int Flags, uint16 nFrame)                  { return m_pSrc->mfCheckUpdate(eVertFormat, Flags, nFrame); }

	virtual CRenderChunk*      mfGetMatInfo()                                                                          { return m_pSrc->mfGetMatInfo(); }
	virtual TRenderChunkArray* mfGetMatInfoList()                                                                      { return m_pSrc->mfGetMatInfoList(); }
	virtual int                mfGetMatId()                                                                            { return m_pSrc->mfGetMatId(); }
	virtual void               mfReset()                                                                               { m_pSrc->mfReset(); }
	virtual bool               mfIsHWSkinned()                                                                         { return m_pSrc->mfIsHWSkinned(); }
	virtual CRenderElement*    mfCopyConstruct(void)                                                                   { return m_pSrc->mfCopyConstruct(); }
	virtual void               mfCenter(Vec3& centr, CRenderObject* pObj, const SRenderingPassInfo& passInfo)          { m_pSrc->mfCenter(centr, pObj, passInfo); }
	virtual void               mfGetBBox(Vec3& vMins, Vec3& vMaxs) const                                               { m_pSrc->mfGetBBox(vMins, vMaxs); }
	virtual void               mfGetPlane(Plane& pl)                                                                   { m_pSrc->mfGetPlane(pl); }

	virtual void*              mfGetPointer(ESrcPointer ePT, int* Stride, EParamType Type, ESrcPointer Dst, int Flags) { return m_pSrc->mfGetPointer(ePT, Stride, Type, Dst, Flags); }

	virtual bool               mfUpdate(InputLayoutHandle eVertFormat, int Flags, bool bTessellation = false)          
	{
		bool bRet = m_pSrc->mfUpdate(eVertFormat, Flags, bTessellation); 
		return bRet;
	}

	virtual void               mfExport(struct SShaderSerializeContext& SC)                                            { m_pSrc->mfExport(SC); }
	virtual int                Size()                                                                                  { return m_pSrc->Size(); }
	virtual void               GetMemoryUsage(ICrySizer* pSizer) const                                                 { m_pSrc->GetMemoryUsage(pSizer); }
};

// OLD PIPELINE
/*
bool CREBaker::mfDraw(CShader* ef, SShaderPass* sfm)
{
	static CCryNameR triPosName("TRI_POS");
	static CCryNameR triBiNormName("TRI_BINORM");
	static CCryNameR triTangName("TRI_TANGENT");
	static CCryNameR triUVName("TRI_UV");
	static CCryNameR triColorName("TRI_COLOR");
	static CCryNameR triZRange("ZOFFSET");
	Vec4 zRange(10.0f, 0.5f, 0.0f, 0.0f);
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	const bool bReverseDepth = true;
	zRange.z = (float)m_nPhase - 0.5f;
	zRange.w = bReverseDepth ? 1.0f : 0.0f;

	if (m_pDst.empty())
	{
		CryLog("BakeMesh: Failed as pOutput is NULL in CREBaker::mfDraw\n");
		return false;
	}

	for (std::vector<IIndexedMesh*>::iterator mit = m_pDst.begin(), mend = m_pDst.end(); mit != mend; ++mit)
	{
		IIndexedMesh* pOutput = *mit;
		if (!pOutput)
			continue;
		int numOutputTriangles = pOutput->GetIndexCount() / 3;
		int numPositions = 0;
		CMesh* pOutputMesh = pOutput->GetMesh();
		Vec3* pOutPos = pOutputMesh->GetStreamPtr<Vec3>(CMesh::POSITIONS, &numPositions);
		SMeshTangents* pOutTangents = pOutputMesh->GetStreamPtr<SMeshTangents>(CMesh::TANGENTS);
		vtx_idx* pOutIndices = pOutputMesh->GetStreamPtr<vtx_idx>(CMesh::INDICES);
		SMeshTexCoord* pOutTexCoords = pOutputMesh->GetStreamPtr<SMeshTexCoord>(CMesh::TEXCOORDS);

		std::vector<Vec3> smoothedNormals;
		if (m_bSmoothNormals)
		{
			smoothedNormals.resize(numPositions);
			for (int pi = 0; pi < numPositions; pi++)
			{
				Vec3 p = pOutPos[pi];
				Vec3 n(ZERO);
				for (int i = 0; i < numOutputTriangles; i++)
				{
					vtx_idx idx0 = pOutIndices[3 * i + 0];
					vtx_idx idx1 = pOutIndices[3 * i + 1];
					vtx_idx idx2 = pOutIndices[3 * i + 2];
					if (pOutPos[idx0] == p || pOutPos[idx1] == p || pOutPos[idx2] == p)
					{
						Vec3 faceN = (pOutPos[idx1] - pOutPos[idx0]).Cross(pOutPos[idx2] - pOutPos[idx0]);
						faceN.NormalizeSafe();
						n += faceN;
					}
				}
				if (n.NormalizeSafe() == 0.0f)
				{
					// Make sure we actually get a valid normal even if everything went wrong
					n = pOutTangents[pi].GetN().normalize();
				}
				smoothedNormals[pi] = n;
			}
		}

		// Allocate and fill per-stream vertex buffers
		TempDynVB<SVF_P3F_T2F_T3F> vb0;
		TempDynVB<SPipTangents> vb1;

		{
			vb0.Allocate(numOutputTriangles * 3);
			SVF_P3F_T2F_T3F* vInputs = vb0.Lock();

			for (int i = numOutputTriangles * 3 - 1; i >= 0; i--)
			{
				SVF_P3F_T2F_T3F& v = vInputs[i];
				vtx_idx idx = pOutIndices[i];
				v.st0 = pOutTexCoords[idx].GetUV();
				v.p = pOutPos[idx];
				if (m_bSmoothNormals)
				{
					v.st1 = smoothedNormals[idx];
				}
				else
				{
					v.st1 = pOutTangents[idx].GetN().normalize();
				}
			}

			vb0.Unlock();
			vb0.Bind(VSF_GENERAL);
		}

		{
			vb1.Allocate(numOutputTriangles * 3);
			SPipTangents* pTangentsVB = vb1.Lock();

			for (int i = 0; i < numOutputTriangles * 3; i++)
			{
				vtx_idx idx = pOutIndices[i];
				pOutTangents[idx].ExportTo(pTangentsVB[i]);
			}

			vb1.Unlock();
			vb1.Bind(VSF_TANGENTS);
		}

		int nStencilState =
		  STENC_FUNC(FSS_STENCFUNC_ALWAYS) |
		  STENCOP_FAIL(FSS_STENCOP_KEEP) |
		  STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
		  STENCOP_PASS(FSS_STENCOP_REPLACE);

		uint32 State = gRenDev->m_RP.m_CurState | GS_STENCIL;

		if (ef->m_Flags & EF_DECAL)
		{
			PROFILE_LABEL("DECAL");
			State = (State & ~GS_BLEND_MASK);
			State |= GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
		}

		rd->FX_SetStencilState(nStencilState, 1, 0xFFFFFFFF, 0xFFFFFFFF);
		rd->FX_SetState(State);

		if (!FAILED(rd->FX_SetVertexDeclaration(VSM_GENERAL | ((1 << VSF_TANGENTS)), EDefaultInputLayouts::P3F_T2F_T3F)))
		{
			CMesh* pInputMesh = m_pSrcMesh;
			Vec3* pInPos = pInputMesh->GetStreamPtr<Vec3>(CMesh::POSITIONS);
			vtx_idx* pInIndices = pInputMesh->GetStreamPtr<vtx_idx>(CMesh::INDICES);
			SMeshTexCoord* pInTexCoords = pInputMesh->GetStreamPtr<SMeshTexCoord>(CMesh::TEXCOORDS);
			SMeshTangents* pInTangents = pInputMesh->GetStreamPtr<SMeshTangents>(CMesh::TANGENTS);
			SMeshColor* pInColors = pInputMesh->GetStreamPtr<SMeshColor>(CMesh::COLORS_0);
			Matrix44* pTexMtx = NULL;
			if (gRenDev->m_RP.m_pShaderResources)
			{
				SEfResTexture* pTex = gRenDev->m_RP.m_pShaderResources->GetTexture(0);
				if (pTex && pTex->IsHasModificators())
				{
					SEfTexModificator* mod = pTex->m_Ext.m_pTexModifier;
					pTexMtx = &mod->m_TexMatrix;
				}
			}

			float textureBakeLoopTimer = gEnv->pTimer->GetAsyncCurTime();
			for (int i = 0; i < gRenDev->m_RP.m_RendNumIndices / 3; i++)
			{
				Vec4 triPos[3];
				Vec4 triUV[3];
				Vec4 triTangents[2][3];
				Vec4 triColor[3];

				for (int t = 0; t < 3; t++)
				{
					vtx_idx idx = pInIndices[gRenDev->m_RP.m_FirstIndex + i * 3 + t];
					triPos[t].x = pInPos[idx].x;
					triPos[t].y = pInPos[idx].y;
					triPos[t].z = pInPos[idx].z;
					pInTexCoords[idx].GetUV(triUV[t]);
					if (pTexMtx)
					{
						Vec4 modTex = (*pTexMtx) * Vec4(triUV[t].x, triUV[t].y, 0.0f, 1.0f);
						triUV[t].x = modTex.x;
						triUV[t].y = modTex.y;
					}
					pInTangents[idx].GetTB(triTangents[0][t], triTangents[1][t]);
					if (pInColors)
						pInColors[idx].GetRGBA(triColor[t]), triColor[t] /= 255.0f;
					else
						triColor[t] = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
				}
				if (triPos[0] != triPos[1] && triPos[0] != triPos[2] && triPos[1] != triPos[2])
				{
					for (int ss = 0; ss < pOutputMesh->GetSubSetCount(); ss++)
					{
						const SMeshSubset& pSubSet = pOutput->GetSubSet(ss);
						if (pSubSet.nMatID < m_numParams)
						{
							if (m_params[pSubSet.nMatID].bIgnore)
							{
								continue;
							}
							zRange.x = m_params[pSubSet.nMatID].rayLength;
							zRange.y = m_params[pSubSet.nMatID].rayIndent;
						}
						else
						{
							zRange.x = 10.0f;
							zRange.y = 0.5f;
						}

						if (i > 0)
						{
							CHWShader_D3D* pCurVS = (CHWShader_D3D*)sfm->m_VShader;
							CHWShader_D3D* pCurPS = (CHWShader_D3D*)sfm->m_PShader;

							pCurVS->mfSetParametersPB();
							pCurPS->mfSetParametersPB();
						}
						gRenDev->m_RP.m_pShader->FXSetVSFloat(triPosName, triPos, 3);
						gRenDev->m_RP.m_pShader->FXSetPSFloat(triPosName, triPos, 3);
						gRenDev->m_RP.m_pShader->FXSetPSFloat(triUVName, triUV, 3);
						gRenDev->m_RP.m_pShader->FXSetPSFloat(triZRange, &zRange, 1);
						gRenDev->m_RP.m_pShader->FXSetPSFloat(triTangName, triTangents[0], 3);
						gRenDev->m_RP.m_pShader->FXSetPSFloat(triBiNormName, triTangents[1], 3);
						gRenDev->m_RP.m_pShader->FXSetPSFloat(triColorName, triColor, 3);
						rd->FX_Commit();
						rd->FX_DrawPrimitive(eptTriangleList, pSubSet.nFirstIndexId, pSubSet.nNumIndices);
					}
				}
				if ((i % 8) == 7)
				{
					if (!rd->m_pRT || rd->m_pRT->IsRenderThread())
					{
						// Send the commands to the GPU to make sure we don't timeout the driver
#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
						rd->GetDeviceContext().Flush();
#endif
						Sleep(1);
					}
				}
			}

			// Print a message every now and then to show we are still working.
			if (gEnv->pTimer->GetAsyncCurTime() - textureBakeLoopTimer > 20.0f)
			{
				textureBakeLoopTimer = gEnv->pTimer->GetAsyncCurTime();
				CryLog("LoD texture baker - Loop check\n");
			}
		}

		// Release buffers once finished
		vb0.Release();
		vb1.Release();
	}

	return true;
}
*/

/*
struct CompareRendItemMeshBaker
{
	bool operator()(const SRendItem& a, const SRendItem& b) const
	{
		CShader* pShaderA, * pShaderB;
		CShaderResources* pResA, * pResB;
		int nTech;
		SRendItem::mfGet(a.SortVal, nTech, pShaderA, pResA);
		SRendItem::mfGet(b.SortVal, nTech, pShaderB, pResB);

		if (pShaderA && pShaderB)
		{
			// sort decals to the end
			if ((pShaderA->m_Flags & EF_DECAL) != (pShaderB->m_Flags & EF_DECAL))
				return (pShaderA->m_Flags & EF_DECAL) < (pShaderB->m_Flags & EF_DECAL);
		}

		if (pResA && pResB)
		{
			if (pResA->IsTransparent() != pResB->IsTransparent())
				return pResA->GetStrengthValue(EFTT_OPACITY) > pResB->GetStrengthValue(EFTT_OPACITY);
			if (pResA->IsAlphaTested() != pResB->IsAlphaTested())
				return pResA->GetAlphaRef() < pResB->GetAlphaRef();
		}

		int nNearA = (a.ObjSort & FOB_NEAREST);
		int nNearB = (b.ObjSort & FOB_NEAREST);
		if (nNearA != nNearB)               // Sort by nearest flag
			return nNearA > nNearB;

		if (a.SortVal != b.SortVal)         // Sort by shaders
			return a.SortVal < b.SortVal;

		if (a.pElem != b.pElem)               // Sort by geometry
			return a.pElem < b.pElem;

		return (a.ObjSort & 0xFFFF) < (b.ObjSort & 0xFFFF);   // Sort by distance
	}
};
*/

static void PatchShaderItemRecurse(IMaterial* pDst, IMaterial* pMat)
{
	IMaterialManager* pMatMan = gEnv->p3DEngine->GetMaterialManager();
	if (pMat && pDst && pMat != pMatMan->GetDefaultMaterial())
	{
		SShaderItem& siSrc(pMat->GetShaderItem());
		if (siSrc.m_pShader)
		{
			SInputShaderResourcesPtr pIsr = gEnv->pRenderer->EF_CreateInputShaderResource(siSrc.m_pShaderResources);
			SShaderItem siDst(gcpRendD3D->EF_LoadShaderItem("Illum.MeshBaker", false, 0, pIsr, siSrc.m_pShader->GetGenerationMask()));
			pDst->AssignShaderItem(siDst);
			siDst.m_pShaderResources->UpdateConstants(siDst.m_pShader);
		}
		for (int i = 0; i < pMat->GetSubMtlCount(); i++)
		{
			IMaterial* pSubMat = pMat->GetSubMtl(i);
			PatchShaderItemRecurse(pDst->GetSubMtl(i), pSubMat);
		}
	}
}

static IMaterial* PatchMaterial(IMaterial* pMat)
{
	IMaterialManager* pMatMan = gEnv->p3DEngine->GetMaterialManager();
	IMaterial* pResult = pMatMan->CloneMaterial(pMat);

	PatchShaderItemRecurse(pResult, pMat);

	return pResult;
}

static void EtchAlphas(std::vector<IIndexedMesh*> outputList, IMaterial* pMaterial, const SMeshBakingMaterialParams* params, int numParams)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	if (outputList.empty())
		return;

	int i = 0;
	for (std::vector<IIndexedMesh*>::iterator mit = outputList.begin(), mend = outputList.end(); mit != mend; ++mit)
	{
		IIndexedMesh* pOutput = *mit;
		if (!pOutput)
			continue;
		int numOutputTriangles = pOutput->GetIndexCount() / 3;
		CMesh* pOutputMesh = pOutput->GetMesh();
		vtx_idx* pOutIndices = pOutputMesh->GetStreamPtr<vtx_idx>(CMesh::INDICES);
		SMeshTexCoord* pOutTexCoords = pOutputMesh->GetStreamPtr<SMeshTexCoord>(CMesh::TEXCOORDS);

		// OLD PIPELINE
		ASSERT_LEGACY_PIPELINE
		/*
		{
			TempDynVB<SVF_P3F_T2F_T3F> vb;
			vb.Allocate(numOutputTriangles * 3);
			SVF_P3F_T2F_T3F* vInputs = vb.Lock();

			for (int i = 0; i < numOutputTriangles * 3; i++)
			{
				SVF_P3F_T2F_T3F& v = vInputs[i];
				const vtx_idx idx = pOutIndices[i];
				const Vec2 uv = pOutTexCoords[idx].GetUV();

				v.p.x = uv.x;
				v.p.y = uv.y;
				v.p.z = 0.0f;
				v.st0.x = v.p.x;
				v.st0.y = v.p.y;
			}

			vb.Unlock();
			vb.Bind(VSF_GENERAL);
			vb.Release();
		}

		if (!FAILED(rd->FX_SetVertexDeclaration(VSM_GENERAL, EDefaultInputLayouts::P3F_T2F_T3F)))
		{
			rd->FX_Commit();
			for (int ss = 0; ss < pOutputMesh->GetSubSetCount(); ss++)
			{
				const SMeshSubset& pSubSet = pOutput->GetSubSet(ss);
				if (pSubSet.nMatID < numParams && (params[pSubSet.nMatID].bAlphaCutout && !params[pSubSet.nMatID].bIgnore))
				{
					rd->FX_DrawPrimitive(eptTriangleList, pSubSet.nFirstIndexId, pSubSet.nNumIndices);
				}
			}
		}
		if ((++i % 8) == 7)
		{
			if (!rd->m_pRT || rd->m_pRT->IsRenderThread())
			{
				// Send the commands to the GPU to make sure we don't timeout the driver
#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
				rd->GetDeviceContext().Flush();
#endif
				Sleep(1);
			}
		}
		*/
	}
}

static bool Dilate(CTexture* pTex, CTexture* pOutput, int nPhase, std::vector<IIndexedMesh*> pInputIndexedMesh, IMaterial* pMaterial, const SMeshBakingMaterialParams* params, int numParams, CTexture* pDepthStencil, const SMeshBakingInputParams* pInputParams)
{
	// OLD PIPELINE
	ASSERT_LEGACY_PIPELINE

/*
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	PROFILE_LABEL_SCOPE("BakeMeshDilate");
	static CCryNameR missColourName("MISSCOLOUR");
	static CCryNameR tintColourName("TINTCOLOUR");
	static CCryNameR pixSizeName("PIXSIZE");
	static CCryNameTSCRC TechName = "MeshBakerDilate";
	CShader* pSH = rd->m_cEF.mfForName("MeshBakerDilate", 0);
	uint32 uvMapWidth = pTex->GetWidth();
	uint32 uvMapHeight = pTex->GetHeight();
	Vec4 pixSize = Vec4(1.0f / (float)uvMapWidth, 1.0f / (float)uvMapHeight, 0, 0);
	const ColorF& inMissColour = pInputParams->dilateMagicColour;
	Vec4 missColour = Vec4(inMissColour.r, inMissColour.g, inMissColour.b, inMissColour.a);
	Vec4 whiteColour = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
	uint32 extraPasses = 0;
	uint32 nPasses = 0;
	if (!pSH)
	{
		CryLog("BakeMesh: Dilate shader missing\n");
		return false;
	}

	bool bAlphaCutout = false;
	for (int i = 0; i < numParams; i++)
	{
		if (params[i].bAlphaCutout && !params[i].bIgnore)
			bAlphaCutout = (nPhase == 0);
	}

	CTexture* pTemp = CTexture::GetOrCreateRenderTarget("MeshBaker_DilateTemp", uvMapWidth, uvMapHeight, ColorF(1.0f, 1.0f, 1.0f, 1.0f), eTT_2D, FT_STATE_CLAMP, pTex->GetTextureDstFormat());
	int TempX, TempY, TempWidth, TempHeight;
	rd->GetViewport(&TempX, &TempY, &TempWidth, &TempHeight);
	rd->RT_SetViewport(0, 0, uvMapWidth, uvMapHeight);

	rd->FX_ResetPipe();
	rd->D3DSetCull(eCULL_None);
	pSH->FXSetTechnique(TechName);
	pSH->FXBegin(&nPasses, FEF_DONTSETSTATES | FEF_DONTSETTEXTURES);

	const uint32 nPassDilateWithThresholdAlpha = 0;
	const uint32 nPassDilate = 1;
	const uint32 nPassDilateWithZeroAlpha = 2;
	const uint32 nPassGammaCorrect = 3;
	const uint32 nPassNormalCorrect = 4;
	const uint32 nPassPassthrough = 5;

	if (pInputParams->bDoDilationPass)
	{
		// Set miss colour wherever we missed
		rd->FX_PushRenderTarget(0, pTex, pDepthStencil);
		int nStencilState = STENC_FUNC(FSS_STENCFUNC_EQUAL) | STENCOP_FAIL(FSS_STENCOP_KEEP) | STENCOP_ZFAIL(FSS_STENCOP_KEEP) | STENCOP_PASS(FSS_STENCOP_KEEP);
		rd->FX_SetStencilState(nStencilState, 0, 0xFFFFFFFF, 0xFFFFFFFF);
		rd->FX_SetState(GS_NODEPTHTEST | GS_STENCIL);
		pSH->FXBeginPass(nPassPassthrough);
		CRendererResources::s_ptexWhite->Apply(0);
		pSH->FXSetPSFloat(tintColourName, &missColour, 1);
		rd->FX_Commit();
		rd->DrawQuad(0.0f, 0.0f, 1.0f, 1.0f, ColorF(1, 1, 1, 1));
		pSH->FXEndPass();
		rd->FX_PopRenderTarget(0);

		uint32 dilatePasses = max(uvMapWidth, uvMapHeight);

		for (int p = 0; p < 2; p++)
		{
			if (p == 1 || (bAlphaCutout && nPhase == 0)) // If doing alpha cutout we need to dilate the alpha cutout sections first (so geometry doesn't dilate into alphaed areas)
			{
				for (uint32 i = 0; i < dilatePasses; i++) // Dilate as much as possible (make sure even number so result ends up in correct buffer)
				{
					rd->FX_PushRenderTarget(0, pTemp, pDepthStencil);
					rd->FX_SetState(GS_NODEPTHTEST);
					// Pass through all existing data
					pSH->FXBeginPass(nPassPassthrough);
					pTex->Apply(0);
					rd->FX_Commit();
					pSH->FXSetPSFloat(tintColourName, &whiteColour, 1);
					rd->DrawQuad(0.0f, 0.0f, 1.0f, 1.0f, ColorF(1, 1, 1, 1));
					// Dilate using stencil/magic colour to identify areas to dilate
					if (nPhase == 0)
						pSH->FXBeginPass(p ? nPassDilateWithThresholdAlpha : nPassDilateWithZeroAlpha);
					else if (nPhase == 1)
						pSH->FXBeginPass(p ? nPassDilate : nPassDilateWithZeroAlpha);
					else
						pSH->FXBeginPass(nPassDilateWithZeroAlpha);
					pTex->Apply(0);
					pSH->FXSetPSFloat(missColourName, &missColour, 1);
					pSH->FXSetPSFloat(pixSizeName, &pixSize, 1);
					nStencilState = STENC_FUNC(FSS_STENCFUNC_EQUAL) | STENCOP_FAIL(FSS_STENCOP_KEEP) | STENCOP_ZFAIL(FSS_STENCOP_KEEP) | STENCOP_PASS(FSS_STENCOP_INCR);
					rd->FX_SetStencilState(nStencilState, 0, 0xFFFFFFFF, 0xFFFFFFFF);
					rd->FX_SetState(GS_NODEPTHTEST | GS_STENCIL);
					if (p)
						rd->DrawQuad(0.0f, 0.0f, 1.0f, 1.0f, ColorF(1, 1, 1, 1));
					else
						EtchAlphas(pInputIndexedMesh, pMaterial, params, numParams);
					rd->FX_PopRenderTarget(0);
					// ping pong
					CTexture* tmp = pTex;
					pTex = pTemp;
					pTemp = tmp;
				}
			}
		}
		pSH->FXEndPass();
	}
	else if (bAlphaCutout)
	{
		// not doing dilate pass but still show alpha
		Vec4 zeroAlpha(1, 1, 1, 0);
		rd->FX_PushRenderTarget(0, pTex, pDepthStencil);
		int nStencilState = STENC_FUNC(FSS_STENCFUNC_EQUAL) | STENCOP_FAIL(FSS_STENCOP_KEEP) | STENCOP_ZFAIL(FSS_STENCOP_KEEP) | STENCOP_PASS(FSS_STENCOP_KEEP);
		rd->FX_SetStencilState(nStencilState, 0, 0xFFFFFFFF, 0xFFFFFFFF);
		rd->FX_SetState(GS_NODEPTHTEST | GS_STENCIL | GS_NOCOLMASK_RGB);
		pSH->FXBeginPass(nPassPassthrough);
		CRendererResources::s_ptexWhite->Apply(0);
		pSH->FXSetPSFloat(tintColourName, &zeroAlpha, 1);
		rd->FX_Commit();
		EtchAlphas(pInputIndexedMesh, pMaterial, params, numParams);
		pSH->FXEndPass();
		rd->FX_PopRenderTarget(0);
	}
	rd->FX_SetState(GS_NODEPTHTEST);
	if (pOutput) // Fix SRGB/Flipped normals problems
	{
		pSH->FXBeginPass((nPhase == 1) ? nPassNormalCorrect : nPassGammaCorrect);
		rd->FX_PushRenderTarget(0, pOutput, NULL);
		pTex->Apply(0);
		rd->FX_Commit();
		rd->DrawQuad(0.0f, 0.0f, 1.0f, 1.0f, ColorF(1, 1, 1, 1));
		rd->FX_PopRenderTarget(0);
		pSH->FXEndPass();
	}
	pSH->FXEnd();
	rd->RT_SetViewport(TempX, TempY, TempWidth, TempHeight);
	rd->FX_SetState(0);
	rd->FX_Commit();
	rd->FX_ResetPipe();
	pTemp->Release();
	pSH->Release();

	*/
	return true;
}

static bool IsRenderableSubObject(IStatObj* obj, int child)
{
	return obj->GetSubObject(child)->nType == STATIC_SUB_OBJECT_MESH &&
	       obj->GetSubObject(child)->pStatObj != NULL &&
	       obj->GetSubObject(child)->pStatObj->GetRenderMesh() != NULL;
}
#endif

bool CD3D9Renderer::BakeMesh(const SMeshBakingInputParams* pInputParams, SMeshBakingOutput* pReturnValues)
{
#if !CRY_PLATFORM_CONSOLE && !CRY_PLATFORM_MOBILE
	if (gEnv->IsEditor())
	{
#if defined(CREATE_RENDERDOC_CAPTURE)
		BeginRenderDocCapture();
#endif

		std::vector<IRenderMesh*> pRM;
		std::vector<IMaterial*> pInputMaterial;
		std::vector<CMesh*> pInputMesh;
		std::vector<IIndexedMesh*> pOutputMesh;
		IMaterial* pOutputMaterial;
		int outputWidth = pInputParams->outputTextureWidth;
		int outputHeight = pInputParams->outputTextureHeight;
		int cachedShaderCompileCvar = CRenderer::CV_r_shadersasynccompiling;

		if (pInputParams->pInputMesh)
		{
			IStatObj* obj = pInputParams->pInputMesh;
			if (obj->GetRenderMesh() == NULL)
			{
				if (obj->GetSubObjectCount() == 0)
				{
					CryLog("BakeMesh: Failed due to input mesh having no rendermesh and no subobjects\n");
					return false;
				}
				for (int i = 0; i < obj->GetSubObjectCount(); i++)
				{
					if (IsRenderableSubObject(obj, i))
					{
						IStatObj* pObj = obj->GetSubObject(i)->pStatObj;
						pRM.push_back(pObj->GetRenderMesh());
						pInputMaterial.push_back(pInputParams->pMaterial ? pInputParams->pMaterial : pObj->GetMaterial());
						pInputMesh.push_back(pObj->GetIndexedMesh(true)->GetMesh());
					}
				}
			}
			else
			{
				pRM.push_back(obj->GetRenderMesh());
				pInputMaterial.push_back(pInputParams->pMaterial ? pInputParams->pMaterial : obj->GetMaterial());
				pInputMesh.push_back(obj->GetIndexedMesh(true)->GetMesh());
			}
		}
		else if (pInputParams->pInputCharacter)
		{
			IDefaultSkeleton& rIDefaultSkeleton = pInputParams->pInputCharacter->GetIDefaultSkeleton();
			pRM.push_back(rIDefaultSkeleton.GetIRenderMesh());
			pInputMaterial.push_back(pInputParams->pMaterial ? pInputParams->pMaterial : pInputParams->pInputCharacter->GetIMaterial());
			pInputMesh.push_back(pRM.back()->GetIndexedMesh()->GetMesh());
		}

		// HACK TO GET STREAMING SYSTEM TO MAKE SURE USED TEXTURES ARE STREAMED IN
		gEnv->p3DEngine->ProposeContentPrecache();
		//		gEnv->p3DEngine->UpdateStreaming(SRenderingPassInfo::CreateGeneralPassRenderingInfo(gEnv->pSystem->GetViewCamera()));
		for (std::vector<IMaterial*>::iterator it = pInputMaterial.begin(), end = pInputMaterial.end(); it != end; ++it)
		{
			float start = gEnv->pTimer->GetAsyncCurTime();
			while (true)
			{
				int pRoundIds[MAX_STREAM_PREDICTION_ZONES] = { 0 };
				(*it)->PrecacheMaterial(0.0f, NULL, true, false);
				(*it)->PrecacheMaterial(0.0f, NULL, false, false);
				CTexture::Update();
				gEnv->pSystem->GetStreamEngine()->Update();
				if ((*it)->IsStreamedIn(pRoundIds, NULL))
				{
					break;
				}
				if (gEnv->pTimer->GetAsyncCurTime() - start > 5.0f)
				{
					LogWarning("Time out waiting for textures to stream\n");
					break;
				}
			}
		}

		if (pRM.empty())
		{
			CryLog("BakeMesh: Failed due to no inputs\n");
			return false;
		}

		if (pInputParams->pCageMesh)
		{
			IStatObj* obj = pInputParams->pCageMesh;

			if (obj->GetRenderMesh() == NULL)
			{
				if (obj->GetSubObjectCount() == 0)
				{
					CryLog("BakeMesh: Failed due to cage mesh having no rendermesh and no subobjects\n");
					return false;
				}
				for (int i = 0; i < obj->GetSubObjectCount(); i++)
				{
					if (IsRenderableSubObject(obj, i))
					{
						IStatObj* subobj = obj->GetSubObject(i)->pStatObj;
						pOutputMesh.push_back(subobj->GetIndexedMesh(true));
					}
				}
			}
			else
			{
				pOutputMesh.push_back(obj->GetIndexedMesh(true));
			}
			pOutputMaterial = obj->GetMaterial();
		}
		else if (pInputParams->pCageCharacter)
		{
			IDefaultSkeleton& rIDefaultSkeleton = pInputParams->pCageCharacter->GetIDefaultSkeleton();
			pOutputMesh.push_back(rIDefaultSkeleton.GetIRenderMesh()->GetIndexedMesh());
			pOutputMaterial = pInputParams->pCageCharacter->GetIMaterial();
		}
		else
		{
			CryLog("BakeMesh: Failed due to no low poly cage\n");
			return false;
		}

		CRenderer::CV_r_shadersasynccompiling = 0;

		std::vector<_smart_ptr<IMaterial>> pBakeMaterial;
		pBakeMaterial.reserve(pInputMaterial.size());
		for (std::vector<IMaterial*>::iterator it = pInputMaterial.begin(), end = pInputMaterial.end(); it != end; ++it)
		{
			pBakeMaterial.push_back(PatchMaterial(*it)); // Replace current shader with MeshBake
		}

		CTexture* pTmpDepthSurface = CreateDepthTarget(outputWidth, outputHeight, Clr_Empty, eTF_Unknown);
		if (!pTmpDepthSurface)
		{
			CryLog("BakeMesh: Failed as temporary depth surface could not be created of size %dx%d\n", outputWidth, outputHeight);
			CRenderer::CV_r_shadersasynccompiling = cachedShaderCompileCvar;
			return false;
		}

		PROFILE_LABEL_SCOPE("BakeMesh");

		SRenderingPassInfo passInfo = SRenderingPassInfo::CreateGeneralPassRenderingInfo(gEnv->p3DEngine->GetRenderingCamera());

		bool bAlphaCutout = false;
		for (int i = 0; i < pInputParams->numMaterialParams; i++)
		{
			if (pInputParams->pMaterialParams[i].bAlphaCutout && !pInputParams->pMaterialParams[i].bIgnore)
				bAlphaCutout = true;
		}

		// OLD PIPELINE
		ASSERT_LEGACY_PIPELINE
		/*
		CTexture* pHighPrecisionBuffer[3], * pOutputBuffer[3];
		for (int nPhase = 0; nPhase < 3; nPhase++)
		{
			const bool bReverseDepth = true;
			ColorF Clr_Phase;

			if (nPhase == 0)
				Clr_Phase = pInputParams->defaultBackgroundColour;
			else if (nPhase == 1)
				Clr_Phase = ColorF(0.5f, 0.5f, 1.0f, 1.0f);
			else
				Clr_Phase = ColorF(0.0f, 0.0f, 0.0f, 1.0f);

			char uniqueName[128];
			const char* suffix[3] = { "Albedo", "Normal", "Refl" };
			cry_sprintf(uniqueName, "MeshBaker_16Bit%s_%d%s", suffix[nPhase], pInputParams->nLodId, bAlphaCutout ? "_alpha" : "");
			pHighPrecisionBuffer[nPhase] = CTexture::GetOrCreateRenderTarget(uniqueName, outputWidth, outputHeight, Clr_Phase, eTT_2D, FT_STATE_CLAMP, eTF_R16G16B16A16F ); // , -1, 95
			cry_sprintf(uniqueName, "MeshBaker_8Bit%s_%d%s", suffix[nPhase], pInputParams->nLodId, bAlphaCutout ? "_alpha" : "");
			pOutputBuffer[nPhase] = CTexture::GetOrCreateRenderTarget(uniqueName, outputWidth, outputHeight, Clr_Phase, eTT_2D, FT_STATE_CLAMP | FT_STAGE_READBACK, eTF_R8G8B8A8 ); // , -1, 95

			PROFILE_LABEL_SCOPE(suffix[nPhase]);

			FX_ResetPipe();
			FX_ClearTarget(pHighPrecisionBuffer[nPhase], Clr_Phase);
			FX_ClearTarget(pTmpDepthSurface, CLEAR_ZBUFFER | CLEAR_STENCIL, Clr_FarPlane_R.r, 0);
			FX_PushRenderTarget(0, pHighPrecisionBuffer[nPhase], pTmpDepthSurface);
			RT_SetViewport(0, 0, outputWidth, outputHeight);
			m_RP.m_pCurrentRenderView = passInfo.GetRenderView();

			int nThreadID = m_pRT->GetThreadList();

			FX_PreRender(3);

			SRenderPipeline& RESTRICT_REFERENCE rRP = m_RP;
			rRP.m_pRenderFunc = nullptr;//FX_FlushShader_General;
			rRP.m_nPassGroupID = EFSLIST_GENERAL;
			rRP.m_nPassGroupDIP = rRP.m_nPassGroupID;
			FX_StartBatching();
			rRP.m_nBatchFilter = FB_GENERAL;

			int numChunks = 0;
			for (int m = 0; m < pRM.size(); m++)
			{
				TRenderChunkArray& chunkList = pRM[m]->GetChunks();
				numChunks += chunkList.size();
			}

			std::vector<CRenderObject*> pObjs;
			SRendItem* ri = new SRendItem[numChunks];
			numChunks = 0;
			int chunkIds[1024];
			for (int m = 0; m < pRM.size(); m++)
			{
				TRenderChunkArray& chunkList = pRM[m]->GetChunks();

				CRenderObject* pObj = new CRenderObject();
				pObj->Init();
				pObj->m_II.m_Matrix.SetIdentity();
				pObj->m_II.m_Matrix.SetTranslationMat(GetCamera().GetPosition());
				pObj->m_ObjFlags = 0;
				pObj->m_II.m_AmbColor = Col_White;
				pObj->m_nSort = 0;
				pObj->m_ObjFlags |= FOB_NO_FOG | FOB_TRANS_TRANSLATE | ((!pInputMesh[m]) ? FOB_SKINNED : 0);
				pObj->m_pCurrMaterial = pBakeMaterial[m];
				pObjs.push_back(pObj);
				for (int i = 0; i < chunkList.size(); i++)
				{
					CRenderChunk* pChunk = &chunkList[i];
					SShaderItem& pSH = pBakeMaterial[m]->GetShaderItem(pChunk->m_nMatID);
					CShaderResources* pR = (CShaderResources*)pSH.m_pShaderResources;
					CShader* __restrict pShader = (CShader*)pSH.m_pShader;
					CREMesh* pRE = pChunk->pRE;

					if (pShader->m_Flags & EF_DECAL)
					{
						const bool bHasDiffuse = (pR && pR->GetTexture(EFTT_DIFFUSE)) ? true : false;
						const bool bHasNormal = (pR && pR->GetTexture(EFTT_NORMALS)) ? true : false;
						const bool bHasGloss = (pR && pR->GetTexture(EFTT_SMOOTHNESS)) ? true : false;

						// emulate gbuffer blend masking (since we don't MRT, this won't work correctly
						// in the normal pipeline)

						if (!bHasDiffuse && nPhase == 0)
							continue;
						if (!bHasNormal && nPhase == 1)
							continue;
						if (!bHasGloss && nPhase == 2)
							continue;
					}

					ri[numChunks].pObj = pObj;
					chunkIds[numChunks] = m; // Stash in this in something that doesn't effect sorting
					ri[numChunks].ObjSort = (pObj->m_ObjFlags & 0xffff0000) | pObj->m_nSort;
					int nThreadID = m_RP.m_nProcessThreadID;
					ri[numChunks].nBatchFlags = EF_BatchFlags(pSH, pObj, pRE, passInfo, 1); // naughty
					//ri[numChunks].nStencRef = pObj->m_nClipVolumeStencilRef+1;
					ri[numChunks].SortVal = SRendItem::PackShaderItem(pSH);
					ri[numChunks].pElem = pRE;
					numChunks++;
				}
			}

			std::sort(ri, ri + numChunks, CompareRendItemMeshBaker());

			for (int i = 0; i < numChunks; i++)
			{
				CShader* pShader;
				CShaderResources* pRes;
				int nTech;
				SRendItem::mfGet(ri[i].SortVal, nTech, pShader, pRes);
				if (pShader)
				{
					int m = chunkIds[i];
					CREBaker wrappedRE(ri[i].pElem, pInputMesh[m], pOutputMesh, nPhase, pInputParams->pMaterialParams, pInputParams->numMaterialParams, pInputParams->bSmoothNormals);
					ri[i].pElem = &wrappedRE;
					FX_ObjectChange(pShader, pRes, ri[i].pObj, &wrappedRE);
					FX_Start(pShader, nTech, pRes, &wrappedRE);
					wrappedRE.mfPrepare(true);
					rRP.m_RIs[0].AddElem(&ri[i]);
					// OLD PIPELINE
					// TODO fix it
					//FX_FlushShader_General();
				}
			}

			for (int i = 0; i < pObjs.size(); i++)
			{
				delete pObjs[i];
			}
			delete[] ri;

			FX_PostRender();

			FX_PopRenderTarget(0);

			Dilate(pHighPrecisionBuffer[nPhase], pOutputBuffer[nPhase], nPhase, pOutputMesh, pOutputMaterial, pInputParams->pMaterialParams, pInputParams->numMaterialParams, pTmpDepthSurface, pInputParams);
		}

		for (int i = 0; i < 3; i++)
		{
			if (i == 2 && !pInputParams->bSaveSpecular)
			{
				SAFE_RELEASE(pOutputBuffer[i]);
				pReturnValues->ppOuputTexture[i] = NULL;

				SAFE_RELEASE(pHighPrecisionBuffer[i]);
				pReturnValues->ppIntermediateTexture[i] = NULL;
				continue;
			}

			pOutputBuffer[i]->GetDevTexture()->DownloadToStagingResource(0);

			pReturnValues->ppOuputTexture[i] = pOutputBuffer[i];
			pReturnValues->ppIntermediateTexture[i] = pHighPrecisionBuffer[i];
		}
		*/

		SAFE_RELEASE(pTmpDepthSurface);
		CRenderer::CV_r_shadersasynccompiling = cachedShaderCompileCvar;

#if defined(CREATE_RENDERDOC_CAPTURE)
		EndRenderDocCapture();
#endif

		return true;
	}
	else
#endif
	{
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR, "BakeMesh: Only exists within editor\n");
		return false;
	}
}
