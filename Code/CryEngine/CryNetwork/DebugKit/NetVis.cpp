// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NetVis.h"

#if ENABLE_DEBUG_KIT

	#include "Context/ClientContextView.h"
	#include "NetCVars.h"
	#include <CryEntitySystem/IEntitySystem.h>
	#include <CryRenderer/IRenderAuxGeom.h>

static void xyztorgb(Vec3 xyz, float a, ColorF& rgb)
{
	rgb.r = 3.0651f * xyz.x + -1.3942f * xyz.y + -0.4761f * xyz.z;
	rgb.g = -0.9690f * xyz.x + 1.8755f * xyz.y + 0.0415f * xyz.z;
	rgb.b = 0.0679f * xyz.x + -0.2290f * xyz.y + 1.0698f * xyz.z;
	for (int i = 0; i < 3; i++)
		rgb[i] = CLAMP(rgb[i], 0, 1);
	rgb.a = a;
}

void CNetVis::AddSample(SNetObjectID obj, uint16 prop, float value)
{
	SStatistic& stat = m_stats[SIndex(obj, prop)];
	stat.total += value;
	stat.samples.push(value);
}

void CNetVis::Update()
{
	ASSERT_GLOBAL_LOCK;

	int mode = CNetCVars::Get().VisMode;
	if (mode <= 0 || mode > 100)
		return;
	mode--;

	bool is3D = mode < 10;
	mode %= 10;

	CTimeValue window = CTimeValue(float(CNetCVars::Get().VisWindow));

	SMinMax& mm = m_minmax[mode];
	float& avMax = mm.maxVal;
	float& avMin = mm.minVal;
	int cnt = 0;

	for (TStatsMap::iterator iter = m_stats.begin(); iter != m_stats.end(); )
	{
		TStatsMap::iterator next = iter;
		++next;

		SStatistic& stat = iter->second;
		while (!stat.samples.empty() && (g_time - stat.samples.front().when) > window)
		{
			stat.total -= stat.samples.front().value;
			stat.samples.pop();
		}
		if (stat.samples.empty())
		{
			m_stats.erase(iter);
		}
		else if (iter->first.prop == mode)
		{
			if ((g_time - stat.samples.front().when).GetSeconds() > 0.5f)
				stat.average = stat.total / (g_time - stat.samples.front().when).GetSeconds();
			else
				stat.average = 0;

			if (stat.average > avMax)
				avMax = stat.average;
			if (stat.average < avMin)
				avMin = stat.average;

			cnt++;
		}

		iter = next;
	}

	if (!cnt || avMax <= avMin)
		return;

	float reda[4] = { 1, 0, 0, 1 };
	float greena[4] = { 0, 1, 0, 1 };
	IRenderAuxText::Draw2dLabel(10, 10, 1.3f, greena, false, "%.3f", avMin);
	IRenderAuxText::Draw2dLabel(10, 30, 1.3f, reda, false, "%.3f", avMax);

	IRenderAuxGeom* pRAG = gEnv->pRenderer->GetIRenderAuxGeom();

	Vec3 witnessPos;
	Vec3 witnessDir;
	#if FULL_ON_SCHEDULING
	bool haveWitnessPos = m_pCtxView->GetWitnessPosition(witnessPos);
	bool haveWitnessDir = m_pCtxView->GetWitnessDirection(witnessDir);
	#else
	bool haveWitnessPos = false;
	bool haveWitnessDir = false;
	#endif
	if (!is3D && (!haveWitnessDir || !haveWitnessPos))
		;
	else
	{
		if (is3D)
			pRAG->SetRenderFlags(e_Mode3D | e_AlphaBlended | e_DrawInFrontOff | e_FillModeSolid | e_CullModeBack | e_DepthWriteOn | e_DepthTestOn);
		else
			pRAG->SetRenderFlags(e_Def2DPublicRenderflags | e_AlphaBlended);

		for (TStatsMap::iterator iter = m_stats.lower_bound(SIndex(SNetObjectID(0, 0), mode)); iter != m_stats.end() && iter->first.prop == mode; ++iter)
		{
			if (iter->first.obj == m_pCtxView->GetWitness())
				continue;

			SContextObjectRef obj = m_pCtxView->ContextState()->GetContextObject(iter->first.obj);
			if (!obj.main)
				continue;

			const Vec3 red(0.430314f, 0.22188f, 0.020184f);
			const Vec3 green(0.341649f, 0.706839f, 0.129621f);
			const float t = (iter->second.average - avMin) / (avMax - avMin);
			ColorF clr;
			xyztorgb(red * t + green * (1 - t), 0.4f, clr);

			IEntity* pEnt = gEnv->pEntitySystem->GetEntity(obj.main->userID);
			if (!pEnt)
				continue;

			if (is3D)
			{
				AABB bbox;
				pEnt->GetWorldBounds(bbox);

				pRAG->DrawAABB(bbox, true, clr, eBBD_Faceted);
			}
			else
			{
				Vec3 pos = Quat::CreateRotationV0V1(witnessDir, Vec3(0, -1, 0)) * (pEnt->GetWorldPos() - witnessPos);
				pos /= 1000.0f;
				pos += Vec3(0.5f, 0.5f, -pos.z);

				const int n = 5;
				float r = 4.0f / 800.0f;
				for (int i = 0; i < n; i++)
				{
					float a0 = gf_PI2 * i / n;
					float a1 = gf_PI2 * (i + 1) / n;

					Vec3 p0 = pos + r * Vec3(0.75f * cosf(a0), sinf(a0), 0);
					Vec3 p1 = pos + r * Vec3(0.75f * cosf(a1), sinf(a1), 0);
					pRAG->DrawLine(p0, clr, p1, clr);
				}
			}
		}
	}

	float avMid = (avMax + avMin) * 0.5f;
	float g = gEnv->pTimer->GetFrameTime() / 4;
	avMax = (1 - g) * avMax + g * avMid;
	avMin = (1 - g) * avMin + g * avMid;
}

#endif
