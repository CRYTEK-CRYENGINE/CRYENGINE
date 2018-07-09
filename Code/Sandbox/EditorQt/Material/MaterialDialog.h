// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseLibraryDialog.h"
#include "Controls/PropertyCtrlEx.h"
#include "MaterialBrowser.h"
#include "Dialogs/BaseFrameWnd.h"

#define MATERIAL_EDITOR_NAME "Material Editor Legacy"
#define MATERIAL_EDITOR_VER  "1.00"

class CMaterial;
class CMaterialManager;
class CMatEditPreviewDlg;
class CMaterialSender;
class CMaterialImageListCtrl;

/** Dialog which hosts entity prototype library.
 */

struct SMaterialExcludedVars
{
	CMaterial* pMaterial;
	CVarBlock  vars;

	SMaterialExcludedVars() :
		pMaterial(nullptr)
	{
	}
};

class CMaterialDialog : public CBaseFrameWnd, public IMaterialBrowserListener, public IDataBaseManagerListener
{
	DECLARE_DYNCREATE(CMaterialDialog)
public:
	CMaterialDialog(CWnd* pParent, RECT rc);
	CMaterialDialog();
	~CMaterialDialog();

	virtual UINT GetDialogMenuID();

public:
	afx_msg void OnAssignMaterialToSelection();
	afx_msg void OnResetMaterialOnSelection();
	afx_msg void OnGetMaterialFromSelection();

protected:
	virtual void PostNcDestroy();
	BOOL         OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG* pMsg);

	afx_msg void OnDestroy();

	afx_msg void OnAddItem();
	afx_msg void OnSaveItem();
	afx_msg void OnUpdateMtlSelected(CCmdUI* pCmdUI);
	afx_msg void OnUpdateMtlSaved(CCmdUI* pCmdUI);
	afx_msg void OnUpdateObjectSelected(CCmdUI* pCmdUI);
	afx_msg void OnUpdateAssignMtlToSelection(CCmdUI* pCmdUI);
	afx_msg void OnPickMtl();
	afx_msg void OnUpdatePickMtl(CCmdUI* pCmdUI);
	afx_msg void OnCopy();
	afx_msg void OnPaste();
	afx_msg void OnMaterialPreview();
	afx_msg void OnSelectAssignedObjects();
	afx_msg void OnChangedBrowserListType();

	//////////////////////////////////////////////////////////////////////////
	// Some functions can be overriden to modify standart functionality.
	//////////////////////////////////////////////////////////////////////////
	virtual void InitToolbar(UINT nToolbarResID);

	virtual void SelectItem(CBaseLibraryItem* item, bool bForceReload = false);
	virtual void DeleteItem(CBaseLibraryItem* pItem);
	virtual bool SetItemName(CBaseLibraryItem* item, const CString& groupName, const CString& itemName);
	virtual void ReloadItems();

	//////////////////////////////////////////////////////////////////////////
	// IMaterialBrowserListener implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnBrowserSelectItem(IDataBaseItem* pItem);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IDataBaseManagerListener implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnDataBaseItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	CMaterial* GetSelectedMaterial();
	void       OnUpdateProperties(IVariable* var);
	void       OnUndo(IVariable* pVar);

	void       UpdateShaderParamsUI(CMaterial* pMtl);

	void       UpdatePreview();

	//void SetTextureVars( CVariableArray *texVar,CMaterial *mtl,int id,const CString &name );
	void SetMaterialVars(CMaterial* mtl);

	DECLARE_MESSAGE_MAP()

	CMaterialBrowserCtrl m_wndMtlBrowser;
	CStatusBar m_statusBar;
	//CXTCaption    m_wndCaption;
	// Library control.
	CComboBox         m_listTypeCtrl;

	CPropertyCtrlEx   m_propsCtrl;
	bool              m_bForceReloadPropsCtrl;

	CDialog           m_emptyDlg;

	CImageList*       m_dragImage;

	CBaseLibraryItem* m_pPrevSelectedItem;

	// Material manager.
	CMaterialManager* m_pMatManager;

	CVarBlockPtr      m_vars;
	CVarBlockPtr      m_publicVars;

	// collection of excluded vars from m_publicVars for remembering values
	// when updating shader params
	SMaterialExcludedVars                 m_excludedPublicVars;

	CPropertyItem*                        m_publicVarsItems;
	CVarBlockPtr                          m_shaderGenParamsVars;
	CPropertyItem*                        m_shaderGenParamsVarsItem;
	CVarBlockPtr                          m_textureSlots;
	CPropertyItem*                        m_textureSlotsItem;

	class CMaterialUI*                    m_pMaterialUI;

	HTREEITEM                             m_hDropItem;
	HTREEITEM                             m_hDraggedItem;

	CMatEditPreviewDlg*                   m_pPreviewDlg;

	std::auto_ptr<CMaterialImageListCtrl> m_pMaterialImageListCtrl;
};

