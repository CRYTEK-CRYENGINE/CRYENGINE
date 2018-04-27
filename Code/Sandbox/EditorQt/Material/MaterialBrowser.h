// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IDataBaseManager.h"
#include "Dialogs/ToolbarDialog.h"
#include "Controls/TreeCtrlReport.h"
#include "Controls/EditorDialog.h"
#include "Material.h"

class CMaterialBrowserCtrl;
class CMaterialImageListCtrl;

//////////////////////////////////////////////////////////////////////////
// Item class for browser.
//////////////////////////////////////////////////////////////////////////
class CMaterialBrowserRecord : public CTreeItemRecord
{
	DECLARE_DYNAMIC(CMaterialBrowserRecord)
public:
	CMaterialBrowserRecord() { pMaterial = 0; bFolder = false; nSubMtlSlot = -1; bUpdated = false; }

	void                    CreateItems();
	CMaterialBrowserRecord* GetParent() { return (CMaterialBrowserRecord*)GetParentRecord(); }
	void                    UpdateChildItems();
	CString                 CollectChildItems();

public:
	int                   nSubMtlSlot;
	CString               materialName;
	uint32                materialNameCrc32;
	_smart_ptr<CMaterial> pMaterial;
	bool                  bFolder;
	bool                  bUpdated;
};
typedef std::vector<CMaterialBrowserRecord*> TMaterialBrowserRecords;

//////////////////////////////////////////////////////////////////////////
class CMaterialBrowserTreeCtrl : public CTreeCtrlReport
{
public:
	CMaterialBrowserTreeCtrl();
	void                           SetMtlBrowser(CMaterialBrowserCtrl* pMtlBrowser) { m_pMtlBrowser = pMtlBrowser; }

	static CMaterialBrowserRecord* RowToRecord(const CXTPReportRow* pRow)
	{
		if (!pRow)
			return 0;
		CXTPReportRecord* pRecord = pRow->GetRecord();
		if (!pRecord)
			return 0;
		if (!pRecord->IsKindOf(RUNTIME_CLASS(CMaterialBrowserRecord)))
			return 0;
		return (CMaterialBrowserRecord*)pRecord;
	}

	//////////////////////////////////////////////////////////////////////////
	// Callbacks.
	//////////////////////////////////////////////////////////////////////////
	virtual void             OnFillItems();
	virtual void             OnSelectionChanged();
	virtual void             OnItemExpanded(CXTPReportRow* pRow, bool bExpanded);
	virtual CTreeItemRecord* CreateGroupRecord(const char* name, int nGroupIcon);
	virtual bool             OnFilterTest(CTreeItemRecord* pRecord);
	bool                     OnFilterTestSubmaterials(CTreeItemRecord* pRecord, bool bAddOperation);
	bool                     OnFilterTestTextures(CTreeItemRecord* pRecord, bool bAddOperation);

	//////////////////////////////////////////////////////////////////////////

	// Override from parent class
	virtual void            DeleteAllItems();

	void                    SelectItem(CMaterialBrowserRecord* pRecord, const TMaterialBrowserRecords* pMarkedRecords);
	void                    UpdateItemState(CMaterialBrowserRecord* pItem);

	static int _cdecl       RowCompareFunc(const CXTPReportRow** pRow1, const CXTPReportRow** pRow2);

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnNMRclick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTreeKeyDown(NMHDR* pNMHDR, LRESULT* pResult);

protected:
	CMaterialBrowserCtrl* m_pMtlBrowser;
};

//////////////////////////////////////////////////////////////////////////
struct IMaterialBrowserListener
{
	virtual void OnBrowserSelectItem(IDataBaseItem* pItem) = 0;
};

//////////////////////////////////////////////////////////////////////////
// CMaterialPickerDialog
//////////////////////////////////////////////////////////////////////////

class QObjectTreeWidget;
class QPreviewWidget;
class QHBoxLayout;


class QTexturePreview : public QLabel
{
public:
	QTexturePreview(const QString& path, QWidget* pParent = nullptr);
	QTexturePreview(const QString& path, QSize size, QWidget* pParent = nullptr);

	bool IsPower2(int n);
	void LoadTexture(const QString& fileName, CImageEx& image);

	void resizeEvent(QResizeEvent *event) override;

	QSize sizeHint() const override;

private:
	QSize m_maxSize;
	QPixmap m_originalPixmap;
};

class QMaterialPreview : public QWidget
{
	Q_OBJECT
public:
	QMaterialPreview(const QString& file = QString(), QWidget* pParent = nullptr);
	~QMaterialPreview();


	void PreviewItem(const QString& path);

signals:
	void widgetShown();

protected:
	virtual void paintEvent(QPaintEvent* pEvent) override;
	virtual void showEvent(QShowEvent* pEvent) override;

	void validateSize();

private:
	QPreviewWidget* m_pPreviewWidget;
	QHBoxLayout*    m_pPreviewImagesLayout;
};

//////////////////////////////////////////////////////////////////////////
// CMaterialToolBar
//////////////////////////////////////////////////////////////////////////
class CMaterialToolBar : public CXTPToolBar
{
public:
	CMaterialToolBar();

	enum EFilter
	{
		eFilter_Materials = 0,
		eFilter_Submaterials,
		eFilter_Textures,
		eFilter_Materials_And_Textures
	};

	void           Create(CWnd* pParentWnd);
	void           Reset();
	void           OnFilter();

	const CString& GetFilterText(bool* pOutIsNew = nullptr);
	EFilter        GetCurFilter() const { return m_curFilter; }

protected:
	BOOL OnCommand(WPARAM wParam, LPARAM lParam);

private:
	CXTPControlEdit* m_pEdit;

	CString          m_filterText;
	float            m_fIdleFilterTextTime;
	EFilter          m_curFilter;
};

//////////////////////////////////////////////////////////////////////////
// CMaterialBrowserCtrl
//////////////////////////////////////////////////////////////////////////
class CMaterialBrowserCtrl : public CToolbarDialog, public IDataBaseManagerListener, public IEditorNotifyListener
{
public:
	enum EViewType
	{
		VIEW_LEVEL = 0,
		VIEW_ALL   = 1,
	};

	CMaterialBrowserCtrl();
	~CMaterialBrowserCtrl();

	void                            SetListener(IMaterialBrowserListener* pListener) { m_pListener = pListener; }
	BOOL                            Create(const RECT& rect, CWnd* pParentWnd, UINT nID);
	virtual CMaterialBrowserRecord* InsertItemToTree(IDataBaseItem* pItem, int nSubMtlSlot = -1, CMaterialBrowserRecord* pParentRecord = 0);

	void                            ReloadItems(EViewType viewType);
	EViewType                       GetViewType() const     { return m_viewType; };
	CMaterialToolBar::EFilter       GetSearchFilter() const { return m_wndToolBar.GetCurFilter(); }

	void                            ClearItems();

	void                            AddItemToTree(IDataBaseItem* pItem, bool bUpdate);
	void                            SelectItem(IDataBaseItem* pItem, IDataBaseItem* pParentItem);
	bool                            SelectItemByName(const CString materialName, const CString parentMaterialName);
	void                            DeleteItem();

	void                            PopulateItems();

	bool                            ShowCheckedOutRecursive(CXTPReportRecords* pRecords);

	virtual BOOL                    PreTranslateMessage(MSG* pMsg);

	afx_msg void                    OnCopy();
	afx_msg void                    OnCopyName();
	afx_msg void                    OnPaste();
	afx_msg void                    OnCut();
	afx_msg void                    OnDuplicate();
	afx_msg void                    OnAddNewMaterial();
	afx_msg void                    OnAddNewMultiMaterial();
	afx_msg void                    OnConvertToMulti();
	afx_msg void                    OnMergeMaterials();
	afx_msg void                    OnReloadItems();
	afx_msg void                    OnFilter();
	afx_msg void                    OnShowCheckedOut();
	afx_msg void                    OnUpdateShowCheckedOut(CCmdUI* pCmdUI);
	bool                            CanPaste();
	void                            IdleSaveMaterial();

	void                            SetImageListCtrl(CMaterialImageListCtrl* pCtrl);

	//////////////////////////////////////////////////////////////////////////
	// IDataBaseManagerListener implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnDataBaseItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event);
	//////////////////////////////////////////////////////////////////////////

	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);

protected:
	// Item definition.
	enum ESourceControlOp
	{
		ESCM_GETPATH,
		ESCM_IMPORT,
		ESCM_CHECKIN,
		ESCM_CHECKOUT,
		ESCM_UNDO_CHECKOUT,
		ESCM_GETLATEST,
		ESCM_GETLATESTTEXTURES,
		ESCM_HISTORY,
	};

	void ReloadLevelItems();
	void ReloadAllItems();

	void ReloadTreeSubMtls(CMaterialBrowserRecord* pRecord, bool bForceCtrlUpdate = true);
	void RemoveItemFromTree(CMaterialBrowserRecord* pRecord);
	void DeleteItem(CMaterialBrowserRecord* pRecord);
	void SetSelectedItem(CMaterialBrowserRecord* pRecord, const TMaterialBrowserRecords* pMarkedRecords, bool bFromOnSelectOfCtrl);
	void OnAddSubMtl();
	void OnSelectAssignedObjects();
	void OnAssignMaterialToSelection();
	void OnRenameItem();
	void OnResetItem();
	void OnSetSubMtlCount(CMaterialBrowserRecord* pRecord);

	void DropToItem(CMaterialBrowserRecord* pTrgItem, CMaterialBrowserRecord* pSrcItem);

	void DoSourceControlOp(CMaterialBrowserRecord * pRecord, ESourceControlOp);

	void       OnMakeSubMtlSlot(CMaterialBrowserRecord* pRecord);
	void       OnClearSubMtlSlot(CMaterialBrowserRecord* pRecord);
	void       SetSubMaterial(CMaterialBrowserRecord* pRecord, int nSlot, CMaterial* pSubMaterial, bool bSelectSubMtl);

	void       OnImageListCtrlSelect(struct CImageListCtrlItem* pItem);
	void       OnSaveToFile(bool bMulti);

	void       RefreshSelected();

	void       TickRefreshMaterials();
	void       TryLoadRecordMaterial(CMaterialBrowserRecord* pRecord);
	void       LoadFromFiles(const CFileUtil::FileArray& files);

	void       ShowContextMenu(CMaterialBrowserRecord* pRecord, CPoint point);

	CMaterial* GetCurrentMaterial();

	uint32     MaterialNameToCrc32(const char* str);

	// Return record currently selected.
	CMaterialBrowserRecord* GetSelectedRecord();
	CString                 GetSelectedMaterialID();

	//////////////////////////////////////////////////////////////////////////
	// Messages.
	//////////////////////////////////////////////////////////////////////////
	DECLARE_MESSAGE_MAP()
	afx_msg void OnSize(UINT nType, int cx, int cy);
	//////////////////////////////////////////////////////////////////////////

private:
	friend class CMaterialBrowserTreeCtrl;

	CMaterialToolBar         m_wndToolBar;
	CImageList               m_imageList;

	CMaterialBrowserTreeCtrl m_tree;

	bool                     m_bIgnoreSelectionChange;
	bool                     m_bItemsValid;

	//CMaterialBrowserDlg m_browserDlg;
	CMaterialManager*         m_pMatMan;
	IMaterialBrowserListener* m_pListener;
	CMaterialImageListCtrl*   m_pMaterialImageListCtrl;

	EViewType                 m_viewType;
	bool                      m_bNeedReload;

	bool                      m_bHighlightMaterial;
	uint32                    m_timeOfHighlight;

	typedef std::set<CMaterialBrowserRecord*> Items;
	Items                   m_items;

	CMaterialBrowserRecord* m_pCurrentRecord;
	TMaterialBrowserRecords m_markedRecords;

	CMaterial*              m_pLastActiveMultiMaterial;

	float                   m_fIdleSaveMaterialTime;
	bool                    m_bShowOnlyCheckedOut;

	HCURSOR                 m_hCursorDefault;
	HCURSOR                 m_hCursorNoDrop;
	HCURSOR                 m_hCursorCreate;
	HCURSOR                 m_hCursorReplace;
};

