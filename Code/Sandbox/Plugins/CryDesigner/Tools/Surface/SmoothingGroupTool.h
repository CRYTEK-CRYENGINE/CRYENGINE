// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/Select/SelectTool.h"
#include "Core/SmoothingGroupManager.h"

namespace Designer
{
class SmoothingGroupTool : public SelectTool
{
public:
	SmoothingGroupTool(EDesignerTool tool) : SelectTool(tool)
	{
		m_nPickFlag = ePF_Polygon;
		m_fThresholdAngle = 45.0f;
	}

	void                                              Enter() override;

	bool                                              OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags) override;
	void                                              OnEditorNotifyEvent(EEditorNotifyEvent event) override;

	void                                              AddSmoothingGroup();
	void                                              AddPolygonsToSmoothingGroup(const char* name);
	void                                              SelectPolygons(const char* name);
	void                                              DeleteSmoothingGroup(const char* name);
	bool                                              RenameSmoothingGroup(const char* former_name, const char* name);
	void                                              ApplyAutoSmooth();

	void                                              SetThresholdAngle(float thresholdAngle) { m_fThresholdAngle = thresholdAngle; }
	float                                             GetThresholdAngle() const               { return m_fThresholdAngle; }

	std::vector<std::pair<string, SmoothingGroupPtr>> GetSmoothingGroupList() const;

private:
	void ClearSelectedElements();
	float m_fThresholdAngle;
};
}

