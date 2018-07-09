// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __Vehicle_Paints_Panel__h__
#define __Vehicle_Paints_Panel__h__

#include "IDataBaseManager.h"
#include "Controls\PropertyCtrl.h"

class CVehiclePaintsPanel
	: public CXTResizeDialog
	  , public IDataBaseManagerListener
{
	DECLARE_DYNCREATE(CVehiclePaintsPanel)

public:
	CVehiclePaintsPanel();
	virtual ~CVehiclePaintsPanel();

	enum { IDD = IDD_VEHICLE_PAINTS };

	void InitPaints(IVariable* pPaints);
	void Clear();

	// IDataBaseManagerListener
	virtual void OnDataBaseItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event);
	// ~IDataBaseManagerListener

protected:
	afx_msg void OnAddNewPaint();
	afx_msg void OnRemoveSelectedPaint();
	afx_msg void OnAssignMaterialToSelectedPaint();
	afx_msg void OnApplyPaintToVehicle();
	afx_msg void OnPaintNameSelectionChanged();
	DECLARE_MESSAGE_MAP()

	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual void OnOK()     {}
	virtual void OnCancel() {}

	void         CreateNewPaint(const string& paintName);

private:
	void       RebuildPaintNames();

	bool       IsPaintNameSelected() const;
	string     GetSelectedPaintName() const;
	int        GetSelectedPaintNameVarIndex() const;
	void       RenameSelectedPaintName(const string& name);
	void       AddPaintName(const string& paintName, int paintVarIndex);
	void       ClearPaintNames();

	void       HidePaintProperties();
	void       ShowPaintPropertiesByName(const string& paintName);
	void       ShowPaintPropertiesByVarIndex(int paintVarIndex);
	void       ShowPaintProperties(IVariable* pPaint);

	IVariable* GetPaintVarByName(const string& paintName);
	IVariable* GetPaintVarByIndex(int index);

	void       OnSelectedPaintNameChanged(IVariable* pVar);
	void       OnSelectedPaintMaterialChanged(IVariable* pVar);

	void       UpdateAssignMaterialButtonState();

private:
	CListBox      m_paintNames;
	CPropertyCtrl m_paintProperties;
	CButton       m_applyMaterialButton;
	CButton       m_applyToVehicleButton;

	IVariable*    m_pPaints;
	IVariable*    m_pSelectedPaint;
};

#endif

