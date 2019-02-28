// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AIAnchor.h"

#include <CryAISystem/IAgent.h>
#include <CryAISystem/IAIObject.h>
#include "IIconManager.h"
#include "Gizmos/AxisHelper.h"
#include "Objects/DisplayContext.h"

REGISTER_CLASS_DESC(CAIAnchorClassDesc);

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CAIAnchor, CEntityObject)

//////////////////////////////////////////////////////////////////////////
float CAIAnchor::m_helperScale = 1;

//////////////////////////////////////////////////////////////////////////
CAIAnchor::CAIAnchor()
{
	m_entityClass = "AIAnchor";
	UseMaterialLayersMask(false);
}

//////////////////////////////////////////////////////////////////////////
bool CAIAnchor::Init(CBaseObject* prev, const string& file)
{
	SetColor(RGB(0, 255, 0));
	return __super::Init(prev, file);
}

//////////////////////////////////////////////////////////////////////////
float CAIAnchor::GetRadius()
{
	return 0.5f * m_helperScale * gGizmoPreferences.helperScale;
}

//////////////////////////////////////////////////////////////////////////
void CAIAnchor::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	const Matrix34& wtm = GetWorldTM();

	//CHANGED_BY_IVO
	Vec3 x = wtm.TransformVector(Vec3(0, -1, 0));

	Vec3 wp = wtm.GetTranslation();

	if (IsFrozen())
		dc.SetFreezeColor();
	else
		dc.SetColor(GetColor());

	Matrix34 tm = wtm;
	objRenderHelper.Render(tm);

	if (IsSelected())
	{
		dc.SetColor(GetColor());

		dc.PushMatrix(wtm);
		float r = GetRadius();
		dc.DrawWireBox(-Vec3(r, r, r), Vec3(r, r, r));

		IAIObject* pAIObj = GetIEntity() ? GetIEntity()->GetAI() : 0;

		if (pAIObj && pAIObj->GetRadius() > 0.f)
		{
			dc.SetColor(GetColor());
			dc.DrawWireSphere(Vec3(0, 0, 0), pAIObj->GetRadius());
		}

		// Display detail only for small selections.
		bool bDrawDetail = pAIObj && (pAIObj->GetAIType() == AIANCHOR_COMBAT_HIDESPOT) && (GetIEditorImpl()->GetSelection()->GetCount() < 10);

		if (bDrawDetail)
		{
			// Draw cone which shows the hide direction.
			dc.SetColor(ColorB(255, 255, 255, 255));

			// Build the circle of the cone.
			Vec3 circle[21];
			const float angle = acos_tpl(HIDESPOT_COVERAGE_ANGLE_COS);
			const float d = tanf(angle);
			const float m = 4.0f * r;
			for (int i = 0; i <= 20; i++)
			{
				float a = ((float)i / 20.0f) * gf_PI * 2.0f;
				float cx = cosf(a);
				float cy = sinf(a);
				circle[i].x = cx * d * m;
				circle[i].y = m;
				circle[i].z = cy * d * m;
			}
			// Draw the circle of the cone.
			dc.DrawPolyLine(circle, 21);
			// Draw four side lines.
			dc.DrawLine(Vec3(0, 0, 0), circle[0] * 1.5f);
			dc.DrawLine(Vec3(0, 0, 0), circle[5] * 1.5f);
			dc.DrawLine(Vec3(0, 0, 0), circle[10] * 1.5f);
			dc.DrawLine(Vec3(0, 0, 0), circle[15] * 1.5f);
		}

		dc.PopMatrix();

		if (bDrawDetail)
		{
			IAISystem* pAISystem = GetIEditorImpl()->GetSystem()->GetAISystem();

			// Draw helper indicating how the AI system will use the hide.
			Vec3 pos(wtm.GetTranslation());
			Vec3 dir(wtm.TransformVector(Vec3(0, 1, 0)));
			Vec3 right(dir.y, -dir.x, 0.0f);
			right.NormalizeSafe();

			const float agentRad(0.5f);   // Approx human agent radius.
			const float testHeight(0.7f); // Approx human crouch weapon height.

			pAISystem->AdjustDirectionalCoverPosition(pos, dir, agentRad, testHeight);
			// Draw the are the agent occupies.
			const int npts(20);
			Vec3 circle[npts + 1];
			// Collision
			dc.SetColor(GetColor(), 0.3f);
			for (int i = 0; i < npts; i++)
			{
				float a = ((float)i / (float)npts) * gf_PI2;
				circle[i] = pos + right * (cosf(a) * agentRad) + dir * (sinf(a) * agentRad);
			}
			for (int i = 0; i < npts; i++)
				dc.DrawTri(pos, circle[i], circle[(i + 1) % npts]);
			// Safe
			dc.SetColor(RGB(255, 0, 0), 0.5f);
			for (int i = 0; i <= 20; i++)
			{
				float a = ((float)i / (float)npts) * gf_PI2;
				circle[i] = pos + right * (cosf(a) * (agentRad + 0.25f)) + dir * (sinf(a) * (agentRad + 0.25f));
			}
			dc.DrawPolyLine(circle, npts + 1);

			Vec3 groundPos(pos - Vec3(0, 0, testHeight));

			// Direction to ground.
			dc.SetColor(RGB(255, 0, 0), 0.75f);
			dc.DrawLine(pos, groundPos + Vec3(0, 0, 0.15f));

			// Approx strafe line
			dc.SetColor(RGB(220, 16, 0), 0.5f);
			Vec3 strafePos(groundPos + Vec3(0, 0, 0.15f));
			dc.DrawQuad(strafePos - right * 2.5f, strafePos - right * 2.5f - dir * 0.05f,
			            strafePos + right * 2.5f - dir * 0.05f, strafePos + right * 2.5f);
			// Approx crouch shoot height
			Vec3 crouchPos(groundPos + Vec3(0, 0, 0.5f) + dir * (0.7f + agentRad));
			dc.DrawQuad(crouchPos - right * 0.6f + dir * 0.3f, crouchPos - right * 0.6f,
			            crouchPos + right * 0.6f, crouchPos + right * 0.6f + dir * 0.3f);
			// Approx standing shoot height.
			Vec3 standingPos(groundPos + Vec3(0, 0, 1.2f) + dir * (0.7f + agentRad));
			dc.DrawQuad(standingPos - right * 0.6f + dir * 0.3f, standingPos - right * 0.6f,
			            standingPos + right * 0.6f, standingPos + right * 0.6f + dir * 0.3f);
		}
	}
	DrawDefault(dc);
}

//////////////////////////////////////////////////////////////////////////
bool CAIAnchor::HitTest(HitContext& hc)
{
	Vec3 origin = GetWorldPos();
	float radius = GetRadius();

	Vec3 w = origin - hc.raySrc;
	w = hc.rayDir.Cross(w);
	float d = w.GetLength();

	if (d < radius + hc.distanceTolerance)
	{
		hc.dist = hc.raySrc.GetDistance(origin);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CAIAnchor::GetLocalBounds(AABB& box)
{
	float r = GetRadius();
	box.min = -Vec3(r, r, r);
	box.max = Vec3(r, r, r);
}

