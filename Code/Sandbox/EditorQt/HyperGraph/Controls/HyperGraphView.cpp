// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HyperGraphView.h"

#include <Gdiplus.h>
#include <iterator>

#include "IDataBaseItem.h"
#include "DataBaseDialog.h"
#include "Viewport.h"
#include "Controls/MemDC.h"
#include "Controls/PropertyCtrl.h"
#include "Controls/SharedFonts.h"
#include "Util/Clipboard.h"
#include "GameEngine.h"

#include "HyperGraph/HyperGraphManager.h"
#include "HyperGraph/HyperGraph.h"
#include "HyperGraph/FlowGraphManager.h"
#include "HyperGraph/FlowGraphModuleManager.h"
#include "HyperGraph/FlowGraph.h"
#include "HyperGraph/FlowGraphNode.h"
#include "HyperGraph/Nodes/CommentNode.h"
#include "HyperGraph/Nodes/CommentBoxNode.h"
#include "HyperGraph/Nodes/BlackBoxNode.h"
#include "HyperGraph/Nodes/TrackEventNode.h"
#include "HyperGraph/Nodes/QuickSearchNode.h"
#include "HyperGraph/FlowGraphVariables.h"
#include "HyperGraph/FlowGraphPreferences.h"

#include "HyperGraphEditorWnd.h"
#include "FlowGraphDebuggerEditor.h"
#include "FlowGraphSearchCtrl.h"
#include "FlowGraphModuleDialogs.h"

#include "GameTokens/GameTokenItem.h"
#include "GameTokens/GameTokenDialog.h"
#include "GameTokens/GameTokenManager.h"

#include "Objects/ObjectLoader.h"
#include "Objects/ObjectLayerManager.h"
#include "Objects/EntityObject.h"
#include "Objects/PrefabObject.h"
#include "Prefabs/PrefabManager.h"
#include "Prefabs/PrefabEvents.h"


enum
{
	ID_GRAPHVIEW_RENAME = 1,
	ID_GRAPHVIEW_ADD_SELECTED_ENTITY,
	ID_GRAPHVIEW_ADD_DEFAULT_ENTITY,
	ID_GRAPHVIEW_TARGET_SELECTED_ENTITY,
	ID_GRAPHVIEW_TARGET_GRAPH_ENTITY,
	ID_GRAPHVIEW_TARGET_GRAPH_ENTITY2,
	ID_GRAPHVIEW_TARGET_UNSELECT_ENTITY,
	//ID_GRAPHVIEW_BLACKBOX_SELECTED_ENTITY,
	ID_GRAPHVIEW_ADD_COMMENT,
	ID_GRAPHVIEW_ADD_COMMENTBOX,
	ID_GRAPHVIEW_FIT_TOVIEW,
	ID_GRAPHVIEW_SELECT_ENTITY,
	ID_GRAPHVIEW_SELECT_AND_GOTO_ENTITY,
	ID_GRAPHVIEW_TARGET_SELECTED_PREFAB,
	ID_GRAPHVIEW_TARGET_UNSELECT_PREFAB,
	ID_GRAPHVIEW_SELECTION_EDGES_ENABLE,
	ID_GRAPHVIEW_SELECTION_EDGES_DISABLE,
	ID_GRAPHVIEW_ADD_BLACK_BOX,
	ID_GRAPHVIEW_ADD_TRACKEVENT,
	ID_GRAPHVIEW_ADD_STARTNODE,
	ID_GRAPHVIEW_ADD_BREAKPOINT,
	ID_GRAPHVIEW_REMOVE_BREAKPOINT,
	ID_GRAPHVIEW_REMOVE_BREAKPOINTS_FOR_NODE,
	ID_GRAPHVIEW_REMOVE_ALL_BREAKPOINTS,
	ID_BREAKPOINT_ENABLE,
	ID_TRACEPOINT_ENABLE,
	ID_INPUTS_SHOW_ALL,
	ID_INPUTS_HIDE_ALL,
	ID_OUTPUTS_SHOW_ALL,
	ID_OUTPUTS_HIDE_ALL,
	ID_INPUTS_DISABLE_LINKS,
	ID_INPUTS_ENABLE_LINKS,
	ID_OUTPUTS_DISABLE_LINKS,
	ID_OUTPUTS_ENABLE_LINKS,
	ID_INPUTS_DELETE_LINKS,
	ID_OUTPUTS_DELETE_LINKS,
	ID_INPUTS_CUT_LINKS,
	ID_INPUTS_COPY_LINKS,
	ID_INPUTS_PASTE_LINKS,
	ID_OUTPUTS_CUT_LINKS,
	ID_OUTPUTS_COPY_LINKS,
	ID_OUTPUTS_PASTE_LINKS,
	ID_GOTO_MODULE,
	ID_GOTO_TOKEN,
	ID_TOKEN_FIND_USAGES,
};

#define BASE_NEW_NODE_CMD 10000
#define BASE_INPUTS_CMD   20000
#define BASE_OUTPUTS_CMD  21000

//////////////////////////////////////////////////////////////////////////
#define BACKGROUND_COLOR       GET_GDI_COLOR_NO_TRANSPARENCY(gFlowGraphColorPreferences.colorBackground)
#define GRID_COLOR             GET_GDI_COLOR_NO_TRANSPARENCY(gFlowGraphColorPreferences.colorGrid)
#define ARROW_COLOR            GET_GDI_COLOR(gFlowGraphColorPreferences.colorArrow)
#define ARROW_SEL_COLOR_IN     GET_GDI_COLOR(gFlowGraphColorPreferences.colorInArrowHighlighted)
#define ARROW_SEL_COLOR_OUT    GET_GDI_COLOR(gFlowGraphColorPreferences.colorOutArrowHighlighted)
#define PORT_EDGE_HIGHLIGHT    GET_GDI_COLOR(gFlowGraphColorPreferences.colorPortEdgeHighlighted)
#define ARROW_DIS_COLOR        GET_GDI_COLOR(gFlowGraphColorPreferences.colorArrowDisabled)
#define ARROW_WIDTH            14

#define MIN_ZOOM               0.01f
#define MAX_ZOOM               100.0f
#define MIN_ZOOM_CAN_EDIT      0.8f
#define MIN_ZOOM_CHANGE_HEIGHT 1.2f

#define DEFAULT_EDIT_HEIGHT    14

#define SMALLNODE_SPLITTER     40
#define SMALLNODE_WIDTH        160

#define PORT_HEIGHT            16

//////////////////////////////////////////////////////////////////////////
// CHyperGraphViewQuickSearchEdit implementation.
//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CHyperGraphViewQuickSearchEdit, CEdit)
ON_WM_GETDLGCODE()
ON_WM_KILLFOCUS()
ON_CONTROL_REFLECT(EN_KILLFOCUS, OnSearchKillFocus)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphViewQuickSearchEdit::PreTranslateMessage(MSG* msg)
{
	if (!m_pView)
		return TRUE;

	if (msg->message == WM_LBUTTONDOWN || msg->message == WM_RBUTTONDOWN)
	{
		CPoint pnt;
		GetCursorPos(&pnt);
		CRect rc;
		GetWindowRect(rc);
		if (!rc.PtInRect(pnt))
		{
			// Accept edit.
			m_pView->OnAcceptSearch();
			return TRUE;
		}
	}
	else if (msg->message == WM_KEYDOWN)
	{
		switch (msg->wParam)
		{
		case VK_UP:
			return TRUE;
			break;
		}
	}
	else if (msg->message == WM_KEYUP)
	{
		switch (msg->wParam)
		{
		case VK_RETURN:
			{
				GetAsyncKeyState(VK_CONTROL);
				if (!GetAsyncKeyState(VK_CONTROL))
				{
					// Send accept message.
					m_pView->OnAcceptSearch();
					return TRUE;
				}
			}
			break;

		case VK_ESCAPE:
			// Cancel edit.
			m_pView->OnCancelSearch();
			return TRUE;
			break;
		case VK_UP:
			m_pView->OnSelectNextSearchResult(true);
			return TRUE;
			break;
		case VK_DOWN:
		case VK_TAB:
			m_pView->OnSelectNextSearchResult(false);
			return TRUE;
			break;
		default:
			if ((msg->wParam >= 'A' && msg->wParam <= 'Z') || msg->wParam == VK_BACK)
				m_pView->OnSearchNodes();
			break;
		}
	}
	return CEdit::PreTranslateMessage(msg);
}

void CHyperGraphViewQuickSearchEdit::OnKillFocus(CWnd* pWnd)
{
	// Send accept
	if (m_pView)
		m_pView->OnCancelSearch();

	CEdit::OnKillFocus(pWnd);
}

void CHyperGraphViewQuickSearchEdit::OnSearchKillFocus()
{
	// Send accept
	if (m_pView)
		m_pView->OnAcceptSearch();
}

//////////////////////////////////////////////////////////////////////////
// CHyperGraphViewRenameEdit implementation.
//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CHyperGraphViewRenameEdit, CEdit)
ON_WM_GETDLGCODE()
ON_WM_KILLFOCUS()
ON_CONTROL_REFLECT(EN_KILLFOCUS, OnEditKillFocus)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphViewRenameEdit::PreTranslateMessage(MSG* msg)
{
	if (msg->message == WM_LBUTTONDOWN || msg->message == WM_RBUTTONDOWN)
	{
		CPoint pnt;
		GetCursorPos(&pnt);
		CRect rc;
		GetWindowRect(rc);
		if (!rc.PtInRect(pnt))
		{
			// Accept edit.
			m_pView->OnAcceptRename();
			return TRUE;
		}
	}
	else if (msg->message == WM_KEYDOWN)
	{
		switch (msg->wParam)
		{
		case VK_RETURN:
			{
				GetAsyncKeyState(VK_CONTROL);
				if (!GetAsyncKeyState(VK_CONTROL))
				{
					// Send accept message.
					m_pView->OnAcceptRename();
					return TRUE;
				}

				/*
				   // Add carriege return into the end of string.
				   const char *szStr = "\n";
				   int len = GetWindowTextLength();
				   SendMessage( EM_SETSEL,len,-1 );
				   SendMessage( EM_SCROLLCARET, 0,0 );
				   SendMessage( EM_REPLACESEL, FALSE, (LPARAM)szStr );
				 */
			}
			break;

		case VK_ESCAPE:
			// Cancel edit.
			m_pView->OnCancelRename();
			return TRUE;
			break;
		}
	}
	return CEdit::PreTranslateMessage(msg);
}

void CHyperGraphViewRenameEdit::OnKillFocus(CWnd* pWnd)
{
	// Send accept
	m_pView->OnAcceptRename();
	CEdit::OnKillFocus(pWnd);
}

void CHyperGraphViewRenameEdit::OnEditKillFocus()
{
	// Send accept
	m_pView->OnAcceptRename();
}

//////////////////////////////////////////////////////////////////////////
// CEditPropertyCtrl implementation.
//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CEditPropertyCtrl, CPropertyCtrl)
ON_WM_KEYDOWN()
END_MESSAGE_MAP()

void CEditPropertyCtrl::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	CPropertyCtrl::OnKeyDown(nChar, nRepCnt, nFlags);

	switch (nChar)
	{
	case VK_RETURN:
		m_pView->HideEditPort();
		break;
	}
}

void CEditPropertyCtrl::SelectItem(CPropertyItem* item)
{
	CPropertyCtrl::SelectItem(item);
	if (!item)
		m_pView->HideEditPort();
}

//////////////////////////////////////////////////////////////////////////
void CEditPropertyCtrl::Expand(CPropertyItem* item, bool bExpand, bool bRedraw)
{
	// Avoid parent method calling
}

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CHyperGraphView, CWnd)

BEGIN_MESSAGE_MAP(CHyperGraphView, CWnd)
ON_WM_MOUSEMOVE()
ON_WM_DESTROY()
ON_WM_ERASEBKGND()
ON_WM_PAINT()
ON_WM_SIZE()
ON_WM_MOUSEWHEEL()
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_RBUTTONDOWN()
ON_WM_RBUTTONUP()
ON_WM_MBUTTONDOWN()
ON_WM_MBUTTONUP()
ON_WM_LBUTTONDBLCLK()
ON_WM_KEYDOWN()
ON_WM_KEYUP()
ON_WM_SETCURSOR()
ON_WM_HSCROLL()
ON_WM_VSCROLL()

ON_COMMAND(ID_EDIT_DELETE, OnCommandDelete)
ON_COMMAND(ID_EDIT_COPY, OnCommandCopy)
ON_COMMAND(ID_EDIT_PASTE, OnCommandPaste)
ON_COMMAND(ID_EDIT_PASTE_WITH_LINKS, OnCommandPasteWithLinks)
ON_COMMAND(ID_EDIT_CUT, OnCommandCut)
ON_COMMAND(ID_UNDO, OnUndo)
ON_COMMAND(ID_REDO, OnRedo)
//ON_COMMAND(ID_MINIMIZE,OnToggleMinimize)
ON_COMMAND(ID_EDIT_DELETE_KEEP_LINKS, OnCommandDeleteKeepLinks)
END_MESSAGE_MAP()

struct GraphicsHelper
{
	GraphicsHelper(CWnd* pWnd) : m_pWnd(pWnd), m_pDC(pWnd->GetDC()), m_pGr(new Gdiplus::Graphics(m_pDC->GetSafeHdc()))
	{
	}
	~GraphicsHelper()
	{
		delete m_pGr;
		m_pWnd->ReleaseDC(m_pDC);
	}

	operator Gdiplus::Graphics * ()
	{
		return m_pGr;
	}

	CWnd*              m_pWnd;
	CDC*               m_pDC;
	Gdiplus::Graphics* m_pGr;
};

//////////////////////////////////////////////////////////////////////////

TFlowNodeId CHyperGraphView::s_quickSearchNodeId = InvalidFlowNodeTypeId;

CHyperGraphView::CHyperGraphView()
{
	m_pGraph = 0;
	m_pSrcDragNode = 0;
	m_mouseDownPos = CPoint(0, 0);
	m_scrollOffset = CPoint(0, 0);
	m_rcSelect.SetRect(0, 0, 0, 0);
	m_mode = NothingMode;

	m_pPropertiesCtrl = 0;
	m_isEditPort = false;
	m_zoom = 1.0f;
	m_bCopiedBlackBox = false;
	m_componentViewMask = 0; // will be set by dialog
	m_bRefitGraphInOnSize = false;
	m_pEditPort = 0;
	m_bIsFrozen = false;
	m_pQuickSearchNode = NULL;
	m_bIgnoreHyperGraphEvents = false;
	m_bBatchingChangesInProgress = false;

	m_bDraggingFixedOutput = false;
	m_bEdgeWasJustDisconnected = false;

	s_quickSearchNodeId = gEnv->pFlowSystem->GetTypeId("QuickSearch");
}

//////////////////////////////////////////////////////////////////////////
CHyperGraphView::~CHyperGraphView()
{
	m_graphVisuallyChangedHelper.Reset(true);
	SetGraph(NULL);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::SetPropertyCtrl(CPropertyCtrl* propsCtrl)
{
	m_pPropertiesCtrl = propsCtrl;

	if (m_pPropertiesCtrl)
	{
		m_pPropertiesCtrl->SetUpdateCallback(functor(*this, &CHyperGraphView::OnUpdateProperties));
		m_pPropertiesCtrl->EnableUpdateCallback(true);
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphView::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	if (!m_hWnd)
	{
		//////////////////////////////////////////////////////////////////////////
		// Create window.
		//////////////////////////////////////////////////////////////////////////
		CRect rcDefault(0, 0, 100, 100);
		LPCTSTR lpClassName = AfxRegisterWndClass(CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, AfxGetApp()->LoadStandardCursor(IDC_ARROW), NULL, NULL);
		VERIFY(CreateEx(NULL, lpClassName, "HyperGraphView", dwStyle, rcDefault, pParentWnd, nID));

		if (!m_hWnd)
			return FALSE;

		CRect rc(0, 0, 220, 18);
		m_editParamCtrl.Create(WS_CHILD, rc, this);
		m_editParamCtrl.SetView(this);
		m_editParamCtrl.SetFlags(CPropertyCtrl::F_NOBITMAPS);
		m_editParamCtrl.ExpandAll();
	}
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::PostNcDestroy()
{
}

void CHyperGraphView::OnDestroy()
{
	if (m_pGraph)
		m_pGraph->RemoveListener(this);
	__super::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphView::PreTranslateMessage(MSG* pMsg)
{
	if (!m_tooltip.m_hWnd)
	{
		CRect rc;
		GetClientRect(rc);
		m_tooltip.Create(this);
		m_tooltip.SetDelayTime(1000);
		m_tooltip.SetMaxTipWidth(600);
		m_tooltip.AddTool(this, "", rc, 1);
		m_tooltip.Activate(FALSE);
	}
	m_tooltip.RelayEvent(pMsg);

	BOOL bResult = __super::PreTranslateMessage(pMsg);
	return bResult;
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	HideEditPort();
	if (m_pQuickSearchNode)
		OnCancelSearch();

	float multiplier = 0.001f;

	BOOL bShift = (GetKeyState(VK_SHIFT) & 0x8000);
	if (bShift)
		multiplier *= 2.0f;

	// zoom around the mouse cursor position, first get old mouse pos
	GetCursorPos(&pt);
	ScreenToClient(&pt);
	Gdiplus::PointF oldZoomFocus = ViewToLocal(pt);

	// zoom
	SetZoom(m_zoom + m_zoom * multiplier * zDelta);

	// determine where the focus has moved as part of the zoom
	CPoint newZoomFocus = LocalToView(oldZoomFocus);

	// adjust offset based on displacement and update windows stuff
	m_scrollOffset -= (newZoomFocus - pt);
	SetScrollExtents(false);
	Invalidate();

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	//marcok: the following code handles the case when we've tried to fit the graph to view whilst not knowing the correct size of the client rect
	if (m_bRefitGraphInOnSize)
	{
		m_bRefitGraphInOnSize = false;
		OnCommandFitAll();
	}

	SetScrollExtents(false);
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphView::OnEraseBkgnd(CDC* pDC)
{
	/*
	   RECT rect;
	   CBrush cFillBrush;

	   // Get the rect of the client window
	   GetClientRect(&rect);

	   // Create the brush
	   cFillBrush.CreateSolidBrush(BACKGROUND_COLOR);

	   // Fill the entire client area
	   pDC->FillRect(&rect, &cFillBrush);
	 */
	return TRUE;
}

static bool LinesegRectSingle(Gdiplus::PointF* pWhere, const Lineseg& line1, float x1, float y1, float x2, float y2)
{
	Vec3 a(x1, y1, 0);
	Vec3 b(x2, y2, 0);
	Lineseg line2(a, b);
	float t1, t2;
	if (Intersect::Lineseg_Lineseg2D(line1, line2, t1, t2))
	{
		Vec3 pos = line1.start + t1 * (line1.end - line1.start);
		pWhere->X = pos.x;
		pWhere->Y = pos.y;
		return true;
	}
	return false;
}

static void LinesegRectIntersection(Gdiplus::PointF* pWhere, EHyperEdgeDirection* pDir, const Lineseg& line, const Gdiplus::RectF& rect)
{
	if (LinesegRectSingle(pWhere, line, rect.X, rect.Y, rect.X + rect.Width, rect.Y))
		*pDir = eHED_Up;
	else if (LinesegRectSingle(pWhere, line, rect.X, rect.Y, rect.X, rect.Y + rect.Height))
		*pDir = eHED_Left;
	else if (LinesegRectSingle(pWhere, line, rect.X + rect.Width, rect.Y, rect.X + rect.Width, rect.Y + rect.Height))
		*pDir = eHED_Right;
	else if (LinesegRectSingle(pWhere, line, rect.X, rect.Y + rect.Height, rect.X + rect.Width, rect.Y + rect.Height))
		*pDir = eHED_Down;
	else
	{
		*pDir = eHED_Down;
		pWhere->X = rect.X + rect.Width * 0.5f;
		pWhere->Y = rect.Y + rect.Height * 0.5f;
	}
}

void CHyperGraphView::ReroutePendingEdges()
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	std::vector<PendingEdges::iterator> eraseIters;

	for (PendingEdges::iterator iter = m_edgesToReroute.begin(); iter != m_edgesToReroute.end(); ++iter)
	{
		CHyperEdge* pEdge = *iter;
		//		if (pEdge->nodeOut == -1)
		//			DEBUG_BREAK;
		CHyperNode* pNodeOut = (CHyperNode*) m_pGraph->FindNode(pEdge->nodeOut);
		CHyperNode* pNodeIn = (CHyperNode*) m_pGraph->FindNode(pEdge->nodeIn);
		bool bDraggingEdge = std::find(m_draggingEdges.begin(), m_draggingEdges.end(), pEdge) != m_draggingEdges.end();
		if ((!pNodeIn && m_bDraggingFixedOutput && !bDraggingEdge) || (!pNodeOut && !m_bDraggingFixedOutput && !bDraggingEdge))
			continue;

		Gdiplus::RectF rectOut, rectIn;
		if (pNodeIn)
		{
			if (!pNodeIn->GetAttachRect(pEdge->nPortIn, &rectIn) && !pNodeIn->GetBlackBox())
				continue;
			rectIn.Offset(pNodeIn->GetPos());
		}
		else if (m_bDraggingFixedOutput && bDraggingEdge)
		{
			rectIn = Gdiplus::RectF(pEdge->pointIn.X, pEdge->pointIn.Y, 0, 0);
		}
		else
		{
			continue;
		}

		if (pNodeOut)
		{
			if (!pNodeOut->GetAttachRect(pEdge->nPortOut + 1000, &rectOut) && !pNodeOut->GetBlackBox())
				continue;
			rectOut.Offset(pNodeOut->GetPos());
		}
		else if (!m_bDraggingFixedOutput && bDraggingEdge)
		{
			rectOut = Gdiplus::RectF(pEdge->pointOut.X, pEdge->pointOut.Y, 0, 0);
		}
		else
		{
			continue;
		}

		if (rectOut.IsEmptyArea() && rectIn.IsEmptyArea())
		{
			rectOut.GetLocation(&pEdge->pointOut);
			rectIn.GetLocation(&pEdge->pointIn);
			// hack... but it works
			pEdge->dirOut = eHED_Right;
			pEdge->dirIn = eHED_Left;
		}
		else if (!rectOut.IsEmptyArea() && !rectIn.IsEmptyArea())
		{
			Vec3 centerOut(rectOut.X + rectOut.Width * 0.5f, rectOut.Y + rectOut.Height * 0.5f, 0.0f);
			Vec3 centerIn(rectIn.X + rectIn.Width * 0.5f, rectIn.Y + rectIn.Height * 0.5f, 0.0f);
			Vec3 dir = centerOut - centerIn;
			dir.Normalize();
			dir *= 10.0f;
			std::swap(dir.x, dir.y);
			dir.x = -dir.x;
			centerIn += dir;
			centerOut += dir;
			Lineseg centerLine(centerOut, centerIn);
			LinesegRectIntersection(&pEdge->pointOut, &pEdge->dirOut, centerLine, rectOut);
			LinesegRectIntersection(&pEdge->pointIn, &pEdge->dirIn, centerLine, rectIn);
		}
		else if (!rectOut.IsEmptyArea() && rectIn.IsEmptyArea())
		{
			Vec3 centerOut(rectOut.X + rectOut.Width * 0.5f, rectOut.Y + rectOut.Height * 0.5f, 0.0f);
			Vec3 centerIn(rectIn.X + rectIn.Width * 0.5f, rectIn.Y + rectIn.Height * 0.5f, 0.0f);
			Lineseg centerLine(centerOut, centerIn);
			LinesegRectIntersection(&pEdge->pointOut, &pEdge->dirOut, centerLine, rectOut);
			rectIn.GetLocation(&pEdge->pointIn);
			pEdge->dirIn = eHED_Left;
		}
		else if (rectOut.IsEmptyArea() && !rectIn.IsEmptyArea())
		{
			Vec3 centerOut(rectOut.X + rectOut.Width * 0.5f, rectOut.Y + rectOut.Height * 0.5f, 0.0f);
			Vec3 centerIn(rectIn.X + rectIn.Width * 0.5f, rectIn.Y + rectIn.Height * 0.5f, 0.0f);
			Lineseg centerLine(centerOut, centerIn);
			LinesegRectIntersection(&pEdge->pointIn, &pEdge->dirIn, centerLine, rectIn);
			rectOut.GetLocation(&pEdge->pointOut);
			pEdge->dirOut = eHED_Right;
		}
		else
		{
			assert(false);
		}

		eraseIters.push_back(iter);
	}
	while (!eraseIters.empty())
	{
		m_edgesToReroute.erase(eraseIters.back());
		eraseIters.pop_back();
	}
}

void CHyperGraphView::RerouteAllEdges()
{
	std::vector<CHyperEdge*> edges;
	m_pGraph->GetAllEdges(edges);
	std::copy(edges.begin(), edges.end(), std::inserter(m_edgesToReroute, m_edgesToReroute.end()));

	std::set<CHyperNode*> nodes;
	GraphicsHelper g(this);
	for (std::vector<CHyperEdge*>::const_iterator iter = edges.begin(); iter != edges.end(); ++iter)
	{
		CHyperNode* pNodeIn = (CHyperNode*) m_pGraph->FindNode((*iter)->nodeIn);
		CHyperNode* pNodeOut = (CHyperNode*) m_pGraph->FindNode((*iter)->nodeOut);
		if (pNodeIn && pNodeOut)
		{
			nodes.insert(pNodeIn);
			nodes.insert(pNodeOut);
		}
	}
	for (std::set<CHyperNode*>::const_iterator iter = nodes.begin(); iter != nodes.end(); ++iter)
	{
		(*iter)->Draw(this, *(Gdiplus::Graphics*)g, false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnPaint()
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	CPaintDC PaintDC(this); // device context for painting

	CRect rc = PaintDC.m_ps.rcPaint;
	Gdiplus::Bitmap backBufferBmp(rc.Width(), rc.Height());

	Gdiplus::Graphics mainGr(PaintDC.GetSafeHdc());

	Gdiplus::Graphics* pBackBufferGraphics = Gdiplus::Graphics::FromImage(&backBufferBmp);

	if (!pBackBufferGraphics)
	{
		return;
	}

	Gdiplus::Graphics& gr = *pBackBufferGraphics;

	gr.TranslateTransform(m_scrollOffset.x - rc.left, m_scrollOffset.y - rc.top);
	gr.ScaleTransform(m_zoom, m_zoom);
	gr.SetSmoothingMode(Gdiplus::SmoothingModeNone);
	gr.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

	CRect rect = PaintDC.m_ps.rcPaint;
	Gdiplus::RectF clip = ViewToLocalRect(rect);

	gr.Clear(BACKGROUND_COLOR);
	gr.SetSmoothingMode(Gdiplus::SmoothingModeNone);

	if (m_pGraph)
	{
		DrawGrid(gr, rect);
		ReroutePendingEdges();

		if (gFlowGraphGeneralPreferences.edgesOnTopOfNodes)
		{
			DrawNodes(gr, rect);
			gr.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
			DrawEdges(gr, rect);
		}
		else
		{
			gr.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
			DrawEdges(gr, rect);
			gr.SetSmoothingMode(Gdiplus::SmoothingModeNone);
			DrawNodes(gr, rect);
		}

		for (size_t i = 0; i < m_draggingEdges.size(); ++i)
		{
			DrawArrow(gr, m_draggingEdges[i], 0);
		}
	}

	if (m_bIsFrozen)
	{
		Gdiplus::SolidBrush fillBrush((BACKGROUND_COLOR.ToCOLORREF() & 0x00FFFFFF) | 0x80000000);
		gr.FillRectangle(&fillBrush, clip);
	}

	mainGr.DrawImage(&backBufferBmp, rc.left, rc.top, rc.Width(), rc.Height());

	delete pBackBufferGraphics;
}

//////////////////////////////////////////////////////////////////////////
bool CHyperGraphView::HitTestEdge(CPoint pnt, CHyperEdge*& pOutEdge, int& nIndex)
{
	if (!m_pGraph)
		return false;

	std::vector<CHyperEdge*> edges;
	if (m_pGraph->GetAllEdges(edges))
	{
		Gdiplus::PointF mousePt = ViewToLocal(pnt);

		for (int i = 0; i < edges.size(); i++)
		{
			CHyperEdge* pEdge = edges[i];

			for (int j = 0; j < 4; j++)
			{
				float dx = 0.0f;
				float dy = 0.0f;
				if (gFlowGraphGeneralPreferences.splineArrows && pEdge->cornerModified && (j == 0))
				{
					dx = pEdge->originalCenter.X - mousePt.X;
					dy = pEdge->originalCenter.Y - mousePt.Y;
				}
				else
				{
					dx = pEdge->cornerPoints[j].X - mousePt.X;
					dy = pEdge->cornerPoints[j].Y - mousePt.Y;
				}

				if ((dx * dx + dy * dy) < 7)
				{
					pOutEdge = pEdge;
					nIndex = j;
					return true;
				}
			}
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (!m_pGraph)
		return;

	HideEditPort();

	m_pHitEdge = 0;

	SetFocus();
	// Save the mouse down position
	m_mouseDownPos = point;

	if (m_bIsFrozen)
		return;

	bool bAltClick = CheckVirtualKey(VK_MENU);
	bool bCtrlClick = (nFlags & MK_CONTROL);
	bool bShiftClick = (nFlags & MK_SHIFT);

	Gdiplus::PointF mousePt = ViewToLocal(point);

	CHyperEdge* pHitEdge = 0;
	int nHitEdgePoint = 0;
	if (HitTestEdge(point, pHitEdge, nHitEdgePoint))
	{
		m_pHitEdge = pHitEdge;
		m_nHitEdgePoint = nHitEdgePoint;
		m_prevCornerW = m_pHitEdge->cornerW;
		m_prevCornerH = m_pHitEdge->cornerH;
		m_mode = EdgePointDragMode;
		CaptureMouse();
		return;
	}

	CHyperNode* pNode = GetMouseSensibleNodeAtPoint(point);
	if (pNode)
	{
		int objectClicked = pNode->GetObjectAt(GraphicsHelper(this), ViewToLocal(point));

		CHyperNodePort* pPort = NULL;
		static const int WAS_A_PORT = -10000;
		static const int WAS_A_SELECT = -10001;

		if (objectClicked >= eSOID_ShiftFirstOutputPort && objectClicked <= eSOID_ShiftLastOutputPort)
		{
			if (bShiftClick)
				objectClicked = eSOID_FirstOutputPort + (objectClicked - eSOID_ShiftFirstOutputPort);
			else if (bAltClick)
				objectClicked = eSOID_FirstOutputPort + (objectClicked - eSOID_ShiftFirstOutputPort);
			else
				objectClicked = eSOID_Background;
		}

		if ((bAltClick && (objectClicked < eSOID_FirstOutputPort && objectClicked > eSOID_LastOutputPort)) || bCtrlClick)
		{
			objectClicked = WAS_A_SELECT;
		}
		else if (objectClicked >= eSOID_FirstInputPort && objectClicked <= eSOID_LastInputPort && !pNode->GetInputs()->empty())
		{
			pPort = &pNode->GetInputs()->at(objectClicked - eSOID_FirstInputPort);
			objectClicked = WAS_A_PORT;
		}
		else if (objectClicked >= eSOID_FirstOutputPort && objectClicked <= eSOID_LastOutputPort && !pNode->GetOutputs()->empty())
		{
			pPort = &pNode->GetOutputs()->at(objectClicked - eSOID_FirstOutputPort);
			objectClicked = WAS_A_PORT;
		}
		else if (strcmp(pNode->GetClassName(), CBlackBoxNode::GetClassType()) == 0)
		{
			pPort = ((CBlackBoxNode*)(pNode))->GetPortAtPoint(ViewToLocal(point));
			if (pPort)
			{
				objectClicked = WAS_A_PORT;
				pNode = ((CBlackBoxNode*)(pNode))->GetNode(pPort);
			}
			else if (objectClicked == eSOID_Minimize)
			{
				((CBlackBoxNode*)(pNode))->Minimize();
				objectClicked = eSOID_Background;
			}
			else if (objectClicked == -1)
			{
				CBlackBoxNode* pBB = (CBlackBoxNode*)pNode;
				if (pBB->IsMinimized() == false)
				{
					std::vector<CHyperNode*>* nodes = pBB->GetNodes();
					for (int i = 0; i < nodes->size(); ++i)
					{
						int clicked = ((*nodes)[i])->GetObjectAt(GraphicsHelper(this), ViewToLocal(point));
						if (clicked > 0)
						{
							pNode = (*nodes)[i];
							objectClicked = clicked;
							break;
						}
					}
				}
			}
		}
		switch (objectClicked)
		{
		case eSOID_InputTransparent:
			break;
		case WAS_A_SELECT:
		case eSOID_AttachedEntity:
		case eSOID_Background:
		case eSOID_Title:
			{
				bool bUnselect = bCtrlClick && pNode->IsSelected();
				if (!pNode->IsSelected() || bUnselect)
				{
					SStartNodeOperationBatching startBatching(this);
					// Select this node if not selected yet.
					if (!bCtrlClick && !bUnselect)
						m_pGraph->UnselectAll();

					IHyperGraphEnumerator* pEnum = pNode->GetRelatedNodesEnumerator();
					for (IHyperNode* pRelative = pEnum->GetFirst(); pRelative; pRelative = pEnum->GetNext())
						static_cast<CHyperNode*>(pRelative)->SetSelected(!bUnselect);
					pEnum->Release();
					OnSelectionChange();
				}
				m_mode = MoveMode;
				m_graphVisuallyChangedHelper.Reset(false);
				GetIEditorImpl()->GetIUndoManager()->Begin();
			}
			break;
		case eSOID_InputGripper:
			ShowPortsConfigMenu(point, true, pNode);
			break;
		case eSOID_OutputGripper:
			ShowPortsConfigMenu(point, false, pNode);
			break;
		case eSOID_Minimize:
			OnToggleMinimizeNode(pNode);
			break;
		case eSOID_Border_UpRight:
		case eSOID_Border_Right:
		case eSOID_Border_DownRight:
		case eSOID_Border_Down:
		case eSOID_Border_DownLeft:
		case eSOID_Border_Left:
		case eSOID_Border_UpLeft:
		case eSOID_Border_Up:
			{
				if (pNode->GetResizeBorderRect())
				{
					m_mode = NodeBorderDragMode;
					m_DraggingBorderNodeInfo.m_pNode = pNode;
					m_DraggingBorderNodeInfo.m_border = objectClicked;
					m_DraggingBorderNodeInfo.m_origNodePos = pNode->GetPos();
					m_DraggingBorderNodeInfo.m_origBorderRect = *pNode->GetResizeBorderRect();
					m_DraggingBorderNodeInfo.m_clickPoint = mousePt;
				}
				break;
			}

		default:
			//assert(false);
			break;

		case WAS_A_PORT:

			if (!bAltClick && (!pPort->bInput || pPort->nConnected <= 1))
			{
				if (!pNode->IsSelected())
				{
					// Select this node if not selected yet.
					m_pGraph->UnselectAll();
					pNode->SetSelected(true);
					OnSelectionChange();
				}

				if (pPort->bInput && !pPort->nConnected && pPort->pVar->GetType() != IVariable::UNKNOWN && m_zoom > MIN_ZOOM_CAN_EDIT)
				{
					Gdiplus::RectF rect = pNode->GetRect();
					if (mousePt.X - rect.X > ARROW_WIDTH)
						m_pEditPort = pPort;
				}

				// Undo must start before edge delete.
				GetIEditorImpl()->GetIUndoManager()->Begin();
				pPort->bSelected = true;

				// If trying to drag output port, disconnect input parameter and drag it.
				if (pPort->bInput)
				{
					CHyperEdge* pEdge = m_pGraph->FindEdge(pNode, pPort);
					if (pEdge)
					{
						m_pGraph->RecordUndo();
						// Disconnect this edge.
						pNode = (CHyperNode*)m_pGraph->FindNode(pEdge->nodeOut);
						if (pNode)
						{
							pPort = &pNode->GetOutputs()->at(pEdge->nPortOut);
							InvalidateEdge(pEdge);
							// on input ports holding down shift has the effect of *cloning* rather than *moving* an edge.
							if (!bShiftClick)
							{
								m_pGraph->RemoveEdge(pEdge);
								m_bEdgeWasJustDisconnected = true;
							}
						}
						else
							pPort = 0;
						m_bDraggingFixedOutput = true;
					}
					else
					{
						m_bDraggingFixedOutput = false;
					}
				}
				else
				{
					// on output ports holding down shift has the effect of "fixing" the input and moving the output
					// of the edges originating from the clicked output port.
					m_bDraggingFixedOutput = !bShiftClick;
				}
				if (!pPort || !pNode)
				{
					GetIEditorImpl()->GetIUndoManager()->Cancel();
					return;
				}

				m_pSrcDragNode = pNode;
				m_mode = PortDragMode;
				m_draggingEdges.clear();

				// if shift pressed and we are editing an output port then batch move edges
				if (bShiftClick && !m_bDraggingFixedOutput)
				{
					std::vector<CHyperEdge*> edges;
					m_pGraph->FindEdges(pNode, edges);
					for (size_t i = 0; i < edges.size(); ++i)
					{
						CHyperEdge* edge = edges[i];
						if (edge->nPortOut != pPort->nPortIndex || edge->nodeOut != pNode->GetId())
							continue;
						// we create a new edge and remove the old one because the
						// HyperGraph::m_edgesMap needs to be updated as well
						_smart_ptr<CHyperEdge> pNewEdge = m_pGraph->CreateEdge();
						m_draggingEdges.push_back(pNewEdge);
						m_edgesToReroute.insert(pNewEdge);
						pNewEdge->nodeOut = -1;
						pNewEdge->nPortOut = -1;
						pNewEdge->pointOut = edge->pointOut;
						pNewEdge->nodeIn = edge->nodeIn;
						pNewEdge->nPortIn = edge->nPortIn;
						pNewEdge->pointIn = edge->pointIn;
						m_pGraph->RemoveEdge(edge);
					}
					pPort->bSelected = false;
				}
				else // if shift not pressed operate normally
				{
					_smart_ptr<CHyperEdge> pNewEdge = m_pGraph->CreateEdge();
					m_draggingEdges.push_back(pNewEdge);
					m_edgesToReroute.insert(pNewEdge);

					pNewEdge->nodeIn = -1;
					pNewEdge->nPortIn = -1;
					pNewEdge->nodeOut = -1;
					pNewEdge->nPortOut = -1;

					if (m_bDraggingFixedOutput)
					{
						pNewEdge->nodeOut = pNode->GetId();
						pNewEdge->nPortOut = pPort->nPortIndex;
						pNewEdge->pointIn = ViewToLocal(point);
					}
					else
					{
						pNewEdge->nodeIn = pNode->GetId();
						pNewEdge->nPortIn = pPort->nPortIndex;
						pNewEdge->pointOut = ViewToLocal(point);
					}
					pPort->bSelected = true;
				}
				InvalidateNode(pNode, true);
			}
			else
			{
				CMenu menu;
				menu.CreatePopupMenu();
				std::vector<CHyperEdge*> edges;
				m_pGraph->GetAllEdges(edges);
				for (size_t i = 0; i < edges.size(); ++i)
				{
					if (edges[i]->nodeIn == pNode->GetId() && 0 == stricmp(edges[i]->portIn, pPort->GetName()))
					{
						CString title;
						CHyperNode* pToNode = (CHyperNode*) m_pGraph->FindNode(edges[i]->nodeOut);
						title.Format("Remove %s:%s", pToNode->GetClassName(), edges[i]->portOut);
						menu.AppendMenu(MF_STRING, i + 1, title);
					}
				}
				CPoint pt;
				GetCursorPos(&pt);
				int nEdge = menu.TrackPopupMenuEx(TPM_RETURNCMD, pt.x, pt.y, this, NULL);
				if (nEdge)
				{
					nEdge--;
					m_pGraph->RemoveEdge(edges[nEdge]);
				}
			}
			break;
		}
	}
	else
	{
		if (!bAltClick && !bCtrlClick && m_pGraph->IsSelectedAny())
		{
			m_pGraph->UnselectAll();
			OnSelectionChange();
		}
		m_mode = SelectMode;
	}

	CaptureMouse();

	//CWnd::OnLButtonDown(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
bool CHyperGraphView::CheckVirtualKey(int virtualKey)
{
	GetAsyncKeyState(virtualKey);
	if (GetAsyncKeyState(virtualKey))
		return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (!m_pGraph)
		return;

	if (m_bIsFrozen)
		return;

	ReleaseCapture();

	bool bAltClick = CheckVirtualKey(VK_MENU);
	bool bCtrlClick = (nFlags & MK_CONTROL);
	bool bShiftClick = (nFlags & MK_SHIFT);

	switch (m_mode)
	{
	case SelectMode:
		{
			if (!m_rcSelect.IsRectEmpty())
			{
				bool bUnselect = bAltClick;
				if (!bAltClick && !bCtrlClick && m_pGraph->IsSelectedAny())
				{
					m_pGraph->UnselectAll();
				}

				SStartNodeOperationBatching startBatching(this);
				std::multimap<int, CHyperNode*> nodes;
				GetNodesInRect(m_rcSelect, nodes, true);
				for (std::multimap<int, CHyperNode*>::reverse_iterator iter = nodes.rbegin(); iter != nodes.rend(); ++iter)
				{
					CHyperNode* pNode = iter->second;
					IHyperGraphEnumerator* pEnum = pNode->GetRelatedNodesEnumerator();
					for (IHyperNode* pRelative = pEnum->GetFirst(); pRelative; pRelative = pEnum->GetNext())
					{
						static_cast<CHyperNode*>(pRelative)->SetSelected(!bUnselect);
					}
				}
				OnSelectionChange();
				Invalidate();
			}
		}
		break;
	case MoveMode:
		{
			m_graphVisuallyChangedHelper.Reset(false);
			GetIEditorImpl()->GetIUndoManager()->Accept("Move Graph Node(s)");
			SetScrollExtents();
			Invalidate();
			std::vector<CHyperNode*> apNodes;
			GetSelectedNodes(apNodes);
			SStartNodeOperationBatching startBatching(this);
			for (int i = 0, iNodeCount(apNodes.size()); i < iNodeCount; ++i)
				m_pGraph->SendNotifyEvent(apNodes[i], EHG_NODE_MOVE);
		}
		break;
	case EdgePointDragMode:
		{

		}
		break;
	case NodeBorderDragMode:
		{
			if (m_pGraph && m_DraggingBorderNodeInfo.m_pNode)
			{
				m_graphVisuallyChangedHelper.NodeResized();
				SStartNodeOperationBatching startBatching(this);
				m_pGraph->SendNotifyEvent(m_DraggingBorderNodeInfo.m_pNode, EHG_NODE_RESIZE);
			}

			m_DraggingBorderNodeInfo.m_pNode = 0;
		}
		break;
	case PortDragMode:
		{
			for (size_t i = 0; i < m_draggingEdges.size(); ++i)
			{
				if (CHyperNode* pNode = (CHyperNode*) m_pGraph->FindNode(m_draggingEdges[i]->nodeIn))
				{
					pNode->UnselectAllPorts();
				}
				if (CHyperNode* pNode = (CHyperNode*) m_pGraph->FindNode(m_draggingEdges[i]->nodeOut))
				{
					pNode->UnselectAllPorts();
				}
			}

			// unselect the port of the node from which the edges where dragged away
			if (m_pSrcDragNode)
			{
				m_pSrcDragNode->UnselectAllPorts();
				m_pSrcDragNode = 0;
			}

			CHyperNode* pTrgNode = GetNodeAtPoint(point);
			if (pTrgNode)
			{
				int objectClicked = pTrgNode->GetObjectAt(GraphicsHelper(this), ViewToLocal(point));
				if (objectClicked >= eSOID_ShiftFirstOutputPort && objectClicked <= eSOID_ShiftLastOutputPort)
				{
					if (bShiftClick || bAltClick)
						objectClicked = eSOID_FirstInputPort + (objectClicked - eSOID_ShiftFirstOutputPort);
					else
						objectClicked = eSOID_Background;
				}
				CHyperNodePort* pTrgPort = NULL;
				if (objectClicked >= eSOID_FirstInputPort && objectClicked <= eSOID_LastInputPort && !pTrgNode->GetInputs()->empty())
					pTrgPort = &pTrgNode->GetInputs()->at(objectClicked - eSOID_FirstInputPort);
				else if (objectClicked >= eSOID_FirstOutputPort && objectClicked <= eSOID_LastOutputPort && !pTrgNode->GetOutputs()->empty())
					pTrgPort = &pTrgNode->GetOutputs()->at(objectClicked - eSOID_FirstOutputPort);
				else if (strcmp(pTrgNode->GetClassName(), CBlackBoxNode::GetClassType()) == 0)
				{
					pTrgPort = ((CBlackBoxNode*)(pTrgNode))->GetPortAtPoint(ViewToLocal(point));
					if (pTrgPort)
						pTrgNode = ((CBlackBoxNode*)(pTrgNode))->GetNode(pTrgPort);
				}

				if (pTrgPort)
				{
					CHyperNode* pOutNode = 0, * pInNode = 0;
					CHyperNodePort* pOutPort = 0, * pInPort = 0;
					for (size_t i = 0; i < m_draggingEdges.size(); ++i)
					{
						if (m_bDraggingFixedOutput)
						{
							pOutNode = (CHyperNode*) m_pGraph->FindNode(m_draggingEdges[i]->nodeOut);
							pOutPort = &pOutNode->GetOutputs()->at(m_draggingEdges[i]->nPortOut);
							pInNode = pTrgNode;
							pInPort = pTrgPort;
						}
						else
						{
							pOutNode = pTrgNode;
							pOutPort = pTrgPort;
							pInNode = (CHyperNode*) m_pGraph->FindNode(m_draggingEdges[i]->nodeIn);
							pInPort = &pInNode->GetInputs()->at(m_draggingEdges[i]->nPortIn);
						}

						CHyperEdge* pExistingEdge = NULL;
						if (m_pGraph->CanConnectPorts(pOutNode, pOutPort, pInNode, pInPort, pExistingEdge))
						{
							// Connect
							m_pGraph->RecordUndo();
							m_pGraph->ConnectPorts(pOutNode, pOutPort, pInNode, pInPort);
							InvalidateNode(pOutNode, true);
							InvalidateNode(pInNode, true);
							OnNodesConnected();
						}
						else
						{
							if (pExistingEdge && pExistingEdge->enabled == false)
								m_pGraph->EnableEdge(pExistingEdge, true);
						}
					}
				}
			}
			else
			{
				// dragged the edge into nothing -> present new node popup
				if (m_bDraggingFixedOutput && !m_bEdgeWasJustDisconnected)
				{
					GetIEditorImpl()->GetIUndoManager()->Accept("Connect Graph Node");

					m_pQuickSearchNode = CreateNode(CQuickSearchNode::GetClassType(), point);
					if (m_pQuickSearchNode)
					{
						m_mode = DragQuickSearchMode;
						QuickSearchNode(m_pQuickSearchNode);
					}
				}
				m_bEdgeWasJustDisconnected = false;
			}

			Invalidate();

			if (m_mode != DragQuickSearchMode)
			{
				GetIEditorImpl()->GetIUndoManager()->Accept("Connect Graph Node");
				m_draggingEdges.clear();
			}
		}
		break;
	}
	m_rcSelect.SetRect(0, 0, 0, 0);

	if (m_mode == ScrollModeDrag)
	{
		Invalidate();
		GetIEditorImpl()->GetIUndoManager()->Accept("Connect Graph Node");
		m_draggingEdges.clear();
		m_mode = ScrollMode;
	}
	else if (m_mode != DragQuickSearchMode)
	{
		m_mode = NothingMode;
	}
	//CWnd::OnLButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnRButtonDown(UINT nFlags, CPoint point)
{
	if (!m_pGraph)
		return;

	HideEditPort();

	CHyperEdge* pHitEdge = 0;
	int nHitEdgePoint = 0;
	if (!m_bIsFrozen && HitTestEdge(point, pHitEdge, nHitEdgePoint))
	{
		CMenu menu;
		menu.CreatePopupMenu();
		menu.AppendMenu(MF_STRING, 1, _T("Remove"));
		if (pHitEdge->IsEditable())
			menu.AppendMenu(MF_STRING, 2, _T("Edit"));
		if (m_pGraph->IsFlowGraph())
		{
			menu.AppendMenu(MF_STRING, 3, pHitEdge->enabled ? _T("Disable") : _T("Enable"));
			menu.AppendMenu(MF_SEPARATOR);
			menu.AppendMenu(MF_STRING, 5, _T("Time:Delay"));
			menu.AppendMenu(MF_STRING, 6, _T("Logic:Any"));
		}

		// makes only sense for non-AIActions and while fg system is updated
		if (m_pGraph->IsFlowGraph() && m_pGraph->GetAIAction() == 0 && GetIEditorImpl()->GetGameEngine()->IsFlowSystemUpdateEnabled())
			menu.AppendMenu(MF_STRING, 4, _T("Simulate flow [Double click]"));

		CPoint pt;
		GetCursorPos(&pt);
		switch (menu.TrackPopupMenuEx(TPM_RETURNCMD, pt.x, pt.y, this, NULL))
		{
		case 1:
			{
				CUndo undo("Remove Graph Edge");
				m_pGraph->RecordUndo();
				m_pGraph->RemoveEdge(pHitEdge);
				OnEdgeRemoved();
			}
			break;
		case 2:
			{
				CUndo undo("Edit Graph Edge");
				m_pGraph->RecordUndo();
				m_pGraph->EditEdge(pHitEdge);
			}
			break;
		case 3:
			{
				CUndo undo(pHitEdge->enabled ? "Disable Edge" : "Enable Edge");
				m_pGraph->RecordUndo();
				m_pGraph->EnableEdge(pHitEdge, !pHitEdge->enabled);
				InvalidateEdge(pHitEdge);
			}
			break;
		case 4:
			SimulateFlow(pHitEdge);
			break;
		case 5:
			{
				CUndo undo("Add Time:Delay");
				CHyperNode* pDelayNode = CreateNode("Time:Delay", point);

				Gdiplus::RectF rect = pDelayNode->GetRect();
				Gdiplus::PointF adjustedPoint;

				adjustedPoint.X = rect.X - (rect.Width / 2.0f);
				adjustedPoint.Y = rect.Y;

				pDelayNode->SetPos(adjustedPoint);

				CHyperNodePort* pDelayOutputPort = &pDelayNode->GetOutputs()->at(0);
				CHyperNodePort* pDelayInputPort = &pDelayNode->GetInputs()->at(0);

				CHyperNode* nodeIn = static_cast<CHyperNode*>(m_pGraph->FindNode(pHitEdge->nodeIn));
				CHyperNode* nodeOut = static_cast<CHyperNode*>(m_pGraph->FindNode(pHitEdge->nodeOut));

				if (pDelayInputPort && pDelayOutputPort && nodeIn && nodeOut)
				{
					m_pGraph->ConnectPorts(nodeOut, nodeOut->FindPort(pHitEdge->portOut, false), pDelayNode, pDelayInputPort);
					m_pGraph->ConnectPorts(pDelayNode, pDelayOutputPort, nodeIn, nodeIn->FindPort(pHitEdge->portIn, true));
				}
			}
			break;
		case 6:
			{
				CUndo undo("Add Logic:Any");
				CHyperNode* pLogicAnyNode = CreateNode("Logic:Any", point);

				Gdiplus::RectF rect = pLogicAnyNode->GetRect();
				Gdiplus::PointF adjustedPoint;

				adjustedPoint.X = rect.X - (rect.Width / 2.0f);
				adjustedPoint.Y = rect.Y;

				pLogicAnyNode->SetPos(adjustedPoint);

				CHyperNodePort* pLogicAnyOutputPort = &pLogicAnyNode->GetOutputs()->at(0);
				CHyperNodePort* pLogicAnyInputPort = &pLogicAnyNode->GetInputs()->at(0);

				CHyperNode* nodeIn = static_cast<CHyperNode*>(m_pGraph->FindNode(pHitEdge->nodeIn));
				CHyperNode* nodeOut = static_cast<CHyperNode*>(m_pGraph->FindNode(pHitEdge->nodeOut));

				if (pLogicAnyInputPort && pLogicAnyOutputPort && nodeIn && nodeOut)
				{
					m_pGraph->ConnectPorts(nodeOut, nodeOut->FindPort(pHitEdge->portOut, false), pLogicAnyNode, pLogicAnyInputPort);
					m_pGraph->ConnectPorts(pLogicAnyNode, pLogicAnyOutputPort, nodeIn, nodeIn->FindPort(pHitEdge->portIn, true));
				}
			}
			break;
		}
		return;
	}

	SetFocus();
	if (m_mode == PortDragMode)
	{
		m_mode = ScrollModeDrag;
	}
	else
	{
		if (m_mode == MoveMode)
			GetIEditorImpl()->GetIUndoManager()->Accept("Move Graph Node(s)");

		m_mode = ScrollMode;
	}
	m_RButtonDownPos = point;
	m_mouseDownPos = point;
	SetCapture();

	__super::OnRButtonDown(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnRButtonUp(UINT nFlags, CPoint point)
{
	if (!m_pGraph)
		return;

	ReleaseCapture();

	// make sure to reset the mode, otherwise we can end up in scrollmode while the context menu is open
	if (m_mode == ScrollModeDrag)
	{
		m_mode = PortDragMode;
	}
	else
	{
		m_mode = NothingMode;
	}

	if (m_bIsFrozen)
		return;

	if (abs(m_RButtonDownPos.x - point.x) < 3 && abs(m_RButtonDownPos.y - point.y) < 3)
	{
		// Show context menu if right clicked on the same point.
		CHyperNode* pNode = GetMouseSensibleNodeAtPoint(point);
		ShowContextMenu(point, pNode);
		return;
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnMButtonDown(UINT nFlags, CPoint point)
{
	__super::OnMButtonDown(nFlags, point);

	if (!m_pGraph)
		return;

	HideEditPort();
	SetFocus();
	if (m_mode == PortDragMode)
	{
		m_mode = ScrollModeDrag;
	}
	else
	{
		m_mode = ScrollMode;
	}
	m_RButtonDownPos = point;
	m_mouseDownPos = point;
	SetCapture();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnMButtonUp(UINT nFlags, CPoint point)
{
	__super::OnMButtonDown(nFlags, point);

	if (!m_pGraph)
		return;

	ReleaseCapture();
	if (m_mode == ScrollModeDrag)
	{
		m_mode = PortDragMode;
	}
	else
	{
		m_mode = NothingMode;
	}

	if ((GetKeyState(VK_CONTROL) & 0x80) && m_pGraph->IsFlowGraph())
	{
		CFlowNode* pNode = static_cast<CFlowNode*>(GetMouseSensibleNodeAtPoint(point));
		if (pNode)
		{
			CHyperNodePort* pPort = pNode->GetPortAtPoint(GraphicsHelper(this), ViewToLocal(point));

			if (pPort)
			{
				CFlowGraphDebuggerEditor* pFlowgraphDebugger = GetIEditorImpl()->GetFlowGraphDebuggerEditor();

				if (pFlowgraphDebugger)
				{
					const bool hasBreakPoint = pFlowgraphDebugger->HasBreakpoint(pNode, pPort);
					const bool invalidateNode = hasBreakPoint || (pPort->nConnected || !pPort->bInput);

					if (hasBreakPoint)
					{
						pFlowgraphDebugger->RemoveBreakpoint(pNode, pPort);
					}
					// if the input is not connected at all with an edge, don't add a breakpoint
					else if (pPort->nConnected || !pPort->bInput)
					{
						pFlowgraphDebugger->AddBreakpoint(pNode, pPort);
					}

					// redraw the node to update the "visual" breakpoint, otherwise you have to move the mouse first
					// to trigger a redraw
					if (invalidateNode)
						InvalidateNode(pNode, true, false);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnMouseMove(UINT nFlags, CPoint point)
{
	if (!m_pGraph)
		return;

	if (m_pMouseOverNode)
		m_pMouseOverNode->UnselectAllPorts();

	m_pMouseOverNode = NULL;
	m_pMouseOverPort = NULL;

	switch (m_mode)
	{
	case MoveMode:
		{
			GetIEditorImpl()->GetIUndoManager()->Restore(false);
			CPoint offset = point - m_mouseDownPos;
			MoveSelectedNodes(offset);
		}
		break;
	case ScrollMode:
		OnMouseMoveScrollMode(nFlags, point);
		break;
	case SelectMode:
		{
			// Update select rectangle.
			CRect rc(m_mouseDownPos.x, m_mouseDownPos.y, point.x, point.y);
			rc.NormalizeRect();
			CRect rcClient;
			GetClientRect(rcClient);
			rc.IntersectRect(rc, rcClient);

			CDC* dc = GetDC();
			dc->DrawDragRect(rc, CSize(1, 1), m_rcSelect, CSize(1, 1));
			ReleaseDC(dc);
			m_rcSelect = rc;
		}
		break;
	case PortDragMode:
		OnMouseMovePortDragMode(nFlags, point);
		break;
	case NodeBorderDragMode:
		OnMouseMoveBorderDragMode(nFlags, point);
		break;
	case EdgePointDragMode:
		if (m_pHitEdge)
		{
			if (gFlowGraphGeneralPreferences.splineArrows)
			{
				Gdiplus::PointF p1 = ViewToLocal(point);
				m_pHitEdge->cornerPoints[0] = p1;
				m_pHitEdge->cornerModified = true;
			}
			else
			{
				Gdiplus::PointF p1 = ViewToLocal(point);
				Gdiplus::PointF p2 = ViewToLocal(m_mouseDownPos);
				if (m_nHitEdgePoint < 2)
				{
					float w = p1.X - p2.X;
					m_pHitEdge->cornerW = m_prevCornerW + w;
					m_pHitEdge->cornerModified = true;
				}
				else
				{
					float h = p1.Y - p2.Y;
					m_pHitEdge->cornerH = m_prevCornerH + h;
					m_pHitEdge->cornerModified = true;
				}
			}
			Invalidate();
		}
		break;
	case ScrollModeDrag:
		OnMouseMoveScrollMode(nFlags, point);
		OnMouseMovePortDragMode(nFlags, point);
		break;
	default:
		{
			CHyperNodePort* pPort = NULL;
			CHyperNode* pNode = GetNodeAtPoint(point);

			if (pNode)
			{
				m_pMouseOverNode = pNode;
				pPort = pNode->GetPortAtPoint(GraphicsHelper(this), ViewToLocal(point));

				if (pPort)
				{
					pPort->bSelected = true;
				}

				m_pMouseOverPort = pPort;
			}

			if (prevMovePoint != point)
			{
				UpdateTooltip(pNode, pPort);
				prevMovePoint = point;
			}
		}
	}

	__super::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnMouseMoveScrollMode(UINT nFlags, CPoint point)
{
	if (!((abs(m_RButtonDownPos.x - point.x) < 3 && abs(m_RButtonDownPos.y - point.y) < 3)))
	{
		CPoint offset = point - m_mouseDownPos;
		m_scrollOffset += offset;
		m_mouseDownPos = point;
		SetScrollExtents(false);
		Invalidate(FALSE);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnMouseMoveBorderDragMode(UINT nFlags, CPoint point)
{
	if (m_DraggingBorderNodeInfo.m_pNode)
	{
		Gdiplus::PointF mousePt = ViewToLocal(point);
		Gdiplus::PointF moved = mousePt - m_DraggingBorderNodeInfo.m_clickPoint;

		if (moved.Y == 0 && moved.X == 0)
			return;

		bool bUp = false;
		bool bDown = false;
		bool bLeft = false;
		bool bRight = false;

		Gdiplus::PointF nodePos = m_DraggingBorderNodeInfo.m_origNodePos;
		Gdiplus::RectF borderRect = m_DraggingBorderNodeInfo.m_origBorderRect;

		switch (m_DraggingBorderNodeInfo.m_border)
		{
		case eSOID_Border_UpRight:
			bUp = true;
			bRight = true;
			break;

		case eSOID_Border_Right:
			bRight = true;
			break;

		case eSOID_Border_DownRight:
			bDown = true;
			bRight = true;
			break;

		case eSOID_Border_Down:
			bDown = true;
			break;

		case eSOID_Border_DownLeft:
			bDown = true;
			bLeft = true;
			break;

		case eSOID_Border_Left:
			bLeft = true;
			break;

		case eSOID_Border_UpLeft:
			bUp = true;
			bLeft = true;
			break;

		case eSOID_Border_Up:
			bUp = true;
			break;
		}

		if (bUp)
		{
			float inc = -moved.Y;
			nodePos.Y -= inc;
			borderRect.Height += inc;
		}

		if (bDown)
		{
			float inc = moved.Y;
			borderRect.Height += inc;
		}

		if (bRight)
		{
			float inc = moved.X;
			borderRect.Width += inc;
		}

		if (bLeft)
		{
			float inc = -moved.X;
			nodePos.X -= inc;
			borderRect.Width += inc;
		}

		if (borderRect.Width < 10 || borderRect.Height < 10)
			return;

		if (bLeft || bUp)
			m_DraggingBorderNodeInfo.m_pNode->SetPos(nodePos);
		m_DraggingBorderNodeInfo.m_pNode->SetResizeBorderRect(borderRect);
		m_DraggingBorderNodeInfo.m_pNode->Invalidate(true, false);
		ForceRedraw();
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnMouseMovePortDragMode(UINT nFlags, CPoint point)
{
	CHyperNode* pNode = GetNodeAtPoint(point);

	for (size_t i = 0; i < m_draggingEdges.size(); ++i)
	{
		InvalidateEdgeRect(LocalToView(m_draggingEdges[i]->pointOut), LocalToView(m_draggingEdges[i]->pointIn));

		/*
		   if (CHyperNode * pNode = (CHyperNode*) m_pGraph->FindNode( m_pDraggingEdge->nodeIn ))
		   {
		   pNode->GetInputs()->at(m_pDraggingEdge->nPortIn).bSelected = false;
		   InvalidateNode( pNode );
		   }
		   if (CHyperNode * pNode = (CHyperNode*) m_pGraph->FindNode( m_pDraggingEdge->nodeOut ))
		   InvalidateNode( pNode );
		 */

		CHyperNode* pNodeIn = (CHyperNode*) m_pGraph->FindNode(m_draggingEdges[i]->nodeIn);
		CHyperNode* pNodeOut = (CHyperNode*) m_pGraph->FindNode(m_draggingEdges[i]->nodeOut);
		if (pNodeIn)
		{
			CHyperNode::Ports* ports = pNodeIn->GetInputs();
			if (ports->size() > m_draggingEdges[i]->nPortIn)
				ports->at(m_draggingEdges[i]->nPortIn).bSelected = false;
		}

		if (m_bDraggingFixedOutput)
		{
			m_draggingEdges[i]->pointIn = ViewToLocal(point);
			m_draggingEdges[i]->nodeIn = -1;
		}
		else
		{
			m_draggingEdges[i]->pointOut = ViewToLocal(point);
			m_draggingEdges[i]->nodeOut = -1;
		}

		if (pNode)
		{
			if (CHyperNodePort* pPort = pNode->GetPortAtPoint(GraphicsHelper(this), ViewToLocal(point)))
			{
				if (m_bDraggingFixedOutput && pPort->bInput)
				{
					m_draggingEdges[i]->nodeIn = pNode->GetId();
					m_draggingEdges[i]->nPortIn = pPort->nPortIndex;
					pNode->GetInputs()->at(m_draggingEdges[i]->nPortIn).bSelected = true;
				}
				else if (!m_bDraggingFixedOutput && !pPort->bInput)
				{
					pNode->UnselectAllPorts();
					m_draggingEdges[i]->nodeOut = pNode->GetId();
					m_draggingEdges[i]->nPortOut = pPort->nPortIndex;
					pNode->GetOutputs()->at(m_draggingEdges[i]->nPortOut).bSelected = true;
				}
			}
		}
		m_edgesToReroute.insert(m_draggingEdges[i]);

		if (pNode)
			InvalidateNode(pNode, true);
		if (pNodeIn)
			InvalidateNode(pNodeIn, true);
		if (pNodeOut)
		{
			if (m_draggingEdges[i]->nPortOut >= 0 && m_draggingEdges[i]->nPortOut < pNodeOut->GetOutputs()->size())
				pNodeOut->GetOutputs()->at(m_draggingEdges[i]->nPortOut).bSelected = true;
			InvalidateNode(pNodeOut, true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::UpdateTooltip(CHyperNode* pNode, CHyperNodePort* pPort)
{
	if (m_tooltip.m_hWnd)
	{
		if (pNode && pNode->IsTooltipShowable() && gFlowGraphGeneralPreferences.showToolTip)
		{
			string tip;
			if (pPort) // Port tooltip.
			{
				string type;
				switch (pPort->pVar->GetType())
				{
				case IVariable::INT:
					type = "Integer";
					break;
				case IVariable::FLOAT:
					type = "Float";
					break;
				case IVariable::BOOL:
					type = "Boolean";
					break;
				case IVariable::VECTOR:
					type = "Vector";
					break;
				case IVariable::STRING:
					type = "String";
					break;
				//case IVariable::VOID: type = "Void"; break;
				default:
					type = "Any";
					break;
				}
				const char* desc = pPort->pVar->GetDescription();
				if (desc && *desc)
					tip.Format("[%s] %s", (const char*)type, desc);
				else
					tip.Format("[%s] %s", (const char*)type, (const char*)pNode->GetDescription());
			}
			else // Node tooltip.
			{
				tip.Format("%s\n%s", pNode->GetInfoAsString(), pNode->GetDescription());
			}
			m_tooltip.UpdateTipText(tip, this, 1);
			m_tooltip.Activate(TRUE);
		}
		else
		{
			m_tooltip.Activate(FALSE);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	__super::OnLButtonDblClk(nFlags, point);

	if (m_pGraph == 0)
		return;

	if (m_bIsFrozen)
		return;

	CHyperEdge * pHitEdge = 0;
	int nHitEdgePoint = 0;
	if (HitTestEdge(point, pHitEdge, nHitEdgePoint))
	{
		// makes only sense for non-AIActions and while fg system is updated
		if (m_pGraph->IsFlowGraph() && m_pGraph->GetAIAction() == 0 && GetIEditorImpl()->GetGameEngine()->IsFlowSystemUpdateEnabled())
		{
			SimulateFlow(pHitEdge);
		}
	}
	else
	{
		CHyperNode* pNode = GetMouseSensibleNodeAtPoint(point);
		const string nodeClassName = pNode ? pNode->GetClassName() : "";

		if (pNode && pNode->IsEditorSpecialNode())
		{
			if(!nodeClassName.compare("QuickSearch"))
				QuickSearchNode(pNode);
			else
				RenameNode(pNode);
		}
		else
		{
			if (m_pEditPort)
				ShowEditPort(pNode, m_pEditPort);
			else
			{
				if (m_pGraph->IsFlowGraph())
					OnSelectEntity();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	bool processed = true;
	switch (nChar)
	{
	case VK_ADD:
		SetZoom(m_zoom + 0.3f);
		InvalidateView();
		break;
	case VK_SUBTRACT:
		SetZoom(m_zoom - 0.3f);
		InvalidateView();
		break;
	case 'Q':
		{
			if (m_pGraph && m_pGraph->IsFlowGraph())
			{
				if (!m_pQuickSearchNode)
				{
					CPoint point;
					GetCursorPos(&point);
					ScreenToClient(&point);
					m_pQuickSearchNode = CreateNode(CQuickSearchNode::GetClassType(), point);
					if (m_pQuickSearchNode)
						QuickSearchNode(m_pQuickSearchNode);
				}

				InvalidateView();
			}
		}
		break;
	default:
		__super::OnKeyDown(nChar, nRepCnt, nFlags);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	bool processed = false;
	if (nChar == 'R')
	{
		processed = true;
		ForceRedraw();
	}
	if (!processed)
		__super::OnKeyUp(nChar, nRepCnt, nFlags);
}

//////////////////////////////////////////////////////////////////////////perforce
BOOL CHyperGraphView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	if (m_mode == NothingMode)
	{
		CPoint point;
		GetCursorPos(&point);
		ScreenToClient(&point);

		CHyperEdge* pHitEdge = 0;
		int nHitEdgePoint = 0;
		if (HitTestEdge(point, pHitEdge, nHitEdgePoint))
		{
			HCURSOR hCursor;
			hCursor = AfxGetApp()->LoadCursor(IDC_ARRWHITE);
			SetCursor(hCursor);
			return TRUE;
		}

		CHyperNode* pNode = GetNodeAtPoint(point);
		if (pNode)
		{
			int object = pNode->GetObjectAt(GraphicsHelper(this), ViewToLocal(point));
			LPCTSTR resource = NULL;
			switch (object)
			{
			case eSOID_Border_UpRight:
			case eSOID_Border_DownLeft:
				resource = IDC_SIZENESW;
				break;

			case eSOID_Border_Right:
			case eSOID_Border_Left:
				resource = IDC_SIZEWE;
				break;

			case eSOID_Border_DownRight:
			case eSOID_Border_UpLeft:
				resource = IDC_SIZENWSE;
				break;

			case eSOID_Border_Up:
			case eSOID_Border_Down:
				resource = IDC_SIZENS;
				break;
			}
			if (resource)
			{
				HCURSOR hCursor;
				hCursor = AfxGetApp()->LoadStandardCursor(resource);
				SetCursor(hCursor);
				return TRUE;
			}
		}
	}

	return __super::OnSetCursor(pWnd, nHitTest, message);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::InvalidateEdgeRect(CPoint p1, CPoint p2)
{
	CRect rc(p1, p2);
	rc.NormalizeRect();
	rc.InflateRect(20, 7, 20, 7);
	InvalidateRect(rc);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::InvalidateEdge(CHyperEdge* pEdge)
{
	m_edgesToReroute.insert(pEdge);
	InvalidateEdgeRect(LocalToView(pEdge->pointIn), LocalToView(pEdge->pointOut));
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::InvalidateNode(CHyperNode* pNode, bool bRedraw, bool bIsModified /*=true*/)
{
	assert(pNode);
	if (!m_pGraph)
		return;

	CDC* pDC = GetDC();
	if (pDC)
	{
		if (pNode->IsEditorSpecialNode())
			InvalidateRect(LocalToViewRect(pNode->GetRect()), FALSE);
		Gdiplus::Graphics gr(pDC->GetSafeHdc());
		Gdiplus::RectF rc = pNode->GetRect();
		Gdiplus::SizeF sz = pNode->CalculateSize(gr);
		rc.Width = sz.Width;
		rc.Height = sz.Height;
		ReleaseDC(pDC);

		pNode->SetRect(rc);
		rc.Inflate(GRID_SIZE, GRID_SIZE);
		InvalidateRect(LocalToViewRect(rc), FALSE);

		if (bRedraw)
			pNode->Invalidate(true, bIsModified);

		// Invalidate all edges connected to this node.
		std::vector<CHyperEdge*> edges;
		if (m_pGraph->FindEdges(pNode, edges))
		{
			// Invalidate all node edges.
			for (int i = 0; i < edges.size(); i++)
			{
				InvalidateEdge(edges[i]);
			}
		}
	}
	else
	{
		assert(false);
	}
}

void CHyperGraphView::UpdateDebugCount(CHyperNode* pNode)
{
	if (!m_pGraph || !pNode)
		return;

	std::vector<CHyperEdge*> edges;
	int count = pNode->GetDebugCount();

	if (m_pGraph->FindEdges(pNode, edges))
	{
		for (int i = 0; i < edges.size(); ++i)
		{
			CHyperNode* nodeIn = static_cast<CHyperNode*>(m_pGraph->FindNode(edges[i]->nodeIn));
			if (!nodeIn)
				continue;

			CHyperNodePort* portIn = nodeIn->FindPort(edges[i]->portIn, true);
			if (!portIn)
				continue;

			if (nodeIn->IsDebugPortActivated(portIn))
			{
				edges[i]->isInInitializationPhase = false;
				nodeIn->GetAdditionalDebugPortInformation(*portIn, edges[i]->isInInitializationPhase);
				if (edges[i]->isInInitializationPhase)
					edges[i]->debugCountInitialization++;
				edges[i]->debugCount++;
				nodeIn->ResetDebugPortActivation(portIn);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::DrawEdges(Gdiplus::Graphics& gr, const CRect& rc)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	std::vector<CHyperEdge*> edges;
	m_pGraph->GetAllEdges(edges);

	CHyperEdge* pHitEdge = 0;
	int nHitEdgePoint = 0;

	CPoint cursorPos;
	GetCursorPos(&cursorPos);
	ScreenToClient(&cursorPos);
	HitTestEdge(cursorPos, pHitEdge, nHitEdgePoint);

	for (std::vector<CHyperEdge*>::const_iterator it = edges.begin(), end = edges.end(); it != end; ++it)
	{
		CHyperEdge* pEdge = *it;

		CHyperNode* nodeIn = (CHyperNode*)m_pGraph->FindNode(pEdge->nodeIn);
		CHyperNode* nodeOut = (CHyperNode*)m_pGraph->FindNode(pEdge->nodeOut);
		if (!nodeIn || !nodeOut)
			continue;
		CHyperNodePort* portIn = nodeIn->FindPort(pEdge->portIn, true);
		CHyperNodePort* portOut = nodeOut->FindPort(pEdge->portOut, false);
		if (!portIn || !portOut)
			continue;

		// Draw arrow.
		int selection = 0;
		selection |= (int)(gFlowGraphGeneralPreferences.highlightEdges && nodeIn->IsSelected());
		selection |= (int)(gFlowGraphGeneralPreferences.highlightEdges && nodeOut->IsSelected()) << 1;
		if (selection == 0)
			selection = (nodeIn->IsPortActivationModified(portIn)) ? 4 : 0;

		if (m_pMouseOverPort == portIn || m_pMouseOverPort == portOut || pHitEdge == pEdge)
		{
			selection = 8;
		}

		// Check for custom selection mode here
		int customSelMode = pEdge->GetCustomSelectionMode();
		if (selection == 0 && customSelMode != -1)
			selection = customSelMode;

		if (nodeIn->GetBlackBox() || nodeOut->GetBlackBox())
		{
			if (nodeIn->GetBlackBox() && nodeOut->GetBlackBox())
				continue;

			if (nodeIn->GetBlackBox())
			{
				pEdge->pointIn = ((CBlackBoxNode*)(nodeIn->GetBlackBox()))->GetPointForPort(portIn);
			}
			else
			{
				pEdge->pointOut = ((CBlackBoxNode*)(nodeOut->GetBlackBox()))->GetPointForPort(portOut);
			}
			Gdiplus::RectF box = DrawArrow(gr, pEdge, true, selection);
		}
		else
		{
			Gdiplus::RectF box = DrawArrow(gr, pEdge, true, selection);
			pEdge->DrawSpecial(&gr, Gdiplus::PointF(box.X + box.Width / 2, box.Y + box.Height / 2));
		}
	}
}
//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::CenterViewAroundNode(CHyperNode* poNode, bool boFitZoom, float fZoom)
{
	CRect rc;
	GetClientRect(rc);

	m_scrollOffset.SetPoint(0, 0);

	CRect rcGraph = LocalToViewRect(poNode->GetRect());

	if (boFitZoom)
	{
		float zoom = (float)max(rc.Width(), rc.Height()) / (max(rcGraph.Width(), rcGraph.Height()) + GRID_SIZE * 4);
		if (zoom < 1)
		{
			SetZoom(zoom);
		}
	}

	if (fZoom > 0.0f)
	{
		SetZoom(fZoom);
	}

	rcGraph = LocalToViewRect(poNode->GetRect());
	if (rcGraph.Width() < rc.Width())
	{
		m_scrollOffset.x += (rc.Width() - rcGraph.Width()) / 2;
	}
	if (rcGraph.Height() < rc.Height())
	{
		m_scrollOffset.y += (rc.Height() - rcGraph.Height()) / 2;
	}

	m_scrollOffset.x += -rcGraph.left + GRID_SIZE;
	m_scrollOffset.y += -rcGraph.top + GRID_SIZE;

	InvalidateView();
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::PropagateChangesToGroupsAndPrefabs()
{
	if (m_pGraph && m_pGraph->IsFlowGraph())
	{
		if (CEntityObject* pGraphEntity = static_cast<CHyperFlowGraph*>(m_pGraph)->GetEntity())
		{
			pGraphEntity->UpdateGroup();
			pGraphEntity->UpdatePrefab();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::DrawNodes(Gdiplus::Graphics& gr, const CRect& rc)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	std::multimap<int, CHyperNode*> nodes;
	GetNodesInRect(rc, nodes);

	std::vector<CHyperNode*> selectedNodes;
	const bool bNodesSelected = GetSelectedNodes(selectedNodes);

	for (std::multimap<int, CHyperNode*>::iterator iter = nodes.begin(); iter != nodes.end(); ++iter)
	{
		CHyperNode* pNode = iter->second;

		if ((false == bNodesSelected) || (false == stl::find(selectedNodes, pNode)))
		{
			pNode->Draw(this, gr, true);
		}
	}

	if (bNodesSelected)
	{
		for (std::vector<CHyperNode*>::const_iterator iter = selectedNodes.begin(); iter != selectedNodes.end(); ++iter)
		{
			(*iter)->Draw(this, gr, true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CRect CHyperGraphView::LocalToViewRect(const Gdiplus::RectF& localRect) const
{
	Gdiplus::RectF temp = localRect;
	temp.X *= m_zoom;
	temp.Y *= m_zoom;
	temp.Width *= m_zoom;
	temp.Height *= m_zoom;
	temp.Offset(Gdiplus::PointF(m_scrollOffset.x, m_scrollOffset.y));
	return CRect(temp.X - 1.0f, temp.Y - 1.0f, temp.GetRight() + 1.05f, temp.GetBottom() + 1.0f);
}

//////////////////////////////////////////////////////////////////////////
Gdiplus::RectF CHyperGraphView::ViewToLocalRect(const CRect& viewRect) const
{
	Gdiplus::RectF rc(viewRect.left, viewRect.top, viewRect.Width(), viewRect.Height());
	rc.Offset(-m_scrollOffset.x, -m_scrollOffset.y);
	rc.X /= m_zoom;
	rc.Y /= m_zoom;
	rc.Width /= m_zoom;
	rc.Height /= m_zoom;
	return rc;
}

//////////////////////////////////////////////////////////////////////////
CPoint CHyperGraphView::LocalToView(Gdiplus::PointF point)
{
	return CPoint(point.X * m_zoom + m_scrollOffset.x, point.Y * m_zoom + m_scrollOffset.y);
}

//////////////////////////////////////////////////////////////////////////
Gdiplus::PointF CHyperGraphView::ViewToLocal(CPoint point)
{
	return Gdiplus::PointF((point.x - m_scrollOffset.x) / m_zoom, (point.y - m_scrollOffset.y) / m_zoom);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnHyperGraphEvent(IHyperNode* pINode, EHyperGraphEvent event)
{
	if (m_bIgnoreHyperGraphEvents)
		return;

	CHyperNode* pNode = static_cast<CHyperNode*>(pINode);
	switch (event)
	{
	// case EHG_GRAPH_REMOVED: handled by main dialog window
	case EHG_GRAPH_INVALIDATE:
		m_edgesToReroute.clear();
		m_pEditedNode = 0;
		m_pMultiEditedNodes.resize(0);
		m_pMouseOverNode = NULL;
		m_pMouseOverPort = NULL;
		m_DraggingBorderNodeInfo.m_pNode = 0;
		m_graphVisuallyChangedHelper.Reset(false);
		InvalidateView();
		OnSelectionChange();
		break;
	case EHG_NODE_ADD:
		InvalidateView();
		break;
	case EHG_NODE_DELETE:
		if (pINode == static_cast<CHyperNode*>(m_pMouseOverNode))
		{
			m_pMouseOverNode = NULL;
			m_pMouseOverPort = NULL;
		}
		InvalidateView();
		break;
	case EHG_NODE_CHANGE:
		{
			InvalidateNode(pNode);
		}
		break;
	case EHG_NODE_CHANGE_DEBUG_PORT:
		InvalidateNode(pNode, true, false);
		if (m_pGraph && m_pGraph->FindNode(pNode->GetId()))
		{
			UpdateDebugCount(pNode);
		}
		break;
	case EHG_GRAPH_REMOVED:
		m_pGraph = nullptr;
		break;
	}

	// Something in the graph changes so update group/prefab
	if( 
			/* GRAPH EVENTS*/
			event == EHG_GRAPH_ADDED      || event == EHG_GRAPH_UNDO_REDO          || event == EHG_GRAPH_TOKENS_UPDATED     || 
			event == EHG_GRAPH_INVALIDATE || event == EHG_GRAPH_EDGE_STATE_CHANGED || event == EHG_GRAPH_PROPERTIES_CHANGED || 
			/* NODE CHANGE EVENTS */
			event == EHG_NODE_RESIZE      || event == EHG_NODE_UPDATE_ENTITY       || event == EHG_NODE_PROPERTIES_CHANGED  ||
			event == EHG_NODES_PASTED     ||
			((event == EHG_NODE_DELETED || event == EHG_NODE_SET_NAME || event == EHG_NODE_ADD) && (pNode->GetTypeId() != s_quickSearchNodeId))
		)
	{
		if(!m_bBatchingChangesInProgress)
		{
			PropagateChangesToGroupsAndPrefabs();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::SetGraph(CHyperGraph* pGraph)
{
	if (pGraph == nullptr && m_pGraph == nullptr)
		return;

	if (m_pGraph == pGraph)
	{
		InvalidateView(true);
		return;
	}

	HideEditPort();

	if (m_graphVisuallyChangedHelper.HasGraphVisuallyChanged())
		PropagateChangesToGroupsAndPrefabs();

	m_edgesToReroute.clear();
	m_pEditedNode = 0;
	m_pMultiEditedNodes.resize(0);
	m_pMouseOverNode = NULL;
	m_pMouseOverPort = NULL;
	m_DraggingBorderNodeInfo.m_pNode = 0;
	m_graphVisuallyChangedHelper.Reset(true);

	if (m_pGraph)
	{
		// CryLogAlways("CHyperGraphView: (1) Removing as listener from 0x%p before switching to 0x%p", m_pGraph, pGraph);
		m_pGraph->RemoveListener(this);
		// CryLogAlways("CHyperGraphView: (2) Removing as listener from 0x%p before switching to 0x%p", m_pGraph, pGraph);
	}

	bool bFitAll = true;

	m_pGraph = pGraph;
	if (m_pGraph)
	{
		if (m_pGraph->GetViewPosition(m_scrollOffset, m_zoom))
		{
			bFitAll = false;
			InvalidateView();
		}

		RerouteAllEdges();
		m_pGraph->AddListener(this);
		NotifyZoomChangeToNodes();
	}

	// Invalidate all view.
	if (m_hWnd && bFitAll)
	{
		OnCommandFitAll();
	}
	if (m_hWnd)
	{
		OnSelectionChange();
	}

	UpdateFrozen();

	if (m_pGraph && m_pGraph->IsNodeActivationModified())
		OnPaint();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnCommandFitAll()
{
	CRect rc;
	GetClientRect(rc);

	if (rc.IsRectEmpty())
	{
		m_bRefitGraphInOnSize = true;
		return;
	}

	SetZoom(1);
	m_scrollOffset.SetPoint(0, 0);
	UpdateWorkingArea();
	CRect rcGraph = LocalToViewRect(m_workingArea);

	float zoom = min(float(rc.Width()) / rcGraph.Width(), float(rc.Height()) / rcGraph.Height());
	if (zoom < 1)
	{
		SetZoom(zoom);
		rcGraph = LocalToViewRect(m_workingArea); // used new zoom
	}

	if (rcGraph.Width() < rc.Width())
		m_scrollOffset.x += (rc.Width() - rcGraph.Width()) / 2;
	if (rcGraph.Height() < rc.Height())
		m_scrollOffset.y += (rc.Height() - rcGraph.Height()) / 2;

	m_scrollOffset.x -= rcGraph.left;
	m_scrollOffset.y -= rcGraph.top;

	InvalidateView();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::ShowAndSelectNode(CHyperNode* pToNode, bool bSelect)
{
	assert(pToNode != 0);

	if (!m_pGraph)
		return;

	CRect rc;
	GetClientRect(rc);

	CRect nodeRect = LocalToViewRect(pToNode->GetRect());

	//// check if fully inside
	//if (nodeRect.left >= rc.left &&
	//	nodeRect.top >= rc.top &&
	//	nodeRect.bottom <= rc.bottom &&
	//	nodeRect.right <= rc.right)
	//{
	//	// fully inside
	//	// do nothing yet...
	//}
	//else
	{
		if (rc.IsRectEmpty())
		{
			m_bRefitGraphInOnSize = true;
		}
		else
		{
			m_scrollOffset.SetPoint(0, 0);

			Gdiplus::RectF stRect = pToNode->GetRect();

			if (stRect.IsEmptyArea())
			{
				CDC* pDC = GetDC();
				if (pDC)
				{
					Gdiplus::Graphics gr(pDC->GetSafeHdc());
					pToNode->CalculateSize(gr);
					stRect = pToNode->GetRect();
					ReleaseDC(pDC);
				}
				else
				{
					assert(false);
				}
			}

			CRect rcGraph;

			rcGraph.left = stRect.X;
			rcGraph.right = stRect.X + stRect.Width;

			rcGraph.top = stRect.Y;
			rcGraph.bottom = stRect.Y + stRect.Height;

			int maxWH = max(rcGraph.Width(), rcGraph.Height());
			if (maxWH > 0)
			{
				float fZoom = (float)(max(rc.Width(), rc.Height())) * 0.33f / maxWH;
				SetZoom(fZoom);
			}
			else
			{
				SetZoom(1.0f);
			}

			rcGraph = LocalToViewRect(pToNode->GetRect());

			if (rcGraph.Width() < rc.Width())
				m_scrollOffset.x += (rc.Width() - rcGraph.Width()) / 2;
			if (rcGraph.Height() < rc.Height())
				m_scrollOffset.y += (rc.Height() - rcGraph.Height()) / 2;

			m_scrollOffset.x += -rcGraph.left + GRID_SIZE;
			m_scrollOffset.y += -rcGraph.top + GRID_SIZE;

			//m_scrollOffset.SetPoint(0,0);
			//CRect rcGraph = LocalToViewRect(pToNode->GetRect());
			//float zoom = (float)max(rc.Width(),rc.Height()) / (max(rcGraph.Width(),rcGraph.Height()) + GRID_SIZE*4);
			//if (zoom < 1)
			//	m_zoom = CLAMP( zoom, MIN_ZOOM, MAX_ZOOM );
			//rcGraph = LocalToViewRect(pToNode->GetRect());

			//if (rcGraph.Width() < rc.Width())
			//	m_scrollOffset.x += (rc.Width() - rcGraph.Width()) / 2;
			//if (rcGraph.Height() < rc.Height())
			//	m_scrollOffset.y += (rc.Height() - rcGraph.Height()) / 2;

			//m_scrollOffset.x += -rcGraph.left + GRID_SIZE;
			//m_scrollOffset.y += -rcGraph.top + GRID_SIZE;
		}
	}

	if (bSelect)
	{
		SStartNodeOperationBatching startBatching(this);
		IHyperGraphEnumerator* pEnum = m_pGraph->GetNodesEnumerator();
		for (IHyperNode* pINode = pEnum->GetFirst(); pINode; pINode = pEnum->GetNext())
		{
			CHyperNode* pNode = (CHyperNode*)pINode;
			if (pNode->IsSelected())
				pNode->SetSelected(false);
		}
		pToNode->SetSelected(true);

		OnSelectionChange();
		pEnum->Release();
	}
	InvalidateView();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::DrawGrid(Gdiplus::Graphics& gr, const CRect& updateRect)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	int gridStep = GRID_SIZE;

	float z = m_zoom;
	assert(z >= MIN_ZOOM);
	while (z < 0.99f)
	{
		z *= 2;
		gridStep *= 2;
	}

	// Draw grid line every 5 pixels.
	Gdiplus::RectF updLocalRect = ViewToLocalRect(updateRect);
	float startX = updLocalRect.X - fmodf(updLocalRect.X, gridStep);
	float startY = updLocalRect.Y - fmodf(updLocalRect.Y, gridStep);
	float stopX = startX + updLocalRect.Width;
	float stopY = startY + updLocalRect.Height;

	Gdiplus::Pen gridPen(GRID_COLOR, 1.0f);

	// Draw vertical grid lines.
	for (float x = startX; x < stopX; x += gridStep)
		gr.DrawLine(&gridPen, Gdiplus::PointF(x, startY), Gdiplus::PointF(x, stopY));

	// Draw horizontal grid lines.
	for (float y = startY; y < stopY; y += gridStep)
		gr.DrawLine(&gridPen, Gdiplus::PointF(startX, y), Gdiplus::PointF(stopX, y));
}

//////////////////////////////////////////////////////////////////////////
Gdiplus::RectF CHyperGraphView::DrawArrow(Gdiplus::Graphics& gr, CHyperEdge* pEdge, bool helper, int selection)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	// optimization: don't draw helper or label text when zoomed out
	static const float fHelperDrawMinimumZoom = 0.4f;
	Gdiplus::PointF pout = pEdge->pointOut;
	Gdiplus::PointF pin = pEdge->pointIn;
	EHyperEdgeDirection dout = pEdge->dirOut;
	EHyperEdgeDirection din = pEdge->dirIn;

	struct
	{
		float x1, y1;
		float x2, y2;
	}
	h[] =
	{
		{ 0,     -0.5f, 0,     -0.5f },
		{ 0,     0.5f,  0,     0.5f  },
		{ -0.5f, 0,     -0.5f, 0     },
		{ 0.5f,  0,     0.5f,  0     }
	};

	float dx = CLAMP(fabsf(pout.X - pin.X), 20.0f, 150.0f);
	float dy = CLAMP(fabsf(pout.Y - pin.Y), 20.0f, 150.0f);

	if (fabsf(pout.X - pin.X) < 0.0001f && fabsf(pout.Y - pin.Y) < 0.0001f)
		return Gdiplus::RectF(0, 0, 0, 0);

	Gdiplus::PointF pnts[6];

	pnts[0] = Gdiplus::PointF(pout.X, pout.Y);
	pnts[1] = Gdiplus::PointF(pnts[0].X + h[dout].x1 * dx, pnts[0].Y + h[dout].y1 * dy);
	pnts[3] = Gdiplus::PointF(pin.X, pin.Y);
	pnts[2] = Gdiplus::PointF(pnts[3].X + h[din].x2 * dx, pnts[3].Y + h[din].y2 * dy);
	Gdiplus::PointF center = Gdiplus::PointF((pnts[1].X + pnts[2].X) * 0.5f, (pnts[1].Y + pnts[2].Y) * 0.5f);

	float zoom = m_zoom;
	if (zoom > 1.0f)
		zoom = 1.0f;

	Gdiplus::Color color;
	Gdiplus::Pen pen(color);

	switch (selection)
	{
	case 1: // incoming
		color = ARROW_SEL_COLOR_IN;
		break;
	case 2: // outgoing
		color = ARROW_SEL_COLOR_OUT;
		break;
	case 3: // incoming+outgoing
		color = Gdiplus::Color(244, 244, 244);
		break;
	case 4: // debugging port activation
		{
			// MATUS : EDGE COLOR HERE

			color = pEdge->isInInitializationPhase ? GET_GDI_COLOR(gFlowGraphColorPreferences.colorPortDebuggingInitialization) : GET_GDI_COLOR(gFlowGraphColorPreferences.colorPortDebugging);
			
			//color = GET_GDI_COLOR(gFlowGraphColorPreferences.colorPortDebugging);

			/*
			CHyperNode* nodeIn  = (CHyperNode*) m_pGraph->FindNode(pEdge->nodeIn);
			CHyperNode* nodeOut = (CHyperNode*) m_pGraph->FindNode(pEdge->nodeOut);
			if (nodeIn)
			{
				bool bIsInitializationPhase = false;
				nodeIn->GetAdditionalDebugPortInformation(*nodeIn->FindPort(pEdge->nPortIn, true), bIsInitializationPhase);
				if (bIsInitializationPhase)
					color = GET_GDI_COLOR(gFlowGraphColorPreferences.colorPortDebuggingInitialization);
			}
			if (nodeOut)
			{
				bool bIsInitializationPhase = false;
				nodeOut->GetAdditionalDebugPortInformation(*nodeOut->FindPort(pEdge->nPortOut, false), bIsInitializationPhase);
				if (bIsInitializationPhase)
					color = GET_GDI_COLOR(gFlowGraphColorPreferences.colorPortDebuggingInitialization);
			}
			*/
			
		}
		break;
	case 5: // AG Modifier Link Color
		color = Gdiplus::Color(44, 44, 90);
		pen.SetDashStyle(Gdiplus::DashStyleDot);
		pen.SetWidth(1.5f);
		break;
	case 6: // AG Regular Link Color
		color = Gdiplus::Color(40, 40, 44);
		break;
	case 7: // Selection Tree Link Color
		color = Gdiplus::Color(0, 0, 0);
	case 8:
		color = PORT_EDGE_HIGHLIGHT;
		pen.SetWidth(1.5f);
		break;
	default:
		color = ARROW_COLOR;
		break;
	}

	if (!pEdge->enabled)
	{
		color = ARROW_DIS_COLOR;
		pen.SetWidth(2.0f);
		pen.SetDashStyle(Gdiplus::DashStyleDot);
	}

	pen.SetColor(color);

	Gdiplus::AdjustableArrowCap cap(5 * zoom, 6 * zoom);
	pen.SetCustomEndCap(&cap);

	const float HELPERSIZE = 4.0f;

	Gdiplus::RectF helperRect(center.X - HELPERSIZE / 2, center.Y - HELPERSIZE / 2, HELPERSIZE, HELPERSIZE);

	if (gFlowGraphGeneralPreferences.splineArrows)
	{
		if (pEdge->cornerModified)
		{
			float cdx = pEdge->cornerPoints[0].X - center.X;
			float cdy = pEdge->cornerPoints[0].Y - center.Y;

			dx += cdx;
			dy += cdy;

			pnts[0] = Gdiplus::PointF(pout.X, pout.Y);
			pnts[1] = Gdiplus::PointF(pnts[0].X + h[dout].x1 * dx, pnts[0].Y + h[dout].y1 * dy);
			pnts[3] = Gdiplus::PointF(pin.X, pin.Y);
			pnts[2] = Gdiplus::PointF(pnts[3].X + h[din].x2 * dx, pnts[3].Y + h[din].y2 * dy);
		}
		else
		{
			pEdge->cornerPoints[0] = center;
			pEdge->originalCenter = center;
		}

		gr.DrawBeziers(&pen, pnts, 4);

		if (helper && m_zoom > fHelperDrawMinimumZoom)
		{
			gr.DrawEllipse(&pen, helperRect);
		}

		if (4 == selection && pEdge && m_zoom > fHelperDrawMinimumZoom)
		{
			if (pEdge->debugCount > 1)
			{
				Gdiplus::Font font(L"Tahoma", 9.0f);
				Gdiplus::PointF point(center.X, center.Y - 15.0f);
				Gdiplus::SolidBrush brush(color);

				wchar_t buf[32];
				swprintf_s(buf, L"%d/%d", pEdge->debugCountInitialization, pEdge->debugCount - pEdge->debugCountInitialization);
				gr.DrawString(buf, -1, &font, point, &brush);
			}
		}
	}
	else
	{
		float w = 20 + pEdge->nPortOut * 10;
		if (w > fabs((pout.X - pin.X) / 2))
			w = fabs((pout.X - pin.X) / 2);

		if (!pEdge->cornerModified)
		{
			pEdge->cornerW = w;
			pEdge->cornerH = 40;
		}
		w = pEdge->cornerW;
		float ph = pEdge->cornerH;

		if (pin.X >= pout.X)
		{
			pnts[0] = pout;
			pnts[1] = Gdiplus::PointF(pout.X + w, pout.Y);
			pnts[2] = Gdiplus::PointF(pout.X + w, pin.Y);
			pnts[3] = pin;
			gr.DrawLines(&pen, pnts, 4);

			pEdge->cornerPoints[0] = pnts[1];
			pEdge->cornerPoints[1] = pnts[2];
			pEdge->cornerPoints[2] = Gdiplus::PointF(0, 0);
			pEdge->cornerPoints[3] = Gdiplus::PointF(0, 0);
		}
		else
		{
			pnts[0] = pout;
			pnts[1] = Gdiplus::PointF(pout.X + w, pout.Y);
			pnts[2] = Gdiplus::PointF(pout.X + w, pout.Y + ph);
			pnts[3] = Gdiplus::PointF(pin.X - w, pout.Y + ph);
			pnts[4] = Gdiplus::PointF(pin.X - w, pin.Y);
			pnts[5] = pin;
			gr.DrawLines(&pen, pnts, 6);

			pEdge->cornerPoints[0] = pnts[1];
			pEdge->cornerPoints[1] = pnts[2];
			pEdge->cornerPoints[2] = pnts[3];
			pEdge->cornerPoints[3] = pnts[4];
		}
		if (helper)
		{
			gr.DrawRectangle(&pen, Gdiplus::RectF(pnts[1].X - HELPERSIZE / 2, pnts[1].Y - HELPERSIZE / 2, HELPERSIZE, HELPERSIZE));
			gr.DrawRectangle(&pen, Gdiplus::RectF(pnts[2].X - HELPERSIZE / 2, pnts[2].Y - HELPERSIZE / 2, HELPERSIZE, HELPERSIZE));
		}
	}

	return helperRect;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::CaptureMouse()
{
	if (GetCapture() != this)
	{
		SetCapture();
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::ReleaseMouse()
{
	if (GetCapture() == this)
	{
		ReleaseCapture();
	}
}

//////////////////////////////////////////////////////////////////////////
CHyperNode* CHyperGraphView::GetNodeAtPoint(CPoint point)
{
	std::multimap<int, CHyperNode*> nodes;
	if (!GetNodesInRect(CRect(point.x, point.y, point.x, point.y), nodes))
		return 0;

	return nodes.rbegin()->second;
}

//////////////////////////////////////////////////////////////////////////
CHyperNode* CHyperGraphView::GetMouseSensibleNodeAtPoint(CPoint point)
{
	CHyperNode* pNode = GetNodeAtPoint(point);
	if (pNode && pNode->GetObjectAt(GraphicsHelper(this), ViewToLocal(point)) == eSOID_InputTransparent)
		pNode = NULL;

	return pNode;
}

//////////////////////////////////////////////////////////////////////////
bool CHyperGraphView::GetNodesInRect(const CRect& viewRect, std::multimap<int, CHyperNode*>& nodes, bool bFullInside)
{
	if (!m_pGraph)
		return false;
	bool bFirst = true;
	Gdiplus::RectF localRect = ViewToLocalRect(viewRect);
	IHyperGraphEnumerator* pEnum = m_pGraph->GetNodesEnumerator();
	nodes.clear();

	for (IHyperNode* pINode = pEnum->GetFirst(); pINode; pINode = pEnum->GetNext())
	{
		CHyperNode* pNode = (CHyperNode*)pINode;
		const Gdiplus::RectF& itemRect = pNode->GetRect();

		if (!localRect.IntersectsWith(itemRect))
			continue;
		if (bFullInside && !localRect.Contains(itemRect))
			continue;

		nodes.insert(std::make_pair(pNode->GetDrawPriority(), pNode));
	}
	pEnum->Release();

	return !nodes.empty();
}

//////////////////////////////////////////////////////////////////////////
bool CHyperGraphView::GetSelectedNodes(std::vector<CHyperNode*>& nodes, SelectionSetType setType)
{
	if (!m_pGraph)
		return false;

	bool onlyParents = false;
	bool includeRelatives = false;
	switch (setType)
	{
	case SELECTION_SET_INCLUDE_RELATIVES:
		includeRelatives = true;
		break;
	case SELECTION_SET_ONLY_PARENTS:
		onlyParents = true;
		break;
	case SELECTION_SET_NORMAL:
		break;
	}

	if (includeRelatives)
	{
		std::set<CHyperNode*> nodeSet;

		IHyperGraphEnumerator* pEnum = m_pGraph->GetNodesEnumerator();
		for (IHyperNode* pINode = pEnum->GetFirst(); pINode; pINode = pEnum->GetNext())
		{
			CHyperNode* pNode = (CHyperNode*)pINode;
			if (pNode->IsSelected())
			{
				//nodes.push_back( pNode );
				IHyperGraphEnumerator* pRelativesEnum = pNode->GetRelatedNodesEnumerator();
				for (IHyperNode* pRelative = pRelativesEnum->GetFirst(); pRelative; pRelative = pRelativesEnum->GetNext())
					nodeSet.insert(static_cast<CHyperNode*>(pRelative));
				pRelativesEnum->Release();
			}
		}
		pEnum->Release();

		nodes.clear();
		nodes.reserve(nodeSet.size());
		std::copy(nodeSet.begin(), nodeSet.end(), std::back_inserter(nodes));
	}
	else
	{
		IHyperGraphEnumerator* pEnum = m_pGraph->GetNodesEnumerator();
		nodes.clear();
		for (IHyperNode* pINode = pEnum->GetFirst(); pINode; pINode = pEnum->GetNext())
		{
			CHyperNode* pNode = (CHyperNode*)pINode;
			if (pNode->IsSelected() && (!onlyParents || pNode->GetParent() == 0))
				nodes.push_back(pNode);
		}
		pEnum->Release();
	}

	return !nodes.empty();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::ClearSelection()
{
	if (!m_pGraph)
		return;
	SStartNodeOperationBatching startBatching(this);
	IHyperGraphEnumerator* pEnum = m_pGraph->GetNodesEnumerator();
	for (IHyperNode* pINode = pEnum->GetFirst(); pINode; pINode = pEnum->GetNext())
	{
		CHyperNode* pNode = (CHyperNode*)pINode;
		if (pNode->IsSelected())
			pNode->SetSelected(false);
	}
	OnSelectionChange();
	pEnum->Release();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::MoveSelectedNodes(CPoint offset)
{
	if (offset == CPoint(0, 0))
		return;

	std::vector<CHyperNode*> nodes;
	if (!GetSelectedNodes(nodes))
		return;

	Gdiplus::RectF bounds;

	for (int i = 0; i < nodes.size(); i++)
	{
		// Only move parent nodes.
		if (nodes[i]->GetParent() == 0)
		{
			m_graphVisuallyChangedHelper.NodeMoved(*nodes[i]);

			if (!i)
				bounds = nodes[i]->GetRect();
			else
				Gdiplus::RectF::Union(bounds, bounds, nodes[i]->GetRect());

			Gdiplus::PointF pos = nodes[i]->GetPos();
			Gdiplus::PointF firstPos = m_graphVisuallyChangedHelper.GetNodeStartPosition(*nodes[i]);
			pos.X = firstPos.X + offset.x / m_zoom;
			pos.Y = firstPos.Y + offset.y / m_zoom;

			// Snap rectangle to the grid.
			pos.X = floor(((float)pos.X / GRID_SIZE) + 0.5f) * GRID_SIZE;
			pos.Y = floor(((float)pos.Y / GRID_SIZE) + 0.5f) * GRID_SIZE;

			nodes[i]->SetPos(pos);
			InvalidateNode(nodes[i]);
			IHyperGraphEnumerator* pRelativesEnum = nodes[i]->GetRelatedNodesEnumerator();
			for (IHyperNode* pRelative = pRelativesEnum->GetFirst(); pRelative; pRelative = pRelativesEnum->GetNext())
			{
				CHyperNode* pRelativeNode = static_cast<CHyperNode*>(pRelative);
				Gdiplus::RectF::Union(m_workingArea, m_workingArea, pRelativeNode->GetRect());
				Gdiplus::RectF::Union(bounds, bounds, pRelativeNode->GetRect());
			}
			pRelativesEnum->Release();
		}
	}
	CRect invRect = LocalToViewRect(bounds);
	invRect.InflateRect(32, 32, 32, 32);
	InvalidateRect(invRect);
	SetScrollExtents();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnSelectEntity()
{
	std::vector<CHyperNode*> nodes;
	if (!GetSelectedNodes(nodes))
		return;

	CUndo undo("Select Object(s)");
	GetIEditorImpl()->ClearSelection();
	for (int i = 0; i < nodes.size(); i++)
	{
		// only can CFlowNode* if not a comment, argh...
		if (!nodes[i]->IsEditorSpecialNode())
		{
			CFlowNode* pFlowNode = (CFlowNode*)nodes[i];
			if (pFlowNode->GetEntity())
			{
				GetIEditorImpl()->SelectObject(pFlowNode->GetEntity());
			}
			else if(strcmp(pFlowNode->GetClassName(), "Prefab:Instance") == 0)
			{
				CString sInstanceName;
				CHyperNodePort* pPort = pFlowNode->FindPort("InstanceName", true);
				if (pPort)
				{
					pPort->pVar->Get(sInstanceName);
					if (!sInstanceName.IsEmpty())
					{
						IEditor* const pEditor = GetIEditor();
						CBaseObject* const pObject = pEditor->GetObjectManager()->FindObject(sInstanceName);
						pEditor->SelectObject(pObject);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnToggleMinimize()
{
	std::vector<CHyperNode*> nodes;
	if (!GetSelectedNodes(nodes))
		return;

	for (int i = 0; i < nodes.size(); i++)
	{
		OnToggleMinimizeNode(nodes[i]);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnToggleMinimizeNode(CHyperNode* pNode)
{
	bool bMinimize = false;
	if (!bMinimize)
	{
		for (int i = 0; i < pNode->GetInputs()->size(); i++)
		{
			CHyperNodePort* pPort = &pNode->GetInputs()->at(i);
			if (pPort->bVisible && (pPort->nConnected == 0))
			{
				bMinimize = true;
				break;
			}
		}
	}
	if (!bMinimize)
	{
		for (int i = 0; i < pNode->GetOutputs()->size(); i++)
		{
			CHyperNodePort* pPort = &pNode->GetOutputs()->at(i);
			if (pPort->bVisible && (pPort->nConnected == 0))
			{
				bMinimize = true;
				break;
			}
		}
	}

	bool bVisible = !bMinimize;
	{
		for (int i = 0; i < pNode->GetInputs()->size(); i++)
		{
			CHyperNodePort* pPort = &pNode->GetInputs()->at(i);
			pPort->bVisible = bVisible;
		}
	}
	{
		for (int i = 0; i < pNode->GetOutputs()->size(); i++)
		{
			CHyperNodePort* pPort = &pNode->GetOutputs()->at(i);
			pPort->bVisible = bVisible;
		}
	}
	InvalidateNode(pNode, true);
	Invalidate();
}

void CHyperGraphView::AddPrefabMenu(CHyperNode* pNode, CMenu& menu)
{
	menu.AppendMenu(MF_STRING, ID_GRAPHVIEW_TARGET_SELECTED_PREFAB, "Assign selected Prefab");
	menu.AppendMenu(MF_STRING, ID_GRAPHVIEW_TARGET_UNSELECT_PREFAB, "Unassign Prefab");
	menu.AppendMenu(MF_SEPARATOR);
}

void CHyperGraphView::HandlePrefabMenu(CHyperNode* pNode, const uint cmd)
{
	CBaseObject *pSelObj = nullptr;

	// (cmd == ID_GRAPHVIEW_TARGET_UNSELECT_PREFAB) will use the default empty value
	if (cmd == ID_GRAPHVIEW_TARGET_SELECTED_PREFAB)
	{
		pSelObj = GetIEditor()->GetSelectedObject();
		if (pSelObj && !pSelObj->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
		{
			pSelObj = nullptr; // selected object is not prefab
		}
	}

	CPrefabEvents* const pPrefabEvents = GetIEditor()->GetPrefabManager()->GetPrefabEvents();

	std::vector<CHyperNode*> nodes;
	GetSelectedNodes(nodes); // all selected nodes (possibly including the node clicked on)
	nodes.push_back(pNode); // node under cursor (clicked on)
	for (std::vector<CHyperNode*>::const_iterator iter = nodes.begin(), iterEnd = nodes.end(); iter != iterEnd; ++iter)
	{
		// a selected node may not be for prefabs, only the clicked on node is guaranteed to be
		if (strcmp(pNode->GetClassName(), "Prefab:Instance") == 0)
		{
			pPrefabEvents->UpdatePrefabInstanceNodeFromSelection(static_cast<CFlowNode*>(pNode), static_cast<CPrefabObject*>(pSelObj));
		}
	}

	InvalidateNode(pNode, true);
	Invalidate();
}

void CHyperGraphView::AddEntityMenu(CHyperNode* pNode, CMenu& menu)
{
	bool bAddMenu = pNode->CheckFlag(EHYPER_NODE_ENTITY);

	std::vector<CHyperNode*> nodes;
	if (!bAddMenu && GetSelectedNodes(nodes))
	{
		for (std::vector<CHyperNode*>::const_iterator iter = nodes.begin(), iterEnd = nodes.end(); iter != iterEnd; ++iter)
		{
			CHyperNode* const pNode = (*iter);
			if (pNode->CheckFlag(EHYPER_NODE_ENTITY))
			{
				bAddMenu = true;
				break;
			}
		}
	}

	if (bAddMenu)
	{
		menu.AppendMenu(MF_STRING, ID_GRAPHVIEW_TARGET_SELECTED_ENTITY, "Assign Selected Entity");
		menu.AppendMenu(MF_STRING, ID_GRAPHVIEW_TARGET_GRAPH_ENTITY, "Assign Graph Entity");
		menu.AppendMenu(MF_STRING, ID_GRAPHVIEW_TARGET_UNSELECT_ENTITY, "Unassign Entity");
		menu.AppendMenu( MF_STRING,ID_GRAPHVIEW_SELECT_ENTITY,"Select Assigned Entity" );
		menu.AppendMenu(MF_SEPARATOR);
	}
}

void CHyperGraphView::HandleEntityMenu(CHyperNode* pNode, const uint cmd)
{
	switch (cmd)
	{
	case ID_GRAPHVIEW_TARGET_SELECTED_ENTITY:
	case ID_GRAPHVIEW_TARGET_GRAPH_ENTITY:
	case ID_GRAPHVIEW_TARGET_UNSELECT_ENTITY:
		{
			// update node under cursor
			UpdateNodeEntity(pNode, cmd);

			// update all selected nodes
			std::vector<CHyperNode*> nodes;
			if (GetSelectedNodes(nodes))
			{
				std::vector<CHyperNode*>::const_iterator iter = nodes.begin();
				for (; iter != nodes.end(); ++iter)
				{
					UpdateNodeEntity((*iter), cmd);
				}
			}
		}
		break;
	case ID_GRAPHVIEW_SELECT_ENTITY:
		OnSelectEntity();
		break;
	}
}

void CHyperGraphView::AddGameTokenMenu(CMenu& menu)
{
	menu.AppendMenu(MF_STRING, ID_GOTO_TOKEN, "Token: Goto definition");
	menu.AppendMenu(MF_STRING, ID_TOKEN_FIND_USAGES, "Token: Find usages");
	menu.AppendMenu(MF_SEPARATOR);
}

void CHyperGraphView::HandleGameTokenMenu(IFlowGraph* pGraph, const CString& tokenName, const uint cmd)
{
	switch (cmd)
	{
	case ID_GOTO_TOKEN:
		{
			CGameTokenManager *m_pGameTokenManager = GetIEditorImpl()->GetGameTokenManager();
			CGameTokenItem* pTokenItem = static_cast<CGameTokenItem*>(m_pGameTokenManager->FindItemByName(tokenName));

			if (pTokenItem)
			{
				CBaseLibraryDialog *pDBDlg = GetIEditorImpl()->OpenDataBaseLibrary(EDB_TYPE_GAMETOKEN, (IDataBaseItem*)pTokenItem);
				if (pDBDlg && pDBDlg->IsKindOf(RUNTIME_CLASS(CGameTokenDialog)))
				{
					CGameTokenDialog* pTokenDlg = (CGameTokenDialog*)pDBDlg;
					int pos = 0;
					string libName = tokenName.Tokenize(".", pos);
					pTokenDlg->SelectLibrary(libName);
					pTokenDlg->SetSelectedGameToken(pTokenItem);
				}
			}
			else
			{
				// maybe it's a graph token?
				CWnd* pWnd = GetIEditor()->FindView("Flow Graph");
				if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CHyperGraphDialog)))
				{
					CHyperGraphDialog* pHGDlg = static_cast<CHyperGraphDialog*>(pWnd);
					pHGDlg->OpenGraphTokenEditor();
				}
			}
		}
		break;
	case ID_TOKEN_FIND_USAGES:
		{
			CWnd* pWnd = GetIEditor()->FindView("Flow Graph");
			if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CHyperGraphDialog)))
			{
				CHyperGraphDialog* pHGDlg = static_cast<CHyperGraphDialog*>(pWnd);
				CFlowGraphSearchCtrl* pSC = pHGDlg->GetSearchControl();
				if (pSC)
				{
					CFlowGraphSearchOptions* pOpts = CFlowGraphSearchOptions::GetSearchOptions();
					pOpts->m_bIncludeValues = true;
					pOpts->m_findSpecial = CFlowGraphSearchOptions::eFLS_None;
					pOpts->m_LookinIndex = CFlowGraphSearchOptions::eFL_All;
					pSC->Find(tokenName, false, false, true);
				}
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::AddModuleMenu(CHyperNode* pNode, CMenu& menu)
{
	menu.AppendMenu(MF_STRING, ID_GOTO_MODULE, "Goto Module definition");
	menu.AppendMenu(MF_STRING, ID_TOOLS_EDITMODULE, "Edit Module Ports ...");
	menu.AppendMenu(MF_SEPARATOR);
}

void CHyperGraphView::HandleGotoModule(const string &moduleGraphName)
{
	if (CFlowGraphManager* pFlowGraphManager = GetIEditorImpl()->GetFlowGraphManager())
	{
		if (CEditorFlowGraphModuleManager* pModuleManager = GetIEditorImpl()->GetFlowGraphModuleManager())
		{
			IFlowGraphPtr moduleGraph = pModuleManager->GetModuleFlowGraph(moduleGraphName.c_str());
			if (CHyperFlowGraph* moduleFlowGraph = pFlowGraphManager->FindGraph(moduleGraph))
			{
				pFlowGraphManager->OpenView(moduleFlowGraph);
			}
		}
	}
}

void CHyperGraphView::HandleEditModulePortsFromNode(const string &moduleGraphName)
{
	if (CFlowGraphManager* pFlowGraphManager = GetIEditorImpl()->GetFlowGraphManager())
	{
		IFlowGraphModule* pModule = gEnv->pFlowSystem->GetIModuleManager()->GetModule(moduleGraphName);
		if (pModule)
		{
			CFlowGraphEditModuleDlg dlg(pModule, this);
			dlg.DoModal();
		}
	}
}

void CHyperGraphView::HandleModuleMenu(CHyperNode* pNode, const uint cmd)
{
	const string nodeClassName = pNode->GetClassName();
	const string moduleGraphName = nodeClassName.substr(nodeClassName.find("_") + 1, nodeClassName.length());

	switch (cmd)
	{
	case ID_GOTO_MODULE:
		{
			HandleGotoModule(moduleGraphName);
		}
		break;
	case ID_TOOLS_EDITMODULE:
		{
			HandleEditModulePortsFromNode(moduleGraphName);
		}
		break;
	}
}

void CHyperGraphView::AddSelectionMenu(CMenu& menu, CMenu& selectionMenu)
{
	if (m_pGraph->IsSelectedAny())
	{
		menu.AppendMenu(MF_SEPARATOR);

		selectionMenu.CreatePopupMenu();
		menu.AppendMenu(MF_POPUP, (UINT_PTR)selectionMenu.m_hMenu, "Selection");

		selectionMenu.AppendMenu(MF_STRING, ID_GRAPHVIEW_SELECTION_EDGES_ENABLE, "Enable All Edges");
		selectionMenu.AppendMenu(MF_STRING, ID_GRAPHVIEW_SELECTION_EDGES_DISABLE, "Disable All Edges");
		selectionMenu.AppendMenu(MF_SEPARATOR);
		selectionMenu.AppendMenu(MF_STRING, ID_GRAPHVIEW_SELECT_ENTITY, "Select Assigned Entities");
		selectionMenu.AppendMenu(MF_STRING, ID_GRAPHVIEW_SELECT_AND_GOTO_ENTITY, "Select and Goto Assigned Entities");
		selectionMenu.AppendMenu(MF_SEPARATOR);
		selectionMenu.AppendMenu(MF_STRING, ID_FILE_EXPORTSELECTION, "Export Selected Nodes");
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::ShowContextMenu(CPoint point, CHyperNode* pNode)
{ 
	if (!m_pGraph)
		return;

	CMenu menu;
	menu.CreatePopupMenu();

	std::vector<CString> classes;
	SClassMenuGroup classMenu;
	CMenu inputsMenu, outputsMenu, commentsMenu, selectionMenu;

	CPoint screenPoint = point;
	ClientToScreen(&screenPoint);

	CClipboard clipboard;
	const bool bCanPaste = !clipboard.IsEmpty();

	// only add cut, delete... entries to the context menu if it is not the port context menu
	bool isPort = false;

	if (pNode && pNode->IsFlowNode())
	{
		CHyperNodePort* pPort = pNode->GetPortAtPoint(GraphicsHelper(this), ViewToLocal(point));
		if (pPort)
		{
			isPort = true;
			CMenu menu;
			menu.CreatePopupMenu();

			if (pPort->bInput && pPort->nPortIndex == 0)
			{
				if (m_pGraph->GetAIAction() != 0)
				{
					menu.AppendMenu(MF_STRING, ID_GRAPHVIEW_TARGET_GRAPH_ENTITY, "Assign User entity");
					menu.AppendMenu(MF_STRING, ID_GRAPHVIEW_TARGET_GRAPH_ENTITY2, "Assign Object entity");
					menu.AppendMenu(MF_SEPARATOR);
					menu.AppendMenu(MF_STRING, ID_GRAPHVIEW_TARGET_UNSELECT_ENTITY, "Unassign entity");
					menu.AppendMenu(MF_SEPARATOR);
				}
				else if (strcmp(pPort->GetName(), "entityId") == 0)
				{
					AddEntityMenu(pNode, menu);
				}
			}

			if (stricmp(pPort->GetName(), "ModuleName") == 0)
			{
				AddModuleMenu(pNode, menu);
			}
			else if (stricmp(pPort->GetName(), "gametoken_Token") == 0)
			{
				AddGameTokenMenu(menu);
			}

			// breakpoint and tracepoint
			CFlowNode* pFlowNode = static_cast<CFlowNode*>(pNode);
			CFlowGraphDebuggerEditor* pFlowgraphDebuggerEditor = GetIEditorImpl()->GetFlowGraphDebuggerEditor();
			assert(pFlowgraphDebuggerEditor);

			const bool portHasBreakPoint = pFlowgraphDebuggerEditor->HasBreakpoint(pFlowNode, pPort);
			const bool nodeHasBreakPoints = pFlowgraphDebuggerEditor->HasBreakpoint(pFlowNode->GetIFlowGraph(), pFlowNode->GetFlowNodeId());
			const bool graphHasBreakPoints = pFlowgraphDebuggerEditor->HasBreakpoint(pFlowNode->GetIFlowGraph());

			menu.AppendMenu(MF_STRING | (portHasBreakPoint || (pPort->nConnected == 0 && pPort->bInput)) ? MF_GRAYED : 0, ID_GRAPHVIEW_ADD_BREAKPOINT, _T("Add Breakpoint"));
			menu.AppendMenu(MF_STRING | (false == portHasBreakPoint) ? MF_GRAYED : 0, ID_GRAPHVIEW_REMOVE_BREAKPOINT, _T("Remove Breakpoint"));
			menu.AppendMenu(MF_SEPARATOR);
			menu.AppendMenu(MF_STRING | (false == nodeHasBreakPoints) ? MF_GRAYED : 0, ID_GRAPHVIEW_REMOVE_BREAKPOINTS_FOR_NODE, _T("Remove Breakpoints For Node"));
			menu.AppendMenu(MF_STRING | (false == graphHasBreakPoints) ? MF_GRAYED : 0, ID_GRAPHVIEW_REMOVE_ALL_BREAKPOINTS, _T("Remove Breakpoints For Graph"));

			menu.AppendMenu(MF_SEPARATOR);

			SFlowAddress addr;
			addr.node = pFlowNode->GetFlowNodeId();
			addr.port = pPort->nPortIndex;
			addr.isOutput = !pPort->bInput;

			IFlowGraphDebuggerConstPtr pFlowgraphDebugger = GetIFlowGraphDebuggerPtr();
			const bool bIsEnabled = pFlowgraphDebugger ? pFlowgraphDebugger->IsBreakpointEnabled(pFlowNode->GetIFlowGraph(), addr) : false;
			const bool bIsTracepoint = pFlowgraphDebugger ? pFlowgraphDebugger->IsTracepoint(pFlowNode->GetIFlowGraph(), addr) : false;

			UINT enableFlags = (bIsEnabled && portHasBreakPoint) ? MF_CHECKED : !portHasBreakPoint ? MF_GRAYED : MF_UNCHECKED;
			UINT tracepointFlags = (bIsTracepoint && portHasBreakPoint) ? MF_CHECKED : !portHasBreakPoint ? MF_GRAYED : MF_UNCHECKED;

			menu.AppendMenu(MF_STRING | enableFlags, ID_BREAKPOINT_ENABLE, "Enabled");
			menu.AppendMenu(MF_STRING | tracepointFlags, ID_TRACEPOINT_ENABLE, "Tracepoint");


			CPoint pt;
			GetCursorPos(&pt);

			const uint cmd = menu.TrackPopupMenuEx(TPM_RETURNCMD, pt.x, pt.y, this, NULL);
			switch (cmd)
			{
			case ID_GRAPHVIEW_ADD_BREAKPOINT:
				{
					if (false == portHasBreakPoint && (pPort->nConnected || !pPort->bInput))
					{
						pFlowgraphDebuggerEditor->AddBreakpoint(pFlowNode, pPort);
					}
				}
				break;
			case ID_GRAPHVIEW_REMOVE_BREAKPOINT:
				{
					if (portHasBreakPoint)
					{
						pFlowgraphDebuggerEditor->RemoveBreakpoint(pFlowNode, pPort);
					}
				}
				break;
			case ID_GRAPHVIEW_REMOVE_BREAKPOINTS_FOR_NODE:
				{
					pFlowgraphDebuggerEditor->RemoveAllBreakpointsForNode(pFlowNode);
				}
				break;
			case ID_GRAPHVIEW_REMOVE_ALL_BREAKPOINTS:
				{
					pFlowgraphDebuggerEditor->RemoveAllBreakpointsForGraph(pFlowNode->GetIFlowGraph());
				}
				break;
			case ID_BREAKPOINT_ENABLE:
				{
					pFlowgraphDebuggerEditor->EnableBreakpoint(pFlowNode, pPort, !bIsEnabled);
				}
				break;
			case ID_TRACEPOINT_ENABLE:
				{
					pFlowgraphDebuggerEditor->EnableTracepoint(pFlowNode, pPort, !bIsTracepoint);
				}
				break;
			case ID_GRAPHVIEW_TARGET_GRAPH_ENTITY2:
				{
					CFlowNode* pFlowNode = (CFlowNode*)pNode;
					pFlowNode->SetFlag(EHYPER_NODE_GRAPH_ENTITY, false);
					pFlowNode->SetFlag(EHYPER_NODE_GRAPH_ENTITY2, true);
				}
				break;
			case ID_GOTO_MODULE:
				{
					CString moduleName;
					pPort->pVar->Get(moduleName);
					HandleGotoModule(moduleName.GetBuffer());
				}
				break;
			case ID_GRAPHVIEW_TARGET_SELECTED_PREFAB:
			case ID_GRAPHVIEW_TARGET_UNSELECT_PREFAB:
				{
					HandlePrefabMenu(pNode, cmd);
				}
				break;
			case ID_GOTO_TOKEN:
			case ID_TOKEN_FIND_USAGES:
				{
					CString tokenName;
					pPort->pVar->Get(tokenName);
					HandleGameTokenMenu(pFlowNode->GetIFlowGraph(), tokenName, cmd);
				}
				break;
			default:
				HandleEntityMenu(pNode, cmd);
				break;
			}
		}
		else // node title or background
		{
			if (strcmp(pNode->GetClassName(), "Prefab:Instance") == 0)
			{
				AddPrefabMenu(pNode, menu);
			}
			else
			{
				AddEntityMenu(pNode, menu);
			}

			// module nodes (call or start/end)
			const string nodeClassName = pNode->GetClassName();
			if (!nodeClassName.empty() && nodeClassName.find("Module:Call_") != string::npos)
			{
				AddModuleMenu(pNode, menu);
			}
			if (!nodeClassName.empty() && (nodeClassName.find("Start_") != string::npos || nodeClassName.find("End_") != string::npos))
			{
				menu.AppendMenu(MF_STRING, ID_TOOLS_EDITMODULE, "Edit Module Ports ...");
			}

			if (m_pGraph->IsSelectedAny())
			{
				menu.AppendMenu(MF_STRING, ID_GRAPHVIEW_ADD_COMMENTBOX, "Add Comment Box");
				menu.AppendMenu(MF_STRING, ID_GRAPHVIEW_ADD_BLACK_BOX, "Add BlackBox");
				AddSelectionMenu(menu, selectionMenu);
				menu.AppendMenu(MF_SEPARATOR);


			}

			menu.AppendMenu(MF_STRING, ID_GRAPHVIEW_RENAME, "Rename");

			// Inputs
			menu.AppendMenu(MF_SEPARATOR);
			inputsMenu.CreatePopupMenu();
			menu.AppendMenu(MF_POPUP, (UINT_PTR)inputsMenu.GetSafeHmenu(), "Inputs");
			for (int i = 0; i < pNode->GetInputs()->size(); i++)
			{
				CHyperNodePort* pPort = &pNode->GetInputs()->at(i);
				inputsMenu.AppendMenu(MF_STRING |
				                      ((pPort->bVisible || pPort->nConnected != 0) ? MF_CHECKED : 0) |
				                      ((pPort->nConnected != 0) ? MF_GRAYED : 0),
				                      BASE_INPUTS_CMD + i, pPort->GetHumanName());
			}
			{
				int numEnabled = 0;
				int numDisabled = 0;
				std::vector<CHyperEdge*> edges;
				if (m_pGraph->FindEdges(pNode, edges))
				{
					for (int i = 0; i < edges.size(); ++i)
					{
						if (pNode == m_pGraph->FindNode(edges[i]->nodeIn))
						{
							if (edges[i]->enabled)
								numEnabled++;
							else
								numDisabled++;
						}
					}
				}
				inputsMenu.AppendMenu(MF_SEPARATOR);
				inputsMenu.AppendMenu(MF_STRING | (numEnabled ? 0 : MF_GRAYED), ID_INPUTS_DISABLE_LINKS, "Disable Links");
				inputsMenu.AppendMenu(MF_STRING | (numDisabled ? 0 : MF_GRAYED), ID_INPUTS_ENABLE_LINKS, "Enable Links");
				inputsMenu.AppendMenu(MF_SEPARATOR);
				inputsMenu.AppendMenu(MF_STRING | ((numEnabled || numDisabled) ? 0 : MF_GRAYED), ID_INPUTS_CUT_LINKS, "Cut Links");
				inputsMenu.AppendMenu(MF_STRING | ((numEnabled || numDisabled) ? 0 : MF_GRAYED), ID_INPUTS_COPY_LINKS, "Copy Links");
				inputsMenu.AppendMenu(MF_STRING | (bCanPaste ? 0 : MF_GRAYED), ID_INPUTS_PASTE_LINKS, "Paste Links");
				inputsMenu.AppendMenu(MF_STRING | ((numEnabled || numDisabled) ? 0 : MF_GRAYED), ID_INPUTS_DELETE_LINKS, "Delete Links");
			}
			inputsMenu.AppendMenu(MF_SEPARATOR);
			inputsMenu.AppendMenu(MF_STRING, ID_INPUTS_SHOW_ALL, "Show All");
			inputsMenu.AppendMenu(MF_STRING, ID_INPUTS_HIDE_ALL, "Hide All");

			// Outputs
			outputsMenu.CreatePopupMenu();
			menu.AppendMenu(MF_POPUP, (UINT_PTR)outputsMenu.GetSafeHmenu(), "Outputs");
			for (int i = 0; i < pNode->GetOutputs()->size(); i++)
			{
				CHyperNodePort* pPort = &pNode->GetOutputs()->at(i);
				outputsMenu.AppendMenu(MF_STRING |
				                       ((pPort->bVisible || pPort->nConnected != 0) ? MF_CHECKED : 0) |
				                       ((pPort->nConnected != 0) ? MF_GRAYED : 0),
				                       BASE_OUTPUTS_CMD + i, pPort->GetHumanName());
			}
			{
				int numEnabled = 0;
				int numDisabled = 0;
				std::vector<CHyperEdge*> edges;
				if (m_pGraph->FindEdges(pNode, edges))
				{
					for (int i = 0; i < edges.size(); ++i)
					{
						if (pNode == m_pGraph->FindNode(edges[i]->nodeOut))
						{
							if (edges[i]->enabled)
								numEnabled++;
							else
								numDisabled++;
						}
					}
				}
				outputsMenu.AppendMenu(MF_SEPARATOR);
				outputsMenu.AppendMenu(MF_STRING | (numEnabled ? 0 : MF_GRAYED), ID_OUTPUTS_DISABLE_LINKS, "Disable Links");
				outputsMenu.AppendMenu(MF_STRING | (numDisabled ? 0 : MF_GRAYED), ID_OUTPUTS_ENABLE_LINKS, "Enable Links");
				outputsMenu.AppendMenu(MF_SEPARATOR);
				outputsMenu.AppendMenu(MF_STRING | ((numEnabled || numDisabled) ? 0 : MF_GRAYED), ID_OUTPUTS_CUT_LINKS, "Cut Links");
				outputsMenu.AppendMenu(MF_STRING | ((numEnabled || numDisabled) ? 0 : MF_GRAYED), ID_OUTPUTS_COPY_LINKS, "Copy Links");
				outputsMenu.AppendMenu(MF_STRING | (bCanPaste ? 0 : MF_GRAYED), ID_OUTPUTS_PASTE_LINKS, "Paste Links");
				outputsMenu.AppendMenu(MF_STRING | ((numEnabled || numDisabled) ? 0 : MF_GRAYED), ID_OUTPUTS_DELETE_LINKS, "Delete Links");
			}
			outputsMenu.AppendMenu(MF_SEPARATOR);
			outputsMenu.AppendMenu(MF_STRING, ID_OUTPUTS_SHOW_ALL, "Show All");
			outputsMenu.AppendMenu(MF_STRING, ID_OUTPUTS_HIDE_ALL, "Hide All");
		}
	}
	else // not a flow node or port
	{
		PopulateClassMenu(classMenu, classes);
		menu.AppendMenu(MF_POPUP, (UINT_PTR)classMenu.pMenu->GetSafeHmenu(), "Add Node");
		menu.AppendMenu(MF_STRING, ID_GRAPHVIEW_ADD_SELECTED_ENTITY, "Add Selected Entity");
		if (m_pGraph->GetAIAction() == 0)
		{
			menu.AppendMenu(MF_STRING, ID_GRAPHVIEW_ADD_DEFAULT_ENTITY, "Add Graph Default Entity");
		}
		commentsMenu.CreatePopupMenu();
		menu.AppendMenu(MF_POPUP, (UINT_PTR)commentsMenu.GetSafeHmenu(), "Add Comment");

		commentsMenu.AppendMenu(MF_STRING, ID_GRAPHVIEW_ADD_COMMENT, "Add Simple Comment");
		commentsMenu.AppendMenu(MF_STRING, ID_GRAPHVIEW_ADD_COMMENTBOX, "Add Comment Box");
		commentsMenu.AppendMenu(MF_STRING, ID_GRAPHVIEW_ADD_BLACK_BOX, "Add BlackBox");

		menu.AppendMenu(MF_STRING, ID_GRAPHVIEW_ADD_TRACKEVENT, "Add Track Event Node");
		menu.AppendMenu(MF_STRING, ID_GRAPHVIEW_ADD_STARTNODE, "Add Start Node");

		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, ID_FILE_IMPORT, "Import");

		AddSelectionMenu(menu, selectionMenu);
	}

	if (!isPort)
	{
		// copy paste
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, ID_EDIT_CUT, "Cut");
		menu.AppendMenu(MF_STRING, ID_EDIT_COPY, "Copy");
		menu.AppendMenu(MF_STRING | (bCanPaste ? 0 : MF_GRAYED), ID_EDIT_PASTE_WITH_LINKS, "Paste with Links");
		menu.AppendMenu(MF_STRING | (bCanPaste ? 0 : MF_GRAYED), ID_EDIT_PASTE, "Paste without Links");
		menu.AppendMenu(MF_STRING, ID_EDIT_DELETE, "Delete");

		// misc
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, ID_GRAPHVIEW_FIT_TOVIEW, "Fit Graph to View");
	}


	int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY, screenPoint.x, screenPoint.y, this);

	if (cmd >= BASE_NEW_NODE_CMD && cmd < BASE_NEW_NODE_CMD + classes.size())
	{
		CString cls = classes[cmd - BASE_NEW_NODE_CMD];
		CreateNode(cls, point);
		return;
	}

	if (cmd >= BASE_INPUTS_CMD && cmd < BASE_INPUTS_CMD + 1000)
	{
		if (pNode)
		{
			CUndo undo("Graph Port Visibilty");
			pNode->RecordUndo();
			int nPort = cmd - BASE_INPUTS_CMD;
			pNode->GetInputs()->at(nPort).bVisible = !pNode->GetInputs()->at(nPort).bVisible;
			InvalidateNode(pNode, true);
			Invalidate();
		}
		return;
	}
	if (cmd >= BASE_OUTPUTS_CMD && cmd < BASE_OUTPUTS_CMD + 1000)
	{
		if (pNode)
		{
			CUndo undo("Graph Port Visibilty");
			pNode->RecordUndo();
			int nPort = cmd - BASE_OUTPUTS_CMD;
			pNode->GetOutputs()->at(nPort).bVisible = !pNode->GetOutputs()->at(nPort).bVisible;
			InvalidateNode(pNode, true);
			Invalidate();
		}
		return;
	}

	switch (cmd)
	{
	case ID_EDIT_CUT:
		OnCommandCut();
		break;
	case ID_EDIT_COPY:
		OnCommandCopy();
		break;
	case ID_EDIT_PASTE:
		InternalPaste(false, point);
		break;
	case ID_EDIT_PASTE_WITH_LINKS:
		InternalPaste(true, point);
		break;
	case ID_EDIT_DELETE:
		OnCommandDelete();
		break;
	case ID_GRAPHVIEW_RENAME:
		RenameNode(pNode);
		break;
	case ID_FILE_EXPORTSELECTION:
	case ID_FILE_IMPORT:
		if (GetParent())
			GetParent()->SendMessage(WM_COMMAND, MAKEWPARAM(cmd, 0), NULL);
		break;
	case ID_GRAPHVIEW_ADD_SELECTED_ENTITY:
		{
			CreateNode("selected_entity", point);
		}
		break;
	case ID_GRAPHVIEW_ADD_DEFAULT_ENTITY:
		{
			CreateNode("default_entity", point);
		}
		break;
	case ID_GRAPHVIEW_ADD_COMMENT:
		{
			auto pNode = CreateNode<CCommentNode>(point);
			if (pNode)
				RenameNode(pNode);
		}
		break;
	case ID_GRAPHVIEW_ADD_COMMENTBOX:
		{
			std::vector<CHyperNode*> nodes;
			GetSelectedNodes(nodes, SELECTION_SET_ONLY_PARENTS);

			auto pNewNode = CreateNode<CCommentBoxNode>(point);

			// if there are selected nodes, and they are on view, the created commentBox encloses all of them
			if (nodes.size() > 1)
			{
				Gdiplus::RectF totalRect = nodes[0]->GetRect();
				for (int i = 1; i < nodes.size(); ++i)
				{
					CHyperNode* pSelNode = nodes[i];
					totalRect.Union(totalRect, totalRect, pSelNode->GetRect());
				}
				totalRect.Width = ceilf(totalRect.Width);
				totalRect.Height = ceilf(totalRect.Height);

				CRect screenRc;
				GetClientRect(screenRc);
				CRect selectRc = LocalToViewRect(totalRect);
				CRect intersectionRc;
				bool selectedNodesAreOnView = intersectionRc.IntersectRect(screenRc, selectRc);

				if (selectedNodesAreOnView)
				{
					const float MARGIN = 10;
					totalRect.Inflate(MARGIN, MARGIN);
					pNewNode->SetBorderRect(totalRect);
				}
			}

			if (pNewNode)
				RenameNode(pNewNode);
		}
		break;
	case ID_GRAPHVIEW_ADD_BLACK_BOX:
		{
			std::vector<CHyperNode*> nodes;
			GetSelectedNodes(nodes, SELECTION_SET_ONLY_PARENTS);
			if (nodes.size() > 0)
			{
				for (int i = 0; i < nodes.size(); ++i)
				{
					if (strcmp(nodes[i]->GetClassName(), CBlackBoxNode::GetClassType()) == 0)
						return;
				}
				auto pNode = CreateNode<CBlackBoxNode>(point);
				if (pNode)
				{
					RenameNode(pNode);
					CBlackBoxNode* pBB = (CBlackBoxNode*)pNode;
					for (int n = 0; n < nodes.size(); ++n)
						pBB->AddNode((CFlowNode*)(nodes[n]));
				}
			}
		}
		break;
	case ID_GRAPHVIEW_ADD_TRACKEVENT:
		{
			CreateNode<CTrackEventNode>(point);
		}
		break;
	case ID_GRAPHVIEW_ADD_STARTNODE:
		{
			CreateNode("Game:Start", point);
		}
		break;
	case ID_GRAPHVIEW_FIT_TOVIEW:
		OnCommandFitAll();
		break;
	case ID_GRAPHVIEW_SELECT_ENTITY:
		OnSelectEntity();
		break;
	case ID_GRAPHVIEW_SELECT_AND_GOTO_ENTITY:
		{
			OnSelectEntity();
			CViewport* vp = GetIEditorImpl()->GetActiveView();
			if (vp)
				vp->CenterOnSelection();
		}
		break;
	case ID_GRAPHVIEW_SELECTION_EDGES_ENABLE:
	case ID_GRAPHVIEW_SELECTION_EDGES_DISABLE:
		{
			std::set<CHyperEdge*> edges;
			std::vector<CHyperEdge*> edgesTemp;
			std::vector<CHyperNode*> nodes;
			if (GetSelectedNodes(nodes))
			{
				for (CHyperNode* pNode : nodes)
				{
					if (m_pGraph->FindEdges(pNode, edgesTemp))
						edges.insert(edgesTemp.begin(), edgesTemp.end());
				}
			}

			for (CHyperEdge* pEdge : edges)
			{
				m_pGraph->EnableEdge(pEdge, cmd == ID_GRAPHVIEW_SELECTION_EDGES_ENABLE);
			}
		}
		break;
	case ID_OUTPUTS_SHOW_ALL:
	case ID_OUTPUTS_HIDE_ALL:
		if (pNode)
		{
			CUndo undo("Graph Port Visibilty");
			pNode->RecordUndo();
			for (int i = 0; i < pNode->GetOutputs()->size(); i++)
			{
				pNode->GetOutputs()->at(i).bVisible = (cmd == ID_OUTPUTS_SHOW_ALL) || (pNode->GetOutputs()->at(i).nConnected != 0);
			}
			InvalidateNode(pNode, true);
			Invalidate();
		}
		break;
	case ID_INPUTS_SHOW_ALL:
	case ID_INPUTS_HIDE_ALL:
		if (pNode)
		{
			CUndo undo("Graph Port Visibilty");
			pNode->RecordUndo();
			for (int i = 0; i < pNode->GetInputs()->size(); i++)
			{
				pNode->GetInputs()->at(i).bVisible = (cmd == ID_INPUTS_SHOW_ALL) || (pNode->GetInputs()->at(i).nConnected != 0);
			}
			InvalidateNode(pNode, true);
			Invalidate();
		}
		break;
	case ID_OUTPUTS_DISABLE_LINKS:
	case ID_OUTPUTS_ENABLE_LINKS:
		if (pNode)
		{
			CUndo undo("Enabling Graph Node Links");
			pNode->RecordUndo();

			std::vector<CHyperEdge*> edges;
			if (m_pGraph->FindEdges(pNode, edges))
			{
				for (int i = 0; i < edges.size(); ++i)
				{
					if (pNode == m_pGraph->FindNode(edges[i]->nodeOut))
					{
						m_pGraph->EnableEdge(edges[i], cmd == ID_OUTPUTS_ENABLE_LINKS);
						InvalidateEdge(edges[i]);
					}
				}
			}
		}
		break;
	case ID_INPUTS_DISABLE_LINKS:
	case ID_INPUTS_ENABLE_LINKS:
		if (pNode)
		{
			CUndo undo("Enabling Graph Node Links");
			pNode->RecordUndo();

			std::vector<CHyperEdge*> edges;
			if (m_pGraph->FindEdges(pNode, edges))
			{
				for (int i = 0; i < edges.size(); ++i)
				{
					if (pNode == m_pGraph->FindNode(edges[i]->nodeIn))
					{
						m_pGraph->EnableEdge(edges[i], cmd == ID_INPUTS_ENABLE_LINKS);
						InvalidateEdge(edges[i]);
					}
				}
			}
		}
		break;
	case ID_INPUTS_CUT_LINKS:
	case ID_OUTPUTS_CUT_LINKS:
		{
			CUndo undo("Cut Graph Node Links");
			CopyLinks(pNode, cmd == ID_INPUTS_CUT_LINKS);
			DeleteLinks(pNode, cmd == ID_INPUTS_CUT_LINKS);
		}
		break;
	case ID_INPUTS_COPY_LINKS:
	case ID_OUTPUTS_COPY_LINKS:
		CopyLinks(pNode, cmd == ID_INPUTS_COPY_LINKS);
		break;
	case ID_INPUTS_PASTE_LINKS:
	case ID_OUTPUTS_PASTE_LINKS:
		{
			CUndo undo("Paste Graph Node Links");
			m_pGraph->RecordUndo();
			PasteLinks(pNode, cmd == ID_INPUTS_PASTE_LINKS);
		}
		break;
	case ID_INPUTS_DELETE_LINKS:
	case ID_OUTPUTS_DELETE_LINKS:
		{
			CUndo undo("Delete Graph Node Links");
			DeleteLinks(pNode, cmd == ID_INPUTS_DELETE_LINKS);
		}
		break;
	case ID_GOTO_MODULE:
	case ID_TOOLS_EDITMODULE:
		{
			HandleModuleMenu(pNode, cmd);
		}
		break;
	case ID_GRAPHVIEW_TARGET_SELECTED_PREFAB:
	case ID_GRAPHVIEW_TARGET_UNSELECT_PREFAB:
		{
			HandlePrefabMenu(pNode, cmd);
		}
		break;
	default:
		HandleEntityMenu(pNode, cmd);
		break;
	}
}

struct ViewNodeFilter
{
public:
	ViewNodeFilter(uint32 mask) : mask(mask) {}
	bool Visit(CHyperNode* pNode)
	{
		if (pNode->IsEditorSpecialNode())
			return false;
		CFlowNode* pFlowNode = static_cast<CFlowNode*>(pNode);
		if ((pFlowNode->GetCategory() & mask) == 0)
			return false;
		return true;

		// Only if the usage mask is set check if fulfilled -> this is an exclusive thing
		if ((mask & EFLN_USAGE_MASK) != 0 && (pFlowNode->GetUsageFlags() & mask) == 0)
			return false;
		return true;
	}
	uint32 mask;
};

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::PopulateClassMenu(SClassMenuGroup& classMenu, std::vector<CString>& classes)
{
	ViewNodeFilter filter(m_componentViewMask);
	std::vector<THyperNodePtr> prototypes;
	m_pGraph->GetManager()->GetPrototypesEx(prototypes, true, functor_ret(filter, &ViewNodeFilter::Visit));
	classes.resize(0);
	classes.reserve(prototypes.size());
	for (std::vector<THyperNodePtr>::iterator iter = prototypes.begin(); iter != prototypes.end(); ++iter)
	{
		if ((*iter)->IsFlowNode())
		{
			CString fullname = (*iter)->GetClassName();
			if (fullname.Find(':') >= 0)
			{
				classes.push_back(fullname);
			}
			else
			{
				CString name = "Misc:";
				name += fullname;
				name += "0x2410";
				classes.push_back(name);
			}
		}
	}
	std::sort(classes.begin(), classes.end());

	PopulateSubClassMenu(classMenu, classes, BASE_NEW_NODE_CMD);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::PopulateSubClassMenu(SClassMenuGroup& classMenu, std::vector<CString>& classes, int baseIndex)
{
	std::map<CString, SClassMenuGroup*> groupMap;
	std::list<CString> subGroups;
	for (int i = 0; i < classes.size(); i++)
	{
		const CString& fullname = classes[i];
		CString group, node;
		if (fullname.Find(':') >= 0)
		{
			group = fullname.SpanExcluding(":");
			node = fullname.Mid(group.GetLength() + 1);
			int marker = node.Find("0x2410");
			if (marker > 0)
			{
				node = node.Left(marker);
				classes[i] = node;
			}
		}
		else
		{
			group = "_UNKNOWN_"; // should never happen
			node = fullname;
			// assert (false);
		}

		SClassMenuGroup* pGroupMenu = &classMenu;
		if (!group.IsEmpty())
		{
			pGroupMenu = stl::find_in_map(groupMap, group, (SClassMenuGroup*)0);
			if (!pGroupMenu)
			{
				pGroupMenu = classMenu.CreateSubMenu();
				classMenu.pMenu->AppendMenu(MF_POPUP, (UINT_PTR)pGroupMenu->pMenu->GetSafeHmenu(), group);
				groupMap[group] = pGroupMenu;
			}
		}
		else
		{
			assert(false); // should never happen
			continue;
		}

		if (node.Find(':') >= 0)
		{
			CString parentGroup = node.SpanExcluding(":");
			if (!stl::find(subGroups, parentGroup))
			{
				subGroups.push_back(parentGroup);
				std::vector<CString> subclasses;
				for (std::vector<CString>::iterator iter = classes.begin(); iter != classes.end(); ++iter)
				{
					CString subclass = iter->Mid(group.GetLength() + 1);
					if (subclass.Find(':') >= 0 && subclass.SpanExcluding(":") == parentGroup)
						subclasses.push_back(subclass);
				}
				if (subclasses.size() > 0)
				{
					PopulateSubClassMenu(*pGroupMenu, subclasses, i + baseIndex);
				}
			}

		}
		else
			pGroupMenu->pMenu->AppendMenu(MF_STRING, i + baseIndex, node);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::ShowPortsConfigMenu(CPoint point, bool bInput, CHyperNode* pNode)
{
	CMenu menu;
	menu.CreatePopupMenu();

	CHyperNode::Ports* pPorts;
	if (bInput)
	{
		pPorts = pNode->GetInputs();
		menu.AppendMenu(MF_STRING | MF_GRAYED, 0, "Inputs");
	}
	else
	{
		pPorts = pNode->GetOutputs();
		menu.AppendMenu(MF_STRING | MF_GRAYED, 0, "Outputs");
	}
	menu.AppendMenu(MF_SEPARATOR);

	for (int i = 0; i < pPorts->size(); i++)
	{
		CHyperNodePort* pPort = &(*pPorts)[i];
		int flags = MF_STRING;
		if (pPort->bVisible || pPort->nConnected != 0)
			flags |= MF_CHECKED;
		if (pPort->nConnected != 0)
			flags |= MF_GRAYED;
		menu.AppendMenu(flags, i + 1, pPort->pVar->GetName());
	}
	menu.AppendMenu(MF_SEPARATOR);
	if (bInput)
	{
		menu.AppendMenu(MF_STRING, ID_INPUTS_SHOW_ALL, "Show All");
		menu.AppendMenu(MF_STRING, ID_INPUTS_HIDE_ALL, "Hide All");
	}
	else
	{
		menu.AppendMenu(MF_STRING, ID_OUTPUTS_SHOW_ALL, "Show All");
		menu.AppendMenu(MF_STRING, ID_OUTPUTS_HIDE_ALL, "Hide All");
	}

	ClientToScreen(&point);

	int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY, point.x, point.y, this);
	switch (cmd)
	{
	case ID_OUTPUTS_SHOW_ALL:
	case ID_OUTPUTS_HIDE_ALL:
		if (pNode)
		{
			CUndo undo("Graph Port Visibilty");
			pNode->RecordUndo();
			for (int i = 0; i < pNode->GetOutputs()->size(); i++)
			{
				pNode->GetOutputs()->at(i).bVisible = (cmd == ID_OUTPUTS_SHOW_ALL) || (pNode->GetOutputs()->at(i).nConnected != 0);
			}
			InvalidateNode(pNode, true);
			Invalidate();
			return;
		}
		break;
	case ID_INPUTS_SHOW_ALL:
	case ID_INPUTS_HIDE_ALL:
		if (pNode)
		{
			CUndo undo("Graph Port Visibilty");
			pNode->RecordUndo();
			for (int i = 0; i < pNode->GetInputs()->size(); i++)
			{
				pNode->GetInputs()->at(i).bVisible = (cmd == ID_INPUTS_SHOW_ALL) || (pNode->GetInputs()->at(i).nConnected != 0);
			}
			InvalidateNode(pNode, true);
			Invalidate();
			return;
		}
	}
	if (cmd > 0)
	{
		CUndo undo("Graph Port Visibilty");
		pNode->RecordUndo();
		CHyperNodePort* pPort = &(*pPorts)[cmd - 1];
		pPort->bVisible = !pPort->bVisible || (pPort->nConnected != 0);
	}
	InvalidateNode(pNode, true);
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::UpdateWorkingArea()
{
	m_workingArea = Gdiplus::RectF(0, 0, 0, 0);
	if (!m_pGraph)
	{
		m_scrollOffset.SetPoint(0, 0);
		return;
	}

	bool bFirst = true;
	IHyperGraphEnumerator* pEnum = m_pGraph->GetNodesEnumerator();
	for (IHyperNode* pINode = pEnum->GetFirst(); pINode; pINode = pEnum->GetNext())
	{
		const Gdiplus::RectF& itemRect = ((CHyperNode*)pINode)->GetRect();
		if (bFirst)
		{
			m_workingArea = itemRect;
			bFirst = false;
		}
		else
		{
			Gdiplus::RectF rc = m_workingArea;
			m_workingArea.Union(m_workingArea, rc, itemRect);
		}
	}
	pEnum->Release();

	m_workingArea = Gdiplus::RectF(m_workingArea.X - GRID_SIZE, m_workingArea.Y - GRID_SIZE, m_workingArea.Width + GRID_SIZE * 2, m_workingArea.Height + GRID_SIZE * 2);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::SetScrollExtents(bool isUpdateWorkingArea)
{
	if (isUpdateWorkingArea)
		UpdateWorkingArea();

	static bool bNoRecurse = false;
	if (bNoRecurse)
		return;
	bNoRecurse = true;

	// Update scroll.
	CRect rcClient;
	GetClientRect(rcClient);

	Gdiplus::RectF rect = ViewToLocalRect(rcClient);
	rect.Union(rect, rect, m_workingArea);
	CRect rc = LocalToViewRect(rect);

	SCROLLINFO si;
	ZeroStruct(si);
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMax = rc.Width();
	si.nPage = rcClient.Width() + 3;
	si.nPos = -rc.left;
	SetScrollInfo(SB_HORZ, &si, TRUE);

	si.nMax = rc.Height();
	si.nPage = rcClient.Height() + 3;
	si.nPos = -rc.top;
	SetScrollInfo(SB_VERT, &si, TRUE);

	bNoRecurse = false;

	if (m_pGraph && !rcClient.IsRectEmpty())
		m_pGraph->SetViewPosition(m_scrollOffset, m_zoom);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	HideEditPort();

	SCROLLINFO si;
	GetScrollInfo(SB_HORZ, &si);
	int curPos = si.nPos;

	switch (nSBCode)
	{
	case SB_LINELEFT:
		curPos -= 10;
		break;
	case SB_LINERIGHT:
		curPos += 10;
		break;
	case SB_PAGELEFT:
		curPos -= si.nPage;
		break;
	case SB_PAGERIGHT:
		curPos += si.nPage;
		break;
	case SB_THUMBPOSITION:
	case SB_THUMBTRACK:
		curPos = nPos;
		break;
	}

	m_scrollOffset.x += si.nPos - min(int(si.nMax - si.nPage), max(int(si.nMin), curPos));
	SetScrollExtents(false);
	Invalidate();

	__super::OnHScroll(nSBCode, nPos, pScrollBar);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	HideEditPort();

	SCROLLINFO si;
	GetScrollInfo(SB_VERT, &si);
	int curPos = si.nPos;

	switch (nSBCode)
	{
	case SB_LINEUP:
		curPos -= 10;
		break;
	case SB_LINEDOWN:
		curPos += 10;
		break;
	case SB_PAGEUP:
		curPos -= si.nPage;
		break;
	case SB_PAGEDOWN:
		curPos += si.nPage;
		break;
	case SB_THUMBPOSITION:
	case SB_THUMBTRACK:
		curPos = nPos;
		break;
	}

	m_scrollOffset.y += si.nPos - min(int(si.nMax - si.nPage), max(int(si.nMin), curPos));
	SetScrollExtents(false);
	Invalidate();

	__super::OnVScroll(nSBCode, nPos, pScrollBar);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::RenameNode(CHyperNode* pNode)
{
	assert(pNode);
	if (HandleRenameNode(pNode))
		return;

	m_pRenameNode = pNode;
	CRect rc = LocalToViewRect(pNode->GetRect());
	rc.DeflateRect(1, 1);
	rc.bottom = rc.top + DEFAULT_EDIT_HEIGHT;
	if (m_renameEdit.m_hWnd)
		m_renameEdit.DestroyWindow();

	m_renameEdit.SetView(this);
	int nEditFlags = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL;
	if (pNode->IsEditorSpecialNode())
	{
		nEditFlags |= ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN;
		rc.bottom += 42;
	}
	m_renameEdit.Create(nEditFlags, rc, this, IDC_RENAME);
	m_renameEdit.SetWindowText(m_pRenameNode->GetName());
	m_renameEdit.SetFont(CFont::FromHandle((HFONT)SMFCFonts::GetInstance().hSystemFont));
	m_renameEdit.SetCapture();
	m_renameEdit.SetSel(0, -1);
	m_renameEdit.SetFocus();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnAcceptRename()
{
	CString text;
	m_renameEdit.GetWindowText(text);
	if (m_pRenameNode)
	{
		text.Replace("\n", "\\n");
		text.Replace("\r", "");
		text.Replace(" ", "_");
		m_pRenameNode->SetName(text);
		InvalidateNode(m_pRenameNode);
	}
	m_pRenameNode = NULL;
	m_renameEdit.DestroyWindow();
	Invalidate();
	OnNodeRenamed();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnCancelRename()
{
	m_pRenameNode = NULL;
	m_renameEdit.DestroyWindow();
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnSelectionChange()
{
	if (!m_pPropertiesCtrl)
		return;

	m_pEditedNode = 0;
	m_pMultiEditedNodes.resize(0);
	m_pMultiEditVars = 0;

	std::vector<CHyperNode*> nodes;
	GetSelectedNodes(nodes, SELECTION_SET_ONLY_PARENTS);
	m_pPropertiesCtrl->RemoveAllItems();

	if (nodes.size() >= 1 && !m_bIsFrozen)
	{
		CHyperNode* pNode = nodes[0];
		_smart_ptr<CVarBlock> pVarBlock = pNode->GetInputsVarBlock();
		if (pVarBlock && pNode->GetTypeId() == gEnv->pFlowSystem->GetTypeId("Engine:LayerSwitch"))
			SetupLayerList(pVarBlock);

		if (nodes.size() == 1)
		{
			m_pEditedNode = pNode;

			if (pVarBlock)
			{
				m_pPropertiesCtrl->AddVarBlock(pVarBlock);
				m_pPropertiesCtrl->SetDisplayOnlyModified(false);
			}
		}
		else
		{
			bool isMultiselect = false;
			// Find if we have nodes the same type like the first one
			for (int i = 1; i < nodes.size(); ++i)
			{
				if (pNode->GetTypeId() == nodes[i]->GetTypeId())
				{
					isMultiselect = true;
					break;
				}
			}

			if (isMultiselect)
			{
				if (pVarBlock)
				{
					m_pMultiEditVars = pVarBlock->Clone(true);
					m_pPropertiesCtrl->AddVarBlock(m_pMultiEditVars);
				}

				for (int i = 0; i < nodes.size(); ++i)
				{
					if (pNode->GetTypeId() == nodes[i]->GetTypeId())
					{
						m_pMultiEditedNodes.push_back(nodes[i]);
						m_pMultiEditVars->Wire(nodes[i]->GetInputsVarBlock());
					}
				}

				m_pPropertiesCtrl->SetDisplayOnlyModified(true);
			}
		}
	}

	CWnd* pWnd = GetParent();
	if (pWnd->IsKindOf(RUNTIME_CLASS(CHyperGraphDialog)))
	{
		((CHyperGraphDialog*)pWnd)->OnViewSelectionChange();
	}
}

//////////////////////////////////////////////////////////////////////////
THyperNodePtr CHyperGraphView::CreateNode(const CString& sNodeClass, CPoint point)
{
	if (!m_pGraph)
		return 0;

	// Snap to the grid.
	Gdiplus::PointF p = ViewToLocal(point);
	{
		p.X = floor(((float)p.X / GRID_SIZE) + 0.5f) * GRID_SIZE;
		p.Y = floor(((float)p.Y / GRID_SIZE) + 0.5f) * GRID_SIZE;
	}

	THyperNodePtr pNode = NULL;
	if (sNodeClass == CQuickSearchNode::GetClassType())
	{
		std::unique_ptr<CUndo> pUndo;
		if (strcmp(sNodeClass, CQuickSearchNode::GetClassType()))
		{
			pUndo = stl::make_unique<CUndo>("New Graph Node");
			m_pGraph->RecordUndo();
		}

		m_pGraph->UnselectAll();
		pNode = static_cast<CHyperNode*>(m_pGraph->CreateNode(sNodeClass, p));
	}
	else
	{

		m_pGraph->UnselectAll();
		
		std::unique_ptr<CUndo> pUndo;
		pUndo = stl::make_unique<CUndo>("New Graph Node");
		m_pGraph->RecordUndo();

		pNode = static_cast<CHyperNode*>(m_pGraph->CreateNode(sNodeClass, p));
	}
	if (pNode)
	{
		pNode->SetSelected(true);
		OnSelectionChange();
	}
	InvalidateView();
	return pNode;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::UpdateNodeProperties(CHyperNode* pNode, IVariable* pVar)
{
	if (pVar->GetDataType() == IVariable::DT_UIENUM && pNode->GetTypeId() == gEnv->pFlowSystem->GetTypeId("Engine:LayerSwitch"))
	{
		CString val;
		pVar->Get(val);
		_smart_ptr<CVarBlock> pVarBlock = pNode->GetInputsVarBlock();
		IVariable* pVarInternal = pVarBlock->FindVariable("Layer");
		if (pVarInternal)
			pVarInternal->Set(val);
	}

	// TODO: ugliest way to solve this! I should find a better solution... [Dejan]
	if (pVar->GetDataType() == IVariable::DT_SOCLASS)
	{
		CString className;
		pVar->Get(className);
		CHyperNodePort* pHelperInputPort = pNode->FindPort("sohelper_helper", true);
		if (!pHelperInputPort)
			pHelperInputPort = pNode->FindPort("sonavhelper_helper", true);
		if (pHelperInputPort && pHelperInputPort->pVar)
		{
			CString helperName;
			pHelperInputPort->pVar->Get(helperName);
			int f = helperName.Find(':');
			if (f <= 0 || className != helperName.Left(f))
			{
				helperName = className + ':';
				pHelperInputPort->pVar->Set(helperName);
			}
		}
	}

	pNode->OnInputsChanged();
	InvalidateNode(pNode, true);  // force redraw node as param values have changed
	if (m_pGraph)
		m_pGraph->SendNotifyEvent(pNode, EHG_NODE_PROPERTIES_CHANGED);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnUpdateProperties(IVariable* pVar)
{
	if (m_pMultiEditedNodes.size())
	{
		for (std::vector<CHyperNode*>::iterator ppNode = m_pMultiEditedNodes.begin(); ppNode != m_pMultiEditedNodes.end(); ++ppNode)
			if (*ppNode)
				UpdateNodeProperties(*ppNode, pVar);
	}
	else if (m_pEditedNode)
	{
		UpdateNodeProperties(m_pEditedNode, pVar);
	}

	if (m_pGraph && m_pGraph->IsFlowGraph())
	{
		CHyperFlowGraph* pFlowGraph = static_cast<CHyperFlowGraph*>(m_pGraph);
		CEntityObject* pEntity = pFlowGraph->GetEntity();
		if (pEntity)
			pEntity->SetLayerModified();
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::InvalidateView(bool bComplete)
{
	std::vector<CHyperEdge*> edges;
	if (m_pGraph)
		m_pGraph->GetAllEdges(edges);
	std::copy(edges.begin(), edges.end(), std::inserter(m_edgesToReroute, m_edgesToReroute.end()));

	if (bComplete && m_pGraph)
	{
		IHyperGraphEnumerator* pEnum = m_pGraph->GetNodesEnumerator();
		for (IHyperNode* pINode = pEnum->GetFirst(); pINode; pINode = pEnum->GetNext())
		{
			CHyperNode* pNode = (CHyperNode*)pINode;
			pNode->Invalidate(true, false);
		}
		pEnum->Release();
	}

	Invalidate();
	SetScrollExtents();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::FitFlowGraphToView()
{
	OnCommandFitAll();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::ForceRedraw()
{
	if (!m_pGraph)
		return;

	InvalidateView(true);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnCommandDelete()
{
	SStartNodeOperationBatching startBatching(this);
	if (!m_pGraph)
		return;

	CUndo undo("HyperGraph Delete Node(s)");
	m_pGraph->RecordUndo();
	std::vector<CHyperNode*> nodes;
	if (!GetSelectedNodes(nodes, SELECTION_SET_INCLUDE_RELATIVES))
		return;

	for (int i = 0; i < nodes.size(); i++)
	{
		if (!nodes[i]->CheckFlag(EHYPER_NODE_UNREMOVEABLE))
		{
			m_pGraph->RemoveNode(nodes[i]);
		}
	}
	OnSelectionChange();
	InvalidateView();
	PropagateChangesToGroupsAndPrefabs();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnCommandDeleteKeepLinks()
{
	SStartNodeOperationBatching startBatching(this);
	if (!m_pGraph)
		return;

	CUndo undo("HyperGraph Hide Node(s)");
	m_pGraph->RecordUndo();
	std::vector<CHyperNode*> nodes;
	if (!GetSelectedNodes(nodes, SELECTION_SET_INCLUDE_RELATIVES))
		return;

	for (int i = 0; i < nodes.size(); i++)
	{
		m_pGraph->RemoveNodeKeepLinks(nodes[i]);
	}
	OnSelectionChange();
	InvalidateView();
	PropagateChangesToGroupsAndPrefabs();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnCommandCopy()
{
	if (!m_pGraph)
		return;

	std::vector<CHyperNode*> nodes;
	if (!GetSelectedNodes(nodes))
		return;

	m_bCopiedBlackBox = false;
	if (nodes.size() == 1 && strcmp(nodes[0]->GetClassName(), CBlackBoxNode::GetClassType()) == 0)
	{
		CBlackBoxNode* pBlackBox = (CBlackBoxNode*)nodes[0];
		nodes.clear();
		nodes = *(pBlackBox->GetNodes());
		m_bCopiedBlackBox = true;
	}

	CClipboard clipboard;
	//clipboard.Put( )
	//clipboard.

	CHyperGraphSerializer serializer(m_pGraph, 0);

	for (int i = 0; i < nodes.size(); i++)
	{
		CHyperNode* pNode = nodes[i];

		if (!pNode->CheckFlag(EHYPER_NODE_UNREMOVEABLE))
		{
			serializer.SaveNode(pNode, true);
		}
	}
	if (nodes.size() > 0)
	{
		XmlNodeRef node = XmlHelpers::CreateXmlNode("Graph");
		serializer.Serialize(node, false);
		clipboard.Put(node, "Graph");
	}
	InvalidateView();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnCommandPaste()
{
	if (m_bIsFrozen)
		return;
	CPoint point;
	GetCursorPos(&point);
	ScreenToClient(&point);

	CClipboard clipboard;
	XmlNodeRef node = clipboard.Get();

	if (!node)
		return;

	SStartNodeOperationBatching startBatching(this);

	if (node->isTag("Graph"))
	{
		InternalPaste(false, point, nullptr);
	}
	else
	{
		bool isInput = node->isTag("GraphNodeInputLinks");
		if (isInput || node->isTag("GraphNodeOutputLinks"))
		{
			std::vector<CHyperNode*> nodes;
			if (!GetSelectedNodes(nodes))
				return;
			CUndo undo("Paste Graph Node Links");
			m_pGraph->RecordUndo();
			for (int i = 0; i < nodes.size(); i++)
			{
				PasteLinks(nodes[i], isInput);
			}
		}
	}

	PropagateChangesToGroupsAndPrefabs();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnCommandPasteWithLinks()
{
	if (m_bIsFrozen)
		return;

	SStartNodeOperationBatching startBatching(this);
	CPoint point;
	GetCursorPos(&point);
	ScreenToClient(&point);
	InternalPaste(true, point);
	PropagateChangesToGroupsAndPrefabs();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::InternalPaste(bool bWithLinks, CPoint point, std::vector<CHyperNode*>* pPastedNodes)
{
	if (m_bIsFrozen)
		return;

	if (!m_pGraph)
		return;

	CClipboard clipboard;
	XmlNodeRef node = clipboard.Get();

	CObjectArchive remappingInfo(GetIEditorImpl()->GetObjectManager(), node, true);

	CHyperFlowGraph* pFlowGraph = (m_pGraph && m_pGraph->IsFlowGraph()) ? static_cast<CHyperFlowGraph*>(m_pGraph) : nullptr;

	if (pFlowGraph)
		pFlowGraph->ExtractObjectsPrefabIdToGlobalIdMappingFromPrefab(remappingInfo);

	if (node != NULL && node->isTag("Graph"))
	{
		CUndo undo("HyperGraph Paste Node(s)");
		m_pGraph->UnselectAll();

		if (m_bCopiedBlackBox)
			bWithLinks = true;

		CHyperGraphSerializer serializer(m_pGraph, &remappingInfo);
		serializer.SelectLoaded(true);
		serializer.Serialize(node, true, bWithLinks, true); // Paste==true -> don't screw up graph specific data if just pasting nodes

		std::vector<CHyperNode*> nodes;
		serializer.GetLoadedNodes(nodes);
		if (pPastedNodes)
			serializer.GetLoadedNodes(*pPastedNodes);

		if (m_bCopiedBlackBox)
		{
			auto pBB = CreateNode<CBlackBoxNode>(point);
			if (pBB)
				RenameNode(pBB);
			for (int i = 0; i < nodes.size(); ++i)
				pBB->AddNode(nodes[i]);
		}

		CRect rc;
		GetClientRect(rc);

		Gdiplus::PointF offset(100, 100);
		if (point.x > 0 && point.x < rc.Width() && point.y > 0 && point.y < rc.Height())
		{
			Gdiplus::RectF bounds;
			// Calculate bounds.
			for (int i = 0; i < nodes.size(); i++)
			{
				if (i == 0)
					bounds = nodes[i]->GetRect();
				else
				{
					Gdiplus::RectF temp = bounds;
					bounds.Union(bounds, temp, nodes[i]->GetRect());
				}
			}
			Gdiplus::PointF locPoint = ViewToLocal(point);

			// Snap to the grid.
			Gdiplus::PointF p = ViewToLocal(point);
			offset = Gdiplus::PointF(
				(floor(((float)locPoint.X / GRID_SIZE) + 0.5f) * GRID_SIZE) - bounds.X,
				(floor(((float)locPoint.Y / GRID_SIZE) + 0.5f) * GRID_SIZE) - bounds.Y);
		}
		// Offset all pasted nodes.
		for (int i = 0; i < nodes.size(); i++)
		{
			Gdiplus::RectF rc = nodes[i]->GetRect();
			rc.Offset(offset);
			nodes[i]->SetRect(rc);
		}

		m_pGraph->SendNotifyEvent(nullptr, EHG_NODES_PASTED);
	}
	OnSelectionChange();
	InvalidateView();
}

void CHyperGraphView::OnUndo()
{
	GetIEditorImpl()->GetIUndoManager()->Undo();
}

void CHyperGraphView::OnRedo()
{
	GetIEditorImpl()->GetIUndoManager()->Redo();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnCommandCut()
{
	OnCommandCopy();
	OnCommandDelete();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::SimulateFlow(CHyperEdge* pEdge)
{
	// re-set target node's port value
	assert(m_pGraph != 0);
	CHyperNode* pNode = (CHyperNode*) m_pGraph->FindNode(pEdge->nodeIn);
	assert(pNode != 0);
	CFlowNode* pFlowNode = (CFlowNode*) pNode;
	IFlowGraph* pIGraph = pFlowNode->GetIFlowGraph();
	const TFlowInputData* pValue = pIGraph->GetInputValue(pFlowNode->GetFlowNodeId(), pEdge->nPortIn);
	assert(pValue != 0);
	pIGraph->ActivatePort(SFlowAddress(pFlowNode->GetFlowNodeId(), pEdge->nPortIn, false), *pValue);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::SetZoom(float zoom)
{
	m_zoom = CLAMP(zoom, MIN_ZOOM, MAX_ZOOM);
	NotifyZoomChangeToNodes();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::NotifyZoomChangeToNodes()
{
	if (m_pGraph)
	{
		HideEditPort();
		IHyperGraphEnumerator* pEnum = m_pGraph->GetNodesEnumerator();
		for (IHyperNode* pINode = pEnum->GetFirst(); pINode; pINode = pEnum->GetNext())
		{
			CHyperNode* pNode = (CHyperNode*)pINode;
			pNode->OnZoomChange(m_zoom);
		}
		pEnum->Release();
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::CopyLinks(CHyperNode* pNode, bool isInput)
{
	if (!m_pGraph || !pNode)
		return;

	CHyperGraphSerializer serializer(m_pGraph, 0);

	CClipboard clipboard;
	XmlNodeRef node = XmlHelpers::CreateXmlNode(isInput ? "GraphNodeInputLinks" : "GraphNodeOutputLinks");

	std::vector<CHyperEdge*> edges;
	if (m_pGraph->FindEdges(pNode, edges))
	{
		for (int i = 0; i < edges.size(); ++i)
		{
			if (isInput && pNode == m_pGraph->FindNode(edges[i]->nodeIn) ||
			    !isInput && pNode == m_pGraph->FindNode(edges[i]->nodeOut))
			{
				serializer.SaveEdge(edges[i]);
			}
		}
	}
	serializer.Serialize(node, false);
	clipboard.Put(node, "Graph");
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::PasteLinks(CHyperNode* pNode, bool isInput)
{
	if (!m_pGraph || !pNode)
		return;

	CClipboard clipboard;
	XmlNodeRef node = clipboard.Get();
	if (!node ||
	    isInput && !node->isTag("GraphNodeInputLinks") ||
	    !isInput && !node->isTag("GraphNodeOutputLinks"))
		return;

	XmlNodeRef edgesXml = node->findChild("Edges");
	if (edgesXml)
	{
		HyperNodeID nodeIn, nodeOut;
		CString portIn, portOut;
		bool bEnabled;
		bool bAskReassign = true;
		bool bIsReassign = false;
		for (int i = 0; i < edgesXml->getChildCount(); i++)
		{
			XmlNodeRef edgeXml = edgesXml->getChild(i);

			edgeXml->getAttr("nodeIn", nodeIn);
			edgeXml->getAttr("nodeOut", nodeOut);
			edgeXml->getAttr("portIn", portIn);
			edgeXml->getAttr("portOut", portOut);
			if (edgeXml->getAttr("enabled", bEnabled) == false)
				bEnabled = true;

			if (isInput)
				nodeIn = pNode->GetId();
			else
				nodeOut = pNode->GetId();

			CHyperNode* pNodeIn = (CHyperNode*)m_pGraph->FindNode(nodeIn);
			CHyperNode* pNodeOut = (CHyperNode*)m_pGraph->FindNode(nodeOut);
			if (!pNodeIn || !pNodeOut)
				continue;
			CHyperNodePort* pPortIn = pNodeIn->FindPort(portIn, true);
			CHyperNodePort* pPortOut = pNodeOut->FindPort(portOut, false);
			if (!pPortIn || !pPortOut)
				continue;

			// check of reassigning inputs
			if (isInput)
			{
				bool bFound = false;
				std::vector<CHyperEdge*> edges;
				if (m_pGraph->FindEdges(pNodeIn, edges))
				{
					for (int i = 0; i < edges.size(); ++i)
					{
						if (pNodeIn == m_pGraph->FindNode(edges[i]->nodeIn) &&
						    !stricmp(portIn, edges[i]->portIn))
						{
							bFound = true;
							break;
						}
					}
				}
				if (bFound)
				{
					if (bAskReassign)
					{
						if (MessageBox(_T("This operation tries to reassign some not empty input port(s). Do you want to reassign?"), _T("Reassign input ports"), MB_YESNO | MB_ICONQUESTION) == IDYES)
							bIsReassign = true;
						bAskReassign = false;
					}
					if (!bIsReassign)
						continue;
				}
			}

			m_pGraph->ConnectPorts(pNodeOut, pPortOut, pNodeIn, pPortIn);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::DeleteLinks(CHyperNode* pNode, bool isInput)
{
	if (!m_pGraph || !pNode)
		return;

	m_pGraph->RecordUndo();

	std::vector<CHyperEdge*> edges;
	if (m_pGraph->FindEdges(pNode, edges))
	{
		for (int i = 0; i < edges.size(); ++i)
		{
			if (isInput && pNode == m_pGraph->FindNode(edges[i]->nodeIn) ||
			    !isInput && pNode == m_pGraph->FindNode(edges[i]->nodeOut))
				m_pGraph->RemoveEdge(edges[i]);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::ShowEditPort(CHyperNode* pNode, CHyperNodePort* pPort)
{
	m_editParamCtrl.RemoveAllItems();
	_smart_ptr<CVarBlock> pVarBlock = pNode->GetInputsVarBlock();
	if (pVarBlock)
	{
		if (m_zoom > MIN_ZOOM_CHANGE_HEIGHT)
		{
			m_editParamCtrl.SetFontHeight(abs(SMFCFonts::GetInstance().nDefaultFontHieght) * m_zoom);
			m_editParamCtrl.SetItemHeight(DEFAULT_EDIT_HEIGHT * m_zoom);
		}
		else
		{
			m_editParamCtrl.SetFontHeight(0);
			m_editParamCtrl.SetItemHeight(DEFAULT_EDIT_HEIGHT);
		}

		_smart_ptr<CVarBlock> pEditVarBlock = new CVarBlock;
		pEditVarBlock->AddVariable(pPort->pVar);
		if (pNode->GetTypeId() == gEnv->pFlowSystem->GetTypeId("Engine:LayerSwitch"))
		{
			SetupLayerList(pEditVarBlock);
			m_editParamCtrl.SetUpdateCallback(functor(*this, &CHyperGraphView::OnUpdateProperties));
			m_editParamCtrl.EnableUpdateCallback(true);
		}
		m_editParamCtrl.AddVarBlock(pEditVarBlock);

		Gdiplus::RectF rect = pNode->GetRect();
		Gdiplus::RectF rectPort;
		pNode->GetAttachRect(pPort->nPortIndex, &rectPort);
		rectPort.Y -= PORT_HEIGHT / 2;
		rectPort.Height = PORT_HEIGHT;
		rectPort.Offset(pNode->GetPos());

		CPoint LeftTop = LocalToView(Gdiplus::PointF(rectPort.X, rectPort.Y));
		CPoint RightBottom = LocalToView(Gdiplus::PointF(rect.X + rect.Width, rectPort.Y + rectPort.Height));
		int w = RightBottom.x - LeftTop.x - 2;
		int h = MAX(m_editParamCtrl.GetVisibleHeight(), RightBottom.y - LeftTop.y);
		m_editParamCtrl.SetWindowPos(NULL, LeftTop.x + 1, LeftTop.y, w, h + 1, SWP_SHOWWINDOW);
		m_editParamCtrl.ShowWindow(SW_SHOW);
		CPropertyItem* pItem = m_editParamCtrl.FindItemByVar(pPort->pVar);
		if (pItem)
			m_editParamCtrl.SelectItem(pItem);

		if (rect.Width < SMALLNODE_WIDTH)
			m_editParamCtrl.SetSplitter(m_zoom > MIN_ZOOM_CHANGE_HEIGHT ? SMALLNODE_SPLITTER * m_zoom : SMALLNODE_SPLITTER);

		m_isEditPort = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::HideEditPort()
{
	m_pEditPort = 0;
	if (!m_isEditPort)
		return;

	// Kill focus from control before Removing items to assign changes
	SetFocus();

	m_editParamCtrl.RemoveAllItems();
	m_editParamCtrl.ShowWindow(SW_HIDE);
	m_isEditPort = false;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::UpdateFrozen()
{
	bool bOldIsFrozen = m_bIsFrozen;
	m_bIsFrozen = false;
	HideEditPort();
	if (m_pGraph && m_pGraph->IsFlowGraph())
	{
		CHyperFlowGraph* pFlowGraph = static_cast<CHyperFlowGraph*>(m_pGraph);
		CEntityObject* pEntity = pFlowGraph->GetEntity();
		if ((pEntity && pEntity->IsFrozen()) || GetIEditorImpl()->GetGameEngine()->IsFlowSystemUpdateEnabled())
		{
			m_pGraph->UnselectAll();
			OnSelectionChange();
			m_bIsFrozen = true;
		}
		if (bOldIsFrozen != m_bIsFrozen)
			InvalidateView();
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::SetupLayerList(CVarBlock* pVarBlock)
{
	// In the Game a Layer port has a string type, for editing we replace a port variable to the enum type variable
	IVariable* pVar = pVarBlock->FindVariable("Layer");
	if (!pVar)
		return;

	CString val;
	pVar->Get(val);
	pVarBlock->DeleteVariable(pVar);

	CVariableFlowNodeEnum<CString>* pEnumVar = new CVariableFlowNodeEnum<CString>;
	pEnumVar->SetDataType(IVariable::DT_UIENUM);
	pEnumVar->SetName("Layer");

	std::set<string> seenNames;
	const auto& layers = GetIEditorImpl()->GetObjectManager()->GetLayersManager()->GetLayers();
	for (int i = 0; i < layers.size(); ++i)
	{
		if (layers[i] && seenNames.find(layers[i]->GetName()) == seenNames.end())
		{
			pEnumVar->AddEnumItem(layers[i]->GetName(), layers[i]->GetName().GetString());
			seenNames.insert(layers[i]->GetName());
		}
	}
	pEnumVar->Set(val);
	pVarBlock->AddVariable(pEnumVar);
}

void CHyperGraphView::QuickSearchNode(CHyperNode* pNode)
{
	assert(pNode);
	m_pQuickSearchNode = pNode;
	CRect rc = LocalToViewRect(pNode->GetRect());
	rc.DeflateRect(1, 1);
	rc.bottom = rc.top + DEFAULT_EDIT_HEIGHT * m_zoom;

	if (m_quickSearchEdit.m_hWnd)
		m_quickSearchEdit.DestroyWindow();

	m_quickSearchEdit.SetView(this);
	int nEditFlags = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL;

	m_quickSearchEdit.Create(nEditFlags, rc, this, IDC_RENAME);
	m_quickSearchEdit.SetWindowText(m_pQuickSearchNode->GetName());

	HFONT font = ::CreateFont(12 * m_zoom, 0, 0, 0, FW_BOLD, 0, 0, 0,
	                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
	                          CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
	                          DEFAULT_PITCH, "Ms Shell Dlg 2");

	m_quickSearchEdit.SetFont(CFont::FromHandle(font));
	m_quickSearchEdit.SetCapture();
	m_quickSearchEdit.SetSel(0, -1);
	m_quickSearchEdit.SetFocus();
}

void CHyperGraphView::OnAcceptSearch()
{
	if (m_pQuickSearchNode)
	{
		CPoint point;
		GetCursorPos(&point);
		ScreenToClient(&point);
		CString sName = m_pQuickSearchNode->GetName();

		if (sName.Find("Misc:") >= 0)
			sName.Delete(0, 5);

		CQuickSearchNode* pQuickSearchNode = static_cast<CQuickSearchNode*>(m_pQuickSearchNode.get());
		if (pQuickSearchNode && pQuickSearchNode->GetSearchResultCount())
		{
			auto mode = m_mode;

			// Destroy the window before creating the node
			m_quickSearchEdit.DestroyWindow();

			auto pNewNode = CreateNode(sName, point);
			if (mode == DragQuickSearchMode)
			{
				if (!pNewNode->GetInputs()->empty())
				{
					CHyperNode* pOutNode = (CHyperNode*)m_pGraph->FindNode(m_draggingEdges[0]->nodeOut);
					CHyperNodePort* pOutPort = &pOutNode->GetOutputs()->at(m_draggingEdges[0]->nPortOut);
					CHyperNodePort* pInPort = &pNewNode->GetInputs()->at(0);

					{
						// skip entity ports
						CFlowNode* pNewFlowNode = static_cast<CFlowNode*>(pNewNode.get());
						SFlowNodeConfig newNodeConfig;
						CHyperFlowGraph* pFlowGraph = static_cast<CHyperFlowGraph*>(m_pGraph);
						if (pFlowGraph)
						{
							pFlowGraph->GetIFlowGraph()->GetNodeConfiguration(pNewFlowNode->GetFlowNodeId(), newNodeConfig);
							const SInputPortConfig* pPortConfig = newNodeConfig.pInputPorts;
							if (pPortConfig->defaultData.GetType() == eFDT_EntityId)
							{
								if (pNewNode->GetInputs()->size() > 1)
									pInPort = &pNewNode->GetInputs()->at(1);
							}
						}
					}

					CHyperEdge* pExistingEdge = nullptr;
					if (m_pGraph->CanConnectPorts(pOutNode, pOutPort, pNewNode, pInPort, pExistingEdge))
					{
						// Connect
						m_pGraph->ConnectPorts(pOutNode, pOutPort, pNewNode, pInPort);
						InvalidateNode(pOutNode, true);
						InvalidateNode(pNewNode, true);
						OnNodesConnected();
						//GetIEditorImpl()->GetIUndoManager()->Accept("Create and Connect Graph Node");
					}
				}
				else
				{
					//GetIEditorImpl()->GetIUndoManager()->Cancel();
				}
				m_draggingEdges.clear();
				m_mode = NothingMode;
			}
			m_pGraph->RemoveNode(m_pQuickSearchNode);
			m_pQuickSearchNode = NULL;
		}
	}

	m_SearchResults.clear();
	m_sCurrentSearchSelection = "";

	Invalidate();
}

void CHyperGraphView::OnCancelSearch()
{
	if (m_pQuickSearchNode)
		m_pGraph->RemoveNode(m_pQuickSearchNode);

	if (m_mode == DragQuickSearchMode)
	{
		m_mode = NothingMode;
		InvalidateEdge(m_draggingEdges[0]);
		m_draggingEdges.clear();
	}
	m_quickSearchEdit.DestroyWindow();
	m_SearchResults.clear();
	m_sCurrentSearchSelection = "";
	m_pQuickSearchNode = NULL;

	Invalidate();
}

void CHyperGraphView::OnSearchNodes()
{
	if (!m_pGraph || !m_pGraph->GetManager() || !m_pQuickSearchNode)
		return;

	ViewNodeFilter filter(m_componentViewMask);
	std::vector<THyperNodePtr> prototypes;
	m_pGraph->GetManager()->GetPrototypesEx(prototypes, true, functor_ret(filter, &ViewNodeFilter::Visit));

	m_SearchResults.clear();
	m_sCurrentSearchSelection = "";
	CString fullname = "";
	CString fullnameLower = "";
	std::vector<CString> searchStrings;
	CString quickSearchtext = "";

	m_quickSearchEdit.GetWindowText(quickSearchtext);
	quickSearchtext.MakeLower();

	std::vector<CString> quickSearchTokens;
	int Position = 0;
	CString token = quickSearchtext.Tokenize(" ", Position);
	while (!token.IsEmpty())
	{
		quickSearchTokens.push_back(token.MakeLower());
		token = quickSearchtext.Tokenize(" ", Position);
	}

	for (std::vector<THyperNodePtr>::iterator iter = prototypes.begin(); iter != prototypes.end(); ++iter)
	{
		CHyperNode* pPrototypeNode = (*iter).get();

		if (pPrototypeNode && (pPrototypeNode->IsFlowNode() || pPrototypeNode->IsEditorSpecialNode()) && (!pPrototypeNode->IsObsolete() || m_componentViewMask & EFLN_OBSOLETE))
		{
			fullname = pPrototypeNode->GetClassName();
			fullnameLower = fullname;
			fullnameLower.MakeLower();

			if (fullnameLower.Find(":") < 0)
			{
				fullnameLower.Insert(0, "misc:");
				fullname.Insert(0, "Misc:");
			}

			if (QuickSearchMatch(quickSearchTokens, fullnameLower))
				stl::push_back_unique(m_SearchResults, fullname);
		}
	}

	CQuickSearchNode* pQuickSearchNode = static_cast<CQuickSearchNode*>(m_pQuickSearchNode.get());

	if (!pQuickSearchNode)
		return;

	if (m_SearchResults.size() == 0)
	{
		m_sCurrentSearchSelection = "";
		pQuickSearchNode->SetSearchResultCount(0);
		pQuickSearchNode->SetIndex(0);
		m_pQuickSearchNode->SetName("NO SEARCH RESULTS...");
	}
	else
	{
		pQuickSearchNode->SetSearchResultCount(m_SearchResults.size());
		pQuickSearchNode->SetIndex(1);

		fullname = *(m_SearchResults.begin());
		m_pQuickSearchNode->SetName(fullname);
		m_sCurrentSearchSelection = fullname;
	}
}

bool CHyperGraphView::QuickSearchMatch(std::vector<CString> tokens, CString name)
{
	//If each token exists, show the result
	const int count = tokens.size();
	for (int i = 0; i < count; i++)
	{
		if (name.Find(tokens[i]) < 0)
		{
			return false;
		}
	}

	return true;
}

void CHyperGraphView::OnSelectNextSearchResult(bool bUp)
{
	if (!m_pQuickSearchNode)
		return;

	CQuickSearchNode* pQuickSearchNode = static_cast<CQuickSearchNode*>(m_pQuickSearchNode.get());

	if (!pQuickSearchNode)
		return;

	if (bUp)
	{
		for (std::vector<CString>::reverse_iterator iter = m_SearchResults.rbegin(); iter != m_SearchResults.rend(); ++iter)
		{
			CString name = (*iter);

			if (!strcmp(name, m_sCurrentSearchSelection))
			{
				++iter;
				pQuickSearchNode->SetIndex(pQuickSearchNode->GetIndex() - 1);
				if (iter == m_SearchResults.rend())
				{
					iter = m_SearchResults.rbegin();
					pQuickSearchNode->SetIndex(m_SearchResults.size());
				}

				m_pQuickSearchNode->SetName((*iter));
				m_sCurrentSearchSelection = (*iter);
				break;
			}
		}
	}
	else
	{
		for (std::vector<CString>::iterator iter = m_SearchResults.begin(); iter != m_SearchResults.end(); ++iter)
		{
			CString name = (*iter);

			if (!strcmp(name, m_sCurrentSearchSelection))
			{
				++iter;
				pQuickSearchNode->SetIndex(pQuickSearchNode->GetIndex() + 1);

				if (iter == m_SearchResults.end())
				{
					iter = m_SearchResults.begin();
					pQuickSearchNode->SetIndex(1);
				}

				m_pQuickSearchNode->SetName((*iter));
				m_sCurrentSearchSelection = (*iter);
				break;
			}
		}
	}
}

void CHyperGraphView::UpdateNodeEntity(CHyperNode* pNode, const uint cmd)
{
	if (pNode->CheckFlag(EHYPER_NODE_ENTITY))
	{
		CFlowNode* pFlowNode = static_cast<CFlowNode*>(pNode);

		if (cmd == ID_GRAPHVIEW_TARGET_SELECTED_ENTITY)
			pFlowNode->SetSelectedEntity();
		else if (cmd == ID_GRAPHVIEW_TARGET_GRAPH_ENTITY)
			pFlowNode->SetDefaultEntity();
		else
			pFlowNode->SetEntity(NULL);

		pFlowNode->Invalidate(true, true);
		GetIEditorImpl()->GetFlowGraphManager()->SendNotifyEvent(EHG_NODE_UPDATE_ENTITY, 0, pFlowNode);
		if (m_pGraph)
			m_pGraph->SendNotifyEvent(pFlowNode, EHG_NODE_UPDATE_ENTITY);
	}
}

void CHyperGraphView::SGraphVisualChanges::Reset(bool resetFlagChanged)
{
	m_movedNodesStartPositions.clear();
	if (resetFlagChanged)
		m_graphChanged = false;
}

void CHyperGraphView::SGraphVisualChanges::NodeMoved(const CHyperNode& nodeRef)
{
	if (m_movedNodesStartPositions.find(THyperNodePtr((CHyperNode*)&nodeRef)) == m_movedNodesStartPositions.end())
	{
		m_movedNodesStartPositions[THyperNodePtr((CHyperNode*)&nodeRef)] = nodeRef.GetPos();
		m_graphChanged = true;
	}
}

void CHyperGraphView::SGraphVisualChanges::NodeResized()
{
	m_graphChanged = true;
}

const Gdiplus::PointF& CHyperGraphView::SGraphVisualChanges::GetNodeStartPosition(const CHyperNode& nodeRef) const
{
	static Gdiplus::PointF dummyPoint;
	std::map<THyperNodePtr, Gdiplus::PointF>::const_iterator it;
	it = m_movedNodesStartPositions.find(THyperNodePtr((CHyperNode*)(&nodeRef)));
	if (it != m_movedNodesStartPositions.end())
		return it->second;

	return dummyPoint;
}

