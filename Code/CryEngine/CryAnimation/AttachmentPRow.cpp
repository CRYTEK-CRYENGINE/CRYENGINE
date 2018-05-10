// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AttachmentPRow.h"

#include <CryString/StringUtils.h>
#include "AttachmentManager.h"
#include "CharacterInstance.h"

void CAttachmentPROW::UpdateRow(Skeleton::CPoseData& rPoseData)
{
	if (m_nRowJointID < 0)
		return;
	if (m_rowparams.m_nParticlesPerRow == 0)
		return;

	const CCharInstance* pSkelInstance = m_pAttachmentManager->m_pSkelInstance;
	const CDefaultSkeleton& rDefaultSkeleton = *pSkelInstance->m_pDefaultSkeleton;

#ifdef EDITOR_PCDEBUGCODE
	//ERROR-CHECK: check if bones and x-axis align
	if (pSkelInstance->m_CharEditMode & CA_CharacterTool)
	{
		QuatTS rPhysLocation = pSkelInstance->m_location;
		g_pAuxGeom->SetRenderFlags(SAuxGeomRenderFlags(e_Def3DPublicRenderflags));
		for (uint32 i = 0; i < m_rowparams.m_nParticlesPerRow; i++)
		{
			if (m_rowparams.m_arrParticles[i].m_childID != JointIdType(-1))
			{
				JointIdType jid = m_rowparams.m_arrParticles[i].m_jointID;
				JointIdType cid = m_rowparams.m_arrParticles[i].m_childID;

				Vec3 xaxis = rDefaultSkeleton.GetDefaultAbsJointByID(jid).q.GetColumn0();

				const char* pname = rDefaultSkeleton.GetJointNameByID(jid);
				const Vec3& j0 = rDefaultSkeleton.GetDefaultAbsJointByID(jid).t;
				const Vec3& j1 = rDefaultSkeleton.GetDefaultAbsJointByID(cid).t;
				const f32 jointlen = (j1 - j0).GetLength();
				Vec3 naxis = (j1 - j0).GetNormalized();
				f32 dist = (naxis - xaxis).GetLength();
				if (dist > 0.0001f)
				{
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.3f, ColorF(1, 0, 0, 1), false, "Joint '%s' not aligned with X-Axis", pname), g_YLine += 26.0f;
					g_pAuxGeom->DrawLine(rPhysLocation * j0, RGBA8(0x00, 0xff, 0x00, 0x00), rPhysLocation * j1, RGBA8(0x00, 0xff, 0x00, 0x00));
					g_pAuxGeom->DrawLine(rPhysLocation * j0, RGBA8(0xff, 0x00, 0x00, 0x00), rPhysLocation * (j0 + xaxis * jointlen), RGBA8(0xff, 0x0, 0x00, 0x00));
				}
			}
		}
	}
#endif

	JointState* pJointState = rPoseData.GetJointsStatus();
	for (uint32 i = 0; i < m_rowparams.m_nParticlesPerRow; i++)
	{
		Particle& dm = m_rowparams.m_arrParticles[i];
		if ((pJointState[dm.m_jointID] & (eJS_Orientation | eJS_Position)) != (eJS_Orientation | eJS_Position))
		{
			const uint32 pid = rDefaultSkeleton.GetJointParentIDByID(dm.m_jointID);
			rPoseData.SetJointRelative(dm.m_jointID, rDefaultSkeleton.GetDefaultRelJointByID(dm.m_jointID));
			rPoseData.SetJointAbsolute(dm.m_jointID, rPoseData.GetJointAbsolute(pid) * rDefaultSkeleton.GetDefaultRelJointByID(dm.m_jointID));
			pJointState[dm.m_jointID] = eJS_Orientation | eJS_Position;
		}
	}
	m_rowparams.UpdatePendulumRow(m_pAttachmentManager, rPoseData, GetName());
};

const QuatTS CAttachmentPROW::GetAttWorldAbsolute() const
{
	QuatTS rPhysLocation = m_pAttachmentManager->m_pSkelInstance->m_location;
	return rPhysLocation;
};

uint32 CAttachmentPROW::SetJointName(const char* szJointName)
{
	m_nRowJointID = -1;
	if (!szJointName)  { assert(0); return 0; }
	int nJointID = m_pAttachmentManager->m_pSkelInstance->m_pDefaultSkeleton->GetJointIDByName(szJointName);
	if (nJointID < 0)
	{
		// this is a severe bug if the bone name is invalid and someone tries to attach something
		g_pILog->LogError("AttachObjectToJoint is called for bone \"%s\", which is not in the model \"%s\". Ignoring, but this may cause a crash because the corresponding object won't be detached after it's destroyed", szJointName, m_pAttachmentManager->m_pSkelInstance->m_pDefaultSkeleton->GetModelFilePath());
		return 0;
	}
	m_strRowJointName = szJointName;
	m_nRowJointID = nJointID;
	m_pAttachmentManager->m_TypeSortingRequired++;
	return 1;
};

void CAttachmentPROW::PostUpdateSimulationParams(bool bAttachmentSortingRequired, const char* pJointName)
{
	m_pAttachmentManager->m_TypeSortingRequired += bAttachmentSortingRequired;
	m_rowparams.PostUpdate(m_pAttachmentManager, 0);
};

void CAttachmentPROW::HideInRecursion(uint32 x)
{
	m_pAttachmentManager->OnHideAttachment(this, FLAGS_ATTACH_HIDE_RECURSION, x != 0);

	if (x)
		m_AttFlags |= FLAGS_ATTACH_HIDE_RECURSION;
	else
		m_AttFlags &= ~FLAGS_ATTACH_HIDE_RECURSION;
}

void CAttachmentPROW::HideInShadow(uint32 x)
{
	m_pAttachmentManager->OnHideAttachment(this, FLAGS_ATTACH_HIDE_SHADOW_PASS, x != 0);

	if (x)
		m_AttFlags |= FLAGS_ATTACH_HIDE_SHADOW_PASS;
	else
		m_AttFlags &= ~FLAGS_ATTACH_HIDE_SHADOW_PASS;
}

void CPendulaRow::PostUpdate(const CAttachmentManager* pAttachmentManager, const char* pJointName)
{
	const CCharInstance* pSkelInstance = pAttachmentManager->m_pSkelInstance;
	const CDefaultSkeleton& rDefaultSkeleton = *pSkelInstance->m_pDefaultSkeleton;

	m_fConeAngle = max(0.00f, m_fConeAngle);

	m_nSimFPS = m_nSimFPS < 10 ? 10 : m_nSimFPS;
	m_fMaxAngleCos = clamp_tpl(DEG2COS(max(0.01f, m_fConeAngle)), -1.0f, 1.0f);
	m_fMaxAngleHCos = clamp_tpl(DEG2HCOS(max(0.01f, m_fConeAngle)), 0.0f, 1.0f);

	m_vConeRotHCos.x = clamp_tpl(DEG2HCOS(m_vConeRotation.x), -1.0f, 1.0f);
	m_vConeRotHSin.x = clamp_tpl(DEG2HSIN(m_vConeRotation.x), -1.0f, 1.0f);

	m_vConeRotHCos.y = clamp_tpl(DEG2HCOS(m_vConeRotation.y), -1.0f, 1.0f);
	m_vConeRotHSin.y = clamp_tpl(DEG2HSIN(m_vConeRotation.y), -1.0f, 1.0f);

	m_vConeRotHCos.z = clamp_tpl(DEG2HCOS(m_vConeRotation.z), -1.0f, 1.0f);
	m_vConeRotHSin.z = clamp_tpl(DEG2HSIN(m_vConeRotation.z), -1.0f, 1.0f);

	m_pitchc = clamp_tpl(DEG2HCOS(m_vStiffnessTarget.x), -1.0f, 1.0f);
	m_pitchs = clamp_tpl(DEG2HSIN(m_vStiffnessTarget.x), -1.0f, 1.0f);

	m_rollc = clamp_tpl(DEG2HCOS(m_vStiffnessTarget.y), -1.0f, 1.0f);
	m_rolls = clamp_tpl(DEG2HSIN(m_vStiffnessTarget.y), -1.0f, 1.0f);

	m_fMass = max(0.01f, m_fMass);
	m_vCapsule.x = clamp_tpl(max(0.0f, m_vCapsule.x), 0.0f, 5.0f);
	m_vCapsule.y = clamp_tpl(max(0.0f, m_vCapsule.y), 0.0f, 5.0f);

	m_arrProxyIndex.resize(0);
	uint32 numUsedProxies = m_arrProxyNames.size();
	uint32 numAllProxies = pAttachmentManager->m_arrProxies.size();
	for (uint32 i = 0; i < numUsedProxies; i++)
	{
		uint32 nCRC32lower = CCrc32::ComputeLowercase(m_arrProxyNames[i].c_str());
		for (uint32 p = 0; p < numAllProxies; p++)
		{
			if (nCRC32lower == pAttachmentManager->m_arrProxies[p].m_nProxyCRC32)
			{
				m_arrProxyIndex.push_back(p);
				break;
			}
		}
	}
}

void CPendulaRow::UpdatePendulumRow(const CAttachmentManager* pAttachmentManager, Skeleton::CPoseData& rPoseData, const char* pAttName)
{
	const CCharInstance* pSkelInstance = pAttachmentManager->m_pSkelInstance;
	const CDefaultSkeleton& rDefaultSkeleton = *pSkelInstance->m_pDefaultSkeleton;
	QuatTS rPhysLocation = pSkelInstance->m_location;
	const uint32 nBindPose = pSkelInstance->m_CharEditMode & (CA_BindPose | CA_AllowRedirection);
	if (m_nClampMode == RowSimulationParams::TRANSLATIONAL_PROJECTION)
	{
		bool allowTranslation = nBindPose == 0 && m_useSimulation;
		const uint32 numProxies = allowTranslation && m_nProjectionType && (m_vCapsule.x + m_vCapsule.y) ? m_arrProxyIndex.size() : 0;
		for (uint32 i = 0; i < m_nParticlesPerRow; i++)
		{
			Particle& dm = m_arrParticles[i];
			const QuatT absJoint = rPoseData.GetJointAbsolute(dm.m_jointID);
			Vec3 axis = absJoint.q * m_vTranslationAxis;
			if (numProxies && m_nProjectionType == 2 && m_idxDirTransJoint > -1)
				axis = (rPoseData.GetJointAbsolute(m_idxDirTransJoint).t - absJoint.t).GetNormalized();
			Vec3 trans = absJoint.t;
			for (uint32 x = 0; x < numProxies; x++)
			{
				const CProxy& proxy = pAttachmentManager->m_arrProxies[m_arrProxyIndex[x]];
				Vec3 ipos = (trans - proxy.m_ProxyModelRelative.t) * proxy.m_ProxyModelRelative.q;
				if (m_nProjectionType == 1) //shortvec projections: this one works only with a sphere (the head of the capsule)
					trans += proxy.m_ProxyModelRelative.q * proxy.ShortvecTranslationalProjection(ipos, m_vCapsule.y);
				if (m_nProjectionType == 2)
					trans -= axis * proxy.DirectedTranslationalProjection(ipos, axis * proxy.m_ProxyModelRelative.q, m_vCapsule.x, m_vCapsule.y);
			}
			Vec3 trans_delta = trans - absJoint.t;
			if (trans_delta | trans_delta)
			{
				QuatT& absjoint = (QuatT&)rPoseData.GetJointAbsolute(dm.m_jointID);
				absjoint.t += trans_delta;
				const uint32 p = rDefaultSkeleton.GetJointParentIDByID(dm.m_jointID);
				rPoseData.SetJointRelative(dm.m_jointID, rPoseData.GetJointAbsolute(p).GetInverted() * absjoint);
				if (dm.m_childID != JointIdType(-1))
				{
					const uint32 id = dm.m_childID;
					const uint32 pid = rDefaultSkeleton.GetJointParentIDByID(id);
					rPoseData.SetJointAbsolute(id, rPoseData.GetJointAbsolute(pid) * rPoseData.GetJointRelative(id));
				}
			}

			//--------------------------------------------------------------------------------------------------------------

#ifdef EDITOR_PCDEBUGCODE
			uint32 nDrawDynamicProxies = ((m_vCapsule.x + m_vCapsule.y) && (pAttachmentManager->m_nDrawProxies & 1));
			if (nDrawDynamicProxies || m_useDebugSetup || Console::GetInst().ca_DrawAllSimulatedSockets)
			{
				Vec3 delta = trans - absJoint.t;
				const QuatTS AttLocation = rPhysLocation * absJoint;
				QuatT raddTransformation = QuatT(IDENTITY, delta * absJoint.q);
				QuatTS AttWorldAbsolute = rPhysLocation * absJoint * raddTransformation;
				g_pAuxGeom->SetRenderFlags(SAuxGeomRenderFlags(e_Def3DPublicRenderflags));
				axis = rPhysLocation.q * axis;
				uint32 nValidDirTrans = m_nProjectionType == 2 && m_idxDirTransJoint > -1;
				if (nValidDirTrans)
				{
					Vec3 pos = rPhysLocation * rPoseData.GetJointAbsolute(m_idxDirTransJoint).t;
					g_pAuxGeom->DrawLine(pos, RGBA8(0xff, 0xff, 0xff, 0xff), AttLocation.t, RGBA8(0x7f, 0x00, 0x00, 0xff));
					g_pAuxGeom->DrawLine(AttLocation.t, RGBA8(0x7f, 0x00, 0x00, 0xff), (AttLocation.t - axis * 0.15f), RGBA8(0xff, 0x00, 0x00, 0xff));
				}
				QuatTS cloc(Quat::CreateRotationV0V1(Vec3(1, 0, 0), axis), axis * m_vCapsule.x + AttWorldAbsolute.t, AttWorldAbsolute.s);
				f32 outprojection = sqr(delta);
				Draw(cloc, outprojection ? RGBA8(0x67, 0xf0, 0xac, 0xff) : RGBA8(0x67, 0xa0, 0xbc, 0xff), 4, pSkelInstance->m_Viewdir);
				if (outprojection)
				{
					Vec3 ndir = rPhysLocation.q * delta.GetNormalized();
					f32 tanslation = delta.GetLength() + m_vCapsule.y;
					g_pAuxGeom->DrawLine(AttWorldAbsolute.t, RGBA8(0x00, 0xff, 0x00, 0xff), ndir * tanslation + AttWorldAbsolute.t, RGBA8(0x00, 0xff, 0x00, 0xff));
				}
			}
#endif
		}
		return;
	}

#ifdef EDITOR_PCDEBUGCODE
	QuatT remJointLocation[MAX_JOINT_AMOUNT];
	for (uint32 i = 0; i < m_nParticlesPerRow; i++)
	{
		Particle& dm = m_arrParticles[i];
		remJointLocation[i] = rPoseData.GetJointAbsolute(dm.m_jointID);
	}
#endif

	bool allowSimulation = nBindPose == 0 && m_fConeAngle && m_useSimulation;
	if (allowSimulation == 0)
	{
		for (uint32 i = 0; i < m_nParticlesPerRow; i++)
		{
			Particle& dm = m_arrParticles[i];
			dm.m_vBobVelocity.zero();
			dm.m_vBobPosition = (rPhysLocation * rPoseData.GetJointAbsolute(dm.m_jointID)) * Vec3(m_fRodLength, 0.0f, 0.0f);
		}
	}

	const f32 fIPlaybackScale = pSkelInstance->GetPlaybackScale();
	const f32 fLPlaybackScale = pSkelInstance->m_SkeletonAnim.GetLayerPlaybackScale(0);
	const f32 fAverageFrameTime = g_AverageFrameTime * fIPlaybackScale * fLPlaybackScale ? g_AverageFrameTime * fIPlaybackScale * fLPlaybackScale : g_AverageFrameTime;
	const f32 fPhysFramerate = 1.0f / f32(m_nSimFPS);
	const f32 dt = fAverageFrameTime > fPhysFramerate ? fAverageFrameTime : fPhysFramerate;
	m_fTimeAccumulator += fAverageFrameTime;

	const f32 fMaxAngleHSin = sqrt_tpl(fabs(1.0f - m_fMaxAngleHCos * m_fMaxAngleHCos));
	const f32 fInvMass = 1.0f / m_fMass;

	const Quat qx = Quat(m_vConeRotHCos.x, Vec3(m_vConeRotHSin.x, 0, 0));
	const Quat qyz = Quat(m_vConeRotHCos.y, Vec3(0, m_vConeRotHSin.y, 0)) * Quat(m_vConeRotHCos.z, Vec3(0, 0, m_vConeRotHSin.z));

	f32 speed = pSkelInstance->m_SkeletonAnim.GetCurrentVelocity().GetLength() / 6.0f;
	speed = min(1.0f, speed);
	f32 t0 = 1.0f - speed;
	f32 t1 = speed;

#ifdef EDITOR_PCDEBUGCODE
	if (m_useDebugText)
		if (dt) g_YLine += 8.0f, g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, ColorF(0, 1, 0, 1), false, "dt: %f  fRealFPS: %f    fSimFPS: %f  fStiffness: %f  m_fMaxAngle: %f m_fGravity: %f", dt, 1.0f / dt, f32(m_nSimFPS), m_fJointSpring, m_fConeAngle, m_fGravity), g_YLine += 16.0f;
#endif

	uint16 arrSetupError[MAX_JOINTS_PER_ROW];
	memset(arrSetupError, 0, m_nParticlesPerRow * sizeof(uint16));

	Vec3 arrPrevBobPositions[MAX_JOINTS_PER_ROW];
	f32 arrDamping[MAX_JOINTS_PER_ROW];
	if (allowSimulation)
	{
		const uint32 numProxies = m_nProjectionType && (m_vCapsule.x + m_vCapsule.y) ? m_arrProxyIndex.size() : 0;
		for (uint32 i = 0; i < m_nParticlesPerRow; i++)
		{
			Particle& dm = m_arrParticles[i];
			arrPrevBobPositions[i] = dm.m_vBobPosition;
			arrDamping[i] = m_fDamping;

			const QuatT absJoint = rPoseData.GetJointAbsolute(dm.m_jointID) * qyz;
			const QuatTS AttLocation = rPhysLocation * absJoint;

			(f32&)pAttachmentManager->m_fTurbulenceLocal += m_vTurbulence.x;
			f32 val = pAttachmentManager->m_fTurbulenceGlobal + pAttachmentManager->m_fTurbulenceLocal;
			f32 noise = sin_tpl(val) * m_fDamping * m_vTurbulence.y;
			arrDamping[i] = max(0.0f, m_fDamping * t0 + (m_fDamping + noise) * t1);

			//---------------------------------------------------------------------------------

			const f32 t = 1;
			const QuatT AttLocationLerp = QuatT::CreateNLerp(dm.m_vLocationPrev, QuatT(AttLocation.q, AttLocation.t), t);
			const Vec3 hn = (AttLocationLerp.q * qx).GetColumn2();
			const Vec3 vRodDefN = AttLocationLerp.q.GetColumn0(); //this is the Cone-Axis
			const Vec3 vStiffnessTarget = Quat(m_rollc, m_rolls * vRodDefN) * Quat(m_pitchc, m_pitchs * hn) * vRodDefN;

			if (m_fTimeAccumulator >= fPhysFramerate)
			{
				const Vec3 vAcceleration = vStiffnessTarget * m_fJointSpring * fInvMass - dm.m_vBobVelocity * arrDamping[i] - Vec3(0, 0, m_fGravity);
				dm.m_vBobVelocity += vAcceleration * dt * 0.5f; //Integrating the acceleration over time gives the velocity
				dm.m_vBobPosition += dm.m_vBobVelocity * dt;    //Integrating the acceleration twice over time gives the position.
			}

			Vec3 vRodAnim = dm.m_vBobPosition - AttLocationLerp.t, a; //translate pendulum-position into local space
			if (m_nClampMode == RowSimulationParams::PENDULUM_HINGE_PLANE)
				vRodAnim -= hn * (hn | vRodAnim);
			if (m_nClampMode == RowSimulationParams::PENDULUM_HALF_CONE)
				vRodAnim -= hn * ((hn | vRodAnim) > 0.0f ? hn | vRodAnim : 0);

			for (uint32 x = 0; x < numProxies; x++)
			{
				vRodAnim *= isqrt_tpl(vRodAnim | vRodAnim);
				if (sqrt_tpl(fabs_tpl((vRodDefN | vRodAnim) * 0.5f + 0.5f)) < m_fMaxAngleHCos)
					a = vRodDefN % vRodAnim, vRodAnim = Quat(m_fMaxAngleHCos, a * isqrt_tpl(a | a) * fMaxAngleHSin) * vRodDefN;
				const CProxy& proxy = pAttachmentManager->m_arrProxies[m_arrProxyIndex[x]];
				const QuatT absProxyPrev = rPhysLocation.q * proxy.m_ProxyModelRelativePrev;
				const QuatT absProxyCurr = rPhysLocation.q * proxy.m_ProxyModelRelative;
				const QuatT absProxyLerp = QuatT::CreateNLerp(absProxyPrev, absProxyCurr, t);
				const Vec3 ipos = (Vec3::CreateLerp(dm.m_vAttModelRelativePrev, rPhysLocation.q * absJoint.t, t) - absProxyLerp.t) * absProxyLerp.q;
				if (proxy.GetDistance(ipos, m_vCapsule.y) < 0.0007f)
				{
#ifdef EDITOR_PCDEBUGCODE
					if (pSkelInstance->GetCharEditMode())
					{
						f32 dist = proxy.GetDistance(ipos, m_vCapsule.y);
						g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, ColorF(1, 0, 0, 1), false, "Capsule for '%s' starts inside of proxy: %s (dist: %f)", pAttName, proxy.GetName(), dist), g_YLine += 16.0f;
						arrSetupError[i]++;
					}
#endif
					continue;
				}

				const Vec3 idir = (vRodAnim * absProxyLerp.q).GetNormalized();
				if (proxy.TestOverlapping(ipos, idir, m_vCapsule.x * 2, m_vCapsule.y) >= -0.0000001)
					continue;

				if (m_nProjectionType == 2 && m_nClampMode == RowSimulationParams::PENDULUM_HINGE_PLANE)
					vRodAnim = absProxyLerp.q * proxy.DirectedRotationalProjection(ipos, idir, m_vCapsule.x * 2, m_vCapsule.y, -(hn * absProxyLerp.q).GetNormalized());
				else
					vRodAnim = absProxyLerp.q * proxy.ShortarcRotationalProjection(ipos, idir, m_vCapsule.x * 2, m_vCapsule.y);
			}

			vRodAnim *= isqrt_tpl(vRodAnim | vRodAnim);
			if ((vRodDefN | vRodAnim) < m_fMaxAngleCos)
				a = vRodDefN % vRodAnim, vRodAnim = Quat(m_fMaxAngleHCos, a * isqrt_tpl(a | a) * fMaxAngleHSin) * vRodDefN;
			dm.m_vBobPosition = vRodAnim * m_fRodLength + AttLocationLerp.t; //set correct initial position for next frame
		}                                                                  //for loop

		//-------------------------------------------------------------------------------------------------------------------------------------------------------------------

		Vec3 arrLinks[MAX_JOINTS_PER_ROW];
		Vec3 arrParticles[MAX_JOINTS_PER_ROW];
		for (uint32 i = 0; i < m_nParticlesPerRow; i++)
		{
			Particle& dm = m_arrParticles[i];
			const Vec3& jointpos = rPoseData.GetJointAbsolute(dm.m_jointID).t;
			arrLinks[i] = rPhysLocation * jointpos;
		}

		uint32 relaxLinks = m_nParticlesPerRow - !m_cycle;
		for (uint32 c = 0; c < m_relaxationLoops; c++)
		{
			for (uint32 i = 0; i < m_nParticlesPerRow; i++)
			{
				Particle& dm = m_arrParticles[i];
				Vec3 naxis = (dm.m_vBobPosition - arrLinks[i]).GetNormalized();
				arrParticles[i] = naxis * dm.m_vDistance.y + arrLinks[i];
			}

			for (uint32 i0 = 0; i0 < relaxLinks; i0++)
			{
				Particle& dm = m_arrParticles[i0];
				const f32 max_stretch = dm.m_vDistance.x * (1.0f + m_fStretch);
				const f32 min_stretch = dm.m_vDistance.x * (1.0f - m_fStretch);
				uint32 i1 = (i0 + 1) % m_nParticlesPerRow;
				Vec3& p0 = arrParticles[i0];
				Vec3& p1 = arrParticles[i1];
				Vec3 distance = p0 - p1, d, m;
				f32 sqlen = distance | distance;
				if (sqlen == 0)
					continue;
				f32 isqrt = 1.0f / sqrt_tpl(sqlen) * 0.5f;
				distance.x *= isqrt;
				distance.y *= isqrt;
				distance.z *= isqrt;
				if (sqlen > max_stretch * max_stretch)
					d = distance * max_stretch, m = (p0 + p1) * 0.5f, p0 = m + d, p1 = m - d;
				if (sqlen < min_stretch * min_stretch)
					d = distance * min_stretch, m = (p0 + p1) * 0.5f, p0 = m + d, p1 = m - d;
			}

			for (uint32 i = 0; i < m_nParticlesPerRow; i++)
			{
				Particle& dm = m_arrParticles[i];

				Vec3 s0 = arrLinks[i];
				Vec3 p0 = arrParticles[i];
				dm.m_vBobPosition = s0 + (p0 - s0).GetNormalized() * m_fRodLength;
				const QuatT absJoint = rPoseData.GetJointAbsolute(dm.m_jointID) * qyz;
				const QuatTS AttLocation = rPhysLocation * absJoint;

				const Vec3 op = dm.m_vBobPosition;
				f32 t = 1;
				const QuatT AttLocationLerp = QuatT::CreateNLerp(dm.m_vLocationPrev, QuatT(AttLocation.q, AttLocation.t), t);
				const Vec3 hn = (AttLocationLerp.q * qx).GetColumn2();
				const Vec3 vRodDefN = AttLocationLerp.q.GetColumn0();

				Vec3 vRodAnim = dm.m_vBobPosition - AttLocationLerp.t, a; //translate pendulum-position into local space
				if (m_nClampMode == RowSimulationParams::PENDULUM_HINGE_PLANE)
					vRodAnim -= hn * (hn | vRodAnim);
				if (m_nClampMode == RowSimulationParams::PENDULUM_HALF_CONE)
					vRodAnim -= hn * ((hn | vRodAnim) > 0.0f ? hn | vRodAnim : 0);

				for (uint32 x = 0; x < numProxies; x++)
				{
					vRodAnim *= isqrt_tpl(vRodAnim | vRodAnim);
					if (sqrt_tpl(fabs_tpl((vRodDefN | vRodAnim) * 0.5f + 0.5f)) < m_fMaxAngleHCos)
						a = vRodDefN % vRodAnim, vRodAnim = Quat(m_fMaxAngleHCos, a * isqrt_tpl(a | a) * fMaxAngleHSin) * vRodDefN;
					const CProxy& proxy = pAttachmentManager->m_arrProxies[m_arrProxyIndex[x]];
					const QuatT absProxyPrev = rPhysLocation.q * proxy.m_ProxyModelRelativePrev;
					const QuatT absProxyCurr = rPhysLocation.q * proxy.m_ProxyModelRelative;
					const QuatT absProxyLerp = QuatT::CreateNLerp(absProxyPrev, absProxyCurr, t);
					const Vec3 ipos = (Vec3::CreateLerp(dm.m_vAttModelRelativePrev, rPhysLocation.q * absJoint.t, t) - absProxyLerp.t) * absProxyLerp.q;
					if (proxy.GetDistance(ipos, m_vCapsule.y) < 0.001f)
					{
						f32 dist = proxy.GetDistance(ipos, m_vCapsule.y);
						//g_pAuxGeom->Draw2dLabel( 1, g_YLine, 1.3f, ColorF(0,1,0,1), false,"Capsule for '%s' starts inside of proxy: %s (dist: %f)",pAttName,proxy.GetName(),dist), g_YLine+=16.0f;
						continue;
					}
					const Vec3 idir = (vRodAnim * absProxyLerp.q).GetNormalized();
					if (proxy.TestOverlapping(ipos, idir, m_vCapsule.x * 2, m_vCapsule.y) >= -0.0000001)
						continue;
					if (m_nClampMode == RowSimulationParams::PENDULUM_HINGE_PLANE)
						vRodAnim = absProxyLerp.q * proxy.DirectedRotationalProjection(ipos, idir, m_vCapsule.x * 2, m_vCapsule.y, -(hn * absProxyLerp.q).GetNormalized());
					else
						vRodAnim = absProxyLerp.q * proxy.ShortarcRotationalProjection(ipos, idir, m_vCapsule.x * 2, m_vCapsule.y);
				}

				vRodAnim *= isqrt_tpl(vRodAnim | vRodAnim);
				if ((vRodDefN | vRodAnim) < m_fMaxAngleCos)
					a = vRodDefN % vRodAnim, vRodAnim = Quat(m_fMaxAngleHCos, a * isqrt_tpl(a | a) * fMaxAngleHSin) * vRodDefN;
				dm.m_vBobPosition = vRodAnim * m_fRodLength + AttLocationLerp.t; //set correct initial position for next frame

			}
		} //amount of relaxation loops

		//--------------------------------------------------------------------
		//----                apply to skeleton only once      ---------------
		//--------------------------------------------------------------------
		const uint32 nAllowRedirect = nBindPose & CA_BindPose ? nBindPose & CA_AllowRedirection : 1;
		for (uint32 i = 0; i < m_nParticlesPerRow; i++)
		{
			Particle& dm = m_arrParticles[i];
			const QuatT absJoint = rPoseData.GetJointAbsolute(dm.m_jointID) * qyz;
			const QuatTS AttLocation = rPhysLocation * absJoint;

			//add the simulation-quat to the relative AttModelRelative
			const Vec3 vRodAnimN = (dm.m_vBobPosition - AttLocation.t) * isqrt_tpl(sqr(dm.m_vBobPosition - AttLocation.t));
			const Vec3 hn = (AttLocation.q * qx).GetColumn2();
			const Vec3 vRodDefN = AttLocation.q.GetColumn0(); //this is the Cone-Axis
			const Quat R = Quat((vRodDefN | vRodAnimN) + 1.0f, vRodDefN % vRodAnimN).GetNormalized();
			const f32 t = m_nAnimOverrideJoint > 0 ? 1 - rPoseData.GetJointAbsolute(m_nAnimOverrideJoint).t.x : 1;
			if (m_fTimeAccumulator >= fPhysFramerate)
			{
				const QuatT AttLocationLerp = QuatT::CreateNLerp(dm.m_vLocationPrev, QuatT(AttLocation.q, AttLocation.t), t);
				const Vec3 vRodDefN = AttLocationLerp.q.GetColumn0(); //this is the Cone-Axis
				const Vec3 vStiffnessTarget = Quat(m_rollc, m_rolls * vRodDefN) * Quat(m_pitchc, m_pitchs * hn) * vRodDefN;
				const Vec3 vAcceleration = vStiffnessTarget * m_fJointSpring * fInvMass - dm.m_vBobVelocity * arrDamping[i] - Vec3(0, 0, m_fGravity);
				dm.m_vBobVelocity = (dm.m_vBobPosition - arrPrevBobPositions[i]) / dt + 0.5f * vAcceleration * dt; //Implicit Integral: set correct initial velocity for next frame
				f32 len = dm.m_vBobVelocity.GetLength();
				if (len > m_fMaxVelocity)
					dm.m_vBobVelocity = dm.m_vBobVelocity / len * m_fMaxVelocity;
			}

			Quat raddTransformation = Quat(R.w, R.v * AttLocation.q).GetNormalized();
			if (nAllowRedirect)
			{
				const CDefaultSkeleton& rDefaultSkeleton = *pSkelInstance->m_pDefaultSkeleton;
				QuatT& ajt = (QuatT&)rPoseData.GetJointAbsolute(dm.m_jointID);
				ajt.q.SetNlerp(ajt.q, absJoint.q * raddTransformation, t);
				const uint32 p = rDefaultSkeleton.GetJointParentIDByID(dm.m_jointID);
				rPoseData.SetJointRelative(dm.m_jointID, rPoseData.GetJointAbsolute(p).GetInverted() * ajt);
				if (dm.m_childID != JointIdType(-1))
				{
					const uint32 id = dm.m_childID;
					const uint32 pid = rDefaultSkeleton.GetJointParentIDByID(id);
					rPoseData.SetJointAbsolute(id, rPoseData.GetJointAbsolute(pid) * rPoseData.GetJointRelative(id));
				}
			}

			dm.m_vAttModelRelativePrev = rPhysLocation.q * absJoint.t;

			dm.m_vLocationPrev.q = AttLocation.q;
			dm.m_vLocationPrev.t = AttLocation.t;
		} //loop over all particles in the row

		if (m_fTimeAccumulator >= fPhysFramerate)
			m_fTimeAccumulator -= fPhysFramerate;

#ifdef EDITOR_PCDEBUGCODE
		uint32 nDrawDynamicProxies = ((m_vCapsule.x + m_vCapsule.y) && (pAttachmentManager->m_nDrawProxies & 1));
		if (nDrawDynamicProxies || m_useDebugSetup || Console::GetInst().ca_DrawAllSimulatedSockets)
		{
			g_pAuxGeom->SetRenderFlags(SAuxGeomRenderFlags(e_Def3DPublicRenderflags));

			for (uint32 i = 0; i < m_nParticlesPerRow; i++)
			{
				Particle& dm = m_arrParticles[i];
				Vec3 naxis = (dm.m_vBobPosition - arrLinks[i]).GetNormalized();
				arrParticles[i] = naxis * dm.m_vDistance.y + arrLinks[i];
			}

			for (uint32 i = 0; i < m_nParticlesPerRow && nBindPose == 0; i++)
			{
				uint32 i0 = i;
				uint32 i1 = (i + 1) % m_nParticlesPerRow;
				Vec3 p0 = arrParticles[i0];
				Vec3 p1 = arrParticles[i1];
				if (i0 < i1 || m_cycle)
					g_pAuxGeom->DrawLine(p0, RGBA8(0xff, 0x00, 0x00, 0x00), p1, RGBA8(0xff, 0x00, 0x00, 0x00));
				if (m_useDebugSetup || Console::GetInst().ca_DrawAllSimulatedSockets)
					IRenderAuxText::DrawLabelF(p0, 1, "%d", i0);
			}
		}
#endif

	}

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

#ifdef EDITOR_PCDEBUGCODE
	uint32 nDrawDynamicProxies = ((m_vCapsule.x + m_vCapsule.y) && (pAttachmentManager->m_nDrawProxies & 1));
	if (nDrawDynamicProxies || m_useDebugSetup || Console::GetInst().ca_DrawAllSimulatedSockets)
	{
		for (uint32 i = 0; i < m_nParticlesPerRow; i++)
		{
			Particle& dm = m_arrParticles[i];
			const int nJointID = dm.m_jointID;
			const QuatTS AttLocation = rPhysLocation * remJointLocation[i] * qyz;
			const Vec3 vRodAnimN = (dm.m_vBobPosition - AttLocation.t) * isqrt_tpl(sqr(dm.m_vBobPosition - AttLocation.t));
			g_pAuxGeom->SetRenderFlags(SAuxGeomRenderFlags(e_Def3DPublicRenderflags));
			const QuatT absJoint = rPoseData.GetJointAbsolute(nJointID);
			QuatTS caplocation = rPhysLocation * absJoint;
			QuatTS cloc(Quat::CreateRotationV0V1(Vec3(1, 0, 0), vRodAnimN), vRodAnimN * m_vCapsule.x + caplocation.t, caplocation.s);
			Draw(cloc, arrSetupError[i] ? RGBA8(0xef, 0x3f, 0x1f, 0xff) : RGBA8(0x67, 0xa0, 0xbc, 0xff), 4, pSkelInstance->m_Viewdir);
			uint32 res = (m_useDebugSetup || Console::GetInst().ca_DrawAllSimulatedSockets);
			if (res)
			{
				g_pAuxGeom->DrawLine(dm.m_vBobPosition, RGBA8(0x0f, 0x0f, 0x7f, 0x00), dm.m_vBobPosition + dm.m_vBobVelocity, RGBA8(0x00, 0x00, 0xff, 0x00));

				Vec3 points[0x2000];
				ColorB col0 = RGBA8(0x00, 0xff, 0x00, 0x00);
				ColorB col1 = RGBA8(0x00, 0x7f, 0x00, 0x00);

				//-----------------------------------------------------------------
				const Vec3 vSimulationAxisN = Vec3(1, 0, 0);

				Quat hrot = qx;

				Vec3 ortho1 = hrot.GetColumn2();//*vSimulationAxisO;
				Vec3 ortho2 = ortho1 % vSimulationAxisN;
				Matrix33 m33;
				m33.SetFromVectors(ortho1, vSimulationAxisN, ortho2);
				assert(m33.IsOrthonormalRH());

				{
					f32 bsize = m_fRodLength;
					Vec3 vPAxis = (dm.m_vBobPosition - AttLocation.t) * caplocation.s;
					g_pAuxGeom->DrawLine(AttLocation.t, RGBA8(0xff, 0xff, 0x00, 0x00), AttLocation.t + vPAxis, RGBA8(0xff, 0xff, 0x00, 0x00));
					g_pAuxGeom->DrawSphere(AttLocation.t + vPAxis, 0.05f * bsize * caplocation.s, RGBA8(0x00, 0xff, 0xff, 0));

					const Vec3 vRodDefN = AttLocation.q * vSimulationAxisN * caplocation.s;
					g_pAuxGeom->DrawLine(AttLocation.t, RGBA8(0x0f, 0x0f, 0x0f, 0x00), AttLocation.t + vRodDefN * m_fRodLength, RGBA8(0xff, 0xff, 0xff, 0x00));

					Vec3 hnormal = (AttLocation.q * hrot).GetColumn2();//*vSimulationAxisO;
					Vec3 vStiffnessTarget = Quat(m_rollc, m_rolls * vRodDefN) * Quat(m_pitchc, m_pitchs * hnormal) * vRodDefN * m_fRodLength;
					g_pAuxGeom->DrawLine(AttLocation.t, RGBA8(0xff, 0xff, 0xff, 0x00), AttLocation.t + vStiffnessTarget, RGBA8(0xff, 0xff, 0xff, 0x00));
					g_pAuxGeom->DrawSphere(AttLocation.t + vStiffnessTarget, 0.04f * bsize * caplocation.s, RGBA8(0xff, 0xff, 0xff, 0));
					g_pAuxGeom->DrawSphere(caplocation.t, 0.03f * bsize * caplocation.s, RGBA8(0xff, 0x00, 0x00, 0));
				}
				const Vec3 caxis = Vec3(0.3f, 0, 0);
				if (m_nClampMode == RowSimulationParams::PENDULUM_HALF_CONE || m_nClampMode == RowSimulationParams::PENDULUM_CONE)
				{
					f32 fMaxRad = HCOS2RAD(m_fMaxAngleHCos);
					for (f32 r = -fMaxRad; r < fMaxRad; r += 0.05f)
					{
						Vec3 rvec = Quat::CreateRotationAA(r, ortho1) * caxis;
						g_pAuxGeom->DrawLine(AttLocation.t, RGBA8(0x3f, 0x3f, 0x3f, 0x00), AttLocation * rvec, col1);
					}
					uint32 i0 = 0;
					for (f32 r = -fMaxRad; r < fMaxRad; r = r + 0.01f)
						points[i0++] = AttLocation * (Matrix33::CreateRotationAA(r, ortho1) * caxis);
					g_pAuxGeom->DrawPoints(points, i0, col0);

					if (m_nClampMode == RowSimulationParams::PENDULUM_CONE)
					{
						f32 steps = (gf_PI2 / 100.0f);
						uint32 c = 0;
						Vec3 cvec = Quat::CreateRotationAA(m_fMaxAngleHCos, fMaxAngleHSin, ortho1) * caxis;
						for (f32 r = -gf_PI; r < (gf_PI + steps); r += steps)
						{
							Vec3 rvec = Quat::CreateRotationAA(r, vSimulationAxisN) * cvec;
							points[c++] = AttLocation * rvec;
						}
						c--;

						SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
						renderFlags.SetAlphaBlendMode(e_AlphaBlended);
						renderFlags.SetDrawInFrontMode(e_DrawInFrontOn);
						renderFlags.SetFillMode(e_FillModeSolid);
						g_pAuxGeom->SetRenderFlags(renderFlags);
						vtx_idx arrIndices[400];
						Vec3 arrVertices[400];
						ColorB arrVColors[400];
						uint32 numVertices = 0;
						for (uint32 n = 0; n < c; n++)
						{
							arrVertices[numVertices + 0] = points[n + 0];
							arrVColors[numVertices + 0] = RGBA8(0x00, 0x1f, 0x00, 0xcf);
							arrVertices[numVertices + 1] = points[n + 1];
							arrVColors[numVertices + 1] = RGBA8(0x00, 0x1f, 0x00, 0xcf);
							arrVertices[numVertices + 2] = AttLocation.t;
							arrVColors[numVertices + 2] = RGBA8(0x07, 0x07, 0x07, 0xcf);
							arrIndices[numVertices + 0] = numVertices + 0;
							arrIndices[numVertices + 1] = numVertices + 1;
							arrIndices[numVertices + 2] = numVertices + 2;
							numVertices += 3;
						}
						g_pAuxGeom->DrawTriangles(&arrVertices[0], numVertices, arrIndices, numVertices, &arrVColors[0]);
						numVertices = 0;
						for (uint32 n = 0; n < c; n++)
						{
							arrVertices[numVertices + 0] = points[n + 1];
							arrVColors[numVertices + 0] = RGBA8(0x00, 0x9f, 0x00, 0x7f);
							arrVertices[numVertices + 1] = points[n + 0];
							arrVColors[numVertices + 1] = RGBA8(0x00, 0x9f, 0x00, 0x7f);
							arrVertices[numVertices + 2] = AttLocation.t;
							arrVColors[numVertices + 2] = RGBA8(0x0f, 0x0f, 0x07, 0x7f);
							numVertices += 3;
						}
						g_pAuxGeom->DrawTriangles(&arrVertices[0], numVertices, arrIndices, numVertices, &arrVColors[0]);
					}

					if (m_nClampMode == RowSimulationParams::PENDULUM_HALF_CONE)
					{
						Vec3 cvec = Quat::CreateRotationAA(m_fMaxAngleHCos, fMaxAngleHSin, ortho1) * caxis;
						f32 steps = (gf_PI2 / 100.0f);
						uint32 c = 0;
						for (f32 r = -gf_PI; r < (gf_PI + steps); r += steps)
						{
							Vec3 rvec = Quat::CreateRotationAA(r, vSimulationAxisN) * cvec;
							Vec3 o = rvec * m33;
							if (o.x > 0.0f) o.x = 0;
							rvec = m33 * o;
							points[c++] = AttLocation * rvec;
						}
						c--;

						SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
						renderFlags.SetAlphaBlendMode(e_AlphaBlended);
						renderFlags.SetDrawInFrontMode(e_DrawInFrontOn);
						renderFlags.SetFillMode(e_FillModeSolid);
						g_pAuxGeom->SetRenderFlags(renderFlags);
						vtx_idx arrIndices[400];
						Vec3 arrVertices[400];
						ColorB arrVColors[400];
						uint32 numVertices = 0;
						for (uint32 n = 0; n < c; n++)
						{
							arrVertices[numVertices + 0] = points[n + 0];
							arrVColors[numVertices + 0] = RGBA8(0x00, 0x1f, 0x00, 0xcf);
							arrVertices[numVertices + 1] = points[n + 1];
							arrVColors[numVertices + 1] = RGBA8(0x00, 0x1f, 0x00, 0xcf);
							arrVertices[numVertices + 2] = AttLocation.t;
							arrVColors[numVertices + 2] = RGBA8(0x07, 0x07, 0x07, 0xcf);
							arrIndices[numVertices + 0] = numVertices + 0;
							arrIndices[numVertices + 1] = numVertices + 1;
							arrIndices[numVertices + 2] = numVertices + 2;
							numVertices += 3;
						}
						g_pAuxGeom->DrawTriangles(&arrVertices[0], numVertices, arrIndices, numVertices, &arrVColors[0]);
						numVertices = 0;
						for (uint32 n = 0; n < c; n++)
						{
							arrVertices[numVertices + 0] = points[n + 1];
							arrVColors[numVertices + 0] = RGBA8(0x00, 0x9f, 0x00, 0x7f);
							arrVertices[numVertices + 1] = points[n + 0];
							arrVColors[numVertices + 1] = RGBA8(0x00, 0x9f, 0x00, 0x7f);
							arrVertices[numVertices + 2] = AttLocation.t;
							arrVColors[numVertices + 2] = RGBA8(0x0f, 0x0f, 0x07, 0x7f);
							numVertices += 3;
						}
						g_pAuxGeom->DrawTriangles(&arrVertices[0], numVertices, arrIndices, numVertices, &arrVColors[0]);
					}
				}

				if (m_nClampMode == RowSimulationParams::PENDULUM_HINGE_PLANE)
				{
					f32 fMaxRad = HCOS2RAD(m_fMaxAngleHCos);
					for (f32 r = -fMaxRad; r < fMaxRad; r += 0.05f)
					{
						f32 t0 = (r + fMaxRad) / (fMaxRad * 2);
						f32 t1 = 1.0f - t0;
						Vec3 rvec = Quat::CreateRotationAA(r, ortho1) * caxis;
						g_pAuxGeom->DrawLine(AttLocation.t, RGBA8(0x3f, 0x3f, 0x3f, 0x00), AttLocation * rvec, RGBA8(uint8(0x00 * t0 + 0x7f * t1), uint8(0x7f * t0 + 0x00 * t1), 0x00, 0x00));
					}
					uint32 i0 = 0;
					for (f32 r = -fMaxRad; r < fMaxRad; r = r + 0.01f)
						points[i0++] = AttLocation * (Matrix33::CreateRotationAA(r, ortho1) * caxis);
					g_pAuxGeom->DrawPoints(points, i0, col0);
				}
			} // if(res)

		} //for loop

	}

#endif

}

void CPendulaRow::Draw(const QuatTS& qt, const ColorB clr, const uint32 tesselation, const Vec3& vdir)
{
	uint32 subdiv = 4 * tesselation;
	uint32 stride = (subdiv + 1) * (tesselation + 1);
	Matrix34 m34 = Matrix34(qt);
	Matrix33 m33 = Matrix33(m34);
	Matrix33 my(IDENTITY), rely;
	rely.SetRotationY(gf_PI2 / f32(subdiv));
	Matrix33 mx(IDENTITY), relx;
	relx.SetRotationX(gf_PI2 / f32(subdiv));
	Vec3 ldir0 = Vec3(vdir.x, vdir.y, 0).GetNormalized() * 0.5f;
	ldir0.z = f32(sqrt3 * -0.5f);
	Vec3 ldir1(0, 0, 1);
	f32 ca = m_vCapsule.x, cr = m_vCapsule.y;
	Vec3 p3d[0x2000], _p[0x2000];
	ColorB c3d[0x2000], _c[0x2000];
	static struct
	{
		ILINE ColorB shader(Vec3 n, const Vec3& d0, const Vec3& d1, ColorB c)
		{
			f32 a = 0.5f * n | d0, b = 0.1f * n | d1, l = min(1.0f, fabs_tpl(a) - a + fabs_tpl(b) - b + 0.05f);
			return RGBA8(uint8(l * c.r), uint8(l * c.g), uint8(l * c.b), c.a);
		}
	} s;
	for (uint32 a = 0, b = 0; a < tesselation + 1; a++, my *= rely)
	{
		mx.SetIdentity();
		for (uint32 i = 0; i < subdiv + 1; i++, b++, mx *= relx)
		{
			Vec3 v = mx * Vec3(my.m02, my.m12, my.m22);
			p3d[b] = m34 * Vec3(-ca - v.x * cr, v.y * cr, v.z * cr);
			p3d[b + stride] = m34 * Vec3(+ca + v.x * cr, v.y * cr, v.z * cr);
			c3d[b] = s.shader(m33 * Vec3(-v.x, v.y, v.z), ldir0, ldir1, clr);
			c3d[b + stride] = s.shader(m33 * Vec3(v.x, v.y, v.z), ldir0, ldir1, clr);
		}
	}
	uint32 c = 0, p = 0;
	for (uint32 i = 0; i < subdiv; i++)
	{
		_p[p++] = p3d[i + 0], _c[c++] = c3d[i + 0], _p[p++] = p3d[i + stride + 0], _c[c++] = c3d[i + stride + 0];
		_p[p++] = p3d[i + 1], _c[c++] = c3d[i + 1], _p[p++] = p3d[i + stride + 1], _c[c++] = c3d[i + stride + 1];
		_p[p++] = p3d[i + 1], _c[c++] = c3d[i + 1], _p[p++] = p3d[i + stride + 0], _c[c++] = c3d[i + stride + 0];
	}
	for (uint32 a = 0, vc = 0; a < tesselation; a++, vc++)
	{
		for (uint32 i = 0; i < subdiv; i++, vc++)
		{
			_p[p++] = p3d[vc + 1], _c[c++] = c3d[vc + 1], _p[p++] = p3d[vc + subdiv + 1], _c[c++] = c3d[vc + subdiv + 1];
			_p[p++] = p3d[vc + 0], _c[c++] = c3d[vc + 0], _p[p++] = p3d[vc + subdiv + 1], _c[c++] = c3d[vc + subdiv + 1];
			_p[p++] = p3d[vc + 1], _c[c++] = c3d[vc + 1], _p[p++] = p3d[vc + subdiv + 2], _c[c++] = c3d[vc + subdiv + 2];
			_p[p++] = p3d[vc + 0 + stride], _c[c++] = c3d[vc + 0 + stride], _p[p++] = p3d[vc + stride + subdiv + 1], _c[c++] = c3d[vc + stride + subdiv + 1];
			_p[p++] = p3d[vc + 1 + stride], _c[c++] = c3d[vc + 1 + stride], _p[p++] = p3d[vc + stride + subdiv + 2], _c[c++] = c3d[vc + stride + subdiv + 2];
			_p[p++] = p3d[vc + 1 + stride], _c[c++] = c3d[vc + 1 + stride], _p[p++] = p3d[vc + stride + subdiv + 1], _c[c++] = c3d[vc + stride + subdiv + 1];
		}
	}
	uint32 rflag = e_Mode3D | e_AlphaNone | e_DrawInFrontOff | e_FillModeSolid | e_CullModeNone | e_DepthWriteOn | e_DepthTestOn;
	SAuxGeomRenderFlags renderFlags(rflag);
	g_pAuxGeom->SetRenderFlags(renderFlags);
	g_pAuxGeom->DrawTriangles(&_p[0], p, &_c[0]);
}
