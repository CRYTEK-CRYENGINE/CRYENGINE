// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "resource.h"

class CFBXExporterDialog : public CDialog
{
public:
	CFBXExporterDialog(bool bDisplayOnlyFPSSetting = false, CWnd* pParent = NULL);
	// Dialog Data
	enum {IDD = IDD_TV_EXPORT_FBX};
	float GetFPS() const                                   { return m_fps; }
	bool  GetExportCoordsLocalToTheSelectedObject()        { return m_bExportLocalCoords; };
	bool  GetExportOnlyMasterCamera()                      { return m_bExportOnlyMasterCamera; };
	void  SetExportLocalCoordsCheckBoxEnable(bool checked) { m_bExportLocalCoordsCheckBoxEnable = checked; };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	void         OnFPSChange();
	void         SetExportLocalToTheSelectedObjectCheckBox();
	void         SetExportOnlyMasterCameraCheckBox();

	virtual void OnOK();
	DECLARE_MESSAGE_MAP()

	CComboBox m_fpsCombo;
	float   m_fps;
	CButton m_exportLocalCoordsCheckbox;
	CButton m_exportOnlyMasterCameraCheckBox;
	bool    m_bExportLocalCoords;
	bool    m_bExportLocalCoordsCheckBoxEnable;
	bool    m_bExportOnlyMasterCamera;
	bool    m_bDisplayOnlyFPSSetting;
private:
};

