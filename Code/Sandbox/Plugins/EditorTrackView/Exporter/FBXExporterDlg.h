// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Controls/EditorDialog.h"

class QMenuComboBox;

class QCheckBox;

class QFBXExporterDlg : public CEditorDialog
{
public:
	QFBXExporterDlg(bool bDisplayFPSSettingOnly = false, bool bExportMasterCamOnly = false, bool bExportLocalToSelected = false);

	bool GetResult(uint& frameRate, bool& bExportMasterCamOnly, bool& bExportLocalToSelected) const;

protected:
	void OnConfirmed();

	QMenuComboBox* m_pFPS;
	QCheckBox*     m_pExportOnlyMasterCamera;
	QCheckBox*     m_pExportAsLocalToSelected;

	bool           m_bConfirmed;
};

