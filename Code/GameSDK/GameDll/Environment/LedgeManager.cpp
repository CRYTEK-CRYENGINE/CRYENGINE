// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Climb-able Ledge system

   -------------------------------------------------------------------------
   History:
   - 20:04:2009: Created by Michelle Martin

*************************************************************************/

#include "StdAfx.h"
#include "LedgeManager.h"
#include <CryRenderer/IRenderAuxGeom.h>

#include <CryCore/TypeInfo_impl.h>

#define LEDGE_DATA_FILE_VERSION 3

STRUCT_INFO_BEGIN(SLedgeObject)
STRUCT_VAR_INFO(m_entityId, TYPE_INFO(EntityId))
STRUCT_VAR_INFO(m_markersStartIdx, TYPE_INFO(uint16))
STRUCT_VAR_INFO(m_markersCount, TYPE_INFO(uint16))
STRUCT_VAR_INFO(m_ledgeFlags, TYPE_ARRAY(LedgeSide_Count, TYPE_INFO(ELedgeFlagBitfield)))
STRUCT_VAR_INFO(m_ledgeCornerEndAdjustAmount, TYPE_INFO(float))
STRUCT_INFO_END(SLedgeObject)

STRUCT_INFO_BEGIN(SLedgeMarker)
STRUCT_VAR_INFO(m_worldPosition, TYPE_INFO(Vec3))
STRUCT_VAR_INFO(m_facingDirection, TYPE_INFO(Vec3))
STRUCT_VAR_INFO(m_endOrCorner, Type_info(bool))
STRUCT_INFO_END(SLedgeMarker)

namespace
{
ILINE Vec3 _FindVectorToClosestPointOnLedge(const Vec3& referencePoint, const SLedgeInfo& ledgeInfo)
{
	const Vec3 ledgePosition = ledgeInfo.GetPosition();

	Vec3 vXDir = ledgeInfo.GetSegment();
	const float halfWidth = (vXDir.NormalizeSafe() * 0.5f); // this is normalizing so projection below is valid
	const float quarterWidth = halfWidth * 0.5f;            // we use this to ensure when adjusting ends of a ledge a ledge will always be at least half its size. If the desired adjust amount is greater than the ledge size

	float distToP0 = halfWidth;
	float distToP1 = halfWidth;

	float endAdjustment = ledgeInfo.GetCornerEndAdjustAmount();

	// must keep this clamping in sync with editor and game DebugDraw() below
	if (ledgeInfo.AreFlagsSet(kledgeRunTimeOnlyFlag_p0IsEndOrCorner))
	{
		const float adjustAmount = min(endAdjustment, quarterWidth);
		distToP0 -= adjustAmount;
	}
	if (ledgeInfo.AreFlagsSet(kledgeRunTimeOnlyFlag_p1IsEndOrCorner))
	{
		const float adjustAmount = min(endAdjustment, quarterWidth);
		distToP1 -= adjustAmount;
	}

	const float fD = clamp_tpl((referencePoint - ledgePosition) * vXDir, -distToP0, distToP1);

	return ((ledgePosition + fD * vXDir) - referencePoint);
}

ILINE bool IsBestLedge(const Vec3& positionToLedge, const Vec3& testDirection, const SLedgeInfo& ledgeInfo, const float bestDistanceSq, const float cosMaxAngle, const bool enabled, float& newBestDistanceSq)
{
	const float distanceSq = positionToLedge.GetLengthSquared();
	if (distanceSq > bestDistanceSq)
	{
		return false;
	}

	newBestDistanceSq = distanceSq;

	const float fCosAngle = -(testDirection * ledgeInfo.GetFacingDirection());

	// Note: We do the enable check at the end, because,
	// 99% of the time we don't reach this point, we save the extra branching while looping through those ledges

	return ((fCosAngle > cosMaxAngle) && enabled);
}

ILINE bool PointInShere(const Vec3& point, const Sphere& sphere)
{
	return ((sphere.center - point).GetLengthSquared() < (sphere.radius * sphere.radius));
}

void DrawLedge(IRenderAuxGeom* pRenderAuxGeometry, const Vec3 startPoint, const Vec3& endPoint, const Vec3& facingDirection, const ELedgeFlagBitfield flags[LedgeSide_Count])
{
	const float side[2] = { 1.0f, -1.0f };
	const float drawOffset[2] = { 0.01f, 0.01f };

	uint32 currentSide = 0;
	const uint32 sideCount = 1 + ((flags[0] & kLedgeFlag_isDoubleSided) != 0);
	CRY_ASSERT(sideCount <= 2);

	const ColorB colorTable[3][2] = {
		{ Col_Grey, Col_Grey }, { Col_SlateBlue, Col_SlateBlue }, { Col_Red, Col_Orange }
	};

	do
	{
		const Vec3 direction = facingDirection * side[currentSide];
		const Vec3 start = startPoint + (direction * drawOffset[currentSide]);
		const Vec3 end = endPoint + (direction * drawOffset[currentSide]);
		const Vec3 middle = (start + end) * 0.5f;
		const Vec3 ledgeDirection = (end - start).GetNormalized();

		const uint32 colorIdx = ((flags[currentSide] & kLedgeFlag_enabled) == 0) ? 0 : 1 + ((flags[currentSide] & (kLedgeFlag_useVault | kLedgeFlag_useHighVault)) == 0); // TODO - add a different colour for highVault vs normal vault
		CRY_ASSERT(colorIdx <= 2);

		pRenderAuxGeometry->DrawLine(start, colorTable[colorIdx][0], end, colorTable[colorIdx][0], 8.0f);
		pRenderAuxGeometry->DrawLine(middle, colorTable[colorIdx][0], middle + (direction * 0.3f), colorTable[colorIdx][1], 4.0f);
		pRenderAuxGeometry->DrawTriangle(middle - (ledgeDirection * 0.05f), colorTable[colorIdx][0], middle + (ledgeDirection * 0.05f), colorTable[colorIdx][0], middle + (direction * 0.15f), colorTable[colorIdx][1]);
		pRenderAuxGeometry->DrawTriangle(middle + (ledgeDirection * 0.05f), colorTable[colorIdx][0], middle - (ledgeDirection * 0.05f), colorTable[colorIdx][0], middle + (direction * 0.15f), colorTable[colorIdx][1]);

		++currentSide;

	}
	while (currentSide < sideCount);
}

struct SLedgeMarkerBuffer
{
	SLedgeMarkerBuffer(const uint32 _bufferSize)
		: bufferSize(_bufferSize)
		, pMarkers(NULL)
	{
		if (bufferSize)
		{
			pMarkers = new SLedgeMarker[bufferSize];
		}
	}

	~SLedgeMarkerBuffer()
	{
		SAFE_DELETE_ARRAY(pMarkers);
	}

	ILINE void InsertAt(const SLedgeMarker& marker, const uint32 index)
	{
		CRY_ASSERT(index < bufferSize);
		pMarkers[index] = marker;
	}

	SLedgeMarker* pMarkers;
	const uint32  bufferSize;
};
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if LEDGE_MANAGER_EDITING_ENABLED

	#include "Utility/DesignerWarning.h"

void SLedgeObjectEditor::DebugDraw(IRenderAuxGeom* pRenderAux, const Sphere& visibleArea) const
{
	if (PointInShere(m_markers[0].m_worldPosition, visibleArea) == false)
		return;

	for (size_t markerIdx = 0; markerIdx < (m_markers.size() - 1); ++markerIdx)
	{
		ELedgeFlagBitfield ledgeFlags[LedgeSide_Count];
		ledgeFlags[LedgeSide_In] = m_ledgeFlags[LedgeSide_In];
		ledgeFlags[LedgeSide_Out] = m_ledgeFlags[LedgeSide_Out];

		Vec3 startPos = m_markers[markerIdx].m_worldPosition;
		Vec3 endPos = m_markers[markerIdx + 1].m_worldPosition;
		Vec3 startToEnd = endPos - startPos;
		const float startToEndLen = startToEnd.NormalizeSafe();
		const float quarterWidth = startToEndLen * 0.25f; // we use 1/4 width to ensure that a ledge will get no smaller than half its size if the edge adjust amount is larger than the ledge size

		// Must keep this clamping in sync with _FindVectorToClosestPointOnLedge() above
		float endAdjustment = m_ledgeCornerEndAdjustAmount;
		const float adjustAmount = min(endAdjustment, quarterWidth);

		if (m_markers[markerIdx].m_endOrCorner)
		{
			startPos += startToEnd * adjustAmount;

			ledgeFlags[LedgeSide_In] |= kledgeRunTimeOnlyFlag_p0IsEndOrCorner;
			ledgeFlags[LedgeSide_Out] |= kledgeRunTimeOnlyFlag_p0IsEndOrCorner;
		}
		if (m_markers[markerIdx + 1].m_endOrCorner)
		{
			endPos -= startToEnd * adjustAmount;

			ledgeFlags[LedgeSide_In] |= kledgeRunTimeOnlyFlag_p1IsEndOrCorner;
			ledgeFlags[LedgeSide_Out] |= kledgeRunTimeOnlyFlag_p1IsEndOrCorner;
		}

		DrawLedge(pRenderAux, startPos, endPos, m_markers[markerIdx].m_facingDirection, ledgeFlags);
	}
}

void CLedgeManagerEdit::RegisterLedge(EntityId entityId, const SLedgeMarker* pMarkersArray, const uint32 markerCount, ELedgeFlagBitfield ledgeInFlags, ELedgeFlagBitfield ledgeOutFlags, float ledgeCornerMaxAngle, float ledgeCornerEndAdjustAmount)
{
	if (markerCount == 0)
		return;

	TLedgeObjectsEditorContainer::iterator ledgeIt = std::find(m_ledgeObjects.begin(), m_ledgeObjects.end(), entityId);
	const bool notRegistered = ledgeIt == m_ledgeObjects.end();

	if (notRegistered)
	{
		if (m_ledgeObjects.size() == m_ledgeObjects.max_size())
		{
			DesignerWarning(true, "Exceeding maximum ledge count, %d! Not possible to register this ledge in the manager", MAX_LEDGE_ENTITIES);
			return;
		}
		m_ledgeObjects.push_back(SLedgeObjectEditor());
	}

	SLedgeObjectEditor& ledgeObject = notRegistered ? m_ledgeObjects.back() : *ledgeIt;

	ledgeObject.m_entityId = entityId;
	ledgeObject.m_ledgeFlags[LedgeSide_In] = ledgeInFlags;
	ledgeObject.m_ledgeFlags[LedgeSide_Out] = ledgeOutFlags;
	ledgeObject.m_ledgeCornerMaxAngle = ledgeCornerMaxAngle;
	ledgeObject.m_ledgeCornerEndAdjustAmount = ledgeCornerEndAdjustAmount;

	if (ledgeObject.m_markers.size() != markerCount)
	{
		ledgeObject.m_markers.resize(markerCount);
	}

	for (uint32 markerIdx = 0; markerIdx < markerCount; ++markerIdx)
	{
		ledgeObject.m_markers[markerIdx] = pMarkersArray[markerIdx];
	}
}

void CLedgeManagerEdit::UnregisterLedge(EntityId entityId)
{
	const size_t objectCount = m_ledgeObjects.size();

	size_t removeIdx = 0;

	while ((removeIdx < objectCount) && (m_ledgeObjects[removeIdx] != entityId))
	{
		removeIdx++;
	}

	if (removeIdx < objectCount)
	{
		m_ledgeObjects.removeAt((uint32)removeIdx);
	}
}

void CLedgeManagerEdit::UpdateLedgeMarkers(const EntityId entityId, const SLedgeMarker* pMarkersArray, const uint32 markerCount)
{
	TLedgeObjectsEditorContainer::iterator ledgeIt = std::find(m_ledgeObjects.begin(), m_ledgeObjects.end(), entityId);

	if ((ledgeIt != m_ledgeObjects.end()) && (ledgeIt->m_markers.size() == markerCount))
	{
		for (uint32 markerIdx = 0; markerIdx < markerCount; ++markerIdx)
		{
			ledgeIt->m_markers[markerIdx] = pMarkersArray[markerIdx];
		}
	}
}

void CLedgeManagerEdit::EnableLedge(const EntityId entityId, bool enable)
{
	TLedgeObjectsEditorContainer::iterator ledgeIt = std::find(m_ledgeObjects.begin(), m_ledgeObjects.end(), entityId);

	if (ledgeIt != m_ledgeObjects.end())
	{
		SLedgeObjectEditor& ledgeObject = *ledgeIt;
		CRY_ASSERT((ledgeObject.m_ledgeFlags[LedgeSide_In] & kLedgeFlag_static) == 0);

		if (enable)
		{
			ledgeObject.m_ledgeFlags[LedgeSide_In] |= kLedgeFlag_enabled;
			ledgeObject.m_ledgeFlags[LedgeSide_Out] |= kLedgeFlag_enabled;
		}
		else
		{
			ledgeObject.m_ledgeFlags[LedgeSide_In] &= ~kLedgeFlag_enabled;
			ledgeObject.m_ledgeFlags[LedgeSide_Out] &= ~kLedgeFlag_enabled;
		}
	}
}

SLedgeInfo CLedgeManagerEdit::GetLedgeById(const LedgeId& ledgeId) const
{
	const uint16 objectIdx = ledgeId.GetLedgeObjectIdx();
	if (objectIdx < m_ledgeObjects.size())
	{
		const SLedgeObjectEditor& ledgeObject = m_ledgeObjects[objectIdx];

		const uint16 subSegmentIdx = ledgeId.GetSubSegmentIdx();
		if (subSegmentIdx < (ledgeObject.m_markers.size() - 1))
		{
			const uint16 side = ledgeId.GetSide();
			CRY_ASSERT(side < 2);
			const float sideValue[2] = { 1.0f, -1.0f };
			const Vec3 facingDirection = ledgeObject.m_markers[subSegmentIdx].m_facingDirection * sideValue[side];
			const EntityId entityId = (ledgeObject.m_ledgeFlags[side] & kLedgeFlag_static) ? 0 : ledgeObject.m_entityId;

			ELedgeFlagBitfield flags = ledgeObject.m_ledgeFlags[side];
			if (ledgeObject.m_markers[subSegmentIdx].m_endOrCorner)
			{
				flags |= kledgeRunTimeOnlyFlag_p0IsEndOrCorner;
			}
			if (ledgeObject.m_markers[subSegmentIdx + 1].m_endOrCorner)
			{
				flags |= kledgeRunTimeOnlyFlag_p1IsEndOrCorner;
			}

			return SLedgeInfo(entityId, ledgeObject.m_markers[subSegmentIdx].m_worldPosition, ledgeObject.m_markers[subSegmentIdx + 1].m_worldPosition, facingDirection, flags, ledgeObject.m_ledgeCornerEndAdjustAmount);
		}
	}

	return SLedgeInfo();
}

LedgeId CLedgeManagerEdit::FindNearestLedge(const Vec3& referencePosition, const Vec3& testDirection, const float maxDistance, const float angleRange, const float extendedAngleRange) const
{
	LedgeId bestLedgeId;

	float closestDistanceSq = maxDistance * maxDistance;
	const float fCosMaxAngleTable[2] = { cosf(angleRange), cosf(extendedAngleRange) };
	const float side[2] = { 1.0f, -1.0f };

	SLedgeInfo ledgeInfo;

	const uint32 ledgeObjectCount = (uint32)m_ledgeObjects.size();
	for (uint32 objectIdx = 0; objectIdx < ledgeObjectCount; ++objectIdx)
	{
		const SLedgeObjectEditor& ledgeObject = m_ledgeObjects[objectIdx];
		const uint32 markersCount = (uint32)ledgeObject.m_markers.size();
		const uint32 sideCount = 1 + ((ledgeObject.m_ledgeFlags[LedgeSide_In] & kLedgeFlag_isDoubleSided) != 0);
		const bool enabled = (ledgeObject.m_ledgeFlags[LedgeSide_In] & kLedgeFlag_enabled) != 0;

		CRY_ASSERT(sideCount <= 2);

		uint32 currentSide = 0;
		do
		{
			for (uint32 markerIdx = 0; markerIdx < (markersCount - 1); ++markerIdx)
			{
				ELedgeFlagBitfield flags = kLedgeFlag_none;

				if (ledgeObject.m_markers[markerIdx].m_endOrCorner)
				{
					flags |= kledgeRunTimeOnlyFlag_p0IsEndOrCorner;
				}
				if (ledgeObject.m_markers[markerIdx + 1].m_endOrCorner)
				{
					flags |= kledgeRunTimeOnlyFlag_p1IsEndOrCorner;
				}

				ledgeInfo = SLedgeInfo(ledgeObject.m_entityId, ledgeObject.m_markers[markerIdx].m_worldPosition, ledgeObject.m_markers[markerIdx + 1].m_worldPosition,
				                       ledgeObject.m_markers[markerIdx].m_facingDirection * side[currentSide], flags, ledgeObject.m_ledgeCornerEndAdjustAmount);

				// Explanation: (Please do not delete this comment)
				//	The item can be skipped if the angle is too big.
				//	Since only the cosine of angles are compared,
				//	bigger angles result in smaller values (hence the less_than comparison)
				const uint32 thresholdIdx = ((ledgeObject.m_ledgeFlags[currentSide] & (kLedgeFlag_useVault | kLedgeFlag_useHighVault)) != 0);
				CRY_ASSERT(thresholdIdx < 2);

				const float fCosMaxAngle = fCosMaxAngleTable[thresholdIdx];

				const Vec3 vPosToLedge = _FindVectorToClosestPointOnLedge(referencePosition, ledgeInfo);

				float distanceSq;
				if (IsBestLedge(vPosToLedge, testDirection, ledgeInfo, closestDistanceSq, fCosMaxAngle, enabled, distanceSq) == false)
					continue;

				bestLedgeId = LedgeId(objectIdx, markerIdx, currentSide);
				closestDistanceSq = distanceSq;
			}

			currentSide++;

		}
		while (currentSide < sideCount);
	}

	return bestLedgeId;
}

void CLedgeManagerEdit::Export(const char* fileName) const
{
	const uint32 totalLedgeObjectsCount = m_ledgeObjects.size();

	if (totalLedgeObjectsCount > 0)
	{
		CCryFile file;
		if (false != file.Open(fileName, "wb"))
		{
			// Count number of markers ...
			uint32 totalLedgeMarkersCount = 0;
			for (uint32 objectIdx = 0; objectIdx < totalLedgeObjectsCount; ++objectIdx)
			{
				totalLedgeMarkersCount += m_ledgeObjects[objectIdx].m_markers.size();
			}

			// Prepare buffers ...
			SLedgeObject ledgeObjectBuffer[MAX_LEDGE_ENTITIES];
			SLedgeMarkerBuffer ledgeMarkersBuffer(totalLedgeMarkersCount);

			uint32 currentMarkerIdx = 0;
			for (uint32 objectIdx = 0; objectIdx < totalLedgeObjectsCount; ++objectIdx)
			{
				SLedgeObject& ledgeObject = ledgeObjectBuffer[objectIdx];
				const SLedgeObjectEditor& ledgeObjectEdit = m_ledgeObjects[objectIdx];

				ledgeObject.m_entityId = ((ledgeObjectEdit.m_ledgeFlags[LedgeSide_In] & kLedgeFlag_static) == 0) ? ledgeObjectEdit.m_entityId : 0;
				ledgeObject.m_ledgeFlags[LedgeSide_In] = ledgeObjectEdit.m_ledgeFlags[LedgeSide_In];
				ledgeObject.m_ledgeFlags[LedgeSide_Out] = ledgeObjectEdit.m_ledgeFlags[LedgeSide_Out];
				ledgeObject.m_ledgeCornerEndAdjustAmount = ledgeObjectEdit.m_ledgeCornerEndAdjustAmount;
				ledgeObject.m_markersStartIdx = currentMarkerIdx;
				ledgeObject.m_markersCount = static_cast<uint16>(ledgeObjectEdit.m_markers.size());

				CRY_ASSERT((ledgeObject.m_markersStartIdx + ledgeObject.m_markersCount) <= totalLedgeMarkersCount);

				for (size_t markerIdx = 0; markerIdx < ledgeObjectEdit.m_markers.size(); ++markerIdx)
				{
					ledgeMarkersBuffer.InsertAt(ledgeObjectEdit.m_markers[markerIdx], currentMarkerIdx + markerIdx);
				}
				currentMarkerIdx += ledgeObject.m_markersCount;
			}

			// Write to file...

			// File version
			uint32 nFileVersion = LEDGE_DATA_FILE_VERSION;
			file.Write(&nFileVersion, sizeof(nFileVersion));

			// Ledges and markers info
			file.Write(&totalLedgeObjectsCount, sizeof(totalLedgeObjectsCount));
			file.Write(&totalLedgeMarkersCount, sizeof(totalLedgeMarkersCount));

			file.Write(&ledgeObjectBuffer[0], sizeof(ledgeObjectBuffer[0]) * totalLedgeObjectsCount);
			file.Write(&ledgeMarkersBuffer.pMarkers[0], sizeof(ledgeMarkersBuffer.pMarkers[0]) * ledgeMarkersBuffer.bufferSize);

			file.Close();
		}
	}
}

void CLedgeManagerEdit::DebugDraw() const
{
	const bool doDraw = (g_LedgeGrabManager_DebugDrawInEditor >= 2) || ((g_LedgeGrabManager_DebugDrawInEditor == 1) && (gEnv->IsEditing()));

	if (doDraw)
	{
		IRenderAuxGeom* pRenderAuxGeometry = gEnv->pRenderer->GetIRenderAuxGeom();
		const CCamera& viewCamera = gEnv->pSystem->GetViewCamera();
		const Sphere visibleArea(viewCamera.GetPosition(), g_LedgeGrabManager_DebugDrawInEditor_Distance);

		for (uint32 objectIdx = 0; objectIdx < m_ledgeObjects.size(); ++objectIdx)
		{
			m_ledgeObjects[objectIdx].DebugDraw(pRenderAuxGeometry, visibleArea);
		}
	}
}

void CLedgeManagerEdit::OnDisplayHelpersChanged(bool displayHelpers)
{
	if (displayHelpers)
	{
		g_LedgeGrabManager_DebugDrawInEditor = m_lastDebugDrawValue;
	}
	else
	{
		m_lastDebugDrawValue = g_LedgeGrabManager_DebugDrawInEditor;
		g_LedgeGrabManager_DebugDrawInEditor = 0;
	}
}

void CLedgeManagerEdit::RegisterCVars()
{
	REGISTER_CVAR(g_LedgeGrabManager_DebugDrawInEditor, 1, VF_DUMPTODISK, "Toggles debug rendering on ledges in editor: 0 - Disabled / 1 - Enabled in editing mode / 2 - Enabled in game mode / 3 - Visualize only deprecated ledge entities");
	REGISTER_CVAR(g_LedgeGrabManager_DebugDrawInEditor_Distance, 35.0f, VF_CHEAT, "Max distance from camera at which ledges are rendered");

	m_lastDebugDrawValue = g_LedgeGrabManager_DebugDrawInEditor;
}

#endif //LEDGE_MANAGER_EDITING_ENABLED

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CLedgeManager::CLedgeManager()
	: m_editorManager(gEnv->IsEditor())
{
	RegisterCVars();
	Reset();
}

CLedgeManager::~CLedgeManager()
{
	gEnv->pConsole->UnregisterVariable("g_LedgeGrabManager_DebugDraw");
	gEnv->pConsole->UnregisterVariable("g_LedgeGrabManager_DebugDraw_Distance");
	gEnv->pConsole->UnregisterVariable("g_LedgeGrabManager_DebugDrawInEditor");
	gEnv->pConsole->UnregisterVariable("g_LedgeGrabManager_DebugDrawInEditor_Distance");
}

void CLedgeManager::Reset()
{
	if (m_editorManager.IsInEditorMode())
	{
		m_editorManager.Reset();
	}
	else
	{
		m_levelLedges.Release();
	}
}

void CLedgeManager::Load(const char* fileName)
{
	// Clear in case there is anything left
	Reset();

	// In editor we don't load exported data, entities will recreate on load
	if (m_editorManager.IsInEditorMode())
		return;

	CCryFile file;
	if (false != file.Open(fileName, "rb"))
	{
		// File version
		uint32 nFileVersion;
		file.ReadType(&nFileVersion);

		if (nFileVersion != LEDGE_DATA_FILE_VERSION)
		{
			GameWarning("!LedgeManager: Level data could not be loaded, file %s has version %d, expected %d. Level needs re-export", fileName, nFileVersion, LEDGE_DATA_FILE_VERSION);
			return;
		}

		// Ledges and markers info
		uint32 totalLedgeObjectsCount, totalLedgeMarkersCount;
		file.ReadType(&totalLedgeObjectsCount);
		file.ReadType(&totalLedgeMarkersCount);

		m_levelLedges.Allocate(totalLedgeObjectsCount, totalLedgeMarkersCount);

		file.ReadType(&m_levelLedges.m_pLedgeObjects[0], totalLedgeObjectsCount);
		file.ReadType(&m_levelLedges.m_pMarkers[0], totalLedgeMarkersCount);

		file.Close();
	}
}

void CLedgeManager::UpdateLedgeMarkers(const EntityId entityId, const SLedgeMarker* pMarkersArray, const uint32 markerCount)
{
	if (m_editorManager.IsInEditorMode())
	{
		m_editorManager.UpdateLedgeMarkers(entityId, pMarkersArray, markerCount);
	}
	else
	{
		SLedgeObject* pLedgeObject = m_levelLedges.FindLedgeForEntity(entityId);

		if ((pLedgeObject != NULL) && (pLedgeObject->m_markersCount == markerCount))
		{
			const uint32 endMarkerIdx = pLedgeObject->m_markersStartIdx + pLedgeObject->m_markersCount;
			CRY_ASSERT(endMarkerIdx <= m_levelLedges.m_markerCount);

			for (uint32 markerIdx = pLedgeObject->m_markersStartIdx, idx = 0; markerIdx < endMarkerIdx; ++markerIdx, ++idx)
			{
				m_levelLedges.m_pMarkers[markerIdx] = pMarkersArray[idx];
			}
		}
	}
}

void CLedgeManager::EnableLedge(const EntityId entityId, bool enable)
{
	if (m_editorManager.IsInEditorMode())
	{
		m_editorManager.EnableLedge(entityId, enable);
	}
	else
	{
		SLedgeObject* pLedgeObject = m_levelLedges.FindLedgeForEntity(entityId);

		if (pLedgeObject != NULL)
		{
			CRY_ASSERT((pLedgeObject->m_ledgeFlags[LedgeSide_In] & kLedgeFlag_static) == 0);

			if (enable)
			{
				pLedgeObject->m_ledgeFlags[LedgeSide_In] |= kLedgeFlag_enabled;
				pLedgeObject->m_ledgeFlags[LedgeSide_Out] |= kLedgeFlag_enabled;
			}
			else
			{
				pLedgeObject->m_ledgeFlags[LedgeSide_In] &= ~kLedgeFlag_enabled;
				pLedgeObject->m_ledgeFlags[LedgeSide_Out] &= ~kLedgeFlag_enabled;
			}
		}
	}
}

LedgeId CLedgeManager::FindNearestLedge(const Vec3& referencePosition, const Vec3& testDirection, float maxDistance /*= 2.0f*/, float angleRange /*= DEG2RAD(35.0f)*/, float extendedAngleRange /*= DEG2RAD(50.0f)*/) const
{
	if (m_editorManager.IsInEditorMode())
	{
		return m_editorManager.FindNearestLedge(referencePosition, testDirection, maxDistance, angleRange, extendedAngleRange);
	}
	else
	{
		LedgeId bestLedgeId;

		float closestDistanceSq = maxDistance * maxDistance;
		const float fCosMaxAngleTable[2] = { cosf(angleRange), cosf(extendedAngleRange) };
		const float side[2] = { 1.0f, -1.0f };

		SLedgeInfo ledgeInfo;

		const uint32 ledgeObjectCount = m_levelLedges.m_ledgeCount;
		for (uint32 objectIdx = 0; objectIdx < ledgeObjectCount; ++objectIdx)
		{
			const SLedgeObject& ledgeObject = m_levelLedges.m_pLedgeObjects[objectIdx];
			const uint32 startMarkerIdx = ledgeObject.m_markersStartIdx;
			const uint32 endMarkerIdx = ledgeObject.m_markersStartIdx + ledgeObject.m_markersCount;
			CRY_ASSERT(endMarkerIdx <= m_levelLedges.m_markerCount);

			const uint32 sideCount = 1 + ((ledgeObject.m_ledgeFlags[LedgeSide_In] & kLedgeFlag_isDoubleSided) != 0);
			const bool enabled = (ledgeObject.m_ledgeFlags[LedgeSide_In] & kLedgeFlag_enabled) != 0;

			CRY_ASSERT(sideCount <= 2);

			uint32 currentSide = 0;
			do
			{
				for (uint32 markerIdx = startMarkerIdx; markerIdx < (endMarkerIdx - 1); ++markerIdx)
				{
					ELedgeFlagBitfield flags = kLedgeFlag_none;

					if (m_levelLedges.m_pMarkers[markerIdx].m_endOrCorner)
					{
						flags |= kledgeRunTimeOnlyFlag_p0IsEndOrCorner;
					}
					if (m_levelLedges.m_pMarkers[markerIdx + 1].m_endOrCorner)
					{
						flags |= kledgeRunTimeOnlyFlag_p1IsEndOrCorner;
					}

					ledgeInfo = SLedgeInfo(ledgeObject.m_entityId, m_levelLedges.m_pMarkers[markerIdx].m_worldPosition, m_levelLedges.m_pMarkers[markerIdx + 1].m_worldPosition,
					                       m_levelLedges.m_pMarkers[markerIdx].m_facingDirection * side[currentSide], flags, ledgeObject.m_ledgeCornerEndAdjustAmount);

					// Explanation: (Please do not delete this comment)
					//	The item can be skipped if the angle is too big.
					//	Since only the cosine of angles are compared,
					//	bigger angles result in smaller values (hence the less_than comparison)
					const uint32 thresholdIdx = ((ledgeObject.m_ledgeFlags[currentSide] & (kLedgeFlag_useVault | kLedgeFlag_useHighVault)) != 0);
					CRY_ASSERT(thresholdIdx < 2);

					const float fCosMaxAngle = fCosMaxAngleTable[thresholdIdx];

					const Vec3 vPosToLedge = _FindVectorToClosestPointOnLedge(referencePosition, ledgeInfo);

					float distanceSq;
					if (IsBestLedge(vPosToLedge, testDirection, ledgeInfo, closestDistanceSq, fCosMaxAngle, enabled, distanceSq) == false)
						continue;

					bestLedgeId = LedgeId(objectIdx, (markerIdx - startMarkerIdx), currentSide);
					closestDistanceSq = distanceSq;
				}

				currentSide++;

			}
			while (currentSide < sideCount);
		}

		return bestLedgeId;
	}
}

// ---------------------------------------------------

SLedgeInfo CLedgeManager::GetLedgeById(const LedgeId& ledgeId) const
{
	if (m_editorManager.IsInEditorMode())
	{
		return m_editorManager.GetLedgeById(ledgeId);
	}
	else
	{
		const uint16 objectIdx = ledgeId.GetLedgeObjectIdx();
		if (objectIdx < m_levelLedges.m_ledgeCount)
		{
			const SLedgeObject& ledgeObject = m_levelLedges.m_pLedgeObjects[objectIdx];

			CRY_ASSERT(ledgeId.GetSubSegmentIdx() < ledgeObject.m_markersCount);
			const uint16 segmentIdx = ledgeObject.m_markersStartIdx + ledgeId.GetSubSegmentIdx();
			if (segmentIdx < (m_levelLedges.m_markerCount - 1))
			{
				const uint16 side = ledgeId.GetSide();
				CRY_ASSERT(side < 2);
				const float sideValue[2] = { 1.0f, -1.0f };
				const Vec3 facingDirection = m_levelLedges.m_pMarkers[segmentIdx].m_facingDirection * sideValue[side];
				const EntityId entityId = (ledgeObject.m_ledgeFlags[side] & kLedgeFlag_static) ? 0 : ledgeObject.m_entityId;

				ELedgeFlagBitfield flags = ledgeObject.m_ledgeFlags[side];
				if (m_levelLedges.m_pMarkers[segmentIdx].m_endOrCorner)
				{
					flags |= kledgeRunTimeOnlyFlag_p0IsEndOrCorner;
				}
				if (m_levelLedges.m_pMarkers[segmentIdx + 1].m_endOrCorner)
				{
					flags |= kledgeRunTimeOnlyFlag_p1IsEndOrCorner;
				}

				return SLedgeInfo(entityId, m_levelLedges.m_pMarkers[segmentIdx].m_worldPosition, m_levelLedges.m_pMarkers[segmentIdx + 1].m_worldPosition, facingDirection, flags, ledgeObject.m_ledgeCornerEndAdjustAmount);
			}
		}

		return SLedgeInfo();
	}
}

Vec3 CLedgeManager::FindVectorToClosestPointOnLedge(const Vec3& vPoint, const SLedgeInfo& ledgeInfo) const
{
	return _FindVectorToClosestPointOnLedge(vPoint, ledgeInfo);
}

void CLedgeManager::Serialize(TSerialize ser)
{
	if (ser.IsReading())
	{
		ser.BeginGroup("LevelLedges");
		{
			uint32 objectsToSerialize = 0;
			ser.Value("serializedLedgeCount", objectsToSerialize);

			for (uint32 idx = 0; idx < objectsToSerialize; ++idx)
			{
				ser.BeginGroup("Ledge");
				{
					uint32 objectIdx = 0;
					ser.Value("ledgeIndex", objectIdx);

					if (objectIdx < m_levelLedges.m_ledgeCount)
					{
						SLedgeObject& ledgeObject = m_levelLedges.m_pLedgeObjects[objectIdx];

						ser.Value("flagsIn", ledgeObject.m_ledgeFlags[LedgeSide_In]);
						ser.Value("flagsOut", ledgeObject.m_ledgeFlags[LedgeSide_Out]);

						uint32 markerCount = 0;
						ser.Value("markersCount", markerCount);

						if (markerCount == ledgeObject.m_markersCount)
						{
							const uint32 markerEnd = ledgeObject.m_markersStartIdx + ledgeObject.m_markersCount;
							CRY_ASSERT(markerEnd <= m_levelLedges.m_markerCount);
							for (uint32 markerIdx = ledgeObject.m_markersStartIdx; markerIdx < markerEnd; ++markerIdx)
							{
								SLedgeMarker& marker = m_levelLedges.m_pMarkers[markerIdx];

								ser.BeginGroup("Marker");
								{
									ser.Value("pos", marker.m_worldPosition);
									ser.Value("dir", marker.m_facingDirection);
								}
								ser.EndGroup();  // "Marker"
							}
						}
						else
						{
							//Save game not in synch with level data
							GameWarning("LedgeManager - Trying to update markers for ledge %d, but there is a mismatch in the markers count. %d-%d", objectIdx, markerCount, ledgeObject.m_markersCount);
						}
					}
					else
					{
						//Save game not in synch with level data
						GameWarning("LedgeManager - Trying to load saved data for ledge %d, when there is only %d registered", objectIdx, m_levelLedges.m_ledgeCount);
					}
				}
				ser.EndGroup(); // "Ledge"
			}
		}
		ser.EndGroup(); // "LevelLedges"
	}
	else
	{
		ser.BeginGroup("LevelLedges");
		{
			uint32 objectsToSerialize = 0;
			for (uint32 objectIdx = 0; objectIdx < m_levelLedges.m_ledgeCount; ++objectIdx)
			{
				objectsToSerialize += (m_levelLedges.m_pLedgeObjects[objectIdx].NeedsToBeSerialized());
			}
			ser.Value("serializedLedgeCount", objectsToSerialize);

			for (uint32 objectIdx = 0; objectIdx < m_levelLedges.m_ledgeCount; ++objectIdx)
			{
				SLedgeObject& ledgeObject = m_levelLedges.m_pLedgeObjects[objectIdx];
				if (ledgeObject.NeedsToBeSerialized() == false)
					continue;

				ser.BeginGroup("Ledge");
				{
					ser.Value("ledgeIndex", objectIdx);
					ser.Value("flagsIn", ledgeObject.m_ledgeFlags[LedgeSide_In]);
					ser.Value("flagsOut", ledgeObject.m_ledgeFlags[LedgeSide_Out]);
					ser.Value("markersCount", ledgeObject.m_markersCount);

					const uint32 markerEnd = ledgeObject.m_markersStartIdx + ledgeObject.m_markersCount;
					CRY_ASSERT(markerEnd <= m_levelLedges.m_markerCount);
					for (uint32 markerIdx = ledgeObject.m_markersStartIdx; markerIdx < markerEnd; ++markerIdx)
					{
						SLedgeMarker& marker = m_levelLedges.m_pMarkers[markerIdx];

						ser.BeginGroup("Marker");
						{
							ser.Value("pos", marker.m_worldPosition);
							ser.Value("dir", marker.m_facingDirection);
						}
						ser.EndGroup();  // "Marker"
					}
				}
				ser.EndGroup(); // "Ledge"
			}
		}
		ser.EndGroup(); // "LevelLedges"
	}

}

void CLedgeManager::DebugDraw() const
{
	if (m_editorManager.IsInEditorMode())
	{
		m_editorManager.DebugDraw();
	}
	else
	{
		const bool doDraw = (g_LedgeGrabManager_DebugDraw != 0);
		if (doDraw)
		{
			IRenderAuxGeom* pRenderAuxGeometry = gEnv->pRenderer->GetIRenderAuxGeom();
			const CCamera& viewCamera = gEnv->pSystem->GetViewCamera();
			const Sphere visibleArea(viewCamera.GetPosition(), g_LedgeGrabManager_DebugDraw_Distance);

			const int sideCountMultiplier[2] = { 1, 2 };
			int totalLedgeCount = 0;
			int nonStaticLedges = 0;

			for (uint32 objectIdx = 0; objectIdx < m_levelLedges.m_ledgeCount; ++objectIdx)
			{
				const SLedgeObject& ledgeObject = m_levelLedges.m_pLedgeObjects[objectIdx];
				const uint32 startMarkerIdx = ledgeObject.m_markersStartIdx;
				const uint32 endMarkerIdx = ledgeObject.m_markersStartIdx + ledgeObject.m_markersCount;
				CRY_ASSERT(endMarkerIdx <= m_levelLedges.m_markerCount);

				const uint32 sideIdx = ((ledgeObject.m_ledgeFlags[LedgeSide_In] & kLedgeFlag_isDoubleSided) != 0);
				CRY_ASSERT(sideIdx < 2);

				nonStaticLedges += (ledgeObject.m_entityId != 0);
				totalLedgeCount += ((ledgeObject.m_markersCount - 1) * sideCountMultiplier[sideIdx]);

				if (PointInShere(m_levelLedges.m_pMarkers[startMarkerIdx].m_worldPosition, visibleArea) == false)
					continue;

				for (size_t markerIdx = startMarkerIdx; markerIdx < (endMarkerIdx - 1); ++markerIdx)
				{
					DrawLedge(pRenderAuxGeometry, m_levelLedges.m_pMarkers[markerIdx].m_worldPosition, m_levelLedges.m_pMarkers[markerIdx + 1].m_worldPosition, m_levelLedges.m_pMarkers[markerIdx].m_facingDirection, ledgeObject.m_ledgeFlags);
				}
			}

			if (g_LedgeGrabManager_DebugDraw > 1)
			{
				IRenderAuxText::Draw2dLabel( 50.0f, 50.f, 1.5f, Col_White, false, "Total Number of ledges %d - Non static %d", totalLedgeCount, nonStaticLedges );
			}
		}
	}
}

void CLedgeManager::RegisterCVars()
{
	if (m_editorManager.IsInEditorMode() == false)
	{
		REGISTER_CVAR(g_LedgeGrabManager_DebugDraw, 0, VF_CHEAT, "Toggles debug rendering on ledges: 0 - Disabled / 1 - Enabled");
		REGISTER_CVAR(g_LedgeGrabManager_DebugDraw_Distance, 35.0f, VF_CHEAT, "Max distance from camera at which ledges are rendered");
	}
}
