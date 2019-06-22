// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "StatObjFoliage.h"

#include <Cry3DEngine/ISurfaceType.h>
#include <CryParticleSystem/IParticles.h>

CStatObjFoliage::CStatObjFoliage()
{
	m_next = 0;
	m_prev = 0;
	m_lifeTime = 0;
	m_ppThis = 0;
	m_pStatObj = 0;
	m_pRopes = 0;
	m_pRopesActiveTime = 0;
	m_nRopes = 0;
	m_nRefCount = 1;
	m_timeIdle = 0;
	m_pVegInst = 0;
	m_pTrunk = 0;
	m_pSkinningTransformations[0] = 0;
	m_pSkinningTransformations[1] = 0;
	m_iActivationSource = 0;
	m_flags = 0;
	m_bGeomRemoved = 0;
	m_bEnabled = 1;
	m_timeInvisible = 0;
	m_bDelete = 0;
	m_pRenderObject = 0;
	m_minEnergy = 0.0f;
	m_stiffness = 0.0f;
	arrSkinningRendererData[0].pSkinningData = NULL;
	arrSkinningRendererData[0].nFrameID = 0;
	arrSkinningRendererData[1].pSkinningData = NULL;
	arrSkinningRendererData[1].nFrameID = 0;
	arrSkinningRendererData[2].pSkinningData = NULL;
	arrSkinningRendererData[2].nFrameID = 0;
}

CStatObjFoliage::~CStatObjFoliage()
{
	if (m_pRopes)
	{
		for (int i = 0; i < m_nRopes; i++)
			if (m_pRopes[i])
				gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_pRopes[i]);
		delete[] m_pRopes;
		delete[] m_pRopesActiveTime;
	}
	delete[] m_pSkinningTransformations[0];
	delete[] m_pSkinningTransformations[1];
	m_next->m_prev = m_prev;
	m_prev->m_next = m_next;
	if (m_pStatObj)
		m_pStatObj->Release();
	if (*m_ppThis == this)
		*m_ppThis = nullptr;
	if (!m_bDelete)
	{
		if (m_pRenderObject)
		{
			m_pRenderObject->m_ObjFlags &= ~(FOB_SKINNED);
			threadID nThreadID = 0;
			gEnv->pRenderer->EF_Query(EFQ_MainThreadList, nThreadID);
			if (SRenderObjData* pOD = m_pRenderObject->GetObjData())
				pOD->m_pSkinningData = NULL;
		}
	}
}

uint32 CStatObjFoliage::ComputeSkinningTransformationsCount()
{
	uint32 count = 1;
	uint32 spineCount = uint32(m_pStatObj->m_nSpines);
	for (uint32 i = 0; i < spineCount; ++i)
		count += uint32(m_pStatObj->m_pSpines[i].nVtx - 1);
	return count;
}

void CStatObjFoliage::ComputeSkinningTransformations(uint32 nList)
{
	uint32 jointCount = ComputeSkinningTransformationsCount();
	if (!m_pSkinningTransformations[0])
	{
		m_pSkinningTransformations[0] = new QuatTS[jointCount];
		m_pSkinningTransformations[1] = new QuatTS[jointCount];
		memset(m_pSkinningTransformations[0], 0, jointCount * sizeof(QuatTS));
		memset(m_pSkinningTransformations[1], 0, jointCount * sizeof(QuatTS));
	}

	QuatTS* pPose = m_pSkinningTransformations[nList];

	pe_status_rope sr;
	Vec3 point0, point1;
	uint32 jointIndex = 1;
	uint32 spineCount = uint32(m_pStatObj->m_nSpines);
	for (uint32 i = 0; i < spineCount; ++i)
	{
		if (!m_pRopes[i])
		{
			jointIndex += uint32(m_pStatObj->m_pSpines[i].nVtx - 1);
			continue;
		}

		sr.pPoints = NULL;
		m_pRopes[i]->GetStatus(&sr);
		if (sr.timeLastActive > m_pRopesActiveTime[i])
		{
			m_pRopesActiveTime[i] = sr.timeLastActive;

			// We use the scalar component of the first joint for this section
			// to mark the NEED for further computation.
			assert(jointIndex < jointCount);
			PREFAST_ASSUME(jointIndex < jointCount);
			pPose[jointIndex].s = 1.0f;

			sr.pPoints = m_pStatObj->m_pSpines[i].pVtxCur;
			m_pRopes[i]->GetStatus(&sr);
			point0 = sr.pPoints[0];
			uint32 spineVertexCount = m_pStatObj->m_pSpines[i].nVtx - 1;
			for (uint32 j = 0; j < spineVertexCount; ++j)
			{
				assert(jointIndex < jointCount);
				PREFAST_ASSUME(jointIndex < jointCount);

				point1 = sr.pPoints[j + 1];

				Vec3& p0 = pPose[jointIndex].t;
				Vec3& p1 = *(Vec3*)&pPose[jointIndex].q;

				p0 = point0;
				p1 = point1;

				point0 = point1;
				jointIndex++;
			}
		}
		else
		{
			uint32 count = m_pStatObj->m_pSpines[i].nVtx - 1;
			memcpy(&pPose[jointIndex], &m_pSkinningTransformations[(nList + 1) & 1][jointIndex], count * sizeof(QuatTS));

			// SF: no need to do this! Causes a bug, where unprocessed data can be
			// copied from the other list and then marked as not needing processing!
			// pPose[jointIndex].s = 0.0f;

			jointIndex += count;
		}
	}
}

SSkinningData* CStatObjFoliage::GetSkinningData(const Matrix34& RenderMat34, const SRenderingPassInfo& passInfo)
{
	int nList = passInfo.ThreadID();
	QuatTS* pPose = m_pSkinningTransformations[nList];
	if (pPose == 0)
		return NULL;

	if (m_pStatObj && m_pStatObj->m_pRenderMesh)
		m_pStatObj->m_pRenderMesh->NextDrawSkinned();

#if defined(USE_CRY_ASSERT)
	int nNumBones = ComputeSkinningTransformationsCount();
#endif

	// get data to fill
	int nSkinningFrameID = passInfo.GetIRenderView()->GetSkinningPoolIndex();
	int nSkinningList = nSkinningFrameID % 3;
	int nPrevSkinningList = (nSkinningFrameID - 1) % 3;

	//int nCurrentFrameID = passInfo.GetFrameID();

	// before allocating new skinning date, check if we already have for this frame
	if (arrSkinningRendererData[nSkinningList].nFrameID == nSkinningFrameID && arrSkinningRendererData[nSkinningList].pSkinningData)
		return arrSkinningRendererData[nSkinningList].pSkinningData;

	//m_nFrameID = nCurrentFrameID;

	Matrix34 transformation = RenderMat34.GetInverted();

	pPose[0].q.SetIdentity();
	pPose[0].t.x = 0.0f;
	pPose[0].t.y = 0.0f;
	pPose[0].t.z = 0.0f;
	pPose[0].s = 0.0f;

	assert(m_pStatObj);
	uint32 jointIndex = 1;
	uint32 spineCount = uint32(m_pStatObj->m_nSpines);
	for (uint32 i = 0; i < spineCount; ++i)
	{
		if (!m_pRopes[i])
		{
			uint32 spineVertexCount = m_pStatObj->m_pSpines[i].nVtx - 1;
			for (uint32 j = 0; j < spineVertexCount; ++j)
			{
				pPose[jointIndex].q.SetIdentity();
				pPose[jointIndex].t.x = 0.0f;
				pPose[jointIndex].t.y = 0.0f;
				pPose[jointIndex].t.z = 0.0f;
				pPose[jointIndex].s = 0.0f;

				jointIndex++;
			}
			continue;
		}

		// If the scalar component is not set no further computation is needed.
		if (pPose[jointIndex].s < 1.0f)
		{
			jointIndex += uint32(m_pStatObj->m_pSpines[i].nVtx - 1);
			continue;
		}

		uint32 spineVertexCount = m_pStatObj->m_pSpines[i].nVtx - 1;
		for (uint32 j = 0; j < spineVertexCount; ++j)
		{
			Vec3& p0 = pPose[jointIndex].t;
			Vec3& p1 = *(Vec3*)&pPose[jointIndex].q;

			p0 = transformation * p0;
			p1 = transformation * p1;

			pPose[jointIndex].q = Quat::CreateRotationV0V1(
				(m_pStatObj->m_pSpines[i].pVtx[j + 1] - m_pStatObj->m_pSpines[i].pVtx[j]).normalized(),
				(p1 - p0).normalized());
			pPose[jointIndex].t = p0 - pPose[jointIndex].q * m_pStatObj->m_pSpines[i].pVtx[j];

			*(DualQuat*)&m_pSkinningTransformations[nList ^ 1][jointIndex] =
				*(DualQuat*)&pPose[jointIndex] = DualQuat(pPose[jointIndex].q, pPose[jointIndex].t);

			jointIndex++;
		}
	}

	// get data to fill
	assert(jointIndex == nNumBones);

	SSkinningData* pSkinningData = gEnv->pRenderer->EF_CreateSkinningData(passInfo.GetIRenderView(), jointIndex, false);
	assert(pSkinningData);
	PREFAST_ASSUME(pSkinningData);
	memcpy(pSkinningData->pBoneQuatsS, pPose, jointIndex * sizeof(DualQuat));
	arrSkinningRendererData[nSkinningList].pSkinningData = pSkinningData;
	arrSkinningRendererData[nSkinningList].nFrameID = nSkinningFrameID;

	// set data for motion blur
	if (arrSkinningRendererData[nPrevSkinningList].nFrameID == (nSkinningFrameID - 1) && arrSkinningRendererData[nPrevSkinningList].pSkinningData)
	{
		pSkinningData->nHWSkinningFlags |= eHWS_MotionBlured;
		pSkinningData->pPreviousSkinningRenderData = arrSkinningRendererData[nPrevSkinningList].pSkinningData;
	}
	else
	{
		// if we don't have motion blur data, use the some as for the current frame
		pSkinningData->pPreviousSkinningRenderData = pSkinningData;
	}

	return pSkinningData;
}

void CStatObjFoliage::Update(float dt, const CCamera& rCamera)
{
	if (!*m_ppThis && !m_bDelete)
		m_bDelete = 2;
	if (m_bDelete)
	{
		if (--m_bDelete <= 0)
			delete this;
		return;
	}
	FUNCTION_PROFILER_3DENGINE;

	int i, j, nContactEnts = 0, nColl;
	pe_status_rope sr;
	pe_params_bbox pbb;
	pe_status_dynamics sd;
	bool bHasBrokenRopes = false;
	IPhysicalEntity* pContactEnt, *pContactEnts[4];
	AABB bbox;
	bbox.Reset();

	for (i = nColl = 0; i < m_nRopes; i++)
		if (m_pRopes[i] && m_pRopes[i]->GetStatus(&sr))
		{
			pContactEnt = sr.pContactEnts[0];
			if (m_pVegInst && pContactEnt && pContactEnt->GetType() != PE_STATIC)
			{
				for (j = 0; j < nContactEnts && pContactEnts[j] != pContactEnt; j++)
					;
				
				if (j >= nContactEnts && nContactEnts < 4)
					pContactEnts[nContactEnts++] = pContactEnt;
			}
			nColl += sr.nCollDyn;
			if (m_flags & FLAG_FROZEN && sr.nCollStat + sr.nCollDyn)
				BreakBranch(i), bHasBrokenRopes = true;
			else
			{
				m_pRopes[i]->GetParams(&pbb);
				bbox.Add(pbb.BBox[0]);
				bbox.Add(pbb.BBox[1]);
			}
		}
		else
			bHasBrokenRopes = true;

	float clipDist = float(GetCVars()->e_CullVegActivation);
	int bVisible = rCamera.IsAABBVisible_E(bbox);
	int bEnable = bVisible & isneg(((rCamera.GetPosition() - bbox.GetCenter()).len2() - sqr(clipDist)) * clipDist - 0.0001f);
	if (!bEnable)
	{
		if (inrange(m_timeInvisible += dt, 6.0f, 8.0f))
			bEnable = 1;
		else if (m_pTrunk && bVisible)
		{
			pe_status_awake psa;
			bEnable = m_pTrunk->GetStatus(&psa); // Is the trunk awake?
		}

	}
	else
		m_timeInvisible = 0;
	if (m_bEnabled != bEnable)
	{
		pe_params_flags pf;
		pf.flagsAND = ~pef_disabled;
		pf.flagsOR = pef_disabled & ~- bEnable;

		for (i = 0; i < m_nRopes; i++)
			if (m_pRopes[i])
				m_pRopes[i]->SetParams(&pf);
		m_bEnabled = bEnable;
	}

	pe_simulation_params sp;
	sp.minEnergy = sqr(nColl && bVisible ? 0.1f : 0.005f);
	if (sp.minEnergy != m_minEnergy)
		for (i = 0, m_minEnergy = sp.minEnergy; i < m_nRopes; i++)
			if (m_pRopes[i])
				m_pRopes[i]->SetParams(&sp);

	if (nColl && (m_iActivationSource & 2 || bVisible))
		m_timeIdle = 0;
	if (!bHasBrokenRopes && m_lifeTime > 0 && (m_timeIdle += dt) > m_lifeTime)
	{
		*m_ppThis = 0;
		m_bDelete = 2;
	}
	threadID nThreadID = 0;
	gEnv->pRenderer->EF_Query(EFQ_MainThreadList, nThreadID);
	ComputeSkinningTransformations(nThreadID);
}

void CStatObjFoliage::SetFlags(uint flags)
{
	if (flags != m_flags)
	{
		pe_params_rope pr;
		if (flags & FLAG_FROZEN)
		{
			pr.collDist = 0.1f;
			pr.stiffnessAnim = 0;
		}
		else
		{
			pr.collDist = 0.03f;
			pr.stiffnessAnim = m_stiffness;
		}
		for (int i = 0; i < m_pStatObj->m_nSpines; i++)
			if (m_pRopes[i])
				m_pRopes[i]->SetParams(&pr);
		m_flags = flags;
	}
}

void CStatObjFoliage::OnHit(struct EventPhysCollision* pHit)
{
	if (m_flags & FLAG_FROZEN)
	{
		pe_params_foreign_data pfd;
		pHit->pEntity[1]->GetParams(&pfd);
		BreakBranch(pfd.iForeignFlags);
	}
}

void CStatObjFoliage::BreakBranch(int idx)
{
	if (m_pRopes[idx])
	{
		int i, nActiveRopes;

		IParticleEffect* pBranchBreakEffect = m_pStatObj->GetSurfaceBreakageEffect(SURFACE_BREAKAGE_TYPE("freeze_shatter"));
		if (pBranchBreakEffect)
		{
			float t, maxt, kb, kc, dim;
			float effStep = max(0.1f, m_pStatObj->m_pSpines[idx].len * 0.1f);
			Vec3 pos, dir;
			pe_status_rope sr;

			sr.pPoints = m_pStatObj->m_pSpines[idx].pVtxCur;
			m_pRopes[idx]->GetStatus(&sr);
			pos = sr.pPoints[0];
			i = 0;
			t = 0;
			dir = sr.pPoints[1] - sr.pPoints[0];
			dir /= (maxt = dir.len());
			do
			{
				pos = sr.pPoints[i] + dir * t;
				dim = max(fabs_tpl(m_pStatObj->m_pSpines[idx].pSegDim[i].y - m_pStatObj->m_pSpines[idx].pSegDim[i].x),
					fabs_tpl(m_pStatObj->m_pSpines[idx].pSegDim[i].w - m_pStatObj->m_pSpines[idx].pSegDim[i].z));
				dim = max(0.05f, min(1.5f, dim));
				if ((t += effStep) > maxt)
				{
					if (++i >= sr.nSegments)
						break;
					dir = sr.pPoints[i + 1] - sr.pPoints[i];
					dir /= (maxt = dir.len());
					kb = dir * (sr.pPoints[i] - pos);
					kc = (sr.pPoints[i] - pos).len2() - sqr(effStep);
					pos = sr.pPoints[i] + dir * (t = kb + sqrt_tpl(max(1e-5f, kb * kb - kc)));
				}
				pBranchBreakEffect->Spawn(true, IParticleEffect::ParticleLoc(pos, (Vec3(0, 0, 1) - dir * dir.z).GetNormalized(), dim));
			} while (true);
		}

		if (!m_bGeomRemoved)
		{
			for (i = nActiveRopes = 0; i < m_pStatObj->m_nSpines; i++)
				nActiveRopes += m_pRopes[i] != 0;
			if (nActiveRopes * 3 < m_pStatObj->m_nSpines * 2)
			{
				pe_params_rope pr;
				pe_params_part pp;
				m_pRopes[idx]->GetParams(&pr);
				pp.flagsCond = geom_squashy;
				pp.flagsAND = 0;
				pp.flagsColliderAND = 0;
				pp.mass = 0;
				if (pr.pEntTiedTo[0])
					pr.pEntTiedTo[0]->SetParams(&pp);
				m_bGeomRemoved = 1;
			}
		}
		m_pStatObj->GetPhysicalWorld()->DestroyPhysicalEntity(m_pRopes[idx]);
		m_pRopes[idx] = 0;
		if (m_pStatObj->GetFlags() & (STATIC_OBJECT_CLONE))
			m_pStatObj->m_pSpines[idx].bActive = false;

		for (i = 0; i < m_pStatObj->m_nSpines; i++)
			if (m_pStatObj->m_pSpines[i].iAttachSpine == idx)
				BreakBranch(i);
	}
}

int CStatObjFoliage::Serialize(TSerialize ser)
{
	int i, nRopes = m_nRopes;
	bool bVal;
	ser.Value("nropes", nRopes);

	if (!ser.IsReading())
	{
		for (i = 0; i < m_nRopes; i++)
		{
			ser.BeginGroup("rope");
			if (m_pRopes[i])
			{
				ser.Value("active", bVal = true);
				m_pRopes[i]->GetStateSnapshot(ser);
			}
			else
				ser.Value("active", bVal = false);
			ser.EndGroup();
		}
	}
	else
	{
		if (m_nRopes != nRopes)
			return 0;
		for (i = 0; i < m_nRopes; i++)
		{
			ser.BeginGroup("rope");
			ser.Value("active", bVal);
			if (m_pRopes[i] && !bVal)
				m_pStatObj->GetPhysicalWorld()->DestroyPhysicalEntity(m_pRopes[i]), m_pRopes[i] = 0;
			else if (m_pRopes[i])
				m_pRopes[i]->SetStateFromSnapshot(ser);
			m_pRopesActiveTime[i] = -1;
			ser.EndGroup();
		}
	}

	return 1;
}
