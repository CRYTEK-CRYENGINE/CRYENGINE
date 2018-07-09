// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Controls/PropertyCtrl.h"
#include "Controls/PropertyItem.h"
#include "Resource.h"

#include "EquipPack.h"

struct IVariable;

typedef std::list<SEquipment> TLstString;
typedef TLstString::iterator  TLstStringIt;

// CEquipPackDialog dialog
class CEquipPackDialog : public CXTResizeDialog
{
	DECLARE_DYNAMIC(CEquipPackDialog)

private:
	CPropertyCtrl m_AmmoPropWnd;
	CComboBox     m_EquipPacksList;
	CListBox      m_AvailEquipList;
	CTreeCtrl     m_EquipList;
	CButton       m_OkBtn;
	CButton       m_ExportBtn;
	CButton       m_AddBtn;
	CButton       m_DeleteBtn;
	CButton       m_RenameBtn;
	CButton       m_InsertBtn;
	CButton       m_RemoveBtn;
	CEdit         m_PrimaryEdit;

	CEquipPackLib* m_pEquipPacks;
	TLstString    m_lstAvailEquip;
	CString       m_sCurrEquipPack;
	CVarBlockPtr  m_pVarBlock;

private:
	void         UpdateEquipPacksList();
	void         UpdateEquipPackParams();
	void         UpdatePrimary();
	void         AmmoUpdateCallback(IVariable* pVar);
	SEquipment*  GetEquipment(CString sDesc);
	void         StoreUsedEquipmentList();
	HTREEITEM    MoveEquipmentListItem(HTREEITEM existingItem, HTREEITEM insertAfter);
	void         AddEquipmentListItem(const SEquipment& equip);
	void         CopyChildren(HTREEITEM copyToItem, HTREEITEM copyFromItem);
	void         SetChildCheckState(HTREEITEM checkItem, bool checkState, HTREEITEM ignoreItem = NULL);
	bool         HasCheckedChild(HTREEITEM parentItem);
	void         UpdateCheckState(HTREEITEM childItem);
	CVarBlockPtr CreateVarBlock(CEquipPack* pEquip);

public:
	void    SetCurrEquipPack(const CString& sValue) { m_sCurrEquipPack = sValue; }
	CString GetCurrEquipPack()                { return m_sCurrEquipPack; }

public:
	CEquipPackDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CEquipPackDialog();
	// Dialog Data
	enum { IDD = IDD_EQUIPPACKS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL    OnInitDialog();
	afx_msg void    OnCbnSelchangeEquippack();
	afx_msg void    OnBnClickedAdd();
	afx_msg void    OnBnClickedDelete();
	afx_msg void    OnBnClickedRename();
	afx_msg void    OnBnClickedInsert();
	afx_msg void    OnBnClickedRemove();
	afx_msg void    OnLbnSelchangeEquipavaillst();
	afx_msg void    OnBnClickedImport();
	afx_msg void    OnBnClickedExport();
	afx_msg void    OnBnClickedMoveUp();
	afx_msg void    OnBnClickedMoveDown();
	afx_msg void    OnBnClickedCancel();
	afx_msg void    OnTvnSelchangedEquipusedlst(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void    OnNMClickEquipusedlst(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void    OnTVNKeyDownEquipusedlst(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg LRESULT OnCheckStateChange(WPARAM wparam, LPARAM lparam);
	afx_msg void    OnBnClickedOk();
};

