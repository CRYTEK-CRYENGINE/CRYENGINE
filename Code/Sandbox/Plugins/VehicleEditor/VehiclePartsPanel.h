// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VehiclePartsPanel_h__
#define __VehiclePartsPanel_h__
#pragma once

#include "PropertyCtrlExt.h"

#include "Controls\TreeCtrlEx.h"
#include "Controls\SplitterCtrl.h"

#include "VehicleDialogComponent.h"

class CVehicleEditorDialog;
class CVehicleHelper;
class CVehiclePart;
class CVehiclePartsPanel;
class CVehicleWeapon;
class CVehicleSeat;

//! Tools panel
class CPartsToolsPanel : public CDialog
{
public:
	virtual BOOL Create(UINT nIDTemplate, CWnd* pParentWnd /* = NULL */);

	enum { IDD = IDD_PANEL_VEED_PARTS_TOOLS };

public:
	CVehiclePartsPanel* pPartsPanel;

	BOOL                m_bSeats;
	BOOL                m_bAssetHelpers;
	BOOL                m_bVeedHelpers;
	BOOL                m_bWheels;
	BOOL                m_bComps;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnDisplaySeats();
	afx_msg void OnDisplayWheels();
	afx_msg void OnDisplayVeedHelpers();
	afx_msg void OnDisplayAssetHelpers();
	afx_msg void OnDisplayComps();

};

//! CWheelMasterDialog
class CWheelMasterDialog : public CDialog
{
	DECLARE_DYNAMIC(CWheelMasterDialog)

public:
	CWheelMasterDialog(CWnd* pParent = NULL);     // standard constructor
	virtual ~CWheelMasterDialog() {}

	virtual BOOL OnInitDialog();

	void         SetVariable(IVariable* pVar);
	IVariable*   GetVariable()                                  { return m_pVar; }

	void         SetWheels(std::vector<CVehiclePart*>* pWheels) { m_pWheels = pWheels; }

	// Dialog Data
	enum { IDD = IDD_WHEELMASTER };

#define NUM_BOXES 9
	BOOL m_boxes[NUM_BOXES];

protected:
	virtual void DoDataExchange(CDataExchange* pDX);      // DDX/DDV support
	DECLARE_MESSAGE_MAP()
	afx_msg void OnCheckBoxClicked();
	afx_msg void OnOKClicked();
	afx_msg void OnApplyWheels();
	afx_msg void OnToggleAll();

	void         ApplyCheckBoxes();

	CPropertyCtrl               m_propsCtrl;
	CListBox                    m_wheelList;
	IVariable*                  m_pVar;

	std::vector<CVehiclePart*>* m_pWheels;

	BOOL                        m_toggleAll;

};

/*!
 * CVehiclePartsPanel
 */
class CVehiclePartsPanel : public CWnd, public IVehicleDialogComponent
{
	DECLARE_DYNAMIC(CVehiclePartsPanel)
public:
	CVehiclePartsPanel(CVehicleEditorDialog* pDialog);
	virtual ~CVehiclePartsPanel();

	// VehicleEditorComponent
	void          UpdateVehiclePrototype(CVehiclePrototype* pProt);
	void          OnPaneClose();
	void          NotifyObjectsDeletion(CVehiclePrototype* pProt);

	void          UpdateVariables();
	void          ReloadTreeCtrl();
	void          OnApplyChanges(IVariable* pVar);
	void          OnObjectEvent(CBaseObject* object, int event);
	void          DeleteTreeObjects(const CRuntimeClass* pClass);
	CVehiclePart* GetPartForHelper(CVehicleHelper* pHelper);

	void          ReloadPropsCtrl();
	void          OnSetPartClass(IVariable* pVar);

protected:

	virtual void    DoDataExchange(CDataExchange* pDX);
	DECLARE_MESSAGE_MAP()
	afx_msg int     OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void    OnSize(UINT nType, int cx, int cy);
	afx_msg void    OnItemRClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void    OnItemDblClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void    OnTreeKeyDown(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void    OnDestroy();
	afx_msg void    OnClose();

	afx_msg void    OnHelperNew();
	afx_msg void    OnHelperRename();
	afx_msg void    OnHelperRenameDone(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void    OnSelect(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void    OnPartNew();
	afx_msg void    OnPartSelect();
	afx_msg void    OnSeatNew();
	afx_msg void    OnDeleteItem();
	afx_msg void    OnPrimaryWeaponNew();
	afx_msg void    OnSecondaryWeaponNew();
	afx_msg void    OnDisplaySeats();
	afx_msg void    OnEditWheelMaster();
	afx_msg void    OnComponentNew();
	afx_msg void    OnScaleHelpers();

	void            DeleteObject(CBaseObject* pObj);

	void            ExpandProps(CPropertyItem* pItem, bool expand = true);
	int             FillParts(IVariablePtr pData);
	void            AddParts(IVariable* pParts, CBaseObject* pParent);
	void            FillHelpers(IVariablePtr pData);
	void            FillAssetHelpers();
	void            RemoveAssetHelpers();
	void            FillSeats();
	void            FillComps(IVariablePtr pData);

	void            ShowAssetHelpers(bool bShow);
	void            ShowAssetHelpers();
	void            HideAssetHelpers();

	void            ShowVeedHelpers(bool bShow);
	void            ShowSeats(bool bShow);
	void            ShowWheels(bool bShow);
	void            ShowComps(bool bShow);

	CVehicleHelper* CreateHelperObject(const Vec3& pos, const Vec3& dir, const CString& name, bool unique, bool select, bool editLabel, bool isFromAsset, IVariable* pHelperVar = 0);
	void            CreateHelpersFromStatObj(IStatObj* pObj, CVehiclePart* pParent);
	CVehicleWeapon* CreateWeapon(int weaponType, CVehicleSeat* pSeat, IVariable* pVar = 0);

	void            DeleteSeats();
	void            DeleteParts();
	CVehiclePart*   FindPart(CString name);
	void            DumpPartTreeMap();

	void            ShowObject(TPartToTreeMap::iterator it, bool bShow);

	HTREEITEM       InsertTreeItem(CBaseObject* pObj, CBaseObject* pParent, bool select = false);
	HTREEITEM       InsertTreeItem(CBaseObject* pObj, HTREEITEM hParent, bool select = false);
	void            DeleteTreeItem(TPartToTreeMap::iterator it);

	CVehiclePart*   GetMainPart();
	IVariable*      GetVariable();
	void            GetWheels(std::vector<CVehiclePart*>& wheels);

	CVehiclePrototype* m_pVehicle;

	// pointer to main dialog
	CVehicleEditorDialog* m_pDialog;

	TPartToTreeMap&       m_partToTree;

	// controls
	CPropertyCtrlExt m_propsCtrl;
	CTreeCtrlEx      m_treeCtrl;
	HTREEITEM        m_hVehicle;
	CImageList       m_imageList;
	CSplitterCtrl    m_vSplitter;
	CSplitterCtrl    m_hSplitter;

	CBaseObject*     m_pSelItem;

	CVehiclePart*    m_pMainPart;

	IObjectManager*  m_pObjMan;

	CPartsToolsPanel m_toolsPanel;
	friend class CPartsToolsPanel;

	IVariablePtr m_pRootVar;
	CVarBlock    m_classBlock;

	BOOL         m_bSeats;
};

#endif // __VehiclePartsPanel_h__

