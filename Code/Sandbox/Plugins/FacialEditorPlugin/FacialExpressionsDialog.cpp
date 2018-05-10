// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "FacialExpressionsDialog.h"
#include <CryAnimation/ICryAnimation.h>
#include "Dialogs/QStringDialog.h"
#include "Util/Clipboard.h"
#include "FacialExpressionUtils.h"
#include "Dialogs/QNumericBoxDialog.h"
#include "Controls/QuestionDialog.h"
#include "Controls/SharedFonts.h"
#include <QDialogButtonBox>
#include "Util/MFCUtil.h"
#include "Resource.h"
#include "IUndoObject.h"

IMPLEMENT_DYNAMIC(CExpressionsTreeCtrl, CTreeCtrl)
IMPLEMENT_DYNAMIC(CFacialExpressionsDialog, CDialog)

#define IDC_TREE_CONTROL        AFX_IDW_PANE_FIRST
#define IDC_TABCTRL             AFX_IDW_PANE_FIRST + 1

#define TAB_CURVES              0
#define TAB_PREVIEW             1

#define ITEM_COLOR_DISABLED     RGB(160, 160, 160)
#define ITEM_COLOR_MORPH_TARGET RGB(0, 0, 160)

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CExpressionsTreeCtrl, CXTTreeCtrl)
ON_NOTIFY_REFLECT(TVN_SELCHANGED, OnSelChanged)
ON_NOTIFY_REFLECT(TVN_ITEMEXPANDED, OnItemExpanded)
END_MESSAGE_MAP()
//////////////////////////////////////////////////////////////////////////

class CExpressionsDialogDropTarget : public COleDropTarget
{
public:
	CExpressionsDialogDropTarget(CFacialExpressionsDialog* dlg) : m_dlg(dlg) {}
	virtual DROPEFFECT OnDragEnter(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
	{
		point = ConvertToTreeClientCoords(pWnd, point);
		if (m_dlg->m_pContext->GetExpressionNameFromDataSource(pDataObject).size() != 0)
			return DROPEFFECT_MOVE;
		return DROPEFFECT_NONE;
	}

	virtual void OnDragLeave(CWnd* pWnd)
	{
	}

	virtual DROPEFFECT OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
	{
		point = ConvertToTreeClientCoords(pWnd, point);
		if (m_dlg->m_pContext->GetExpressionNameFromDataSource(pDataObject).size() != 0)
			return DROPEFFECT_MOVE;
		return DROPEFFECT_NONE;
	}

	virtual BOOL OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point)
	{
		point = ConvertToTreeClientCoords(pWnd, point);
		HTREEITEM hItem = m_dlg->m_treeCtrl.HitTest(point);
		bool isToGarbageFolder = m_dlg->m_treeCtrl.GetItemText(hItem) == "_garbage";
		IFacialEffCtrl* pEffectorCtrl = (IFacialEffCtrl*)m_dlg->m_treeCtrl.GetItemData(hItem);
		IFacialEffector* pEffector = (pEffectorCtrl ? pEffectorCtrl->GetEffector() : 0);
		if (m_dlg->m_pContext && pEffector)
		{
			m_dlg->m_pContext->HandleExpressionNameDropToLibrary(pEffector, pDataObject, isToGarbageFolder);
			return TRUE;
		}
		return FALSE;
	}

private:
	CPoint ConvertToTreeClientCoords(CWnd* pWnd, const CPoint& point)
	{
		CPoint point2 = point;
		pWnd->ClientToScreen(&point2);
		m_dlg->m_treeCtrl.ScreenToClient(&point2);
		return point2;
	}

	CFacialExpressionsDialog* m_dlg;
};

CExpressionsTreeCtrl::CExpressionsTreeCtrl()
{
	m_pContext = 0;
	m_bIgnoreEvent = false;
}

CExpressionsTreeCtrl::~CExpressionsTreeCtrl()
{}

//////////////////////////////////////////////////////////////////////////
void CExpressionsTreeCtrl::SetContext(CFacialEdContext* pContext)
{
	m_pContext = pContext;
	if (m_pContext)
		m_pContext->RegisterListener(this);

	HandleOrphans();
	Reload();
};

//////////////////////////////////////////////////////////////////////////
_smart_ptr<IFacialEffectorsLibrary> CExpressionsTreeCtrl::CreateLibraryOfSelectedEffectors()
{
	_smart_ptr<IFacialEffectorsLibrary> pNewLibrary = 0;
	if (m_pContext)
	{
		typedef std::map<IFacialEffector*, IFacialEffector*> EffectorCopyMap;

		class Recurser
		{
		public:
			Recurser(CExpressionsTreeCtrl* pCtrl, EffectorCopyMap& copiedEffectors, IFacialEffectorsLibrary* pCloneLibrary) :
				pCtrl(pCtrl),
				effectors(copiedEffectors),
				pCloneLibrary(pCloneLibrary)
			{
			}

			void operator()(HTREEITEM item)
			{
				RecurseToExpression(item);
			}

		private:

			IFacialEffector* RecurseToExpression(HTREEITEM item, bool forceOutputAll = false)
			{
				IFacialEffCtrl* sourceCtrl = (IFacialEffCtrl*)pCtrl->GetItemData(item);
				IFacialEffector* sourceEffector = 0;
				if (sourceCtrl)
					sourceEffector = sourceCtrl->GetEffector();
				if (!sourceEffector)
				{
					sourceEffector = pCloneLibrary->GetRoot();

					if (pCtrl->IsSelected(item))
						forceOutputAll = true;

					HTREEITEM child = pCtrl->GetChildItem(item);
					while (child)
					{
						// For folders, we only copy entries if they are selected or have a child that is selected.
						if (HasSelectedChildExpression(child))
						{
							IFacialEffector* childEffector = RecurseToExpression(child, forceOutputAll);
							sourceEffector->AddSubEffector(childEffector);
						}

						child = pCtrl->GetNextItem(child, TVGN_NEXT);
					}
					return 0;
				}
				else
				{
					if (sourceEffector->GetType() == EFE_TYPE_GROUP)
					{
						EffectorCopyMap::iterator itCopy = effectors.find(sourceEffector);
						if (itCopy == effectors.end())
						{
							IFacialEffector* pEffector = pCloneLibrary->CreateEffector(sourceEffector->GetType(), sourceEffector->GetName());
							itCopy = effectors.insert(std::make_pair(sourceEffector, pEffector)).first;
						}

						IFacialEffector* pEffector = (*itCopy).second;

						for (int i = 0; i < sourceEffector->GetSubEffectorCount(); ++i)
						{
							HTREEITEM child = FindItemForController(item, sourceEffector->GetSubEffCtrl(i));
							assert(child);
							if (!child)
								break;
							;

							// Check whether we have already added this expression to this folder.
							EffectorCopyMap::iterator itChildCopy = effectors.find(sourceEffector->GetSubEffector(i));
							if (itChildCopy != effectors.end())
							{
								IFacialEffector* pChildClone = (*itCopy).second;
								bool foundClone = false;
								for (int subEffectorIndex = 0; subEffectorIndex < pEffector->GetSubEffectorCount(); ++subEffectorIndex)
								{
									if (pEffector->GetSubEffector(subEffectorIndex) == pChildClone)
										foundClone = true;
								}
								if (foundClone)
									continue;
							}

							if (pCtrl->IsSelected(item))
								forceOutputAll = true;

							// For folders, we only copy entries if they are selected or have a child that is selected.
							if (!forceOutputAll && !HasSelectedChildExpression(child))
								continue;

							IFacialEffector* childEffector = RecurseToExpression(child, forceOutputAll);
							int subEffectorIndex = pEffector->GetSubEffectorCount();
							pEffector->AddSubEffector(childEffector);

							// Probably not necessary - entries in folders shouldn't have spline information.
							IFacialEffCtrl* pCloneSubCtrl = pEffector->GetSubEffCtrl(subEffectorIndex);
							IFacialEffCtrl* pOriginalSubCtrl = sourceEffector->GetSubEffCtrl(i);
							CopyController(pCloneSubCtrl, pOriginalSubCtrl);
						}

						return (*itCopy).second;
					}
					else
					{
						EffectorCopyMap::iterator itCopy = effectors.find(sourceEffector);
						if (itCopy == effectors.end())
						{
							IFacialEffector* pEffector = pCloneLibrary->CreateEffector(sourceEffector->GetType(), sourceEffector->GetName());
							itCopy = effectors.insert(std::make_pair(sourceEffector, pEffector)).first;

							CopyExpressionParameters(pEffector, sourceEffector);

							for (int i = 0; i < sourceEffector->GetSubEffectorCount(); ++i)
							{
								HTREEITEM child = FindItemForController(item, sourceEffector->GetSubEffCtrl(i));
								assert(child);
								if (!child)
									break;
								;

								IFacialEffector* childEffector = RecurseToExpression(child);
								int subEffectorIndex = pEffector->GetSubEffectorCount();
								pEffector->AddSubEffector(childEffector);
								IFacialEffCtrl* pCloneSubCtrl = pEffector->GetSubEffCtrl(subEffectorIndex);
								IFacialEffCtrl* pOriginalSubCtrl = sourceEffector->GetSubEffCtrl(i);
								CopyController(pCloneSubCtrl, pOriginalSubCtrl);
							}
						}

						return (*itCopy).second;
					}
				}
			}

			HTREEITEM FindItemForController(HTREEITEM parent, IFacialEffCtrl* ctrl)
			{
				for (HTREEITEM child = pCtrl->GetChildItem(parent); child; (child = pCtrl->GetNextItem(child, TVGN_NEXT)))
				{
					if ((IFacialEffCtrl*)pCtrl->GetItemData(child) == ctrl)
						return child;
				}

				return 0;
			}

			void CopyController(IFacialEffCtrl* pCloneSubCtrl, IFacialEffCtrl* pOriginalSubCtrl)
			{
				pCloneSubCtrl->SetFlags(pOriginalSubCtrl->GetFlags());
				pCloneSubCtrl->SetConstantBalance(pOriginalSubCtrl->GetConstantBalance());
				pCloneSubCtrl->SetConstantWeight(pOriginalSubCtrl->GetConstantWeight());

				// Copy the curve information.
				pCloneSubCtrl->SetType(pOriginalSubCtrl->GetType());
				ISplineInterpolator* pOriginalSpline = pOriginalSubCtrl->GetSpline();
				ISplineInterpolator* pCloneSpline = pCloneSubCtrl->GetSpline();
				while (pCloneSpline && pCloneSpline->GetKeyCount() > 0)
					pCloneSpline->RemoveKey(0);
				for (int key = 0; key < pOriginalSpline->GetKeyCount(); ++key)
				{
					ISplineInterpolator::ValueType value;
					pOriginalSpline->GetKeyValue(key, value);
					int cloneKeyIndex = pCloneSpline->InsertKey(FacialEditorSnapTimeToFrame(pOriginalSpline->GetKeyTime(key)), value);
					pCloneSpline->SetKeyFlags(cloneKeyIndex, pOriginalSpline->GetKeyFlags(key));
					ISplineInterpolator::ValueType tin, tout;
					pOriginalSpline->GetKeyTangents(key, tin, tout);
					pCloneSpline->SetKeyTangents(cloneKeyIndex, tin, tout);
				}
			}

			void CopyExpressionParameters(IFacialEffector* pCloneEffector, IFacialEffector* pOriginalEffector)
			{
				switch (pCloneEffector->GetType())
				{
				case EFE_TYPE_GROUP:
					break;
				case EFE_TYPE_EXPRESSION:
					break;
				case EFE_TYPE_MORPH_TARGET:
					//((CFacialMorphTarget*)pCloneEffector)->SetMorphTargetId(((CFacialMorphTarget*)pOriginalEffector)->GetMorphTargetId());
					break;
				case EFE_TYPE_BONE:
					pCloneEffector->SetParamVec3(EFE_PARAM_BONE_ROT_AXIS, pOriginalEffector->GetParamVec3(EFE_PARAM_BONE_ROT_AXIS));
					pCloneEffector->SetParamVec3(EFE_PARAM_BONE_POS_AXIS, pOriginalEffector->GetParamVec3(EFE_PARAM_BONE_POS_AXIS));
					pCloneEffector->SetParamString(EFE_PARAM_BONE_NAME, pOriginalEffector->GetParamString(EFE_PARAM_BONE_NAME));
					break;
				case EFE_TYPE_MATERIAL:
					break;
				case EFE_TYPE_ATTACHMENT:
					pCloneEffector->SetParamVec3(EFE_PARAM_BONE_ROT_AXIS, pOriginalEffector->GetParamVec3(EFE_PARAM_BONE_ROT_AXIS));
					pCloneEffector->SetParamVec3(EFE_PARAM_BONE_POS_AXIS, pOriginalEffector->GetParamVec3(EFE_PARAM_BONE_POS_AXIS));
					pCloneEffector->SetParamString(EFE_PARAM_BONE_NAME, pOriginalEffector->GetParamString(EFE_PARAM_BONE_NAME));
					break;
				default:
					assert(0);
					break;
				}
			}

			bool HasSelectedChildExpression(HTREEITEM item)
			{
				if (pCtrl->IsSelected(item))
					return true;
				HTREEITEM child = pCtrl->GetChildItem(item);
				while (child != NULL)
				{
					if (HasSelectedChildExpression(child))
						return true;
					child = pCtrl->GetNextItem(child, TVGN_NEXT);
				}
				return false;
			}

			CExpressionsTreeCtrl*    pCtrl;
			EffectorCopyMap&         effectors;
			IFacialEffectorsLibrary* pCloneLibrary;
		};

		EffectorCopyMap copiedEffectors;
		pNewLibrary = GetISystem()->GetIAnimationSystem()->GetIFacialAnimation()->CreateEffectorsLibrary();
		Recurser(this, copiedEffectors, pNewLibrary)(GetRootItem());
	}

	return pNewLibrary;
}

//////////////////////////////////////////////////////////////////////////
void CExpressionsTreeCtrl::Reload()
{
	if (!m_imageList.GetSafeHandle())
	{
		CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_FACED_LIBTREE, 16, RGB(255, 0, 255));
		SetImageList(&m_imageList, TVSIL_NORMAL);
	}

	SetRedraw(FALSE);
	DeleteAllItems();
	m_itemsMap.clear();

	if (!m_pContext || !m_pContext->pLibrary)
		return;

	IFacialEffector* pEffector = m_pContext->pLibrary->GetRoot();
	if (pEffector)
		AddEffector(pEffector, 0, TVI_ROOT);

	HTREEITEM hItem = stl::find_in_map(m_itemsMap, m_pContext->pSelectedEffector, NULL);
	if (hItem && GetSelectedItem() != hItem)
	{
		SelectItem(hItem);
	}

	SetRedraw(TRUE);
}

//////////////////////////////////////////////////////////////////////////
HTREEITEM CExpressionsTreeCtrl::AddEffector(IFacialEffector* pEffector, IFacialEffCtrl* pCtrl, HTREEITEM hParent)
{
	int nImage = 1;
	COLORREF col = 0;

	switch (pEffector->GetType())
	{
	case EFE_TYPE_GROUP:
		nImage = 0;
		break;
	case EFE_TYPE_EXPRESSION:
		nImage = 1;
		break;
	case EFE_TYPE_MORPH_TARGET:
		nImage = 2;
		col = ITEM_COLOR_MORPH_TARGET;
		break;
	case EFE_TYPE_BONE:
		nImage = 3;
		break;
	case EFE_TYPE_ATTACHMENT:
		nImage = 3;
		break;
	}

	HTREEITEM hItem = InsertItem(pEffector->GetName(), nImage, nImage, hParent);

	if (col != 0)
		SetItemColor(hItem, col);

	m_itemsMap[pEffector] = hItem;
	SetItemData(hItem, (DWORD_PTR)pCtrl);

	if (hParent == TVI_ROOT)
		m_hRootItem = hItem;

	int nSubCount = pEffector->GetSubEffectorCount();
	for (int i = 0; i < nSubCount; i++)
	{
		IFacialEffCtrl* pSubCtrl = pEffector->GetSubEffCtrl(i);
		//if (!(pSubCtrl->GetType() == IFacialEffCtrl::CTRL_CONSTANT && pSubCtrl->GetEffector()->GetType() == EFE_TYPE_MORPH_TARGET))

		HTREEITEM hSubItem = AddEffector(pSubCtrl->GetEffector(), pSubCtrl, hItem);
		if (hSubItem && (pSubCtrl->GetFlags() & IFacialEffCtrl::CTRL_FLAG_UI_EXPENDED))
		{
			Expand(hSubItem, TVE_EXPAND);
		}
	}
	if (pEffector->GetFlags() & EFE_FLAG_UI_EXTENDED || hParent == TVI_ROOT)
	{
		Expand(hItem, TVE_EXPAND);
	}
	SortChildren(hItem);
	return hItem;
}

//////////////////////////////////////////////////////////////////////////
void CExpressionsTreeCtrl::OnFacialEdEvent(EFacialEdEvent event, IFacialEffector* pEffector, int nChannelCount, IFacialAnimChannel** ppChannels)
{
	if (m_bIgnoreEvent)
		return;

	switch (event)
	{
	case EFD_EVENT_LIBRARY_UNDO:
	case EFD_EVENT_ADD:
	case EFD_EVENT_REMOVE:
		HandleOrphans();
		Reload();
		break;
	case EFD_EVENT_CHANGE:
		//Reload();
		break;
	case EFD_EVENT_SELECT_EFFECTOR:
		{
			HTREEITEM hItem = stl::find_in_map(m_itemsMap, pEffector, NULL);
			if (hItem && GetSelectedItem() != hItem)
			{
				SelectItem(hItem);
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CExpressionsTreeCtrl::HandleOrphans()
{
	IFacialEffectorsLibrary* pLibrary = (m_pContext ? m_pContext->GetLibrary() : 0);
	int orphanCount = FacialExpressionUtils::AddOrphansToGarbageFacialExpressionFolder(pLibrary);
	if (orphanCount)
	{
		string text;
		text.Format("%d orphaned expressions were found in the library.\nThese have been automatically added as children of the _garbage folder.\nTo delete them permanently, delete them from this folder.", orphanCount);
		MessageBox(text.c_str(), "Orphaned Expressions Found");
		if (m_pContext)
			m_pContext->bLibraryModfied = true;
	}
}

//////////////////////////////////////////////////////////////////////////
IFacialEffCtrl* CExpressionsTreeCtrl::GetSelectedCtrl()
{
	HTREEITEM hItem = GetSelectedItem();
	if (hItem)
	{
		return (IFacialEffCtrl*)GetItemData(hItem);
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
IFacialEffector* CExpressionsTreeCtrl::GetSelectedEffector()
{
	HTREEITEM hItem = GetSelectedItem();
	if (hItem)
	{
		if (hItem == m_hRootItem)
			return m_pContext->pLibrary->GetRoot();

		IFacialEffCtrl* pCtrl = (IFacialEffCtrl*)GetItemData(hItem);
		if (pCtrl)
			return pCtrl->GetEffector();
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
IFacialEffector* CExpressionsTreeCtrl::GetSelectedEffectorParent()
{
	HTREEITEM hItem = GetSelectedItem();
	if (hItem)
	{
		HTREEITEM hParent = GetParentItem(hItem);
		if (!hParent)
			return 0;
		if (hParent == m_hRootItem)
			return m_pContext->pLibrary->GetRoot();

		IFacialEffCtrl* pCtrl = (IFacialEffCtrl*)GetItemData(hParent);
		if (pCtrl)
			return pCtrl->GetEffector();
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CExpressionsTreeCtrl::OnSelChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMTREEVIEW* pNM = (NMTREEVIEW*)pNMHDR;
	if (m_pContext)
	{
		//IFacialEffCtrl *pEffCtrl = (IFacialEffCtrl*)pNM->itemNew.lParam;
		IFacialEffCtrl* pEffCtrl = 0;
		if (GetFocusedItem())
			pEffCtrl = (IFacialEffCtrl*)GetItemData(GetFocusedItem());
		IFacialEffector* pEffector = 0;

		if (GetFocusedItem() == m_hRootItem)
			pEffector = m_pContext->pLibrary->GetRoot();
		else if (pEffCtrl)
			pEffector = pEffCtrl->GetEffector();

		if (pEffector != m_pContext->pSelectedEffector)
		{
			m_bIgnoreEvent = true;
			m_pContext->SelectEffector(pEffector);
			m_bIgnoreEvent = false;
		}
	}
	if (GetFocusedItem())
	{
		SetItemState(GetFocusedItem(), TVIS_BOLD, TVIS_BOLD);
	}
	if (pNM->itemOld.hItem)
	{
		SetItemState(pNM->itemOld.hItem, 0, TVIS_BOLD);
	}
	*pResult = TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CExpressionsTreeCtrl::OnItemExpanded(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMTREEVIEW* pNM = (NMTREEVIEW*)pNMHDR;
	if (m_pContext)
	{
		IFacialEffCtrl* pEffCtrl = (IFacialEffCtrl*)pNM->itemNew.lParam;
		if (pEffCtrl)
		{
			if (pNM->action == TVE_EXPAND)
				pEffCtrl->SetFlags(pEffCtrl->GetFlags() | IFacialEffCtrl::CTRL_FLAG_UI_EXPENDED);
			else if (pNM->action == TVE_COLLAPSE)
				pEffCtrl->SetFlags(pEffCtrl->GetFlags() & (~IFacialEffCtrl::CTRL_FLAG_UI_EXPENDED));
		}
	}
	*pResult = TRUE;
}

//////////////////////////////////////////////////////////////////////////
CFacialExpressionsDialog* CExpressionsTreeCtrl::GetExpressionDialog()
{
	CWnd* pWnd = GetParent();
	while (pWnd)
	{
		if (pWnd->IsKindOf(RUNTIME_CLASS(CFacialExpressionsDialog)))
			return (CFacialExpressionsDialog*)pWnd;
		pWnd = pWnd->GetParent();
	}
	assert(0);
	return 0;
}

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CExpressionsTabCtrl, CTabCtrl)
ON_WM_SIZE()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CExpressionsTabCtrl::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	// Resize all child windows.
	CWnd* pwndChild = GetWindow(GW_CHILD);
	while (pwndChild)
	{
		// Resize windows inside tab.
		CRect rc, irc;
		GetItemRect(0, irc);
		GetClientRect(rc);
		rc.left += 4;
		rc.right -= 4;
		rc.top += irc.bottom - irc.top + 8;
		rc.bottom -= 4;
		pwndChild->MoveWindow(rc, FALSE);
		pwndChild = pwndChild->GetNextWindow();
	}
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CFacialExpressionsDialog, CDialog)
ON_WM_SIZE()
ON_WM_DESTROY()
ON_NOTIFY(TCN_SELCHANGE, IDC_TABCTRL, OnTabSelect)
ON_NOTIFY(NM_RCLICK, IDC_TREE_CONTROL, OnTreeRClick)
ON_WM_ERASEBKGND()
ON_WM_MOUSEWHEEL()
ON_NOTIFY(TVN_BEGINDRAG, IDC_TREE_CONTROL, OnBeginDrag)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
// CFacialExpressionsDialog
//////////////////////////////////////////////////////////////////////////
CFacialExpressionsDialog::CFacialExpressionsDialog() : CDialog(IDD_DATABASE, NULL)
{
	m_pContext = 0;
	m_hAccelerators = 0;
	m_pDropTarget = 0;
}

//////////////////////////////////////////////////////////////////////////
CFacialExpressionsDialog::~CFacialExpressionsDialog()
{
}

//////////////////////////////////////////////////////////////////////////
void CFacialExpressionsDialog::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	if (m_splitWnd.m_hWnd)
	{
		CRect rc;
		GetClientRect(rc);
		m_splitWnd.MoveWindow(rc, FALSE);
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CFacialExpressionsDialog::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CFacialExpressionsDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
}

//////////////////////////////////////////////////////////////////////////
BOOL CFacialExpressionsDialog::OnInitDialog()
{
	m_pDropTarget = new CExpressionsDialogDropTarget(this);
	m_pDropTarget->Register(this);

	m_hAccelerators = LoadAccelerators(AfxGetApp()->m_hInstance, MAKEINTRESOURCE(IDR_FACED_MENU));

	CRect rc(0, 0, 200, 0);

	m_treeCtrl.Create(
	  WS_CHILD | WS_VISIBLE | TVS_HASBUTTONS | TVS_HASLINES | TVS_SHOWSELALWAYS
	  , rc, this, IDC_TREE_CONTROL);
	m_treeCtrl.EnableMultiSelect();

	//m_imagesCtrl.Create( WS_CHILD|WS_VISIBLE,rc,this,IDC_VIEW_CONTROL );

	//////////////////////////////////////////////////////////////////////////
	m_tabCtrl.Create(TCS_HOTTRACK | TCS_TABS | TCS_FOCUSNEVER | TCS_SINGLELINE | WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, rc, this, IDC_TABCTRL);
	//m_tabCtrl.ModifyStyle( WS_BORDER,0,0 );
	//m_tabCtrl.ModifyStyleEx( WS_EX_CLIENTEDGE|WS_EX_STATICEDGE|WS_EX_WINDOWEDGE,0,0 );
	//m_tabCtrl.SetImageList( &m_tabImageList );
	m_tabCtrl.SetFont(CFont::FromHandle((HFONT)SMFCFonts::GetInstance().hSystemFont));
	m_tabCtrl.InsertItem(TAB_CURVES, _T("Curves"), TAB_CURVES);
	m_tabCtrl.InsertItem(TAB_PREVIEW, _T("Preview"), TAB_PREVIEW);
	//////////////////////////////////////////////////////////////////////////

	m_effectorInfoWnd.Create(&m_tabCtrl);
	m_effectorInfoWnd.ShowWindow(SW_SHOW);

	m_splitWnd.CreateStatic(this, 1, 2, WS_CHILD | WS_VISIBLE);
	m_splitWnd.SetPane(0, 0, &m_treeCtrl, CSize(2 * rc.Width() / 3, 100));
	m_splitWnd.SetPane(0, 1, &m_tabCtrl, CSize(1 * rc.Width() / 3, 100));

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
BOOL CFacialExpressionsDialog::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST && m_hAccelerators)
		return ::TranslateAccelerator(m_hWnd, m_hAccelerators, pMsg);
	return CDialog::PreTranslateMessage(pMsg);
}

//////////////////////////////////////////////////////////////////////////
void CFacialExpressionsDialog::SetContext(CFacialEdContext* pContext)
{
	m_pContext = pContext;
	m_treeCtrl.SetContext(pContext);
	if (m_pContext)
		m_pContext->RegisterListener(this);
	m_effectorInfoWnd.SetContext(pContext);
}

//////////////////////////////////////////////////////////////////////////
void CFacialExpressionsDialog::OnTabSelect(NMHDR* pNMHDR, LRESULT* pResult)
{
	int sel = m_tabCtrl.GetCurSel();
	switch (sel)
	{
	case TAB_CURVES:
		m_effectorInfoWnd.ShowWindow(SW_SHOW);
		break;
	case TAB_PREVIEW:
		m_effectorInfoWnd.ShowWindow(SW_HIDE);
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialExpressionsDialog::OnNewFolder()
{
	m_pContext->bLibraryModfied = true;
	if (!m_pContext)
		return;

	if (m_pContext->pSelectedEffector)
	{
		QStringDialog dlg("Folder Name");
		if (dlg.exec() == QDialog::Accepted)
		{
			CString sName = dlg.GetString();
			if (m_pContext->pLibrary->Find(sName))
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Facial Effector with name %s already exist", (const char*)sName);
				return;
			}
			IFacialEffector* pEffector = m_pContext->pLibrary->CreateEffector(EFE_TYPE_GROUP, sName);
			m_pContext->pSelectedEffector->AddSubEffector(pEffector);
			m_pContext->SendEvent(EFD_EVENT_ADD, pEffector);
			m_pContext->SetModified(pEffector);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialExpressionsDialog::OnNewExpression(int nType)
{
	if (!m_pContext)
		return;

	if (m_pContext->pSelectedEffector)
	{
		QStringDialog dlg("Expression Name");
		if (dlg.exec() == QDialog::Accepted)
		{
			CString sName = dlg.GetString();
			IFacialEffector* pEffector = m_pContext->pLibrary->Find(sName);
			if (pEffector)
			{
				if (FacialExpressionUtils::IsExpressionInGarbage(m_pContext->pLibrary, pEffector))
				{
					CString str;
					str.Format(_T("An expression by the name of %s is in the garbage - do you want to delete it and create a new one?"), (const char*)sName);
					if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(str)))
					{
						m_pContext->pLibrary->RemoveEffector(pEffector);
						pEffector = 0;
					}
				}

				if (pEffector)
				{
					CString str;
					str.Format(_T("Facial Expression with name %s already exist\r\nDo you want to use the existing one?"), (const char*)sName);
					if (QDialogButtonBox::StandardButton::No == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(str)))
					{
						return;
					}
				}
			}
			if (!pEffector)
				pEffector = m_pContext->pLibrary->CreateEffector((EFacialEffectorType)nType, sName);
			if (pEffector)
			{
				FacialExpressionUtils::RemoveFromGarbage(m_pContext->pLibrary, pEffector);
				m_pContext->pSelectedEffector->AddSubEffector(pEffector);
				m_pContext->SendEvent(EFD_EVENT_ADD, pEffector);
				m_pContext->SetModified(pEffector);
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Creation of Effector %s Failed", sName);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialExpressionsDialog::OnRename()
{
	if (!m_pContext || !m_pContext->pSelectedEffector)
		return;

	// Root cannot be renamed.
	if (m_pContext->pSelectedEffector == m_pContext->pLibrary->GetRoot())
		return;

	if (m_pContext->pSelectedEffector->GetType() != EFE_TYPE_EXPRESSION && m_pContext->pSelectedEffector->GetType() != EFE_TYPE_GROUP && m_pContext->pSelectedEffector->GetType() != EFE_TYPE_BONE)
		return;

	QStringDialog dlg("Expression Name");
	dlg.SetString(m_pContext->pSelectedEffector->GetName());
	if (dlg.exec() == QDialog::Accepted)
	{
		CString sName = dlg.GetString();
		if (m_pContext->pLibrary->Find(sName) != NULL)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, _T("Facial Expression with name %s already exist"), (const char*)sName);
			return;
		}

		CUndo undo("Rename Expression");
		m_pContext->StoreLibraryUndo();

		m_pContext->pSelectedEffector->SetName(sName);
		m_pContext->SetModified(m_pContext->pSelectedEffector);
		m_pContext->SendEvent(EFD_EVENT_CHANGE_RELOAD, m_pContext->pSelectedEffector);
		m_treeCtrl.Reload();
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialExpressionsDialog::OnRemove()
{
	if (!m_pContext || !m_pContext->pSelectedEffector)
		return;

	// Root cannot be removed.
	if (m_pContext->pSelectedEffector == m_pContext->pLibrary->GetRoot())
		return;

	IFacialEffector* pParentEffector = m_treeCtrl.GetSelectedEffectorParent();
	if (pParentEffector && m_pContext)
	{
		CUndo undo("Remove Expression");
		m_pContext->StoreLibraryUndo();

		IFacialEffector* pEffectorToRemove = m_pContext->pSelectedEffector;
		m_pContext->SelectEffector(NULL);
		pParentEffector->RemoveSubEffector(pEffectorToRemove);
		FacialExpressionUtils::AddOrphansToGarbageFacialExpressionFolder(m_pContext->pLibrary);
		m_pContext->SetModified(pParentEffector);
		m_pContext->SendEvent(EFD_EVENT_CHANGE_RELOAD, pParentEffector);
		m_treeCtrl.Reload();
		m_pContext->SelectEffector(pParentEffector);
		m_pContext->SetModified();
		m_pContext->bLibraryModfied = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialExpressionsDialog::OnDelete()
{
	if (!m_pContext || !m_pContext->pSelectedEffector)
		return;

	// Root cannot be removed.
	if (m_pContext->pSelectedEffector == m_pContext->pLibrary->GetRoot())
		return;

	IFacialEffector* pParentEffector = m_treeCtrl.GetSelectedEffectorParent();
	if (m_pContext)
	{
		CUndo undo("Remove Expression");
		m_pContext->StoreLibraryUndo();

		IFacialEffector* pEffectorToRemove = m_pContext->pSelectedEffector;
		m_pContext->SelectEffector(NULL);
		m_pContext->pLibrary->RemoveEffector(pEffectorToRemove);
		FacialExpressionUtils::DeleteOrphans(m_pContext->pLibrary);
		m_pContext->SetModified(pParentEffector);
		m_pContext->SendEvent(EFD_EVENT_CHANGE_RELOAD, pParentEffector);
		m_treeCtrl.Reload();
		m_pContext->SelectEffector(pParentEffector);
		m_pContext->SetModified();
		m_pContext->bLibraryModfied = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialExpressionsDialog::OnEmptyGarbage()
{
	if (!m_pContext)
		return;

	CUndo undo("Empty Garbage");
	m_pContext->StoreLibraryUndo();

	m_pContext->SelectEffector(NULL);
	IFacialEffector* pGarbageFolder = FacialExpressionUtils::GetGarbageFacialExpressionFolder(m_pContext->pLibrary, false);
	if (pGarbageFolder)
	{
		pGarbageFolder->RemoveAllSubEffectors();
		m_pContext->pLibrary->RemoveEffector(pGarbageFolder);
		FacialExpressionUtils::DeleteOrphans(m_pContext->pLibrary);
		m_pContext->SetModified();
		m_pContext->SendEvent(EFD_EVENT_CHANGE_RELOAD, 0);
		m_treeCtrl.Reload();
		m_pContext->SetModified();
		m_pContext->bLibraryModfied = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialExpressionsDialog::OnCopy()
{
	if (m_pContext && m_pContext->pSelectedEffector)
		m_pContext->CopyExpressionNameToClipboard(m_pContext->pSelectedEffector);
}

//////////////////////////////////////////////////////////////////////////
void CFacialExpressionsDialog::OnSetPoseFromExpression()
{
	if (m_pContext && m_pContext->pSelectedEffector)
		m_pContext->SetPoseFromExpression(m_pContext->pSelectedEffector, m_pContext->GetPreviewWeight(), m_pContext->GetSequenceTime());
}

//////////////////////////////////////////////////////////////////////////
void CFacialExpressionsDialog::OnPaste()
{
	if (m_pContext)
		m_pContext->PasteExpressionFromClipboard();
}

_smart_ptr<IFacialEffectorsLibrary> CFacialExpressionsDialog::CreateLibraryOfSelectedEffectors()
{
	return m_treeCtrl.CreateLibraryOfSelectedEffectors();
}

#define CMD_NEWFOLDER                 100
#define CMD_NEWEXPRESSION             101
#define CMD_GETFROMSLIDERS            102
#define CMD_GETFROMTIMELINE           103
#define CMD_ITEM_COPY                 106
#define CMD_ITEM_PASTE                107
#define CMD_ITEM_REMOVE               110
#define CMD_ITEM_RENAME               111
#define CMD_NEW_ATTACHMENT_CONTROLLER 112
#define CMD_NEW_BONE_CONTROLLER       113
#define CMD_ITEM_DELETE               114
#define CMD_ITEM_EMPTY_GARBAGE        115
#define CMD_SETPOSEFROMEXPRESSION     116

//////////////////////////////////////////////////////////////////////////
void CFacialExpressionsDialog::OnTreeRClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (!m_pContext)
		return;

	CPoint p;
	GetCursorPos(&p);

	CPoint lp = p;
	m_treeCtrl.ScreenToClient(&lp);
	// Select clicked item.
	UINT uFlags;
	HTREEITEM hItem = m_treeCtrl.HitTest(lp, &uFlags);
	if ((hItem != NULL))// && (TVHT_ONITEM & uFlags))
	{
		m_treeCtrl.Select(hItem, TVGN_CARET);
	}

	// Show context menu.
	CMenu menu;
	menu.CreatePopupMenu();

	IFacialEffCtrl* pSelectedCtrl = m_treeCtrl.GetSelectedCtrl();
	IFacialEffector* pSelected = m_treeCtrl.GetSelectedEffector();

	if (pSelected)
	{
		if (pSelected->GetType() == EFE_TYPE_GROUP)
			menu.AppendMenu(MF_STRING, CMD_NEWFOLDER, "New Folder");
		menu.AppendMenu(MF_STRING, CMD_NEWEXPRESSION, "New Expression");
		menu.AppendMenu(MF_STRING, CMD_NEW_BONE_CONTROLLER, "New Bone Control");
		menu.AppendMenu(MF_STRING, CMD_NEW_ATTACHMENT_CONTROLLER, "New Attachment Control");

		//menu.AppendMenu( MF_STRING,CMD_NEWEXPRESSION,"New Attachment Control" );

		menu.AppendMenu(MF_SEPARATOR);
		if (pSelected->GetType() == EFE_TYPE_EXPRESSION || pSelected->GetType() == EFE_TYPE_GROUP || pSelected->GetType() == EFE_TYPE_BONE)
		{
			menu.AppendMenu(MF_STRING, CMD_ITEM_RENAME, "Rename");
		}
		if (pSelected->GetType() == EFE_TYPE_EXPRESSION)
		{
			menu.AppendMenu(MF_STRING, CMD_GETFROMSLIDERS, "Initialize From Sliders");
			menu.AppendMenu(MF_STRING, CMD_GETFROMTIMELINE, "Initialize From Current Timeline");
			menu.AppendMenu(MF_SEPARATOR);
			menu.AppendMenu(MF_STRING, CMD_SETPOSEFROMEXPRESSION, "Use Expression as Pose");
			menu.AppendMenu(MF_SEPARATOR);
		}
		menu.AppendMenu(MF_STRING, CMD_ITEM_COPY, "Copy");
		menu.AppendMenu(MF_STRING, CMD_ITEM_PASTE, "Paste");

		bool isGarbageFolder = false;
		bool isInGarbageFolder = false;
		if (m_treeCtrl.GetItemText(m_treeCtrl.GetFirstSelectedItem()) == "_garbage")
		{
			isGarbageFolder = true;
		}
		else
		{
			for (HTREEITEM hItem = m_treeCtrl.GetFirstSelectedItem(); hItem; hItem = m_treeCtrl.GetParentItem(hItem))
				isInGarbageFolder = isInGarbageFolder || (m_treeCtrl.GetItemText(hItem) == "_garbage");
		}

		if (isGarbageFolder)
			menu.AppendMenu(MF_STRING, CMD_ITEM_EMPTY_GARBAGE, "Empty Garbage");
		else if (isInGarbageFolder)
			menu.AppendMenu(MF_STRING, CMD_ITEM_DELETE, "Delete Permanently");
		else
			menu.AppendMenu(MF_STRING, CMD_ITEM_REMOVE, "Remove");
	}

	int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON, p.x, p.y, this);
	switch (cmd)
	{
	case CMD_NEWFOLDER:
		OnNewFolder();
		break;
	case CMD_NEWEXPRESSION:
		OnNewExpression(EFE_TYPE_EXPRESSION);
		break;
	case CMD_NEW_ATTACHMENT_CONTROLLER:
		OnNewExpression(EFE_TYPE_ATTACHMENT);
		break;
	case CMD_NEW_BONE_CONTROLLER:
		OnNewExpression(EFE_TYPE_BONE);
		break;
	case CMD_GETFROMSLIDERS:
		{
			if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("This command will override all sub effectors of the selected expression")))
			{
				m_pContext->UpdateSelectedFromSliders();
				m_treeCtrl.Reload();
			}
		}
		break;
	case CMD_GETFROMTIMELINE:
		{
			if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("This command will override all sub effectors of the selected expression")))
			{
				QNumericBoxDialog dlg(QObject::tr("Minimum Effector Weight"), 0.1f);
				if (dlg.exec() != QDialog::Accepted)
					return;
				// minimum threshold for an effector to be included
				float minimumWeight = dlg.GetValue();
				minimumWeight = min(max(minimumWeight, -1.0f), 1.0f);

				m_pContext->UpdateSelectedFromSequenceTime(minimumWeight);
				m_treeCtrl.Reload();
			}
		}
		break;
	case CMD_SETPOSEFROMEXPRESSION:
		OnSetPoseFromExpression();
		break;
	case CMD_ITEM_COPY:
		OnCopy();
		break;
	case CMD_ITEM_PASTE:
		OnPaste();
		break;
	case CMD_ITEM_REMOVE:
		OnRemove();
		break;
	case CMD_ITEM_DELETE:
		OnDelete();
		break;
	case CMD_ITEM_EMPTY_GARBAGE:
		OnEmptyGarbage();
		break;
	case CMD_ITEM_RENAME:
		OnRename();
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialExpressionsDialog::OnFacialEdEvent(EFacialEdEvent event, IFacialEffector* pEffector, int nChannelCount, IFacialAnimChannel** ppChannels)
{
	switch (event)
	{
	case EFD_EVENT_ADD:
		break;
	case EFD_EVENT_REMOVE:
		break;
	case EFD_EVENT_CHANGE:
		break;
	case EFD_EVENT_SELECT_EFFECTOR:
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CFacialExpressionsDialog::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	CFacialEdContext::MoveToKeyDirection direction = (zDelta < 0 ? CFacialEdContext::MoveToKeyDirectionBackward : CFacialEdContext::MoveToKeyDirectionForward);
	if (m_pContext)
		m_pContext->MoveToFrame(direction);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CFacialExpressionsDialog::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult)
{
	HTREEITEM hItem = m_treeCtrl.GetFirstSelectedItem();

	for (int i = 0; i < m_treeCtrl.GetSelectedCount(); ++i)
	{
		if (i > 0)
			hItem = m_treeCtrl.GetNextSelectedItem(hItem);

		IFacialEffCtrl* pEffectorCtrl = (IFacialEffCtrl*)m_treeCtrl.GetItemData(hItem);
		IFacialEffector* pEffector = (pEffectorCtrl ? pEffectorCtrl->GetEffector() : 0);
		HTREEITEM hParentItem = m_treeCtrl.GetParentItem(hItem);
		IFacialEffCtrl* pParentEffectorCtrl = (IFacialEffCtrl*)m_treeCtrl.GetItemData(hParentItem);
		IFacialEffector* pParentEffector = (pParentEffectorCtrl ? pParentEffectorCtrl->GetEffector() : 0);

		if (m_pContext && pEffector)
			m_pContext->DoExpressionNameDragDrop(pEffector, pParentEffector);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialExpressionsDialog::OnDestroy()
{
	if (m_pDropTarget)
		delete m_pDropTarget;
}

