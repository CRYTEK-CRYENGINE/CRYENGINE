// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Gizmo.h"
#include "GizmoManager.h"
#include "Objects/DisplayContext.h"
#include "TransformManipulator.h"
#include "IDisplayViewport.h"

#include <CrySerialization/yasli/Enum.h>

YASLI_ENUM_BEGIN_NESTED(SGizmoPreferences, ERotationInteractionMode, "ERotationInteractionMode")
YASLI_ENUM(SGizmoPreferences::eRotationDial, "dial", "Dial");
YASLI_ENUM(SGizmoPreferences::eRotationLinear, "linear", "Linear");
YASLI_ENUM_END()

//////////////////////////////////////////////////////////////////////////
bool SGizmoPreferences::Serialize(yasli::Archive& ar)
{
	ar.openBlock("axisGizmo", "Axis Gizmo");
	ar(axisGizmoSize, "axisGizmoSize", "Axis Gizmo Size");
	ar(axisGizmoText, "axisGizmoText", "Text Labels on Axis Gizmo");
	ar(rotationInteraction, "axisInteraction", "Interaction for Rotation Gizmo");
	ar.closeBlock();

	ar.openBlock("helpers", "Helpers");
	ar(helperScale, "helperScale", "Scale");
	ar.closeBlock();

	return true;
}

EDITOR_COMMON_API SGizmoPreferences gGizmoPreferences;
REGISTER_PREFERENCES_PAGE_PTR(SGizmoPreferences, &gGizmoPreferences)

//////////////////////////////////////////////////////////////////////////
void CGizmoManager::Display(DisplayContext& dc)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	AABB bbox;
	std::vector<CGizmo*> todelete;

	// Only display the highlighted gizmo if we are interacting with it
	if (m_highlighted && m_highlighted->GetFlag(EGIZMO_INTERACTING))
	{
		if (m_highlighted->IsDelete())
		{
			todelete.push_back(m_highlighted);
		}
		// probably hard to get this happening somehow, yet, keep this here
		else if (!m_highlighted->GetFlag(EGIZMO_HIDDEN))
		{
			m_highlighted->GetWorldBounds(bbox);
			if (dc.IsVisible(bbox))
			{
				m_highlighted->Display(dc);
			}
		}
	}

	for (Gizmos::iterator it = m_gizmos.begin(); it != m_gizmos.end(); ++it)
	{
		CGizmo* gizmo = *it;

		// do not display gizmos that are tagged for deletion
		if (gizmo->IsDelete())
		{
			todelete.push_back(gizmo);
			continue;
		}

		if (gizmo->GetFlag(EGIZMO_HIDDEN))
			continue;

		gizmo->GetWorldBounds(bbox);
		if (dc.IsVisible(bbox))
		{
			gizmo->Display(dc);
		}
	}

	// Delete gizmos that needs deletion.
	for (int i = 0; i < todelete.size(); i++)
	{
		RemoveGizmo(todelete[i]);
	}
}

ITransformManipulator* CGizmoManager::AddManipulator(ITransformManipulatorOwner* owner)
{
	if (!owner)
	{
		return nullptr;
	}

	CTransformManipulator* pManipulator = new CTransformManipulator(owner);
	m_gizmos.insert(pManipulator);

	return pManipulator;
}

void CGizmoManager::RemoveManipulator(ITransformManipulator* pManipulator)
{
	CTransformManipulator* pGizmo = static_cast <CTransformManipulator*> (pManipulator);
	RemoveGizmo(pGizmo);
}


//////////////////////////////////////////////////////////////////////////
void CGizmoManager::AddGizmo(CGizmo* gizmo)
{
	m_gizmos.insert(gizmo);
}

//////////////////////////////////////////////////////////////////////////
void CGizmoManager::RemoveGizmo(CGizmo* gizmo)
{
	if (gizmo == m_highlighted)
	{
		m_highlighted = nullptr;
	}

	m_gizmos.erase(gizmo);
}

//////////////////////////////////////////////////////////////////////////
int CGizmoManager::GetGizmoCount() const
{
	return (int)m_gizmos.size();
}

//////////////////////////////////////////////////////////////////////////
CGizmo* CGizmoManager::GetGizmoByIndex(int nIndex) const
{
	int nCount = 0;
	Gizmos::iterator ii = m_gizmos.begin();
	for (; ii != m_gizmos.end(); ++ii)
	{
		if ((nCount++) == nIndex)
			return *ii;
	}
	return NULL;
}

CGizmo* CGizmoManager::GetHighlightedGizmo() const
{
	return m_highlighted;
}

//////////////////////////////////////////////////////////////////////////
bool CGizmoManager::HitTest(HitContext& hc)
{
	float mindist = FLT_MAX;

	HitContext ghc = hc;
	bool bGizmoHit = false;

	AABB bbox;

	for (Gizmos::iterator it = m_gizmos.begin(); it != m_gizmos.end(); ++it)
	{
		CGizmo* gizmo = *it;

		if (gizmo->GetFlag(EGIZMO_SELECTABLE))
		{
			if (gizmo->HitTest(ghc))
			{
				bGizmoHit = true;
				if (ghc.dist < mindist)
				{
					ghc.gizmo = gizmo;
					mindist = ghc.dist;
					hc = ghc;
				}
			}
		}
	}
	return bGizmoHit;
}

bool CGizmoManager::HandleMouseInput(IDisplayViewport* view, EMouseEvent event, CPoint& point, int flags)
{

	if (event == eMouseMove)
	{
		// release previous highlighted gizmo
		if (m_highlighted)
		{
			// only update highlights if gizmo is not interacting right now
			if (m_highlighted->GetFlag(EGIZMO_INTERACTING))
			{
				return m_highlighted->MouseCallback(view, event, point, flags);
			}

			m_highlighted->SetHighlighted(false);
			m_highlighted = nullptr;
		}

		HitContext hitInfo;
		Vec3 raySrc(0, 0, 0), rayDir(1, 0, 0);
		view->ViewToWorldRay(point, hitInfo.raySrc, hitInfo.rayDir);
		hitInfo.view = view;
		hitInfo.point2d = point;

		if (HitTest(hitInfo))
		{
			m_highlighted = hitInfo.gizmo;
			m_highlighted->SetHighlighted(true);
		}

	}

	if (m_highlighted)
	{
		return m_highlighted->MouseCallback(view, event, point, flags);
	}

	return false;
}

