// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "Grid.h"

#include "Objects/ISelectionGroup.h"
#include "Objects/BaseObject.h"

#include <QWidget>
#include <QVBoxLayout>

#include <CrySerialization/Math.h>
#include <CrySerialization/yasli/decorators/Range.h>

class CSnappingPreferencesDialog : public CDockableWidget
{
public:
	CSnappingPreferencesDialog(QWidget* parent = nullptr)
	{
		QVBoxLayout* pLayout = new QVBoxLayout();
		pLayout->setMargin(0);
		pLayout->setSpacing(0);

		pLayout->addWidget(GetIEditor()->GetPreferences()->GetPageWidget("General/Snapping"));
		setLayout(pLayout);
	}

	~CSnappingPreferencesDialog()
	{

	}

	virtual IViewPaneClass::EDockingDirection GetDockingDirection() const override { return IViewPaneClass::DOCK_FLOAT; }
	virtual const char*                       GetPaneTitle() const override { return "Snapping Preferences"; };
	virtual QRect                             GetPaneRect() override { return QRect(0, 0, 270, 200); }
};

REGISTER_HIDDEN_VIEWPANE_FACTORY(CSnappingPreferencesDialog, "Snap Settings", "Settings", true)

EDITOR_COMMON_API SSnappingPreferences gSnappingPreferences;
REGISTER_PREFERENCES_PAGE_PTR(SSnappingPreferences, &gSnappingPreferences)

bool SSnappingPreferences::Serialize(yasli::Archive& ar)
{
	ar.openBlock("gridSnapping", "Grid Snapping");
	ar(yasli::Range(m_gridSize, 0.001, 10000.0), "gridSize", "Grid Size");
	ar(yasli::Range(m_gridScale, 0.001, 10000.0), "gridScale", "Grid Scale");
	ar(yasli::Range(m_gridMajorLine, 1, 10000), "gridMajorLine", "Grid Major Line");
	ar.closeBlock();

	ar.openBlock("angleSnapping", "Angle Snapping");
	ar(yasli::Range(m_angleSnap, 0.001, 360.0), "angleSnap", "Angle Snap");
	ar.closeBlock();

	ar.openBlock("scaleSnapping", "Scale Snapping");
	ar(yasli::Range(m_scaleSnap, 0.01f, 1000.0f), "scaleSnap", "Scale Snap");
	ar.closeBlock();
	return true;
}

double SSnappingPreferences::SnapAngle(double angle) const
{
	if (!m_angleSnappingEnabled)
		return angle;
	return floor(angle / m_angleSnap + 0.5) * m_angleSnap;
}

Ang3 SSnappingPreferences::SnapAngle(const Ang3& vec) const
{
	if (!m_angleSnappingEnabled)
		return vec;
	Ang3 snapped;
	snapped.x = floor(vec.x / m_angleSnap + 0.5) * m_angleSnap;
	snapped.y = floor(vec.y / m_angleSnap + 0.5) * m_angleSnap;
	snapped.z = floor(vec.z / m_angleSnap + 0.5) * m_angleSnap;
	return snapped;
}

double SSnappingPreferences::SnapLength(const double length) const
{
	if (!m_gridSnappingEnabled || m_gridSize < 0.001)
		return length;
	return floor((length / m_gridSize) / m_gridScale + 0.5) * m_gridSize * m_gridScale;
}

float SSnappingPreferences::SnapScale(const float scale) const
{
	if (!m_scaleSnappingEnabled)
		return scale;
	return floor(scale / m_scaleSnap + 0.5) * m_scaleSnap;
}

Vec3 SSnappingPreferences::Snap(const Vec3& vec, bool bForceSnap /*= false*/) const
{
	if ((!bForceSnap && !m_gridSnappingEnabled) || m_gridSize < 0.001)
		return vec;
	Vec3 snapped;
	snapped.x = floor((vec.x / m_gridSize) / m_gridScale + 0.5) * m_gridSize * m_gridScale;
	snapped.y = floor((vec.y / m_gridSize) / m_gridScale + 0.5) * m_gridSize * m_gridScale;
	snapped.z = floor((vec.z / m_gridSize) / m_gridScale + 0.5) * m_gridSize * m_gridScale;
	return snapped;
}

Vec3 SSnappingPreferences::Snap(const Vec3& vec, double fZoom) const
{
	if (!gridSnappingEnabled() || m_gridSize < 0.001f)
		return vec;

	Matrix34 tm = GetMatrix();

	double zoomscale = m_gridScale * fZoom;
	Vec3 snapped;

	Matrix34 invtm = tm.GetInverted();

	snapped = invtm * vec;

	snapped.x = floor((snapped.x / m_gridSize) / zoomscale + 0.5) * m_gridSize * zoomscale;
	snapped.y = floor((snapped.y / m_gridSize) / zoomscale + 0.5) * m_gridSize * zoomscale;
	snapped.z = floor((snapped.z / m_gridSize) / zoomscale + 0.5) * m_gridSize * zoomscale;

	snapped = tm * snapped;

	return snapped;
}

Matrix34 SSnappingPreferences::GetMatrix() const
{
	Matrix34 tm = Matrix34::CreateIdentity();
	const ISelectionGroup* sel = GetIEditor()->GetISelectionGroup();
	sel->GetManipulatorMatrix(GetIEditor()->GetReferenceCoordSys(), tm);
	tm.SetTranslation(Vec3(0, 0, 0));

	return tm;
}
