// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "MFCPropertyTree.h"

#include <IEditor.h>
#include <CrySerialization/IArchive.h>
#include <Serialization/QPropertyTree/QPropertyTree.h>
#include <QPropertyTree/QPropertyDialog.h>

#if QT_VERSION >= 0x50000
	#include <QWindow>
	#include <QGuiApplication>
#endif

CMFCPropertyTreeSignalHandler::CMFCPropertyTreeSignalHandler(CMFCPropertyTree* propertyTree)
	: m_propertyTree(propertyTree)
{
	connect(m_propertyTree->m_propertyTree, SIGNAL(signalSizeChanged()), SLOT(OnSizeChanged()));
	connect(m_propertyTree->m_propertyTree, SIGNAL(signalChanged()), SLOT(OnPropertyChanged()));
}

void CMFCPropertyTreeSignalHandler::OnSizeChanged()
{
	if (m_propertyTree->m_sizeChangeCallback)
	{
		m_propertyTree->m_sizeChangeCallback();
	}
}

void CMFCPropertyTreeSignalHandler::OnPropertyChanged()
{
	if (m_propertyTree->m_propertyChangeCallback)
	{
		m_propertyTree->m_propertyChangeCallback();
	}
}

// ---------------------------------------------------------------------------
IMPLEMENT_DYNCREATE(CMFCPropertyTree, CWnd)

BEGIN_MESSAGE_MAP(CMFCPropertyTree, CWnd)
ON_WM_CREATE()
ON_WM_SETFOCUS()
ON_WM_SIZE()
ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

CMFCPropertyTree::CMFCPropertyTree()
	: m_propertyTree(0)
{
	m_propertyTree = new QPropertyTree(0);
	m_signalHandler = new CMFCPropertyTreeSignalHandler(this);
}

CMFCPropertyTree::~CMFCPropertyTree()
{
	delete m_propertyTree;
	m_propertyTree = 0;

	delete m_signalHandler;
	m_signalHandler = 0;
}

bool CMFCPropertyTree::RegisterWindowClass()
{
	WNDCLASS windowClass = { 0 };

	windowClass.style = CS_DBLCLKS;
	windowClass.lpfnWndProc = &AfxWndProc;
	windowClass.hInstance = AfxGetInstanceHandle();
	windowClass.hIcon = NULL;
	windowClass.hCursor = NULL;
	windowClass.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	windowClass.lpszMenuName = NULL;

	windowClass.lpszClassName = ClassName();

	return AfxRegisterClass(&windowClass) ? true : false;
}

void CMFCPropertyTree::UnregisterWindowClass()
{
}

int CMFCPropertyTree::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	m_propertyTree->setWindowFlags(Qt::Widget);
	HWND child = (HWND)m_propertyTree->winId();
	::SetWindowLong(child, GWL_STYLE, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
#if QT_VERSION >= 0x50000
	QWindow* qwindow = m_propertyTree->windowHandle();
	qwindow->setProperty("_q_embedded_native_parent_handle", QVariant((WId)GetSafeHwnd()));
	::SetParent(child, GetSafeHwnd());
	qwindow->setFlags(Qt::FramelessWindowHint);
#else
	::SetParent(child, GetSafeHwnd());
#endif

	QEvent e(QEvent::EmbeddingControl);
	QCoreApplication::sendEvent(m_propertyTree, &e);
	m_propertyTree->setUndoEnabled(true);
	PropertyTreeStyle treeStyle(QPropertyTree::defaultTreeStyle());
	treeStyle.propertySplitter = false;
	m_propertyTree->setTreeStyle(treeStyle);
	m_propertyTree->show();
	return 0;
}

void CMFCPropertyTree::OnSetFocus(CWnd* oldWnd)
{
	if (m_propertyTree)
		::SetFocus((HWND)m_propertyTree->winId());
}

void CMFCPropertyTree::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (m_propertyTree)
	{
		if (::GetFocus() != (HWND)m_propertyTree->winId())
		{
			m_propertyTree->setFocus(Qt::MouseFocusReason);
		}
	}
	__super::OnLButtonDown(nFlags, point);
}

void CMFCPropertyTree::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	if (m_propertyTree && cx != 0 && cy != 0)
	{
		m_propertyTree->resize(cx, cy);
		HWND hwnd = (HWND)m_propertyTree->winId();
		::MoveWindow(hwnd, 0, 0, cx, cy, FALSE);
		::RedrawWindow(hwnd, 0, 0, RDW_INVALIDATE | RDW_ALLCHILDREN);
	}
}

BOOL CMFCPropertyTree::Create(CWnd* parent, const RECT& rect)
{
	return CreateEx(0, ClassName(), "PropertyTree", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, rect, parent, 0);
}

void CMFCPropertyTree::Release()
{
	delete this;
}

void CMFCPropertyTree::Attach(Serialization::SStruct& ser)
{
	MAKE_SURE(m_propertyTree != 0, return );
	m_propertyTree->attach(ser);
}

void CMFCPropertyTree::Detach()
{
	MAKE_SURE(m_propertyTree != 0, return );
	m_propertyTree->detach();
}

void CMFCPropertyTree::Revert()
{
	MAKE_SURE(m_propertyTree != 0, return );
	m_propertyTree->revert();
}

void CMFCPropertyTree::SetExpandLevels(int expandLevels)
{
	m_propertyTree->setExpandLevels(expandLevels);
}

void CMFCPropertyTree::SetCompact(bool compact)
{
	m_propertyTree->setCompact(compact);
}

void CMFCPropertyTree::SetArchiveContext(Serialization::SContextLink* context)
{
	m_propertyTree->setArchiveContext(context);
}

void CMFCPropertyTree::SetHideSelection(bool hideSelection)
{
	m_propertyTree->setHideSelection(hideSelection);
}

void CMFCPropertyTree::SetPropertyChangeHandler(const Functor0& callback)
{
	m_propertyChangeCallback = callback;
}

void CMFCPropertyTree::SetSizeChangeHandler(const Functor0& callback)
{
	m_sizeChangeCallback = callback;
}

SIZE CMFCPropertyTree::ContentSize() const
{
	QSize s = m_propertyTree->contentSize();
	SIZE r = { s.width(), s.height() };
	return r;
}

void CMFCPropertyTree::Serialize(Serialization::IArchive& ar)
{
	if (m_propertyTree)
		m_propertyTree->Serialize(ar);
}

HWND FindTopLevelFrame(HWND child)
{
	if (child == GetDesktopWindow())
		return 0;
	HWND current = child;
	while (GetParent(current) != 0)
		current = GetParent(current);

	return current;
}

