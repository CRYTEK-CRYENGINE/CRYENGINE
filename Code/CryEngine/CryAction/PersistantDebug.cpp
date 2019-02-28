// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PersistantDebug.h"
#include "CryAction.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include <IUIDraw.h>
#include <CrySystem/ILocalizationManager.h>

CPersistantDebug::CPersistantDebug()
	: m_pETLog(nullptr)
	, m_pETHideAll(nullptr)
	, m_pETHideBehaviour(nullptr)
	, m_pETHideReadability(nullptr)
	, m_pETHideAIDebug(nullptr)
	, m_pETHideFlowgraph(nullptr)
	, m_pETHideScriptBind(nullptr)
	, m_pETFontSizeMultiplier(nullptr)
	, m_pETMaxDisplayDistance(nullptr)
	, m_pETColorOverrideEnable(nullptr)
	, m_pETColorOverrideR(nullptr)
	, m_pETColorOverrideG(nullptr)
	, m_pETColorOverrideB(nullptr)
{
	m_pDefaultFont = gEnv->pCryFont->GetFont("default");
	CRY_ASSERT(m_pDefaultFont);
}

void CPersistantDebug::Begin(const char* name, bool clear)
{
	if (clear)
		m_objects[name].clear();
	else
		m_objects[name];
	m_current = m_objects.find(name);
}

void CPersistantDebug::AddSphere(const Vec3& pos, float radius, ColorF clr, float timeout)
{
	SObj obj;
	obj.obj = eOT_Sphere;
	obj.clr = clr;
	obj.pos = pos;
	obj.radius = radius;
	obj.timeRemaining = timeout <= 0.0f ? kUnlimitedTime : timeout;
	obj.totalTime = obj.timeRemaining;
	m_current->second.push_back(obj);
}

void CPersistantDebug::AddQuat(const Vec3& pos, const Quat& q, float r, ColorF clr, float timeout)
{
	SObj obj;
	obj.obj = eOT_Quat;
	obj.clr = clr;
	obj.pos = pos;
	obj.q = q;
	obj.radius = r;
	obj.timeRemaining = timeout <= 0.0f ? kUnlimitedTime : timeout;
	obj.totalTime = obj.timeRemaining;
	m_current->second.push_back(obj);
}

void CPersistantDebug::AddAABB(const Vec3& min, const Vec3& max, ColorF clr, float timeout)
{
	SObj obj;
	obj.obj = eOT_AABB;
	obj.clr = clr;
	obj.pos = min;
	obj.dir = max;
	obj.timeRemaining = timeout <= 0.0f ? kUnlimitedTime : timeout;
	obj.totalTime = obj.timeRemaining;
	m_current->second.push_back(obj);
}

void CPersistantDebug::AddDirection(const Vec3& pos, float radius, const Vec3& dir, ColorF clr, float timeout)
{
	SObj obj;
	obj.obj = eOT_Arrow;
	obj.clr = clr;
	obj.pos = pos;
	obj.radius = radius;
	obj.timeRemaining = timeout <= 0.0f ? kUnlimitedTime : timeout;
	obj.totalTime = obj.timeRemaining;
	obj.dir = dir.GetNormalizedSafe();
	if (obj.dir.GetLengthSquared() > 0.001f)
		m_current->second.push_back(obj);
}

void CPersistantDebug::AddLine(const Vec3& pos1, const Vec3& pos2, ColorF clr, float timeout)
{
	SObj obj;
	obj.obj = eOT_Line;
	obj.clr = clr;
	obj.pos = pos1;
	obj.dir = pos2 - pos1;
	obj.radius = 0.0f;
	obj.timeRemaining = timeout <= 0.0f ? kUnlimitedTime : timeout;
	obj.totalTime = obj.timeRemaining;
	m_current->second.push_back(obj);
}

void CPersistantDebug::AddPlanarDisc(const Vec3& pos, float innerRadius, float outerRadius, ColorF clr, float timeout)
{
	SObj obj;
	obj.obj = eOT_Disc;
	obj.clr = clr;
	obj.pos = pos;
	obj.radius = std::max(0.0f, std::min(innerRadius, outerRadius));
	obj.radius2 = std::min(100.0f, std::max(0.0f, std::max(innerRadius, outerRadius)));
	obj.timeRemaining = timeout <= 0.0f ? kUnlimitedTime : timeout;
	obj.totalTime = obj.timeRemaining;
	if (obj.radius2)
		m_current->second.push_back(obj);
}

void CPersistantDebug::AddCone(const Vec3& pos, const Vec3& dir, float baseRadius, float height, ColorF clr, float timeout)
{
	SObj obj;
	obj.obj = eOT_Cone;
	obj.clr = clr;
	obj.pos = pos;
	obj.dir = dir;
	obj.radius = std::max(0.001f, baseRadius);
	obj.radius2 = std::max(0.001f, height);
	obj.timeRemaining = timeout <= 0.0f ? kUnlimitedTime : timeout;
	obj.totalTime = obj.timeRemaining;
	m_current->second.push_back(obj);
}

void CPersistantDebug::AddCylinder(const Vec3& pos, const Vec3& dir, float radius, float height, ColorF clr, float timeout)
{
	SObj obj;
	obj.obj = eOT_Cylinder;
	obj.clr = clr;
	obj.pos = pos;
	obj.dir = dir;
	obj.radius = std::max(0.001f, radius);
	obj.radius2 = std::max(0.001f, height);
	obj.timeRemaining = timeout <= 0.0f ? kUnlimitedTime : timeout;
	obj.totalTime = obj.timeRemaining;
	m_current->second.push_back(obj);
}

void CPersistantDebug::AddText(float x, float y, float size, ColorF clr, float timeout, const char* fmt, ...)
{
	char buffer[4096];
	va_list args;
	va_start(args, fmt);
	cry_vsprintf(buffer, fmt, args);
	va_end(args);

	SObj obj;
	obj.obj = eOT_Text2D;
	obj.clr = clr;
	obj.pos.x = x;
	obj.pos.y = y;
	obj.radius = size;
	obj.text = buffer;
	obj.timeRemaining = timeout <= 0.0f ? kUnlimitedTime : timeout;
	obj.totalTime = obj.timeRemaining;
	m_current->second.push_back(obj);
}

void CPersistantDebug::AddText3D(const Vec3& pos, float size, ColorF clr, float timeout, const char* fmt, ...)
{
	char buffer[4096];
	va_list args;
	va_start(args, fmt);
	cry_vsprintf(buffer, fmt, args);
	va_end(args);

	SObj obj;
	obj.obj = eOT_Text3D;
	obj.clr = clr;
	obj.pos = pos;
	obj.radius = size;
	obj.text = buffer;
	obj.timeRemaining = timeout <= 0.0f ? kUnlimitedTime : timeout;
	obj.totalTime = obj.timeRemaining;
	m_current->second.push_back(obj);
}

void CPersistantDebug::Add2DText(const char* text, float size, ColorF clr, float timeout)
{
	if (text == 0 || *text == '\0')
		return;

	STextObj2D obj;
	obj.clr = clr;
	obj.text = text;
	obj.size = size;
	obj.timeRemaining = timeout <= 0.0f ? kUnlimitedTime : timeout;
	obj.totalTime = obj.timeRemaining;
	m_2DTexts.push_front(obj);
}

void CPersistantDebug::Add2DLine(float x1, float y1, float x2, float y2, ColorF clr, float timeout)
{
	int iScreenWidth  = gEnv->pRenderer->GetOverlayWidth();
	int iScreenHeight = gEnv->pRenderer->GetOverlayHeight();

	SObj obj;
	obj.obj = eOT_Line2D;
	obj.pos.x = x1 / iScreenWidth;
	obj.pos.y = y1 / iScreenHeight;
	obj.pos.z = 0;
	obj.dir.x = x2 / iScreenWidth;
	obj.dir.y = y2 / iScreenHeight;
	obj.dir.z = 0;
	obj.clr = clr;
	obj.timeRemaining = timeout <= 0.0f ? kUnlimitedTime : timeout;
	obj.totalTime = obj.timeRemaining;
	m_current->second.push_back(obj);
}

bool CPersistantDebug::Init()
{
	// Init CVars
	m_pETLog = REGISTER_INT("cl_ETLog", 0, VF_DUMPTODISK, "Logging (0=off, 1=editor.log, 2=editor.log + AIlog.log)");
	m_pETHideAll = REGISTER_INT("cl_ETHideAll", 0, VF_DUMPTODISK, "Hide all tags (overrides all other options)");
	m_pETHideBehaviour = REGISTER_INT("cl_ETHideBehaviour", 0, VF_DUMPTODISK, "Hide AI behavior tags");
	m_pETHideReadability = REGISTER_INT("cl_ETHideReadability", 0, VF_DUMPTODISK, "Hide AI readability tags");
	m_pETHideAIDebug = REGISTER_INT("cl_ETHideAIDebug", 0, VF_DUMPTODISK, "Hide AI debug tags");
	m_pETHideFlowgraph = REGISTER_INT("cl_ETHideFlowgraph", 0, VF_DUMPTODISK, "Hide tags created by flowgraph");
	m_pETHideScriptBind = REGISTER_INT("cl_ETHideScriptBind", 0, VF_DUMPTODISK, "Hide tags created by Lua script");
	m_pETFontSizeMultiplier = REGISTER_FLOAT("cl_ETFontSizeMultiplier", 1.0f, VF_DUMPTODISK, "Global font size multiplier");
	m_pETMaxDisplayDistance = REGISTER_FLOAT("cl_ETMaxDisplayDistance", -2.0f, VF_DUMPTODISK, "Max display distance");
	m_pETColorOverrideEnable = REGISTER_INT("cl_ETColorOverrideEnable", 0, VF_DUMPTODISK, "Global color override");
	m_pETColorOverrideR = REGISTER_FLOAT("cl_ETColorOverrideR", 1.0f, VF_DUMPTODISK, "Global color override (RED)");
	m_pETColorOverrideG = REGISTER_FLOAT("cl_ETColorOverrideG", 1.0f, VF_DUMPTODISK, "Global color override (GREEN)");
	m_pETColorOverrideB = REGISTER_FLOAT("cl_ETColorOverrideB", 1.0f, VF_DUMPTODISK, "Global color override (BLUE)");

	Reset();

	return true;
}

void CPersistantDebug::Reset()
{
	m_2DTexts.clear();
	m_objects.clear();
	Begin("default", true); // Safety context if first caller forgets to use Begin(), so we don't crash!
}

void CPersistantDebug::Update(float frameTime)
{
	if (m_objects.empty())
		return;

	IRenderAuxGeom* pAux = IRenderAuxGeom::GetAux();
	if(pAux == nullptr)
		return;

	static const int flags3D = e_Mode3D | e_AlphaBlended | e_DrawInFrontOff | e_FillModeSolid | e_CullModeBack | e_DepthWriteOn | e_DepthTestOn;
	static const int flags2D = e_Mode2D | e_AlphaBlended;

	std::vector<ListObj::iterator> toClear;
	std::vector<MapListObj::iterator> toClearMap;
	for (MapListObj::iterator iterMap = m_objects.begin(); iterMap != m_objects.end(); ++iterMap)
	{
		toClear.resize(0);
		for (ListObj::iterator iterList = iterMap->second.begin(); iterList != iterMap->second.end(); ++iterList)
		{
			if (iterList->totalTime != kUnlimitedTime)
			{
				iterList->timeRemaining -= frameTime;
				if (iterList->timeRemaining <= 0.0f && !(iterList->obj == eOT_EntityTag && iterList->columns.size() > 1))
				{
					toClear.push_back(iterList);
					continue;
				}
			}

			ColorF clr = iterList->clr;
			clr.a *= iterList->timeRemaining / iterList->totalTime;
			switch (iterList->obj)
			{
			case eOT_Sphere:
				pAux->SetRenderFlags(flags3D);
				pAux->DrawSphere(iterList->pos, iterList->radius, clr);
				break;
			case eOT_Quat:
				pAux->SetRenderFlags(flags3D);
				{
					float r = iterList->radius;
					Vec3 x = r * iterList->q.GetColumn0();
					Vec3 y = r * iterList->q.GetColumn1();
					Vec3 z = r * iterList->q.GetColumn2();
					Vec3 p = iterList->pos;
					OBB obb = OBB::CreateOBB(Matrix33::CreateIdentity(), Vec3(0.05f, 0.05f, 0.05f), ZERO);
					pAux->DrawOBB(obb, p, false, clr, eBBD_Extremes_Color_Encoded);
					pAux->DrawLine(p, ColorF(1, 0, 0, clr.a), p + x, ColorF(1, 0, 0, clr.a));
					pAux->DrawLine(p, ColorF(0, 1, 0, clr.a), p + y, ColorF(0, 1, 0, clr.a));
					pAux->DrawLine(p, ColorF(0, 0, 1, clr.a), p + z, ColorF(0, 0, 1, clr.a));
				}
				break;
			case eOT_Arrow:
				pAux->SetRenderFlags(flags3D);
				pAux->DrawLine(iterList->pos - iterList->dir * iterList->radius, clr, iterList->pos + iterList->dir * iterList->radius, clr);
				pAux->DrawCone(iterList->pos + iterList->dir * iterList->radius, iterList->dir, 0.1f * iterList->radius, 0.3f * iterList->radius, clr);
				break;
			case eOT_Line:
				pAux->SetRenderFlags(flags3D);
				pAux->DrawLine(iterList->pos, clr, iterList->pos + iterList->dir, clr);
				break;
			case eOT_Cone:
				pAux->SetRenderFlags(flags3D);
				pAux->DrawCone(iterList->pos, iterList->dir, iterList->radius, iterList->radius2, clr);
				break;
			case eOT_Cylinder:
				pAux->SetRenderFlags(flags3D);
				pAux->DrawCylinder(iterList->pos, iterList->dir, iterList->radius, iterList->radius2, clr);
				break;
			case eOT_AABB:
				pAux->SetRenderFlags(flags3D);
				pAux->DrawAABB(AABB(iterList->pos, iterList->dir), Matrix34(IDENTITY), false, clr, eBBD_Faceted);
				break;
			case eOT_Line2D:
				pAux->SetRenderFlags(flags2D);
				pAux->DrawLine(iterList->pos, clr, iterList->dir, clr);
				break;
			case eOT_Text2D:
				{
					float clrAry[4] = { clr.r, clr.g, clr.b, clr.a };
					IRenderAuxText::Draw2dLabel(iterList->pos.x, iterList->pos.y, iterList->radius, clrAry, false, "%s", iterList->text.c_str());
				}
				break;
			case eOT_Text3D:
				{
					if ((uint8)(clr.a * 255) == 0) // Font renderer ignores the color when alpha==0
						continue;
					IRenderAuxText::DrawText(iterList->pos, iterList->radius, clr, eDrawText_FixedSize | eDrawText_800x600, iterList->text.c_str());
				}
				break;
			case eOT_Disc:
				{
					pAux->SetRenderFlags(flags3D);
					vtx_idx indTriQuad[6] =
					{
						0, 2, 1,
						0, 3, 2
					};
					vtx_idx indTriTri[3] =
					{
						0, 1, 2
					};

					int steps = (int)(10 * iterList->radius2);
					steps = std::max(steps, 10);
					float angStep = gf_PI2 / steps;
					for (int i = 0; i < steps; i++)
					{
						float a0 = angStep * i;
						float a1 = angStep * (i + 1);
						float c0 = cosf(a0);
						float c1 = cosf(a1);
						float s0 = sinf(a0);
						float s1 = sinf(a1);
						Vec3 pts[4];
						int n, n2;
						vtx_idx* indTri;
						if (iterList->radius)
						{
							n = 4;
							n2 = 6;
							pts[0] = iterList->pos + iterList->radius * Vec3(c0, s0, 0);
							pts[1] = iterList->pos + iterList->radius * Vec3(c1, s1, 0);
							pts[2] = iterList->pos + iterList->radius2 * Vec3(c1, s1, 0);
							pts[3] = iterList->pos + iterList->radius2 * Vec3(c0, s0, 0);
							indTri = indTriQuad;
						}
						else
						{
							n = 3;
							n2 = 3;
							pts[0] = iterList->pos;
							pts[1] = pts[0] + iterList->radius2 * Vec3(c0, s0, 0);
							pts[2] = pts[0] + iterList->radius2 * Vec3(c1, s1, 0);
							indTri = indTriTri;
						}
						pAux->DrawTriangles(pts, n, indTri, n2, clr);
					}
				}
				break;
			case eOT_EntityTag:
				{
					UpdateTags(frameTime, *iterList);
				}
				break;
			}
		}
		while (!toClear.empty())
		{
			iterMap->second.erase(toClear.back());
			toClear.pop_back();
		}
		if (iterMap->second.empty())
			toClearMap.push_back(iterMap);
	}
	while (!toClearMap.empty())
	{
		m_objects.erase(toClearMap.back());
		toClearMap.pop_back();
	}
}

void CPersistantDebug::PostUpdate(float frameTime)
{
	IRenderer* pRenderer = gEnv->pRenderer;
	const float x = pRenderer->ScaleCoordX(400.0f);

	float y = 400.0f;

	// Post update entity tags
	for (MapListObj::iterator iterMap = m_objects.begin(); iterMap != m_objects.end(); ++iterMap)
	{
		for (ListObj::iterator iterList = iterMap->second.begin(); iterList != iterMap->second.end(); ++iterList)
		{
			if (eOT_EntityTag == iterList->obj)
			{
				PostUpdateTags(frameTime, *iterList);
			}
		}
	}

	// now draw 2D texts overlay
	for (ListObjText2D::iterator iter = m_2DTexts.begin(); iter != m_2DTexts.end(); )
	{
		STextObj2D& textObj = *iter;
		ColorF clr = textObj.clr;
		clr.a *= textObj.timeRemaining / textObj.totalTime;
		IRenderAuxText::Draw2dLabel(x, y, textObj.size, &clr[0], true, "%s", textObj.text.c_str());
		y += 18.0f;

		textObj.timeRemaining -= frameTime;
		const bool bDelete = textObj.timeRemaining <= 0.0f;
		if (bDelete)
		{
			ListObjText2D::iterator toDelete = iter;
			++iter;
			m_2DTexts.erase(toDelete);
		}
		else
			++iter;
	}
}

void CPersistantDebug::OnWriteToConsole(const char* sText, bool bNewLine)
{
	if (sText)
	{
		Add2DText(sText, 1.0f, ColorF(1.0f, 1.0f, 1.0f), 20.0f);
	}
}
