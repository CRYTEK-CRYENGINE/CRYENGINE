// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SocketSimulation.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include "AttachmentManager.h"
#include "CharacterInstance.h"

void CSimulation::PostUpdate(const CAttachmentManager* pAttachmentManager, const char* pJointName)
{
	const CCharInstance* pSkelInstance = pAttachmentManager->m_pSkelInstance;
	const CDefaultSkeleton& rDefaultSkeleton = *pSkelInstance->m_pDefaultSkeleton;

	m_fMaxAngle = max(0.00f, m_fMaxAngle);
	m_vDiskRotation.x = max(0.00f, m_vDiskRotation.x);
	m_vDiskRotation.y = max(0.00f, m_vDiskRotation.y);

	m_vSphereScale.x = max(0.01f, m_vSphereScale.x);
	m_vSphereScale.y = max(0.01f, m_vSphereScale.y);

	m_nSimFPS = m_nSimFPS < 10 ? 10 : m_nSimFPS;
	m_fMaxAngleCos = clamp_tpl(DEG2COS(max(0.01f, m_fMaxAngle)), -1.0f, 1.0f);
	m_fMaxAngleHCos = clamp_tpl(DEG2HCOS(max(0.01f, m_fMaxAngle)), 0.0f, 1.0f);
	m_fHRotationHCosX = clamp_tpl(DEG2HCOS(m_vDiskRotation.x), -1.0f, 1.0f);
	m_fHRotationHCosZ = clamp_tpl(DEG2HCOS(m_vDiskRotation.y), -1.0f, 1.0f);
	m_fMass = max(0.01f, m_fMass);
	m_vSimulationAxis = m_vSimulationAxis.GetLength() < 0.01f ? Vec3(0, 0.5f, 0) : m_vSimulationAxis; //y-pos is the default axis
	m_vCapsule.x = clamp_tpl(max(0.0f, m_vCapsule.x), 0.0f, 5.0f);
	m_vCapsule.y = clamp_tpl(max(0.0f, m_vCapsule.y), 0.0f, 5.0f);

	m_idxDirTransJoint = -1;
	const char* pDirTransJoint = m_strDirTransJoint.c_str();
	if (pDirTransJoint && pJointName)
	{
		uint32 IsNotIdentical = stricmp(pDirTransJoint, pJointName);
		if (IsNotIdentical)
			m_idxDirTransJoint = rDefaultSkeleton.GetJointIDByName(pDirTransJoint);
	}
	m_aa1c = cos_tpl(m_vStiffnessTarget.x);
	m_aa1s = sin_tpl(m_vStiffnessTarget.x);
	m_aa2c = cos_tpl(m_vStiffnessTarget.y);
	m_aa2s = sin_tpl(m_vStiffnessTarget.y);
	m_crcProcFunction = m_strProcFunction.length() ? CCrc32::Compute(m_strProcFunction.c_str()) : 0;

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

	if (m_nClampType == 0)
		m_useRedirect = 0;
}

void CSimulation::UpdateSimulation(const CAttachmentManager* pAttachmentManager, Skeleton::CPoseData& rPoseData, int32 nRedirectionID, const QuatT& rAttModelRelative, QuatT& raddTransformation, const char* pAttName)
{
	raddTransformation.SetIdentity();

	if (m_nClampType == SimulationParams::PENDULUM_CONE || m_nClampType == SimulationParams::PENDULUM_HINGE_PLANE || m_nClampType == SimulationParams::PENDULUM_HALF_CONE)
		UpdatePendulumSimulation(pAttachmentManager, rPoseData, nRedirectionID, rAttModelRelative, raddTransformation, pAttName);
	if (m_nClampType == SimulationParams::SPRING_ELLIPSOID)
		UpdateSpringSimulation(pAttachmentManager, rPoseData, nRedirectionID, rAttModelRelative, raddTransformation.t, pAttName);
	if (m_nClampType == SimulationParams::TRANSLATIONAL_PROJECTION)
		ProjectJointOutOfLozenge(pAttachmentManager, rPoseData, nRedirectionID, rAttModelRelative, pAttName);
}

void CSimulation::UpdatePendulumSimulation(const CAttachmentManager* pAttachmentManager, Skeleton::CPoseData& rPoseData, int32 nRedirectionID, const QuatT& rAttModelRelative, QuatT& raddTransformation, const char* pAttName)
{
	if (m_nClampType == 0)
		return;
	if (m_useRedirect && nRedirectionID < 0)
		return;

	const CCharInstance* pSkelInstance = pAttachmentManager->m_pSkelInstance;
	const f32 fIPlaybackScale = pSkelInstance->GetPlaybackScale();
	const f32 fLPlaybackScale = pSkelInstance->m_SkeletonAnim.GetLayerPlaybackScale(0);
	const f32 fAverageFrameTime = g_AverageFrameTime * fIPlaybackScale * fLPlaybackScale ? g_AverageFrameTime * fIPlaybackScale * fLPlaybackScale : g_AverageFrameTime;
	const f32 ps = fabs(fAverageFrameTime) > 0.00001f ? clamp_tpl(fAverageFrameTime, 0.001f, 0.1f) : 0.0f;
	const f32 fTS = clamp_tpl(ceilf(ps * f32(m_nSimFPS)), 1.0f, 15.0f);
	const f32 dt = ps / fTS;  //delta-time per time-step;

	const QuatTS& rPhysLocation = pSkelInstance->m_location;
	const QuatTS AttLocation = rPhysLocation * rAttModelRelative;
	const f32 fSimulationAxisSq = m_vSimulationAxis | m_vSimulationAxis;
	const f32 fSimulationAxisLen = sqrt_tpl(fSimulationAxisSq);
	const Vec3 vSimulationAxisN = m_vSimulationAxis / fSimulationAxisLen;  //initial pendulum direction
	const f32 fHRotationHSinX = sqrt_tpl(fabs(1.0f - m_fHRotationHCosX * m_fHRotationHCosX));
	const f32 fMaxAngleHSin = sqrt_tpl(fabs(1.0f - m_fMaxAngleHCos * m_fMaxAngleHCos));
	const f32 fInvMass = 1.0f / m_fMass;
	const Vec3 vSimulationAxisO = vSimulationAxisN.GetOrthogonal().GetNormalized();

	const uint32 nBindPose = pSkelInstance->m_CharEditMode & (CA_BindPose | CA_AllowRedirection);
	const uint32 nAllowRedirect = nBindPose & CA_BindPose ? nBindPose & CA_AllowRedirection : 1;
	uint32 nSetupError = 0;

	if (nBindPose == 0 && m_fMaxAngle && m_useSimulation && fSimulationAxisSq && sqr(m_vBobPosition))
	{
		const uint32 numProxies = m_nProjectionType && (m_vCapsule.x + m_vCapsule.y) ? m_arrProxyIndex.size() : 0;
		for (f32 its = 0; its < fTS; its = its + 1.0f)
		{
			const Vec3 vPositionPrev = m_vBobPosition;
			const f32 t = (its + 1.0f) / fTS;
			const QuatT AttLocationLerp = QuatT::CreateNLerp(m_vAttLocationPrev, QuatT(AttLocation.q, AttLocation.t), t);
			const Vec3 hn = AttLocationLerp.q * Quat(m_fHRotationHCosX, fHRotationHSinX * vSimulationAxisN) * vSimulationAxisO;
			const Vec3 vRodDefN = AttLocationLerp.q * vSimulationAxisN; //this is the Cone-Axis
			const Vec3 vStiffnessTarget = Quat(m_aa2c, m_aa2s * vRodDefN) * Quat(m_aa1c, m_aa1s * hn) * vRodDefN;

			const Vec3 vAcceleration = vStiffnessTarget * m_fStiffness * fInvMass - m_vBobVelocity * m_fDamping - Vec3(0, 0, m_fGravity);
			m_vBobVelocity += vAcceleration * dt * 0.5f; //Integrating the acceleration over time gives the velocity
			m_vBobPosition += m_vBobVelocity * dt;       //Integrating the acceleration twice over time gives the position.

			Vec3 vRodAnim = m_vBobPosition - AttLocationLerp.t, a; //translate pendulum-position into local space
			if (m_nClampType == SimulationParams::PENDULUM_HINGE_PLANE)
				vRodAnim -= hn * (hn | vRodAnim);
			if (m_nClampType == SimulationParams::PENDULUM_HALF_CONE)
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
				const Vec3 ipos = (Vec3::CreateLerp(m_vAttModelRelativePrev, rPhysLocation.q * rAttModelRelative.t, t) - absProxyLerp.t) * absProxyLerp.q;
				if (proxy.GetDistance(ipos, m_vCapsule.y) < 0.001f)
				{
#ifdef EDITOR_PCDEBUGCODE
					if (pSkelInstance->GetCharEditMode())
					{
						f32 dist = proxy.GetDistance(ipos, m_vCapsule.y);
						g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, ColorF(1, 0, 0, 1), false, "Capsule for '%s' starts inside of proxy: %s (dist: %f)", pAttName, proxy.GetName(), dist), g_YLine += 16.0f;
						nSetupError++;
					}
#endif
					continue;
				}

				const Vec3 idir = (vRodAnim * absProxyLerp.q).GetNormalized();
				if (proxy.TestOverlapping(ipos, idir, m_vCapsule.x * 2, m_vCapsule.y) >= -0.0000001)
					continue;

				if (m_nProjectionType == 2 && m_nClampType == SimulationParams::PENDULUM_HINGE_PLANE)
					vRodAnim = absProxyLerp.q * proxy.DirectedRotationalProjection(ipos, idir, m_vCapsule.x * 2, m_vCapsule.y, -(hn * absProxyLerp.q).GetNormalized());
				else
					vRodAnim = absProxyLerp.q * proxy.ShortarcRotationalProjection(ipos, idir, m_vCapsule.x * 2, m_vCapsule.y);
			}

			vRodAnim *= isqrt_tpl(vRodAnim | vRodAnim);
			if ((vRodDefN | vRodAnim) < m_fMaxAngleCos)
				a = vRodDefN % vRodAnim, vRodAnim = Quat(m_fMaxAngleHCos, a * isqrt_tpl(a | a) * fMaxAngleHSin) * vRodDefN;
			m_vBobPosition = vRodAnim * fSimulationAxisLen + AttLocationLerp.t; //set correct initial position for next frame
			if (dt)
			{
				const Vec3 vAcceleration = vStiffnessTarget * m_fStiffness * fInvMass - m_vBobVelocity * m_fDamping - Vec3(0, 0, m_fGravity);
				m_vBobVelocity = (m_vBobPosition - vPositionPrev) / dt + 0.5f * vAcceleration * dt; //Implicit Integral: set correct initial velocity for next frame
			}

#ifdef EDITOR_PCDEBUGCODE
			if (m_useDebugText)
			{
				if (dt) g_YLine += 8.0f, g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, ColorF(0, 1, 0, 1), false, "its: %f  dt: %f  fRealFPS: %f    fSimFPS: %f  fStiffness: %f  m_fMaxAngle: %f", (its + 1.0f) / fTS, dt, 1.0f / dt, f32(m_nSimFPS), m_fStiffness, m_fMaxAngle), g_YLine += 16.0f;
				g_pAuxGeom->DrawLine(m_vBobPosition, RGBA8(0x0f, 0x0f, 0x7f, 0x00), m_vBobPosition + m_vBobVelocity, RGBA8(0x00, 0x00, 0xff, 0x00));
			}
#endif
		} //split time-step loop

	}
	else
	{
		m_vBobVelocity.zero();
		m_vBobPosition = (rPhysLocation * rAttModelRelative) * m_vSimulationAxis;
	}

	//add the simulation-quat to the relative AttModelRelative
	const Vec3 vRodAnimN = (m_vBobPosition - AttLocation.t) * isqrt_tpl(sqr(m_vBobPosition - AttLocation.t));
	const Vec3 vRodDefN = AttLocation.q * vSimulationAxisN; //this is the Cone-Axis
	const Quat R = Quat((vRodDefN | vRodAnimN) + 1.0f, vRodDefN % vRodAnimN).GetNormalized();
	const f32 t = m_nAnimOverrideJoint > 0 ? 1 - rPoseData.GetJointAbsolute(m_nAnimOverrideJoint).t.x : 1;
	m_vAttModelRelativePrev = rPhysLocation.q * rAttModelRelative.t;
	raddTransformation.q = Quat(R.w, R.v * AttLocation.q).GetNormalized();
	raddTransformation.t = raddTransformation.q * m_vPivotOffset;
	m_vAttLocationPrev.q = AttLocation.q;
	m_vAttLocationPrev.t = AttLocation.t;
	if (nAllowRedirect && m_useRedirect)
	{
		const CDefaultSkeleton& rDefaultSkeleton = *pSkelInstance->m_pDefaultSkeleton;
		QuatT ajt = rPoseData.GetJointAbsolute(nRedirectionID);
		ajt.SetNLerp(ajt, rAttModelRelative * raddTransformation, t);
		const uint32 p = rDefaultSkeleton.GetJointParentIDByID(nRedirectionID);
		rPoseData.SetJointAbsolute(nRedirectionID, ajt);
		rPoseData.SetJointRelative(nRedirectionID, rPoseData.GetJointAbsolute(p).GetInverted() * ajt);
		uint32 numChildren = m_arrChildren.size();
		for (uint32 i = 0; i < numChildren; i++)
		{
			const uint32 id = m_arrChildren[i];
			const uint32 pid = rDefaultSkeleton.GetJointParentIDByID(id);
			rPoseData.SetJointAbsolute(id, rPoseData.GetJointAbsolute(pid) * rPoseData.GetJointRelative(id));
		}
	}

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

#ifdef EDITOR_PCDEBUGCODE
	uint32 nDrawDynamicProxies = ((m_vCapsule.x + m_vCapsule.y) && (pAttachmentManager->m_nDrawProxies & 1));
	if (nDrawDynamicProxies || m_useDebugSetup || Console::GetInst().ca_DrawAllSimulatedSockets)
	{
		g_pAuxGeom->SetRenderFlags(SAuxGeomRenderFlags(e_Def3DPublicRenderflags));
		QuatTS caplocation = rPhysLocation * rAttModelRelative * raddTransformation.q;
		QuatTS cloc(Quat::CreateRotationV0V1(Vec3(1, 0, 0), vRodAnimN), vRodAnimN * m_vCapsule.x + caplocation.t, caplocation.s);
		Draw(cloc, nSetupError ? RGBA8(0xef, 0x3f, 0x1f, 0xff) : RGBA8(0x67, 0xa0, 0xbc, 0xff), 4, pSkelInstance->m_Viewdir);
		if (m_useDebugSetup == 0 && Console::GetInst().ca_DrawAllSimulatedSockets == 0)
			return;

		Vec3 points[0x2000];
		ColorB col0 = RGBA8(0x00, 0xff, 0x00, 0x00);
		ColorB col1 = RGBA8(0x00, 0x7f, 0x00, 0x00);

		//-----------------------------------------------------------------
		Quat hrot(m_fHRotationHCosX, fHRotationHSinX * vSimulationAxisN);
		Vec3 ortho1 = hrot * vSimulationAxisO;
		Vec3 ortho2 = ortho1 % vSimulationAxisN;
		Matrix33 m33;
		m33.SetFromVectors(ortho1, vSimulationAxisN, ortho2);
		assert(m33.IsOrthonormalRH());

		{
			QuatTS AttWorldAbsolute = rPhysLocation * rAttModelRelative * raddTransformation;
			f32 bsize = m_vSimulationAxis.GetLength();
			Vec3 vPAxis = (m_vBobPosition - AttLocation.t) * AttWorldAbsolute.s;
			g_pAuxGeom->DrawLine(AttLocation.t, RGBA8(0xff, 0xff, 0x00, 0x00), AttLocation.t + vPAxis, RGBA8(0xff, 0xff, 0x00, 0x00));
			g_pAuxGeom->DrawSphere(AttLocation.t + vPAxis, 0.05f * bsize * AttWorldAbsolute.s, RGBA8(0x00, 0xff, 0xff, 0));

			const Vec3 vRodDefN = AttLocation.q * vSimulationAxisN * AttWorldAbsolute.s;
			g_pAuxGeom->DrawLine(AttLocation.t, RGBA8(0x0f, 0x0f, 0x0f, 0x00), AttLocation.t + vRodDefN * fSimulationAxisLen, RGBA8(0xff, 0xff, 0xff, 0x00));

			Vec3 hnormal = AttLocation.q * hrot * vSimulationAxisO;
			Vec3 vStiffnessTarget = Quat(m_aa2c, m_aa2s * vRodDefN) * Quat(m_aa1c, m_aa1s * hnormal) * vRodDefN * fSimulationAxisLen;
			g_pAuxGeom->DrawLine(AttLocation.t, RGBA8(0xff, 0xff, 0xff, 0x00), AttLocation.t + vStiffnessTarget, RGBA8(0xff, 0xff, 0xff, 0x00));
			g_pAuxGeom->DrawSphere(AttLocation.t + vStiffnessTarget, 0.04f * bsize * AttWorldAbsolute.s, RGBA8(0xff, 0xff, 0xff, 0));

			g_pAuxGeom->DrawSphere(AttWorldAbsolute.t, 0.03f * bsize * AttWorldAbsolute.s, RGBA8(0xff, 0x00, 0x00, 0));
		}

		if (m_nClampType == SimulationParams::PENDULUM_HALF_CONE || m_nClampType == SimulationParams::PENDULUM_CONE)
		{
			f32 fMaxRad = HCOS2RAD(m_fMaxAngleHCos);
			for (f32 r = -fMaxRad; r < fMaxRad; r += 0.05f)
			{
				Vec3 rvec = Quat::CreateRotationAA(r, ortho1) * m_vSimulationAxis;
				g_pAuxGeom->DrawLine(AttLocation.t, RGBA8(0x3f, 0x3f, 0x3f, 0x00), AttLocation * rvec, col1);
			}
			uint32 i0 = 0;
			for (f32 r = -fMaxRad; r < fMaxRad; r = r + 0.01f)
				points[i0++] = AttLocation * (Matrix33::CreateRotationAA(r, ortho1) * m_vSimulationAxis);
			g_pAuxGeom->DrawPoints(points, i0, col0);

			if (m_nClampType == SimulationParams::PENDULUM_CONE)
			{
				f32 steps = (gf_PI2 / 100.0f);
				uint32 c = 0;
				Vec3 cvec = Quat::CreateRotationAA(m_fMaxAngleHCos, fMaxAngleHSin, ortho1) * m_vSimulationAxis;
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

			if (m_nClampType == SimulationParams::PENDULUM_HALF_CONE)
			{
				Vec3 cvec = Quat::CreateRotationAA(m_fMaxAngleHCos, fMaxAngleHSin, ortho1) * m_vSimulationAxis;
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

		if (m_nClampType == SimulationParams::PENDULUM_HINGE_PLANE)
		{
			f32 fMaxRad = HCOS2RAD(m_fMaxAngleHCos);
			for (f32 r = -fMaxRad; r < fMaxRad; r += 0.05f)
			{
				f32 t0 = (r + fMaxRad) / (fMaxRad * 2);
				f32 t1 = 1.0f - t0;
				Vec3 rvec = Quat::CreateRotationAA(r, ortho1) * m_vSimulationAxis;
				g_pAuxGeom->DrawLine(AttLocation.t, RGBA8(0x3f, 0x3f, 0x3f, 0x00), AttLocation * rvec, RGBA8(uint8(0x00 * t0 + 0x7f * t1), uint8(0x7f * t0 + 0x00 * t1), 0x00, 0x00));
			}
			uint32 i0 = 0;
			for (f32 r = -fMaxRad; r < fMaxRad; r = r + 0.01f)
				points[i0++] = AttLocation * (Matrix33::CreateRotationAA(r, ortho1) * m_vSimulationAxis);
			g_pAuxGeom->DrawPoints(points, i0, col0);
		}
	}

	if (m_useDebugText && m_useRedirect)
	{
		const CDefaultSkeleton& rDefaultSkeleton = *pSkelInstance->m_pDefaultSkeleton;
		uint32 numChildren = m_arrChildren.size();
		g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, ColorF(0, 1, 0, 1), false, "children of '%s':", pAttName), g_YLine += 16.0f;
		for (uint32 i = 0; i < numChildren; i++)
		{
			uint32 id = m_arrChildren[i];
			const char* pChildName = rDefaultSkeleton.GetJointNameByID(id);
			g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, ColorF(0, 1, 0, 1), false, "id %d  %s", id, pChildName), g_YLine += 16.0f;
			//	uint32 pid = rDefaultSkeleton.GetJointParentIDByID(id);
			//	const QuatT& cajt = rPoseData.GetJointAbsolute(id);
			//	const QuatT& pajt = rPoseData.GetJointAbsolute(pid);
			//	g_pAuxGeom->DrawLine( rPhysLocation*pajt.t,RGBA8(0x7f,0x0f,0x7f,0x00),  rPhysLocation*cajt.t,RGBA8(0xff,0xff,0xff,0x00) );
			//	break;
		}
	}
#endif

}

/*!
 * Update spring simulation
 *
 * The method is determining the actual acting forces in model-space coordinates. Thus, precision is
 * independent of the actual world-position. Eventually, the forces for the character animation and
 * belonging world-space rotation is determined in model-space - whereas the forces caused by the
 * world-space transformation are transformed to model-space and added supplementary.
 * Variable names ending with WS and MS indicate their reference space - i.e. world-space and model-space.
 */

void CSimulation::UpdateSpringSimulation(const CAttachmentManager* pAttachmentManager, Skeleton::CPoseData& rPoseData, int32 nRedirectionID, const QuatT& rAttModelRelative, Vec3& raddTranslation, const char* pAttName)
{
	if (m_nClampType == 0)
		return;
	if (m_useRedirect && nRedirectionID < 0)
		return;

	const CCharInstance* pSkelInstance = pAttachmentManager->m_pSkelInstance;

	// Determine timestep dt & timesteps per frame (fTS)
	const f32 fIPlaybackScale = pSkelInstance->GetPlaybackScale();
	const f32 fLPlaybackScale = pSkelInstance->m_SkeletonAnim.GetLayerPlaybackScale(0);
	const f32 fAverageFrameTime = g_AverageFrameTime * fIPlaybackScale * fLPlaybackScale ? g_AverageFrameTime * fIPlaybackScale * fLPlaybackScale : g_AverageFrameTime;
	const f32 ps = fabs(fAverageFrameTime) > 0.00001f ? clamp_tpl(fAverageFrameTime, 0.001f, 0.1f) : 0.0f;
	const f32 fTS = clamp_tpl(ceilf(ps * f32(m_nSimFPS)), 1.0f, 15.0f);
	const f32 dt = ps / fTS; // Delta-time per time-step;

	const QuatTS& rPhysLocationWS = pSkelInstance->m_location;        // Actual node in world-space
	const QuatTS AttLocationWS = rPhysLocationWS * rAttModelRelative; // Attachments location in world-space
	const f32 fHRotationHSinX = sqrt_tpl(fabs(1.0f - m_fHRotationHCosX * m_fHRotationHCosX));
	const f32 fHRotationHSinZ = sqrt_tpl(fabs(1.0f - m_fHRotationHCosZ * m_fHRotationHCosZ));
	const f32 fInvMass = 1.0f / m_fMass;

	// Determine Attachments location in model-space (MS)
	// Therefore, split animation into model- and world-space, to get high numerical precision
	// Only world-space delta-translation dx is determined in world-space
	const QuatT AttLocationMSWithoutTranslateWS = QuatT(rPhysLocationWS.q, Vec3(0)) * rAttModelRelative;
	const Vec3 AttLocationMSTranslateWS = (rPhysLocationWS.t - m_rPhysLocationWSPrev.t) * rPhysLocationWS.q;
	QuatT AttLocationMS = AttLocationMSWithoutTranslateWS;
	AttLocationMS.t += AttLocationMSTranslateWS;

	const uint32 numProxies = m_arrProxyIndex.size();
	const uint32 nBindPose = pSkelInstance->m_CharEditMode & (CA_BindPose | CA_AllowRedirection);
	if (nBindPose == 0 && m_useSimulation && sqr(m_vSpringBobPositionMS))
	{
		f32 sx = m_fRadius, sy = m_fRadius, sz = m_fRadius;
		for (f32 its = 0; its < fTS; its = its + 1.0f)
		{
			const f32 t01 = (its + 1.0f) / fTS;
			const Vec3 vSpringBobPositionMSPrev = m_vSpringBobPositionMS;

			// Determine acceleration in model-space (MS) and integrate
			QuatT AttLocationMSLerp = QuatT::CreateNLerp(m_vAttLocationMSWithoutTranslateWSPrev, QuatT(AttLocationMSWithoutTranslateWS.q, AttLocationMSWithoutTranslateWS.t), t01);
			AttLocationMSLerp.t += AttLocationMSTranslateWS;

			const Vec3 vSpringAcceleration = (m_vStiffnessTarget + AttLocationMSLerp.t - m_vSpringBobPositionMS) * m_fStiffness * fInvMass - m_vSpringBobVelocityMS * m_fDamping - Vec3(0, 0, m_fGravity);
			m_vSpringBobVelocityMS += vSpringAcceleration * dt * 0.5f; // Integrating the acceleration over time gives the velocity
			m_vSpringBobPositionMS += m_vSpringBobVelocityMS * dt;     // Integrating the velocity to get position

			// Set WS velocity and position as well, for homogeneity reasons (with pendulum simulation) and elder debug rendering
			const QuatT AttLocationWSLerp = QuatT::CreateNLerp(m_vAttLocationPrev, QuatT(AttLocationWS.q, AttLocationWS.t), t01);
			m_vBobPosition = rPhysLocationWS.t + m_vSpringBobPositionMS;
			m_vBobVelocity = m_vSpringBobVelocityMS;

			// Collision handling according to proxies
			for (uint32 x = 0; x < numProxies; x++)
			{
				const CProxy& proxy = pAttachmentManager->m_arrProxies[m_arrProxyIndex[x]];
				const QuatT absProxyPrev = QuatT(rPhysLocationWS.q, rPhysLocationWS.t) * proxy.m_ProxyModelRelativePrev;
				const QuatT absProxyCurr = QuatT(rPhysLocationWS.q, rPhysLocationWS.t) * proxy.m_ProxyModelRelative;
				const QuatT absProxyLerp = QuatT::CreateNLerp(absProxyPrev, absProxyCurr, t01);
				Vec3 ipos = (m_vBobPosition - absProxyLerp.t) * absProxyLerp.q; // multiply on rhs, thus inverse rotation
				if (m_nProjectionType)                                          // shortvec projection
				{
					m_vSpringBobPositionMS += rPhysLocationWS.q * proxy.m_ProxyModelRelative.q * proxy.ShortvecTranslationalProjection(ipos, m_vCapsule.y);
				}
			}

			// Clamp spring to non-uniform ellipsoid
			Vec3 vSpringPos = m_vSpringBobPositionMS - AttLocationMSLerp.t; // actual spring extension
			f32 l = sqrt_tpl(vSpringPos | vSpringPos);
			if (l > 0.0001f)
			{
				Quat rotX = Quat(m_fHRotationHCosX, fHRotationHSinX * AttLocationMSLerp.q.GetColumn0());
				Quat rotZ = Quat(m_fHRotationHCosZ, fHRotationHSinZ * AttLocationMSLerp.q.GetColumn2());
				Quat h2 = rotZ * rotX * AttLocationMSLerp.q;
				Vec3 vNDir = vSpringPos / l;
				Vec3 vODir = vNDir * h2;
				sz = vODir.z < 0 ? m_fRadius * m_vSphereScale.y : m_fRadius * m_vSphereScale.x;
				f32 fMaxDist = sqrt_tpl(sqr(vODir * isqrt_tpl(sqr(vODir / Vec3(sx, sy, sz))))); // max distance to the surface of the ellipsoid
				if (l > fMaxDist)
				{
					Vec3 pn;
					pn = h2 * Vec3(vODir.x / (sx * sx), vODir.y / (sy * sy), vODir.z / (sz * sz));
					pn.Normalize();
					vSpringPos -= pn * ((pn | vSpringPos) - ((vNDir * fMaxDist) | pn));
				}
				l = sqrt_tpl(vSpringPos | vSpringPos);
				f32 mb = max(sx, sz);
				vSpringPos = l > mb ? vSpringPos / l * mb : vSpringPos;
			}

			// Update spring position
			m_vSpringBobPositionMS = vSpringPos + AttLocationMSLerp.t; // set MS position, in case spring length has changed
			m_vBobPosition = vSpringPos + AttLocationWSLerp.t;         // set WS position as well

			if (dt)
			{
				// Determine new velocity (by integration) according to updated position
				const Vec3 vSpringAcceleration = (m_vStiffnessTarget + AttLocationMSLerp.t - m_vSpringBobPositionMS) * m_fStiffness * fInvMass - m_vSpringBobVelocityMS * m_fDamping - Vec3(0, 0, m_fGravity);
				m_vSpringBobVelocityMS = (m_vSpringBobPositionMS - vSpringBobPositionMSPrev) / dt + vSpringAcceleration * dt * 0.5f; // Implicit Integral: set correct initial velocity for next frame

				// Set WS velocity as well
				m_vBobVelocity = m_vSpringBobVelocityMS;
			}
#ifdef EDITOR_PCDEBUGCODE
			if (m_useDebugText)
			{
				if (dt) g_YLine += 8.0f, g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, ColorF(0, 1, 0, 1), false, "its: %f  dt: %f  fRealFPS: %f    fSimFPS: %f  fStiffness: %f  m_fRadius: %f", (its + 1.0f) / fTS, dt, 1.0f / dt, f32(m_nSimFPS), m_fStiffness, m_fRadius), g_YLine += 16.0f;
				g_pAuxGeom->DrawLine(m_vBobPosition, RGBA8(0x0f, 0x0f, 0x7f, 0x00), m_vBobPosition + m_vBobVelocity, RGBA8(0x00, 0x00, 0xff, 0x00));
			}
#endif
		} //split time-step loop

	}
	else
	{
		// Init positions & velocities in MS
		m_vSpringBobVelocityMS.zero();
		const QuatT AttLocationMSWithoutTranslateWS = QuatT(rPhysLocationWS.q, Vec3(0)) * rAttModelRelative;
		m_vSpringBobPositionMS = AttLocationMSWithoutTranslateWS.t;

		// Init positions & velocities in WS
		m_vBobVelocity.zero();
		m_vBobPosition = (rPhysLocationWS * rAttModelRelative).t;
	}

	// Set delta translation in MS
	raddTranslation = (m_vSpringBobPositionMS - AttLocationMS.t) * rPhysLocationWS.q * rAttModelRelative.q.GetNormalized() + m_vPivotOffset;

	// Redirect to joint
	const uint32 nAllowRedirect = nBindPose & CA_BindPose ? nBindPose & CA_AllowRedirection : 1;
	if (m_useRedirect && nAllowRedirect)
	{
		const CDefaultSkeleton& rDefaultSkeleton = *pSkelInstance->m_pDefaultSkeleton;
		raddTranslation *= m_nAnimOverrideJoint > 0 ? 1 - rPoseData.GetJointAbsolute(m_nAnimOverrideJoint).t.x : 1;
		QuatT ajt = rPoseData.GetJointAbsolute(nRedirectionID);
		ajt.t = rAttModelRelative.t + rAttModelRelative.q * raddTranslation;
		const uint32 p = rDefaultSkeleton.GetJointParentIDByID(nRedirectionID);
		rPoseData.SetJointAbsolute(nRedirectionID, ajt);
		rPoseData.SetJointRelative(nRedirectionID, rPoseData.GetJointAbsolute(p).GetInverted() * ajt);
		uint32 numChildren = m_arrChildren.size();
		for (uint32 i = 0; i < numChildren; i++)
		{
			const uint32 id = m_arrChildren[i];
			const uint32 pid = rDefaultSkeleton.GetJointParentIDByID(id);
			rPoseData.SetJointAbsolute(id, rPoseData.GetJointAbsolute(pid) * rPoseData.GetJointRelative(id));
		}
	}

	// Store some states for differentiation in the next step
	m_vAttLocationPrev.q = AttLocationWS.q;
	m_vAttLocationPrev.t = AttLocationWS.t;
	m_rPhysLocationWSPrev.q = rPhysLocationWS.q;
	m_rPhysLocationWSPrev.t = rPhysLocationWS.t;
	m_vAttLocationMSWithoutTranslateWSPrev.q = AttLocationMSWithoutTranslateWS.q;
	m_vAttLocationMSWithoutTranslateWSPrev.t = AttLocationMSWithoutTranslateWS.t;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
#ifdef EDITOR_PCDEBUGCODE
	uint32 nDrawDynamicProxies = m_vCapsule.y && (pAttachmentManager->m_nDrawProxies & 1);
	if (nDrawDynamicProxies || m_useDebugSetup || Console::GetInst().ca_DrawAllSimulatedSockets)
	{
		g_pAuxGeom->SetRenderFlags(SAuxGeomRenderFlags(e_Def3DPublicRenderflags));
		QuatTS spherelocation = rPhysLocationWS * rAttModelRelative * QuatT(IDENTITY, raddTranslation - m_vPivotOffset);
		QuatTS cloc(IDENTITY, spherelocation.t, spherelocation.s);
		Draw(cloc, RGBA8(0x67, 0xa0, 0xbc, 0xff), 16, pSkelInstance->m_Viewdir);
		if (m_useDebugSetup == 0 && Console::GetInst().ca_DrawAllSimulatedSockets == 0)
			return;

		SAuxGeomRenderFlags renderFlags(e_Def3DPublicRenderflags);
		g_pAuxGeom->SetRenderFlags(renderFlags);
		Vec3 arrPoints[1120], arrVertices[6200];
		ColorB col0 = RGBA8(0x00, 0xff, 0x00, 0x00);
		ColorB col1 = RGBA8(0x00, 0x7f, 0x00, 0x00);

		QuatTS AttWorldAbsolute = rPhysLocationWS * rAttModelRelative * QuatT(IDENTITY, raddTranslation);
		Vec3 vStiffnessTarget = AttLocationWS.t + m_vStiffnessTarget;
		g_pAuxGeom->DrawSphere(vStiffnessTarget, 0.035f * AttLocationWS.s, RGBA8(0xff, 0xff, 0xff, 0));

		g_pAuxGeom->DrawSphere(AttWorldAbsolute.t, 0.03f * AttLocationWS.s, RGBA8(0xff, 0x00, 0x00, 0));
		g_pAuxGeom->DrawSphere(m_vBobPosition, 0.04f * AttLocationWS.s, RGBA8(0x00, 0x00, 0xff, 0));
		g_pAuxGeom->DrawLine(vStiffnessTarget, RGBA8(0xff, 0xff, 0x00, 0x00), m_vBobPosition, RGBA8(0xff, 0xff, 0x00, 0x00));

		Quat hrx = Quat(m_fHRotationHCosX, fHRotationHSinX * AttLocationWS.q.GetColumn0());
		Quat hrz = Quat(m_fHRotationHCosZ, fHRotationHSinZ * AttLocationWS.q.GetColumn2());
		Quat hingherot = hrz * hrx;
		g_pAuxGeom->DrawSphere(AttLocationWS.t, 0.035f * AttLocationWS.s, RGBA8(0xff, 0xff, 0xff, 0));

		Vec3 xaxis = AttLocationWS.q.GetColumn0();
		Vec3 rotaxis = AttLocationWS.q.GetColumn2();
		for (f32 r = -gf_PI; r < gf_PI; r += 0.1f)
		{
			Vec3 rvec = hingherot * Quat::CreateRotationAA(r, rotaxis) * xaxis * m_fRadius * AttLocationWS.s;
			g_pAuxGeom->DrawLine(AttLocationWS.t, RGBA8(0x0f, 0x0f, 0x0f, 0x00), AttLocationWS.t + rvec, RGBA8(0x3f, 0xff, 0x3f, 0x00));
		}

		renderFlags.SetAlphaBlendMode(e_AlphaBlended);
		renderFlags.SetDepthWriteFlag(e_DepthWriteOff);
		renderFlags.SetCullMode(e_CullModeNone);
		g_pAuxGeom->SetRenderFlags(renderFlags);
		uint32 c = 0;
		Matrix33 sp33;
		sp33.SetScale(Vec3(m_fRadius, m_fRadius, m_fRadius * m_vSphereScale.x));
		Matrix33 mp33 = Matrix33(hingherot) * Matrix33(Matrix34(AttLocationWS)) * sp33;
		for (f32 cz = 0; cz < gf_PI * 0.5f + 0.001f; cz = cz + (gf_PI / 32))
		{
			f32 vs, vc;
			sincos_tpl(cz, &vs, &vc);
			for (f32 cy = 0; cy < gf_PI2; cy = cy + (gf_PI2 / 64))
				arrPoints[c++] = mp33 * Vec3(cos_tpl(cy) * vs, sin_tpl(cy) * vs, vc) + AttLocationWS.t;
		} //g_pAuxGeom->DrawPoints( points,c,col0);

		uint32 numVertices = 0;
		for (uint32 c = 0; c < 16; c++)
		{
			uint32 r = c * 65;
			for (uint32 n = 0; n < 64; n++)
			{
				arrVertices[numVertices + 0] = arrPoints[r + n + 1];
				arrVertices[numVertices + 1] = arrPoints[r + n + 0];
				arrVertices[numVertices + 2] = arrPoints[r + n + 65];
				numVertices += 3;
				arrVertices[numVertices + 0] = arrPoints[r + n + 1];
				arrVertices[numVertices + 1] = arrPoints[r + n + 65];
				arrVertices[numVertices + 2] = arrPoints[r + n + 66];
				numVertices += 3;
			}
		}
		g_pAuxGeom->DrawTriangles(&arrVertices[0], numVertices, RGBA8(0x00, 0x7f, 0x3f, 0x6f));

		c = 0;
		Matrix33 sn33;
		sn33.SetScale(Vec3(m_fRadius, m_fRadius, m_fRadius * m_vSphereScale.y));
		Matrix33 mn33 = Matrix33(hingherot) * Matrix33(Matrix34(AttLocationWS)) * sn33;
		for (f32 cz = 0; cz < gf_PI * 0.5f + 0.001f; cz = cz + (gf_PI / 32))
		{
			f32 vs, vc;
			sincos_tpl(cz, &vs, &vc);
			for (f32 cy = 0; cy < gf_PI2; cy = cy + (gf_PI2 / 64))
				arrPoints[c++] = mn33 * Vec3(cos_tpl(cy) * vs, sin_tpl(cy) * vs, -vc) + AttLocationWS.t;
		} //g_pAuxGeom->DrawPoints( points,c,col1);

		for (uint32 c = 0, numVertices = 0; c < 16; c++)
		{
			uint32 r = c * 65;
			for (uint32 n = 0; n < 64; n++)
			{
				arrVertices[numVertices + 0] = arrPoints[r + n + 0];
				arrVertices[numVertices + 1] = arrPoints[r + n + 1];
				arrVertices[numVertices + 2] = arrPoints[r + n + 65];
				numVertices += 3;
				arrVertices[numVertices + 0] = arrPoints[r + n + 65];
				arrVertices[numVertices + 1] = arrPoints[r + n + 1];
				arrVertices[numVertices + 2] = arrPoints[r + n + 66];
				numVertices += 3;
			}
		}
		g_pAuxGeom->DrawTriangles(&arrVertices[0], numVertices, RGBA8(0x3f, 0x7f, 0x00, 0x6f));
	}

	if (m_useDebugText && m_useRedirect)
	{
		const CDefaultSkeleton& rDefaultSkeleton = *pSkelInstance->m_pDefaultSkeleton;
		uint32 numChildren = m_arrChildren.size();
		g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, ColorF(0, 1, 0, 1), false, "children of '%s':", pAttName), g_YLine += 16.0f;
		for (uint32 i = 0; i < numChildren; i++)
		{

			uint32 id = m_arrChildren[i];
			const char* pChildName = rDefaultSkeleton.GetJointNameByID(id);
			g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, ColorF(0, 1, 0, 1), false, "id %d  %s", id, pChildName), g_YLine += 16.0f;
			//	uint32 pid = rDefaultSkeleton.GetJointParentIDByID(id);
			//	const QuatT& cajt = rPoseData.GetJointAbsolute(id);
			//	const QuatT& pajt = rPoseData.GetJointAbsolute(pid);
			//	g_pAuxGeom->DrawLine( rPhysLocation*pajt.t,RGBA8(0x7f,0x0f,0x7f,0x00),  rPhysLocation*cajt.t,RGBA8(0xff,0xff,0xff,0x00) );
			//	break;
		}
	}
#endif

}

void CSimulation::ProjectJointOutOfLozenge(const CAttachmentManager* pAttachmentManager, Skeleton::CPoseData& rPoseData, int32 nRedirectionID, const QuatT& rAttModelRelative, const char* pAttName)
{
	if (m_nClampType == 0)
		return;
	if (m_useRedirect && nRedirectionID < 0)
		return;

	const CCharInstance* pSkelInstance = pAttachmentManager->m_pSkelInstance;
	const CDefaultSkeleton& rDefaultSkeleton = *pSkelInstance->m_pDefaultSkeleton;
	const uint32 nBindPose = pSkelInstance->m_CharEditMode & (CA_BindPose | CA_AllowRedirection);
	const uint32 nAllowRedirect = nBindPose & CA_BindPose ? nBindPose & CA_AllowRedirection : 1;
	const QuatTS& rPhysLocation = pSkelInstance->m_location;

	const f32 fSimulationAxisSq = m_vSimulationAxis | m_vSimulationAxis;
	const f32 fSimulationAxisLen = sqrt_tpl(fSimulationAxisSq);
	const Vec3 vSimulationAxisN = m_vSimulationAxis / fSimulationAxisLen;  //initial pendulum direction

	Vec3 axis = rAttModelRelative.q * vSimulationAxisN;
	const uint32 numProxies = (m_vCapsule.x + m_vCapsule.y) ? m_arrProxyIndex.size() : 0;
	if (numProxies && m_nProjectionType == 2 && m_idxDirTransJoint > -1)
		axis = (rPoseData.GetJointAbsolute(m_idxDirTransJoint).t - rAttModelRelative.t).GetNormalized();
	Vec3 trans = rAttModelRelative.t;
	if (nAllowRedirect)
	{
		if (m_nProjectionType && m_useSimulation)
		{
			for (uint32 x = 0; x < numProxies; x++)
			{
				const CProxy& proxy = pAttachmentManager->m_arrProxies[m_arrProxyIndex[x]];
				Vec3 ipos = (trans - proxy.m_ProxyModelRelative.t) * proxy.m_ProxyModelRelative.q;
				if (m_nProjectionType == 1) //shortvec projections: this one works only with a sphere (the head of the capsule)
					trans += proxy.m_ProxyModelRelative.q * proxy.ShortvecTranslationalProjection(ipos, m_vCapsule.y);
				if (m_nProjectionType == 2)
					trans -= axis * proxy.DirectedTranslationalProjection(ipos, axis * proxy.m_ProxyModelRelative.q, m_vCapsule.x, m_vCapsule.y);
			}
		}
		Vec3 trans_delta = trans - rAttModelRelative.t;
		if (sqr(trans_delta) || sqr(m_vPivotOffset))
		{
			QuatT absjoint = rPoseData.GetJointAbsolute(nRedirectionID);
			absjoint.q = rAttModelRelative.q;
			absjoint.t += trans_delta + rAttModelRelative.q * m_vPivotOffset;
			const uint32 p = rDefaultSkeleton.GetJointParentIDByID(nRedirectionID);
			rPoseData.SetJointAbsolute(nRedirectionID, absjoint);
			rPoseData.SetJointRelative(nRedirectionID, rPoseData.GetJointAbsolute(p).GetInverted() * absjoint);
			uint32 numChildren = m_arrChildren.size();
			for (uint32 i = 0; i < numChildren; i++)
			{
				const uint32 id = m_arrChildren[i];
				const uint32 pid = rDefaultSkeleton.GetJointParentIDByID(id);
				rPoseData.SetJointAbsolute(id, rPoseData.GetJointAbsolute(pid) * rPoseData.GetJointRelative(id));
			}
		}
	}

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

#ifdef EDITOR_PCDEBUGCODE
	uint32 nDrawDynamicProxies = ((m_vCapsule.x + m_vCapsule.y) && (pAttachmentManager->m_nDrawProxies & 1));
	if (nDrawDynamicProxies || m_useDebugSetup || Console::GetInst().ca_DrawAllSimulatedSockets)
	{
		if (m_useDebugText)
		{
			if (m_nProjectionType == 1)
				g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, ColorF(0, 1, 0, 1), false, "Projection Type:  shortvec"), g_YLine += 16.0f;
			if (m_nProjectionType == 2)
			{
				const uint32 numProxies = (m_vCapsule.x + m_vCapsule.y) ? m_arrProxyIndex.size() : 0;
				if (numProxies && m_idxDirTransJoint > -1)
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, ColorF(0, 1, 0, 1), false, "Projection Type:  directed joint-segment"), g_YLine += 16.0f;
				else
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, ColorF(0, 1, 0, 1), false, "Projection Type:  directed translation-axis"), g_YLine += 16.0f;
			}
		}
		if (0)
		{
			Vec3 axisX = rAttModelRelative.q.GetColumn0();
			Vec3 axisY = rAttModelRelative.q.GetColumn1();
			Vec3 axisZ = rAttModelRelative.q.GetColumn2();
			Vec3 vSocketWpos = (rPhysLocation * rAttModelRelative).t;
			g_pAuxGeom->DrawLine(vSocketWpos, RGBA8(0xff, 0x00, 0x00, 0xff), vSocketWpos + axisX * 10, RGBA8(0xff, 0x00, 0x00, 0xff));
			g_pAuxGeom->DrawLine(vSocketWpos, RGBA8(0x00, 0xff, 0x00, 0xff), vSocketWpos + axisY * 10, RGBA8(0x00, 0xff, 0x00, 0xff));
			g_pAuxGeom->DrawLine(vSocketWpos, RGBA8(0x00, 0x00, 0xff, 0xff), vSocketWpos + axisZ * 10, RGBA8(0x00, 0x00, 0xff, 0xff));
		}
		Vec3 delta = trans - rAttModelRelative.t;
		const QuatTS AttLocation = rPhysLocation * rAttModelRelative;
		QuatT raddTransformation = QuatT(IDENTITY, delta * rAttModelRelative.q);
		QuatTS AttWorldAbsolute = rPhysLocation * rAttModelRelative * raddTransformation;
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

			if (m_useDebugText)
			{
				uint32 numChildren = m_arrChildren.size();
				g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, ColorF(0, 1, 0, 1), false, "children of '%s':", pAttName), g_YLine += 16.0f;
				for (uint32 i = 0; i < numChildren; i++)
				{
					uint32 id = m_arrChildren[i];
					const char* pChildName = rDefaultSkeleton.GetJointNameByID(id);
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.3f, ColorF(0, 1, 0, 1), false, "id %d  %s", id, pChildName), g_YLine += 16.0f;
				}
			}
		}

	}

#endif

}

void CSimulation::Draw(const QuatTS& qt, const ColorB clr, const uint32 tesselation, const Vec3& vdir)
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
