// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SmartObjectsEditorDialog_h__
#define __SmartObjectsEditorDialog_h__
#pragma once

#include <list>

//#include <uxtheme.h>

#include <CryAISystem/IAISystem.h>
#include "Controls\PropertyCtrl.h"
#include "Controls\ColorCtrl.h"
#include "Objects/BaseObject.h"

class CSmartObjectEntry;
class CSmartObjectHelperObject;
class CEntityObject;
class CSOParamBase;

class CDragReportCtrl : public CXTPReportControl
{
	DECLARE_DYNCREATE(CDragReportCtrl)

public:
	CDragReportCtrl();
	~CDragReportCtrl();

protected:
	CPoint         m_ptDrag;
	bool           m_bDragging;
	bool           m_bDragEx;
	HTREEITEM      m_hItemSource;
	HTREEITEM      m_hItemTarget;

	HCURSOR        m_hCursorNoDrop;
	HCURSOR        m_hCursorNormal;

	CXTPReportRow* m_pSourceRow;
	CXTPReportRow* m_pTargetRow;

	void DrawDropTarget();

	//{{AFX_VIRTUAL(CDragReportCtrl)
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CDragReportCtrl)
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnCaptureChanged(CWnd* pWnd);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint pos);

	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

namespace SmartObjectsEditor
{

template<class BASE_TYPE>
class CFlatFramed : public BASE_TYPE
{
protected:
	//{{AFX_MSG(CFlatFramed)
	afx_msg void OnNcPaint();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

BEGIN_TEMPLATE_MESSAGE_MAP(CFlatFramed, BASE_TYPE, BASE_TYPE)
//{{AFX_MSG_MAP(CFlatFramed)
ON_WM_NCPAINT()
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

template<class BASE_TYPE>
void CFlatFramed<BASE_TYPE >::OnNcPaint()
{
	__super::OnNcPaint();
	//	if ( !IsAppThemed() )
	//	{
	CWindowDC dc(this);
	CRect rc;
	dc.GetClipBox(rc);
	COLORREF color = GetSysColor(COLOR_3DSHADOW);
	dc.Draw3dRect(rc, color, color);
	//	}

}
}

typedef std::set<string>                SetStrings;
typedef std::list<SmartObjectCondition> SmartObjectConditions;

class CSOLibrary
	: public IEditorNotifyListener
{
private:
	friend class CSmartObjectsEditorDialog;

	static bool SaveToFile(const char* sFileName);
	static bool LoadFromFile(const char* sFileName);

protected:
	~CSOLibrary();

public:
	static void Reload();
	static void InvalidateSOEntities();

	static void CSOLibrary::String2Classes(const string& sClass, SetStrings& classes);

	struct CHelperData
	{
		CString className;
		QuatT   qt;
		CString name;
		CString description;
	};
	// VectorHelperData contains all smart object helpers sorted by name of their smart object class
	typedef std::vector<CHelperData> VectorHelperData;

	struct CEventData
	{
		CString name;
		CString description;
	};
	// VectorEventData contains all smart object events sorted by name
	typedef std::vector<CEventData> VectorEventData;

	struct CStateData
	{
		CString name;
		CString description;
		CString location;
	};
	// VectorStateData contains all smart object states sorted by name
	typedef std::vector<CStateData> VectorStateData;

	struct CClassTemplateData
	{
		string name;

		struct CTemplateHelper
		{
			CString name;
			QuatT   qt;
			float   radius;
			bool    project;
		};
		typedef std::vector<CTemplateHelper> TTemplateHelpers;

		string           model;
		TTemplateHelpers helpers;
	};
	// VectorClassTemplateData contains all class templates sorted by name
	typedef std::vector<CClassTemplateData> VectorClassTemplateData;

	struct CClassData
	{
		CString                   name;
		CString                   description;
		CString                   location;
		CString                   templateName;
		CClassTemplateData const* pClassTemplateData;
	};
	// VectorClassData contains all smart object classes sorted by name
	typedef std::vector<CClassData> VectorClassData;

protected:
	// Called by the editor to notify the listener about the specified event.
	void                       OnEditorNotifyEvent(EEditorNotifyEvent event);

	static CClassTemplateData* AddClassTemplate(const char* name);
	static void                LoadClassTemplates();

	// data
	static SmartObjectConditions   m_Conditions;
	static VectorHelperData        m_vHelpers;
	static VectorEventData         m_vEvents;
	static VectorStateData         m_vStates;
	static VectorClassTemplateData m_vClassTemplates;
	static VectorClassData         m_vClasses;

	static int                     m_iNumEditors;
	static CSOLibrary*             m_pInstance;

	static bool                    m_bSaveNeeded;
	static bool                    m_bLoadNeeded;

public:
	static bool                       StartEditing();

	static SmartObjectConditions&     GetConditions()     { if (m_bLoadNeeded) Load(); return m_Conditions; }
	static VectorHelperData&          GetHelpers()        { if (m_bLoadNeeded) Load(); return m_vHelpers; }
	static VectorEventData&           GetEvents()         { if (m_bLoadNeeded) Load(); return m_vEvents; }
	static VectorStateData&           GetStates()         { if (m_bLoadNeeded) Load(); return m_vStates; }
	static VectorClassTemplateData&   GetClassTemplates() { if (m_bLoadNeeded) Load(); return m_vClassTemplates; }
	static VectorClassData&           GetClasses()        { if (m_bLoadNeeded) Load(); return m_vClasses; }

	static bool                       Load();
	static bool                       Save();

	static VectorHelperData::iterator FindHelper(const CString& className, const CString& helperName);
	static VectorHelperData::iterator HelpersUpperBound(const CString& className);
	static VectorHelperData::iterator HelpersLowerBound(const CString& className);

	static void                       AddEvent(const char* name, const char* description);
	static VectorEventData::iterator  FindEvent(const char* name);

	static void                       AddState(const char* name, const char* description, const char* location);
	static void                       AddAllStates(const string& sStates);
	static VectorStateData::iterator  FindState(const char* name);

	static CClassTemplateData const*  FindClassTemplate(const char* name);

	static void                       AddClass(const char* name, const char* description, const char* location, const char* templateName);
	static VectorClassData::iterator  FindClass(const char* name);
};

//////////////////////////////////////////////////////////////////////////
//
// Main Dialog, the Smart Objects Editor.
//
//////////////////////////////////////////////////////////////////////////
class CSmartObjectsEditorDialog
	: public CXTPFrameWnd
	  , public IEditorNotifyListener
{
	DECLARE_DYNCREATE(CSmartObjectsEditorDialog)

	friend class CDragReportCtrl;

public:
	CSmartObjectsEditorDialog();
	~CSmartObjectsEditorDialog();

	BOOL           Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd);
	CXTPTaskPanel& GetTaskPanel() { return m_taskPanel; }

	void      OnObjectEvent(CObjectEvent& event);
	void      OnObjectEventLegacy(CBaseObject* pObject, int eventType) { OnObjectEvent(CObjectEvent(static_cast<EObjectListenerEvent>(eventType), pObject)); }
	void      OnSelectionChanged();
	void      RecalcLayout(BOOL bNotify = TRUE);

	CString   GetFolderPath(HTREEITEM item) const;
	CString   GetCurrentFolderPath() const;
	HTREEITEM ForceFolder(const CString& folder);
	HTREEITEM SetCurrentFolder(const CString& folder);
	void      ModifyRuleOrder(int from, int to);

	void      SetTemplateDefaults(SmartObjectCondition& condition, const CSOParamBase* param, CString* message = NULL) const;

protected:
	void DeleteHelper(const CString& className, const CString& helperName);

protected:
	DECLARE_MESSAGE_MAP()
	virtual void    OnOK()     {};
	virtual void    OnCancel() {};

	virtual BOOL    PreTranslateMessage(MSG* pMsg);
	virtual BOOL    OnInitDialog();
	virtual void    DoDataExchange(CDataExchange* pDX); // DDX/DDV support
	virtual void    PostNcDestroy();
	afx_msg BOOL    OnEraseBkgnd(CDC* pDC);
	afx_msg void    OnSetFocus(CWnd* pOldWnd);
	afx_msg void    OnDestroy();
	afx_msg void    OnClose();
	//afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg LRESULT OnDockingPaneNotify(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnTaskPanelNotify(WPARAM wParam, LPARAM lParam);

	virtual BOOL    OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);

	//	afx_msg void OnGroupBy( UINT id );

public:
	afx_msg void OnAddEntry();
	afx_msg void OnEditEntry();
	afx_msg void OnRemoveEntry();
	afx_msg void OnDuplicateEntry();

	void         ReloadEntries(bool bFromFile);

protected:
	afx_msg void EnableIfOneSelected(CCmdUI* target);
	afx_msg void EnableIfSelected(CCmdUI* target);

	afx_msg void OnHelpersEdit();
	afx_msg void OnHelpersNew();
	afx_msg void OnHelpersDelete();
	afx_msg void OnHelpersDone();

	afx_msg void OnReportColumnRClick(NMHDR* pNotifyStruct, LRESULT* /*result*/);
	afx_msg void OnReportSelChanged(NMHDR* pNotifyStruct, LRESULT* /*result*/);
	afx_msg void OnReportHyperlink(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportChecked(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportEdit(NMHDR* pNotifyStruct, LRESULT* result);

	afx_msg void OnTreeBeginEdit(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnTreeEndEdit(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnTreeSelChanged(NMHDR* pNotifyStruct, LRESULT* /*result*/);
	afx_msg void OnTreeSetFocus(NMHDR* pNotifyStruct, LRESULT* /*result*/);

	afx_msg void OnDescriptionEdit(NMHDR* pNotifyStruct, LRESULT* /*result*/);

	afx_msg void OnMouseMove(UINT, CPoint);
	afx_msg void OnLButtonUp(UINT, CPoint);

	void                    CreatePanes();
	CXTPDockingPaneManager* GetDockingPaneManager() { return &m_paneManager; }

	void                    SinkSelection();
	void                    UpdatePropertyTables();

	// Called by the editor to notify the listener about the specified event.
	void OnEditorNotifyEvent(EEditorNotifyEvent event);

	// Called by the property editor control
	void OnUpdateProperties(IVariable* pVar);
	bool ChangeTemplate(SmartObjectCondition* pRule, const CSOParamBase* pParam) const;

	void ParseClassesFromProperties(CBaseObject* pObject, SetStrings& classes);

public:
	// Manager.
	//	CSmartObjectsManager*	m_pManager;

	CXTPReportColumn* m_pColType;
	CXTPReportColumn* m_pColName;
	CXTPReportColumn* m_pColDescription;
	CXTPReportColumn* m_pColUserClass;
	CXTPReportColumn* m_pColUserState;
	CXTPReportColumn* m_pColObjectClass;
	CXTPReportColumn* m_pColObjectState;
	CXTPReportColumn* m_pColAction;
	CXTPReportColumn* m_pColOrder;

private:
	bool m_bSinkNeeded;

protected:
	CSmartObjectEntry* m_pEditedEntry;

	CString            m_sNewObjectClass;
	CString            m_sFilterClasses;
	CString            m_sFirstFilterClass;
	CRect              m_rcCloseBtn;
	bool               m_bFilterCanceled;

	//	CXTPDockingPane*		m_pPropertiesPane;
	SmartObjectsEditor::CFlatFramed<CPropertyCtrl> m_Properties;
	CVarBlockPtr m_vars;

	// dialog stuff
	SmartObjectsEditor::CFlatFramed<CDragReportCtrl> m_View;
	CXTPDockingPaneManager                           m_paneManager;
	CXTPTaskPanel           m_taskPanel;

	CXTPTaskPanelGroupItem* m_pItemHelpersEdit;
	CXTPTaskPanelGroupItem* m_pItemHelpersNew;
	CXTPTaskPanelGroupItem* m_pItemHelpersDelete;
	CXTPTaskPanelGroupItem* m_pItemHelpersDone;

	// Smart Object Helpers
	bool             m_bIgnoreNotifications;
	CString          m_sEditedClass;
	typedef std::multimap<CEntityObject*, CSmartObjectHelperObject*> MapHelperObjects;
	MapHelperObjects m_mapHelperObjects;

public:
	// tree view
	SmartObjectsEditor::CFlatFramed<CTreeCtrl> m_Tree;
private:
	CImageList m_imageList;

	// Description
	SmartObjectsEditor::CFlatFramed<CColoredEdit> m_Description;

	SmartObjectConditions::iterator FindRuleByPtr(const SmartObjectCondition* pCondition);
	bool                            CheckOutLibrary();
	bool                            SaveSOLibrary();
	void                            MoveAllRules(const CString& from, const CString& to);
};

#endif // __VehicleEditorDialog_h__

