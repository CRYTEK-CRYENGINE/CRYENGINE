// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AnimatedCharacter.h"
#include "AnimationGraphCVars.h"
#include "PersistantDebug.h"
#include "DebugHistory.h"

//--------------------------------------------------------------------------------

#if DEBUG_VELOCITY()
static const float k_debugVelocityDeltaYPos = 15;
static const float k_debugVelocityFontSize = 1.5f;
static const float k_debugVelocityYPos = 80;
static const float k_debugVelocityXPos = 20;
static const float k_debugVelocityInitialDeltaHeight = 0.2f;
static const float k_debugVelocityDeltaHeight = 0.1f;
static const float k_debugVelocityCylinderRadius = 0.01f;
static const float k_debugVelocityConeRadius = 0.02f;

//--------------------------------------------------------------------------------
struct CDebugVelocity
{
	CDebugVelocity() {}
	CDebugVelocity(const QuatT& movement, const float frameTime, const char* szComment, const ColorF& colorF, const bool pastMovement = false)
		: m_movement(movement), m_frameTime(frameTime), m_sComment(szComment), m_colorF(colorF), m_pastMovement(pastMovement) {}
	void Draw(const Vec3& origin, const float textXPos, const float textYPos) const;

private:
	QuatT  m_movement;
	float  m_frameTime;
	string m_sComment;
	ColorF m_colorF;
	bool   m_pastMovement;
};

//--------------------------------------------------------------------------------
static std::vector<CDebugVelocity, stl::STLGlobalAllocator<CDebugVelocity>> s_debugVelocities;

extern float g_mannequinYPosEnd;
//--------------------------------------------------------------------------------
void CDebugVelocity::Draw(const Vec3& origin, const float textXPos, const float textYPosIn) const
{
	const float textYPos = g_mannequinYPosEnd + 20 + textYPosIn;

	const float curFrameTimeInv = m_frameTime > FLT_MIN ? 1.0f / m_frameTime : 0.0f;

	const Vec3 delta = m_movement.t;
	const float deltaQz = m_movement.q.GetRotZ();
	const Vec3 veloc = delta * curFrameTimeInv;
	const float velocQz = deltaQz * curFrameTimeInv;
	const float colorFBGRA8888[4] = { m_colorF.r, m_colorF.g, m_colorF.b, m_colorF.a };

	IRenderAuxText::Draw2dLabel(textXPos, textYPos, k_debugVelocityFontSize, colorFBGRA8888, false, "Vel: grounds=%2.3f m/s t=[%+2.3f, %+2.3f, %+2.3f] m/s, qz=%2.2f rad/s;  Offs: ground=%2.3f m t=[%+2.3f, %+2.3f, %+2.3f], qz=%2.2f rad;   dt: %2.3f s   %s", veloc.GetLength2D(), veloc.x, veloc.y, veloc.z, velocQz, delta.GetLength2D(), delta.x, delta.y, delta.z, deltaQz, (float)m_frameTime, m_sComment.c_str());

	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);

	const Vec3 start = m_pastMovement ? origin - delta : origin;
	const Vec3 end = start + delta;
	const unsigned int colorB = m_colorF.pack_abgr8888();

	const Vec3 coneStart = LERP(start, end, 0.8f);
	const Vec3 cylinderVec = coneStart - start;
	const Vec3 coneVec = end - coneStart;

	pAuxGeom->DrawCylinder(start + cylinderVec * 0.5f, cylinderVec.GetNormalized(), k_debugVelocityCylinderRadius, cylinderVec.GetLength(), colorB);

	if (coneVec.GetLengthSquared() < 0.001f * 0.001f)
		pAuxGeom->DrawSphere(origin, k_debugVelocityConeRadius, colorB);
	else
		pAuxGeom->DrawCone(coneStart, coneVec.GetNormalized(), k_debugVelocityConeRadius, coneVec.GetLength(), colorB);
}

//--------------------------------------------------------------------------------
void CAnimatedCharacter::AddDebugVelocity(const QuatT& movement, const float frameTime, const char* szComment, const ColorF& colorF, const bool pastMovement) const
{
	static int s_debugVelocityRenderFrameId = -1;

	if (!DebugVelocitiesEnabled())
		return;

	// Empty list of velocities when frame changes
	int renderFrameId = gEnv->pRenderer->GetFrameID();
	if (renderFrameId != s_debugVelocityRenderFrameId)
	{
		s_debugVelocities.resize(0);
		s_debugVelocityRenderFrameId = renderFrameId;
	}

	// Add to list
	s_debugVelocities.push_back(CDebugVelocity(movement, frameTime, szComment, colorF, pastMovement));
}

//--------------------------------------------------------------------------------
static void DrawDebugVelocities(const Vec3& entityPos)
{
	const ColorB white = 0xFFFFFFFF;

	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

	Vec3 lineOrigin = entityPos + Vec3(0, 0, k_debugVelocityInitialDeltaHeight);
	float textYPos = k_debugVelocityYPos;
	for (int i = 0; i < s_debugVelocities.size(); ++i)
	{
		if (i > 0)
			lineOrigin += Vec3(0, 0, k_debugVelocityDeltaHeight);

		s_debugVelocities[i].Draw(lineOrigin, k_debugVelocityXPos, textYPos);

		textYPos += k_debugVelocityDeltaYPos;
	}

	pAuxGeom->DrawLine(entityPos, white, lineOrigin, white);
}

#endif // DEBUG_VELOCITY()

//--------------------------------------------------------------------------------

struct SDebugNodeSizeDesc
{
	Vec2 m_prefered;
	Vec2 m_importance;
};

struct IDebugNode
{
	virtual ~IDebugNode(){}
	virtual const SDebugNodeSizeDesc& GetSizeDesc() const = 0;
	virtual void                      SetActualSize(const Vec2& size) = 0;
};

struct IDebugContainer : public IDebugNode
{
	virtual void                      AddChild(IDebugNode* child);

	virtual const SDebugNodeSizeDesc& GetSizeDesc() const;
	virtual void                      SetAvailableSize(const Vec2& size);
};

class LayoutManager
{
public:
	void setRootNode(IDebugNode* root);
	void setTotalSize(const Vec2& size);
private:
	void layout();
};

/*
   addRow("params");
   add("param1");
   add("param2");
   add("param3");
   add("param4");
   addRow()
   add(param4);
 */

//--------------------------------------------------------------------------------

#ifdef DEBUGHISTORY
void CAnimatedCharacter::LayoutHelper(const char* id, const char* name, bool visible, float minout, float maxout, float minin, float maxin, float x, float y, float w /*=1.0f*/, float h /*=1.0f*/)
{
	if (!visible)
	{
		m_debugHistoryManager->RemoveHistory(id);
		return;
	}

	IDebugHistory* pDH = m_debugHistoryManager->GetHistory(id);
	if (pDH != NULL)
		return;

	pDH = m_debugHistoryManager->CreateHistory(id, name);
	pDH->SetupScopeExtent(minout, maxout, minin, maxin);
	pDH->SetVisibility(true);

	static float x0 = 0.01f;
	static float y0 = 0.05f;
	static float x1 = 0.99f;
	static float y1 = 1.00f;
	static float dx = x1 - x0;
	static float dy = y1 - y0;
	static float xtiles = 6.0f;
	static float ytiles = 4.0f;
	pDH->SetupLayoutRel(x0 + dx * x / xtiles,
	                    y0 + dy * y / ytiles,
	                    dx * w * 0.95f / xtiles,
	                    dy * h * 0.93f / ytiles,
	                    dy * 0.02f / ytiles);
}
#endif

//--------------------------------------------------------------------------------

#ifdef DEBUGHISTORY // Remove function from code memory when not used.
void CAnimatedCharacter::SetupDebugHistories()
{
	static bool showAll = false;
	bool any = false;
	bool showMotionParams = (CAnimationGraphCVars::Get().m_debugMotionParams != 0) || showAll;
	any |= showMotionParams;
	bool showPredError = (CAnimationGraphCVars::Get().m_debugMotionParams != 0) || showAll;
	any |= showPredError;
	bool showPredVelo = (CAnimationGraphCVars::Get().m_debugMotionParams != 0) || showAll;
	any |= showPredVelo;
	bool showAnimMovement = (CAnimationGraphCVars::Get().m_debugLocationsGraphs != 0) || showAll;
	any |= showAnimMovement;
	bool showEntMovement = (CAnimationGraphCVars::Get().m_debugLocationsGraphs != 0) || showAll;
	any |= showEntMovement;
	bool showAsset = (CAnimationGraphCVars::Get().m_debugLocationsGraphs != 0) || showAll;
	any |= showAsset;
	bool showTemp1 = ((CAnimationGraphCVars::Get().m_debugTempValues & 1) != 0) || showAll;
	any |= showTemp1;
	bool showTemp2 = ((CAnimationGraphCVars::Get().m_debugTempValues & 2) != 0) || showAll;
	any |= showTemp2;
	bool showTemp3 = ((CAnimationGraphCVars::Get().m_debugTempValues & 4) != 0) || showAll;
	any |= showTemp3;
	bool showTemp4 = ((CAnimationGraphCVars::Get().m_debugTempValues & 8) != 0) || showAll;
	any |= showTemp4;
	bool showMCM = (CAnimationGraphCVars::Get().m_debugMovementControlMethods != 0) || showAll;
	any |= showMCM;
	bool showFrameTime = (CAnimationGraphCVars::Get().m_debugFrameTime != 0) || showAll;
	any |= showFrameTime;

	bool showAnimTargetMovement = (CAnimationGraphCVars::Get().m_debugAnimTarget != 0) || showAll;
	any |= showAnimTargetMovement;

	if (any)
	{
		LayoutHelper("eDH_FrameTime", "FrameTime", showFrameTime, 0.0f, 1.0f, 0.0f, 0.0f, 5, 0.3f, 1, 0.7f);

		LayoutHelper("eDH_MovementControlMethodH", "MCM H", showMCM, 0, eMCM_COUNT - 1, 0, eMCM_COUNT - 1, 4, 0.2f, 1, 0.4f);
		LayoutHelper("eDH_MovementControlMethodV", "MCM V", showMCM, 0, eMCM_COUNT - 1, 0, eMCM_COUNT - 1, 4, 0.6f, 1, 0.4f);

		LayoutHelper("eDH_TurnSpeed", "TurnSpeed", showMotionParams, -360, +360, -1, +1, 0, 0, 1.0f, 0.5f);
		LayoutHelper("eDH_TravelSlope", "TravelSlope", showMotionParams, -25, 25, -25, 25, 0, 0.5f, 1.0f, 0.5f);
		LayoutHelper("eDH_TravelDirX", "TravelDirX", showMotionParams, -2, +2, -1, +1, 1, 0);
		LayoutHelper("eDH_TravelDirY", "TravelDirY", showMotionParams, -2, +2, -1, +1, 2, 0);
		LayoutHelper("eDH_TravelDistScale", "TravelDistScale", showMotionParams, 0, 1, 0, 1, 3, 0.0f, 1.0f, 0.2f);
		LayoutHelper("eDH_TravelDist", "TravelDist", showMotionParams, 0, 10, 0, 1, 3, 0.2f, 1.0f, 0.3f);
		LayoutHelper("eDH_TravelSpeed", "TravelSpeed", showMotionParams, 0, 10, 0, 1, 3, 0.5f, 1.0f, 0.5f);

		LayoutHelper("eDH_DesiredLocalLocationRZ", "LocalLocaRZ", showPredError, -180, +180, -1, +1, 0, 1, 1, 0.5f);
		LayoutHelper("eDH_DesiredLocalLocationTX", "LocalLocaTX", showPredError, -5, +5, -0.01f, +0.01f, 1, 1, 1, 0.5f);
		LayoutHelper("eDH_DesiredLocalLocationTY", "LocalLocaTY", showPredError, -5, +5, -0.01f, +0.01f, 2, 1, 1, 0.5f);

		LayoutHelper("eDH_DesiredLocalVelocityRZ", "LocalVeloRZ", showPredVelo, -180, +180, -1, +1, 0, 1.5f, 1, 0.5f);
		LayoutHelper("eDH_DesiredLocalVelocityTX", "LocalVeloTX", showPredVelo, -5, +5, -0.01f, +0.01f, 1, 1.5f, 1, 0.5f);
		LayoutHelper("eDH_DesiredLocalVelocityTY", "LocalVeloTY", showPredVelo, -5, +5, -0.01f, +0.01f, 2, 1.5f, 1, 0.5f);

		LayoutHelper("eDH_ReqEntMovementRotZ", "ReqEntMoveRotZ", showEntMovement, -180, +180, -1, +1, 0, 2, 1, 0.5f);
		LayoutHelper("eDH_ReqEntMovementTransX", "ReqEntMoveX", showEntMovement, -10, +10, -0.0f, +0.0f, 1, 2, 1, 0.5f);
		LayoutHelper("eDH_ReqEntMovementTransY", "ReqEntMoveY", showEntMovement, -10, +10, -0.0f, +0.0f, 2, 2, 1, 0.5f);

		LayoutHelper("eDH_EntLocationOriZ", "EntOriZ", showEntMovement, -360, +360, +360, -360, 0, 2.5f, 1, 0.5f);
		LayoutHelper("eDH_EntLocationPosX", "EntPosX", showEntMovement, -10000, +10000, +10000, -10000, 1, 2.5f, 1, 0.5f);
		LayoutHelper("eDH_EntLocationPosY", "EntPosY", showEntMovement, -10000, +10000, +10000, -10000, 2, 2.5f, 1, 0.5f);

		LayoutHelper("eDH_AnimAssetRotZ", "AssetRotZ", showAnimMovement, -180, +180, -1, +1, 0, 3.5f, 1, 0.5f);
		LayoutHelper("eDH_AnimAssetTransX", "AssetX", showAnimMovement, -5, +5, -0.0f, +0.0f, 1, 3.5f, 1, 0.5f);
		LayoutHelper("eDH_AnimAssetTransY", "AssetY", showAnimMovement, -5, +5, -0.0f, +0.0f, 2, 3.5f, 1, 0.5f);
		LayoutHelper("eDH_AnimAssetTransZ", "AssetZ", showAnimMovement, -5, +5, -0.0f, +0.0f, 3, 3.5f, 1, 0.5f);

		LayoutHelper("eDH_AnimTargetCorrectionRotZ", "AnimTargetRotZ", showAnimTargetMovement, -180, +180, -1, +1, 3, 1.5f, 1, 0.5f);
		LayoutHelper("eDH_AnimTargetCorrectionTransX", "AnimTargetX", showAnimTargetMovement, -5, +5, -0.0f, +0.0f, 4, 1.5f, 1, 0.5f);
		LayoutHelper("eDH_AnimTargetCorrectionTransY", "AnimTargetY", showAnimTargetMovement, -5, +5, -0.0f, +0.0f, 5, 1.5f, 1, 0.5f);

		LayoutHelper("eDH_EntMovementErrorRotZ", "EntMoveErrorRotZ", showEntMovement, -180, +180, -1, +1, 3, 2.0f, 1, 0.5f);
		LayoutHelper("eDH_EntMovementErrorTransX", "EntMoveErrorX", showEntMovement, -5, +5, -0.0f, +0.0f, 4, 2.0f, 1, 0.5f);
		LayoutHelper("eDH_EntMovementErrorTransY", "EntMoveErrorY", showEntMovement, -5, +5, -0.0f, +0.0f, 5, 2.0f, 1, 0.5f);

		LayoutHelper("eDH_EntTeleportMovementRotZ", "EntTeleportRotZ", showEntMovement, -180, +180, -1, +1, 3, 2.5f, 1, 0.5f);
		LayoutHelper("eDH_EntTeleportMovementTransX", "EntTeleportX", showEntMovement, -5, +5, -0.0f, +0.0f, 4, 2.5f, 1, 0.5f);
		LayoutHelper("eDH_EntTeleportMovementTransY", "EntTeleportY", showEntMovement, -5, +5, -0.0f, +0.0f, 5, 2.5f, 1, 0.5f);

		LayoutHelper("eDH_TEMP00", "TEMP00", showTemp1, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 5, 0.4f, 1, 0.4f);
		LayoutHelper("eDH_TEMP01", "TEMP01", showTemp1, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 5, 0.8f, 1, 0.4f);
		LayoutHelper("eDH_TEMP02", "TEMP02", showTemp1, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 5, 1.2f, 1, 0.4f);
		LayoutHelper("eDH_TEMP03", "TEMP03", showTemp1, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 5, 1.6f, 1, 0.4f);
		LayoutHelper("eDH_TEMP04", "TEMP04", showTemp2, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 5, 2.0f, 1, 0.4f);
		LayoutHelper("eDH_TEMP05", "TEMP05", showTemp2, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 5, 2.4f, 1, 0.4f);
		LayoutHelper("eDH_TEMP06", "TEMP06", showTemp2, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 5, 2.8f, 1, 0.4f);
		LayoutHelper("eDH_TEMP07", "TEMP07", showTemp2, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 5, 3.2f, 1, 0.4f);

		LayoutHelper("eDH_TEMP10", "TEMP10", showTemp3, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 4, 0.4f, 1, 0.4f);
		LayoutHelper("eDH_TEMP11", "TEMP11", showTemp3, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 4, 0.8f, 1, 0.4f);
		LayoutHelper("eDH_TEMP12", "TEMP12", showTemp3, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 4, 1.2f, 1, 0.4f);
		LayoutHelper("eDH_TEMP13", "TEMP13", showTemp3, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 4, 1.6f, 1, 0.4f);
		LayoutHelper("eDH_TEMP14", "TEMP14", showTemp4, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 4, 2.0f, 1, 0.4f);
		LayoutHelper("eDH_TEMP15", "TEMP15", showTemp4, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 4, 2.4f, 1, 0.4f);
		LayoutHelper("eDH_TEMP16", "TEMP16", showTemp4, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 4, 2.8f, 1, 0.4f);
		LayoutHelper("eDH_TEMP17", "TEMP17", showTemp4, -10000.0f, +10000.0f, +10000.0f, -10000.0f, 4, 3.2f, 1, 0.4f);
	}
	else
	{
		m_debugHistoryManager->Clear();
	}

	/*
	   bool showEntityValues = (CAnimationGraphCVars::Get().m_debugEntityParams != 0);
	   if(m_pEntityMoveSpeed == NULL)
	   {
	   m_pEntityMoveSpeed = m_debugHistoryManager->CreateHistory("EntityMoveSpeed");
	   m_pEntityMoveSpeed->SetupLayoutAbs(10, 10, 200, 200, 5);
	   m_pEntityMoveSpeed->SetupScopeExtent(-360, +360, -1, +1);
	   }

	   if(m_pEntityPhysSpeed == NULL)
	   {
	   m_pEntityPhysSpeed = m_debugHistoryManager->CreateHistory("EntityPhysSpeed");
	   m_pEntityPhysSpeed->SetupLayoutAbs(220, 10, 200, 200, 5);
	   m_pEntityPhysSpeed->SetupScopeExtent(-360, +360, -1, +1);
	   }

	   if(m_pACRequestSpeed == NULL)
	   {
	   m_pACRequestSpeed = m_debugHistoryManager->CreateHistory("ACRequestSpeed");
	   m_pACRequestSpeed->SetupLayoutAbs(430, 10, 200, 200, 5);
	   m_pACRequestSpeed->SetupScopeExtent(-360, +360, -1, +1);
	   }
	 */

	/*
	   LayoutManager.getNode("root").addHorizontal("");
	   LayoutManager.getNode("last").add(DH);
	   LayoutManager.getNode("last").add(DH);
	   LayoutManager.getNode("last").add(DH);
	   LayoutManager.getNode("last").add(DH);
	   LayoutManager.getNode("root").addHorizontal("middle").addVertical("");
	   LayoutManager.getNode("last").add(DH);
	   LayoutManager.getNode("last").add(DH);
	   LayoutManager.getNode("last").add(DH);
	   LayoutManager.getNode("root").addHorizontal("");
	   LayoutManager.getNode("last").add(DH);
	   LayoutManager.getNode("last").add(DH);
	   LayoutManager.getNode("last").add(DH);
	   LayoutManager.getNode("last").add(DH);
	   LayoutManager.getNode("last").add(DH);
	   LayoutManager.getNode("last").add(DH);
	   LayoutManager.getNode("middle").addRows(2);
	   LayoutManager.getNode("middle.row1").addCols(2);
	   LayoutManager.getNode("middle.row1.col0").add(DH);
	   DH.setWidth(preferred, unit); // actual pixels, ref pixels, percent of parent
	   DH.setHeight(preferred, unit);
	   LayoutManager.relax();
	 */
}
#endif

//--------------------------------------------------------------------------------

void CAnimatedCharacter::DebugGraphQT(const QuatT& m, const char* tx, const char* ty, const char* rz, const char* tz /* = UNDEFINED */) const
{
#ifndef DEBUGHISTORY
	return;
#else

	if (!DebugFilter())
		return;

	DebugHistory_AddValue(tx, m.t.x);
	DebugHistory_AddValue(ty, m.t.y);
	DebugHistory_AddValue(tz, m.t.z);
	DebugHistory_AddValue(rz, RAD2DEG(m.q.GetRotZ()));
#endif
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::DebugHistory_AddValue(const char* id, float x) const
{
#ifndef DEBUGHISTORY
	return;
#else

	if (id == NULL)
		return;

	IDebugHistory* pDH = m_debugHistoryManager->GetHistory(id);
	if (pDH != NULL)
		pDH->AddValue(x);

#endif
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

bool CAnimatedCharacter::DebugFilter() const
{
	/*
	   #ifndef DEBUGHISTORY
	   return false;
	   #else
	 */

	const char* entityfilter = CAnimationGraphCVars::Get().m_pDebugFilter->GetString();
	const char* entityname = GetEntity()->GetName();
	if (strcmp(entityfilter, "0") == 0)
		return true;

	return (strcmp(entityfilter, entityname) == 0);

	//#endif
}

//--------------------------------------------------------------------------------
#if DEBUG_VELOCITY()
bool CAnimatedCharacter::DebugVelocitiesEnabled() const
{
	if (CAnimationGraphCVars::Get().m_debugLocations <= 1)
		return false;

	return DebugFilter();
}
#endif

//--------------------------------------------------------------------------------
bool CAnimatedCharacter::DebugTextEnabled() const
{
#ifdef _DEBUG
	if (CAnimationGraphCVars::Get().m_debugText == 0)
		return false;

	return DebugFilter();
#else
	return false;
#endif
}

//--------------------------------------------------------------------------------

// TODO: This is too important and useful to disable even in profile, yet...
void CAnimatedCharacter::DebugRenderCurLocations() const
{
	if (CAnimationGraphCVars::Get().m_debugLocations == 0)
		return;

#if DEBUG_VELOCITY()
	if (DebugVelocitiesEnabled())
	{
		DrawDebugVelocities(GetEntity()->GetWorldPos());
	}
#endif

	CPersistantDebug* pPD = CCryAction::GetCryAction()->GetPersistantDebug();
	if (pPD == NULL)
		return;

	static Vec3 abump(0, 0, 0.11f);
	static Vec3 ebump(0, 0.005f, 0.1f);
	static float entDuration = 0.5f;
	static float animDuration = 0.5f;
	static float entTrailDuration = 10.0f;
	static float animTrailDuration = 10.0f;
	static ColorF entColor0(0, 0, 1, 1);
	static ColorF entColor1(0.2f, 0.2f, 1, 1);

	float sideLength = 0.5f;
	float fwdLength = 1.0f;
	float upLength = 4.0f;

	IEntity* pEntity = GetEntity();
	CRY_ASSERT(pEntity != NULL);
	IPhysicalEntity* pPhysEnt = pEntity->GetPhysics();
	if (pPhysEnt)
	{
		pe_player_dimensions dim;
		if (pPhysEnt->GetParams(&dim) != 0)
		{
			float scaleLength = dim.sizeCollider.GetLength2D();
			sideLength *= scaleLength;
			fwdLength *= scaleLength;
			upLength *= scaleLength;
		}
	}

	pPD->Begin(UNIQUE("AnimatedCharacter.Locations"), true);

	pPD->AddSphere(m_entLocation.t + ebump, 0.05f, entColor0, entDuration);
	pPD->AddLine(m_entLocation.t + ebump, m_entLocation.t + ebump + (m_entLocation.q * Vec3(0, 0, upLength)), entColor0, entDuration);
	pPD->AddLine(m_entLocation.t + ebump, m_entLocation.t + ebump + (m_entLocation.q * Vec3(0, fwdLength, 0)), entColor1, entDuration);
	pPD->AddLine(m_entLocation.t + ebump + (m_entLocation.q * Vec3(-sideLength, 0, 0)), m_entLocation.t + ebump + (m_entLocation.q * Vec3(+sideLength, 0, 0)), entColor0, entDuration);

	pPD->Begin(UNIQUE("AnimatedCharacter.Locations.Trail"), false);

	pPD->AddSphere(m_entLocation.t + ebump, 0.01f, entColor0, entTrailDuration);

#ifdef _DEBUG
	if (DebugTextEnabled())
	{
		Ang3 EntOri(m_entLocation.q);
		const ColorF cWhite = ColorF(1, 1, 1, 1);
		IRenderAuxText::Draw2dLabel(10, 1, 2.0f, (float*)&cWhite, false,
		                             "  Ent Location[%.2f, %.2f, %.2f | %.2f, %.2f, %.2f]",
		                             m_entLocation.t.x, m_entLocation.t.y, m_entLocation.t.z, RAD2DEG(EntOri.x), RAD2DEG(EntOri.y), RAD2DEG(EntOri.z));
	}
#endif
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::DebugDisplayNewLocationsAndMovements(const QuatT& EntLocation, const QuatT& EntMovement,
                                                              const QuatT& AnimMovement,
                                                              float frameTime) const
{
	CPersistantDebug* pPD = CCryAction::GetCryAction()->GetPersistantDebug();
	static float trailTime = 6000.0f;

	if ((pPD != NULL) && (CAnimationGraphCVars::Get().m_debugLocations != 0))
	{
		Vec3 bump(0, 0, 0.1f);

		pPD->Begin(UNIQUE("AnimatedCharacter.PrePhysicsStep2.new"), true);
		pPD->AddLine(Vec3(0, 0, EntLocation.t.z) + bump, EntLocation.t + bump, ColorF(0, 0, 1, 0.5f), 0.5f);

		pPD->AddSphere(EntLocation.t + bump, 0.05f, ColorF(0, 0, 1, 1), 0.5f);
		pPD->AddDirection(EntLocation.t + bump, 0.15f, (EntLocation.q * FORWARD_DIRECTION), ColorF(0, 0, 1, 1), 0.5f);

		pPD->AddLine(EntLocation.t + bump, EntLocation.t + EntMovement.t * 10.0f + bump, ColorF(0, 0, 1, 0.5f), 0.5f);
		pPD->AddLine(EntLocation.t + bump, EntLocation.t + EntMovement.t + bump, ColorF(0.5f, 0.5f, 1, 1.0f), 0.5f);

		pPD->Begin(UNIQUE("AnimatedCharacter.PrePhysicsStep2.trail"), false);

		pPD->AddSphere(EntLocation.t + bump, 0.1f, ColorF(0, 0, 1, 0.5f), trailTime);
	}

#ifdef _DEBUG
	if (DebugTextEnabled())
	{
		Ang3 EntOri(EntLocation.q);
		Ang3 EntRot(EntMovement.q);
		const ColorF cWhite = ColorF(1, 1, 1, 1);
		IRenderAuxText::Draw2dLabel(10, 100, 2.0f, (float*)&cWhite, false,
		                             "N Ent Location[%.2f, %.2f, %.2f | %.2f, %.2f, %.2f]  Movement[%.2f, %.2f, %.2f | %.2f, %.2f, %.2f]",
		                             EntLocation.t.x, EntLocation.t.y, EntLocation.t.z, RAD2DEG(EntOri.x), RAD2DEG(EntOri.y), RAD2DEG(EntOri.z),
		                             EntMovement.t.x / frameTime, EntMovement.t.y / frameTime, EntMovement.t.z / frameTime,
		                             RAD2DEG(EntRot.x), RAD2DEG(EntRot.y), RAD2DEG(EntRot.z));

		Ang3 AnimRot(AnimMovement.q);
		IRenderAuxText::Draw2dLabel(10, 125, 2.0f, (float*)&cWhite, false,
		                             "N Anim Movement[%.2f, %.2f, %.2f | %.2f, %.2f, %.2f]",
		                             AnimMovement.t.x / frameTime, AnimMovement.t.y / frameTime, AnimMovement.t.z / frameTime,
		                             RAD2DEG(AnimRot.x), RAD2DEG(AnimRot.y), RAD2DEG(AnimRot.z));
	}
#endif
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::DebugBlendWeights(const ISkeletonAnim* pSkeletonAnim)
{
	/*
	   if (pSkeleton == NULL)
	    return;

	   float BlendWeight0 = 1.0f;
	   float BlendWeight1 = 0.0f;
	   uint32 numAnimsLayer0 = pSkeleton->GetNumAnimsInFIFO(0);
	   if (numAnimsLayer0 == 1)
	   {
	    CAnimation animationSteady;
	    animationSteady = pSkeleton->GetAnimFromFIFO(0,0);
	    BlendWeight0 = 1.0f;
	   }
	   else if (numAnimsLayer0 == 2)
	   {
	    CAnimation animationBlendIn;
	    CAnimation animationBlendOut;
	    animationBlendOut = pSkeleton->GetAnimFromFIFO(0,0);
	    BlendWeight0 = animationBlendOut.m_fTransitionBlend;
	    animationBlendIn = pSkeleton->GetAnimFromFIFO(0,1);
	    BlendWeight1 = animationBlendIn.m_fTransitionBlend;
	   }
	 */
}

//--------------------------------------------------------------------------------

#ifdef _DEBUG
void DebugRenderDistanceMeasure(CPersistantDebug* pPD, const Vec3& origin, const Vec3& offset, float fraction)
{
	float len = offset.GetLength();
	if (len == 0.0f)
		return;

	Vec3 dir = offset / len;
	float vertical = abs(dir | Vec3(0, 0, 1));

	Vec3 rgt = LERP((dir % Vec3(0, 0, 1)), Vec3(1, 0, 0), vertical);
	Vec3 up = dir % rgt;
	rgt = up % dir;

	static ColorF colorInside(0, 1, 1, 0.5f);
	static ColorF colorOutside(1, 1, 0, 1);
	ColorF color = (fraction < 1.0f) ? colorInside : colorOutside;

	static float crossSize = 0.05f;

	pPD->AddLine(origin, origin + offset, color, 1.0f);
	pPD->AddLine(origin - rgt * crossSize, origin + rgt * crossSize, color, 1.0f);
	pPD->AddLine(origin - up * crossSize, origin + up * crossSize, color, 1.0f);
	pPD->AddLine(origin + offset - rgt * crossSize, origin + offset + rgt * crossSize, color, 1.0f);
	pPD->AddLine(origin + offset - up * crossSize, origin + offset + up * crossSize, color, 1.0f);
}
#endif

//--------------------------------------------------------------------------------

#ifdef _DEBUG
void DebugRenderAngleMeasure(CPersistantDebug* pPD, const Vec3& origin, const Quat& orientation, const Quat& offset, float fraction)
{
	Vec3 baseDir = orientation * FORWARD_DIRECTION;
	Vec3 clampedDir = orientation * offset * FORWARD_DIRECTION;

	static ColorF colorInside(0, 1, 1, 0.5f);
	static ColorF colorOutside(1, 1, 0, 1);
	ColorF color = (fraction < 1.0f) ? colorInside : colorOutside;

	static float armSize = 1.0f;
	pPD->AddLine(origin, origin + baseDir * (armSize * 0.5f), color, 1.0f);
	pPD->AddLine(origin, origin + clampedDir * armSize, color, 1.0f);
	pPD->AddLine(origin + baseDir * (armSize * 0.5f), origin + clampedDir * (armSize * 0.5f), color, 1.0f);
}
#endif

//--------------------------------------------------------------------------------

QuatT GetDebugEntityLocation(const char* name, const QuatT& _default)
{
#ifdef _DEBUG
	IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName(name);
	if (pEntity != NULL)
		return QuatT(pEntity->GetWorldPos(), pEntity->GetWorldRotation());
#endif

	return _default;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

#ifdef _DEBUG
bool RunExtractCombineTest(bool flat)
{
	int errors = 0;

	for (int i = 0; i < 1000; i++)
	{
		QuatT qt1;
		qt1.t = Vec3(0.0f);

		Vec3 rgt0 = Vec3(1, 0, 0);
		Vec3 fwd0 = Vec3(0, 1, 0);
		Vec3 up0 = Vec3(0, 0, 1);

		Vec3 r(
		  cry_random(-100.0f, 100.0f),
		  cry_random(-100.0f, 100.0f),
		  0.0f);
		fwd0 = r.GetNormalizedSafe(fwd0);

		if (!flat)
		{
			r.Set(
			  cry_random(-10.0f, 10.0f),
			  cry_random(-10.0f, 10.0f),
			  100.0f);
			up0 = r.GetNormalizedSafe(up0);
		}

		rgt0 = (fwd0 % up0).GetNormalizedSafe(rgt0);
		up0 = (rgt0 % fwd0).GetNormalizedSafe(up0);
		qt1.q = Quat(Matrix33::CreateFromVectors(rgt0, fwd0, up0));

		Vec3 rgt1 = qt1.q * Vec3(1, 0, 0);
		Vec3 fwd1 = qt1.q * Vec3(0, 1, 0);
		Vec3 up1 = qt1.q * Vec3(0, 0, 1);

		QuatT qth = ExtractHComponent(qt1);
		QuatT qtv = ExtractVComponent(qt1);
		QuatT qt2;

		if (flat)
			qt2 = CombineHVComponents2D(qth, qtv);
		else
			qt2 = CombineHVComponents2D(qth, qtv);

		QuatT hybrid;
		hybrid.SetNLerp(qt1, qt2, 0.5f);
		bool c1 = !qt1.IsEquivalent(qt1, qt2);
		bool c2 = !hybrid.IsEquivalent(hybrid, qt1);
		bool c3 = !hybrid.IsEquivalent(hybrid, qt2);

		if (c1 || c2 || c3)
		{
			errors++;

			Vec3 rgt2 = qt2.q * Vec3(1, 0, 0);
			Vec3 fwd2 = qt2.q * Vec3(0, 1, 0);
			Vec3 up2 = qt2.q * Vec3(0, 0, 1);

			Vec3 rgtH = qth.q * Vec3(1, 0, 0);
			Vec3 fwdH = qth.q * Vec3(0, 1, 0);
			Vec3 upH = qth.q * Vec3(0, 0, 1);

			Vec3 rgtV = qtv.q * Vec3(1, 0, 0);
			Vec3 fwdV = qtv.q * Vec3(0, 1, 0);
			Vec3 upV = qtv.q * Vec3(0, 0, 1);
		}
	}

	return (errors == 0);
}
#endif

//--------------------------------------------------------------------------------

#ifdef _DEBUG
bool RunQuatConcatTest()
{
	int validConcatCountWorst = 10000;
	int validConcatCountBest = 0;
	bool allValid = true;

	for (int i = 0; i < 100; i++)
	{
		Quat o(IDENTITY);

		for (int j = 0; j < 1000; j++)
		{
			if (!o.IsValid())
			{
				validConcatCountWorst = min(j, validConcatCountWorst);
				validConcatCountBest = max(j, validConcatCountBest);
				allValid = false;
				break;
			}

			Quat q;
			/*
			      Vec3 rgt(1, 0, 0);
			      Vec3 fwd(0, 1, 0);
			      Vec3 up(0, 0, 1);
			      Vec3 r(
			        cry_random(-1.0f, 1.0f),
			        cry_random(-1.0f, 1.0f),
			        cry_random(-1.0f, 1.0f));
			      fwd = r.GetNormalizedSafe(fwd);
			      r.Set(
			        cry_random(-1.0f, 1.0f),
			        cry_random(-1.0f, 1.0f),
			        cry_random(-1.0f, 1.0f));
			      up = r.GetNormalizedSafe(up);
			      rgt = (fwd % up).GetNormalizedSafe(rgt);
			      up = (rgt % fwd).GetNormalizedSafe(up);
			      q = Quat(Matrix33::CreateFromVectors(rgt, fwd, up));
			 */
			q.v.x = cry_random(-1.0f, 1.0f);
			q.v.y = cry_random(-1.0f, 1.0f);
			q.v.z = cry_random(-1.0f, 1.0f);
			q.w = cry_random(-1.0f, 1.0f);
			q.Normalize();
			CRY_ASSERT(q.IsValid());

			Quat qInv = q.GetInverted();
			CRY_ASSERT(qInv.IsValid());

			o = o * qInv;
		}
	}

	if (!allValid)
		CryLog("RunQuatConcatTest: valid consecutive concatenations - worst case %d, best case %d.", validConcatCountWorst, validConcatCountBest);

	return allValid;
}
#endif

//--------------------------------------------------------------------------------

#ifdef _DEBUG
bool RunQuatPredicateTest()
{
	int errors = 0;

	for (int i = 0; i < 1000; i++)
	{
		Quat q;
		q.v = Vec3(0.0f, 0.0f, cry_random(0.0f, 1.0f));
		q.w = cry_random(0.0f, 1.0f);

		q.Normalize();

		Vec3 fwd = q.GetColumn1();
		float rz1 = atan2f(fwd.y, fwd.x);
		float rz2 = atan2f(-fwd.x, fwd.y);
		float rz3 = q.GetRotZ();

		if (fabs(rz2 - rz3) > 0.0001f)
		{
			errors++;
		}
	}

	return (errors == 0);
}
#endif

//--------------------------------------------------------------------------------

void CAnimatedCharacter::RunTests()
{
#ifndef _DEBUG
	return;
#else

	static bool m_bDebugRunTests = false;

	if (!m_bDebugRunTests)
		return;

	m_bDebugRunTests = false;

	RunExtractCombineTest(true);
	RunExtractCombineTest(false);
	RunQuatConcatTest();
	RunQuatPredicateTest();

#endif
}

//--------------------------------------------------------------------------------
