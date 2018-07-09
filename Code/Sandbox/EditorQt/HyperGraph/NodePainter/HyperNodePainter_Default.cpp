// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HyperNodePainter_Default.h"

#include <CryFlowGraph/IFlowGraphDebugger.h>

#include "HyperGraph/HyperGraph.h"
#include "HyperGraph/FlowGraphNode.h"
#include "HyperGraph/FlowGraphPreferences.h"

#include "HyperGraph/Controls/FlowGraphDebuggerEditor.h"

#define NODE_BACKGROUND_COLOR                  GET_GDI_COLOR(gFlowGraphColorPreferences.colorNodeBkg)
#define NODE_BACKGROUND_SELECTED_COLOR         GET_GDI_COLOR(gFlowGraphColorPreferences.colorNodeSelected)
#define NODE_BACKGROUND_CUSTOM_COLOR1          GET_GDI_COLOR(gFlowGraphColorPreferences.colorCustomNodeBkg)
#define NODE_BACKGROUND_CUSTOM_SELECTED_COLOR1 GET_GDI_COLOR(gFlowGraphColorPreferences.colorCustomSelectedNodeBkg)
#define NODE_OUTLINE_COLOR                     GET_GDI_COLOR(gFlowGraphColorPreferences.colorNodeOutline)
#define NODE_OUTLINE_COLOR_SELECTED            GET_GDI_COLOR(gFlowGraphColorPreferences.colorNodeOutlineSelected)
#define NODE_DEBUG_TITLE_COLOR                 GET_GDI_COLOR(gFlowGraphColorPreferences.colorDebugNodeTitle)
#define NODE_DEBUG_COLOR                       GET_GDI_COLOR(gFlowGraphColorPreferences.colorDebugNode)
#define NODE_BCKGD_ERROR_COLOR                 GET_GDI_COLOR(gFlowGraphColorPreferences.colorNodeBckgdError)
#define NODE_TITLE_ERROR_COLOR                 GET_GDI_COLOR(gFlowGraphColorPreferences.colorNodeTitleError)

#define TEXT_COLOR                             GET_GDI_COLOR(gFlowGraphColorPreferences.colorText)

#define TITLE_TEXT_COLOR                       GET_GDI_COLOR(gFlowGraphColorPreferences.colorTitleText)
#define TITLE_ENTITY_TEXT_INAVLID_COLOR        GET_GDI_COLOR(gFlowGraphColorPreferences.colorEntityTextInvalid)
#define TITLE_TEXT_SELECTED_COLOR              GET_GDI_COLOR(gFlowGraphColorPreferences.colorTitleTextSelected)

#define PORT_BACKGROUND_COLOR                  GET_GDI_COLOR(gFlowGraphColorPreferences.colorPort)
#define PORT_BACKGROUND_SELECTED_COLOR         GET_GDI_COLOR(gFlowGraphColorPreferences.colorPortSelected)
#define PORT_TEXT_COLOR                        GET_GDI_COLOR(gFlowGraphColorPreferences.colorText)
#define PORT_DEBUGGING_COLOR_INITIALIZATION    GET_GDI_COLOR(gFlowGraphColorPreferences.colorPortDebuggingInitialization)
#define PORT_DEBUGGING_COLOR                   GET_GDI_COLOR(gFlowGraphColorPreferences.colorPortDebugging)
#define PORT_DEBUGGING_TEXT_COLOR              GET_GDI_COLOR(gFlowGraphColorPreferences.colorPortDebuggingText)

#define DOWN_ARROW_COLOR                       GET_GDI_COLOR(gFlowGraphColorPreferences.colorDownArrow)

#define BREAKPOINT_COLOR                       GET_GDI_COLOR(gFlowGraphColorPreferences.colorBreakPoint)
#define BREAKPOINT_DISABLED_COLOR              GET_GDI_COLOR(gFlowGraphColorPreferences.colorBreakPointDisabled)
#define BREAKPOINT_ARROW_COLOR                 GET_GDI_COLOR(gFlowGraphColorPreferences.colorBreakPointArrow)

#define ENTITY_PORT_NOT_CONNECTED_COLOR        GET_GDI_COLOR(gFlowGraphColorPreferences.colorEntityPortNotConnected)

#define PORT_DOWN_ARROW_WIDTH                  20
#define PORT_DOWN_ARROW_HEIGHT                 8

static COLORREF PortTypeToColor[] =
{
	0x0000FF00,
	0x00FF0000,
	0x000050FF,
	0x00FFFFFF,
	0x00FF00FF,
	0x00FFFF00,
	0x0000FFFF,
	0x007F00FF,
	0x0000FFFF,
	0x00FF7F00,
	0x0000FF7F,
	0x007F7F7F,
	0x00000000
};

static const int NumPortColors = sizeof(PortTypeToColor) / sizeof(*PortTypeToColor);

namespace
{

struct SAssets
{
	SAssets() :
		font(L"Tahoma", 10.13f, Gdiplus::FontStyleRegular),
		brushBackgroundSelected(NODE_BACKGROUND_SELECTED_COLOR),
		brushBackgroundUnselected(NODE_BACKGROUND_COLOR),
		brushBackgroundCustomSelected1(NODE_BACKGROUND_CUSTOM_SELECTED_COLOR1),
		brushBackgroundCustomUnselected1(NODE_BACKGROUND_CUSTOM_COLOR1),
		penOutline(NODE_OUTLINE_COLOR),
		penOutlineSelected(NODE_OUTLINE_COLOR_SELECTED, 3.0f),
		brushTitleTextSelected(TITLE_TEXT_SELECTED_COLOR),
		brushTitleTextUnselected(TITLE_TEXT_COLOR),
		brushEntityTextValid(TEXT_COLOR),
		brushEntityTextInvalid(TITLE_ENTITY_TEXT_INAVLID_COLOR),
		penPort(PORT_BACKGROUND_COLOR),
		sfInputPort(),
		sfOutputPort(),
		brushPortText(TEXT_COLOR),
		brushPortSelected(PORT_BACKGROUND_SELECTED_COLOR),
		breakPoint(BREAKPOINT_COLOR),
		breakPointArrow(BREAKPOINT_ARROW_COLOR),
		penBreakpointDisabled(BREAKPOINT_DISABLED_COLOR),
		brushPortDebugActivated(PORT_DEBUGGING_COLOR),
		brushPortDebugActivatedForInitialization(PORT_DEBUGGING_COLOR_INITIALIZATION),
		brushPortDebugText(PORT_DEBUGGING_TEXT_COLOR),
		brushMinimizeArrow(NODE_OUTLINE_COLOR),
		penDownArrow(DOWN_ARROW_COLOR),
		brushEntityPortNotConnected(ENTITY_PORT_NOT_CONNECTED_COLOR),
		brushDebugNode(NODE_DEBUG_COLOR),
		brushDebugNodeTitle(NODE_DEBUG_TITLE_COLOR),
		brushNodeBckgdError(NODE_BCKGD_ERROR_COLOR),
		brushNodeTitleError(NODE_TITLE_ERROR_COLOR)
	{
		sfInputPort.SetAlignment(Gdiplus::StringAlignmentNear);
		sfOutputPort.SetAlignment(Gdiplus::StringAlignmentFar);

		for (int i = 0; i < NumPortColors; i++)
		{
			brushPortFill[i] = new Gdiplus::SolidBrush(PortTypeToColor[i] | (int)GET_TRANSPARENCY << Gdiplus::Color::AlphaShift);
		}
	}
	~SAssets()
	{
		for (int i = 0; i < NumPortColors; i++)
			delete brushPortFill[i];
	}

	void Update()
	{
		brushBackgroundSelected.SetColor(NODE_BACKGROUND_SELECTED_COLOR);
		brushBackgroundUnselected.SetColor(NODE_BACKGROUND_COLOR);
		brushBackgroundCustomSelected1.SetColor(NODE_BACKGROUND_CUSTOM_SELECTED_COLOR1);
		brushBackgroundCustomUnselected1.SetColor(NODE_BACKGROUND_CUSTOM_COLOR1);
		penOutline.SetColor(NODE_OUTLINE_COLOR);
		penOutlineSelected.SetColor(NODE_OUTLINE_COLOR_SELECTED);
		brushTitleTextSelected.SetColor(TITLE_TEXT_SELECTED_COLOR);
		brushTitleTextUnselected.SetColor(TITLE_TEXT_COLOR);
		brushEntityTextValid.SetColor(TEXT_COLOR);
		brushEntityTextInvalid.SetColor(TITLE_ENTITY_TEXT_INAVLID_COLOR);
		penPort.SetColor(PORT_BACKGROUND_COLOR);
		brushPortText.SetColor(TEXT_COLOR);
		brushPortSelected.SetColor(PORT_BACKGROUND_SELECTED_COLOR);
		breakPoint.SetColor(BREAKPOINT_COLOR);
		breakPointArrow.SetColor(BREAKPOINT_ARROW_COLOR);
		penBreakpointDisabled.SetColor(BREAKPOINT_DISABLED_COLOR);
		brushPortDebugActivated.SetColor(PORT_DEBUGGING_COLOR);
		brushPortDebugActivatedForInitialization.SetColor(PORT_DEBUGGING_COLOR_INITIALIZATION);
		brushPortDebugText.SetColor(PORT_DEBUGGING_TEXT_COLOR);
		brushMinimizeArrow.SetColor(NODE_OUTLINE_COLOR);
		penDownArrow.SetColor(DOWN_ARROW_COLOR);
		brushEntityPortNotConnected.SetColor(ENTITY_PORT_NOT_CONNECTED_COLOR);
		brushDebugNodeTitle.SetColor(NODE_DEBUG_TITLE_COLOR);
		brushDebugNode.SetColor(NODE_DEBUG_COLOR);
		brushNodeBckgdError.SetColor(NODE_BCKGD_ERROR_COLOR);
		brushNodeTitleError.SetColor(NODE_TITLE_ERROR_COLOR);

		for (int i = 0; i < NumPortColors; i++)
		{
			brushPortFill[i]->SetColor(PortTypeToColor[i] | (int)GET_TRANSPARENCY << Gdiplus::Color::AlphaShift);
		}
	}

	Gdiplus::Font         font;
	Gdiplus::SolidBrush   brushBackgroundSelected;
	Gdiplus::SolidBrush   brushBackgroundUnselected;
	Gdiplus::SolidBrush   brushBackgroundCustomSelected1;
	Gdiplus::SolidBrush   brushBackgroundCustomUnselected1;
	Gdiplus::SolidBrush   brushDebugNodeTitle;
	Gdiplus::SolidBrush   brushDebugNode;
	Gdiplus::SolidBrush   brushNodeBckgdError;
	Gdiplus::SolidBrush   brushNodeTitleError;
	Gdiplus::Pen          penOutline;
	Gdiplus::Pen          penOutlineSelected;
	Gdiplus::SolidBrush   brushTitleTextSelected;
	Gdiplus::SolidBrush   brushTitleTextUnselected;
	Gdiplus::SolidBrush   brushEntityTextValid;
	Gdiplus::SolidBrush   brushEntityTextInvalid;
	Gdiplus::Pen          penPort;
	Gdiplus::Pen          penDownArrow;
	Gdiplus::StringFormat sfInputPort;
	Gdiplus::StringFormat sfOutputPort;
	Gdiplus::SolidBrush   brushPortText;
	Gdiplus::SolidBrush   brushPortSelected;
	Gdiplus::SolidBrush   breakPoint;
	Gdiplus::SolidBrush   breakPointArrow;
	Gdiplus::Pen          penBreakpointDisabled;
	Gdiplus::SolidBrush   brushPortDebugActivated;
	Gdiplus::SolidBrush   brushPortDebugActivatedForInitialization;
	Gdiplus::SolidBrush   brushPortDebugText;
	Gdiplus::SolidBrush   brushMinimizeArrow;
	Gdiplus::SolidBrush*  brushPortFill[NumPortColors];
	Gdiplus::SolidBrush   brushEntityPortNotConnected;
};

}

struct SDefaultRenderPort
{
	SDefaultRenderPort() : pPortArrow(0), pRectangle(0), pText(0), pBackground(0) {}
	CDisplayPortArrow* pPortArrow;
	CDisplayRectangle* pRectangle;
	CDisplayString*    pText;
	CDisplayRectangle* pBackground;
	int                id;
};

void CHyperNodePainter_Default::Paint(CHyperNode* pNode, CDisplayList* pList)
{
	static SAssets* pAssets = 0;
	struct IHyperGraph* pPrevGraph = 0;

	if (pNode->GetBlackBox()) //hide node
		return;

	if (!pAssets)
		pAssets = new SAssets();

	pAssets->Update();

	const bool bIsFlowgraph = reinterpret_cast<CHyperGraph*>(pNode->GetGraph())->IsFlowGraph();
	CFlowNode* pFlowNode = bIsFlowgraph ? reinterpret_cast<CFlowNode*>(pNode) : NULL;

	// background
	CDisplayRectangle* pBackgroundRect = pList->Add<CDisplayRectangle>();
	if (pNode->IsSelected())
		pBackgroundRect->SetStroked(&pAssets->penOutlineSelected);
	else
		pBackgroundRect->SetStroked(&pAssets->penOutline);
	pBackgroundRect->SetHitEvent(eSOID_Background);

	// node title
	CDisplayRectangle* pTitleRect = pList->Add<CDisplayRectangle>();
	if (pNode->IsSelected())
		pTitleRect->SetStroked(&pAssets->penOutlineSelected);
	else
		pTitleRect->SetStroked(&pAssets->penOutline);

	pTitleRect->SetHitEvent(eSOID_Title);

	CDisplayMinimizeArrow* pMinimizeArrow = pList->Add<CDisplayMinimizeArrow>()
	                                        ->SetStroked(&pAssets->penOutline)
	                                        ->SetFilled(&pAssets->brushMinimizeArrow)
	                                        ->SetHitEvent(eSOID_Minimize);
	CString title = pNode->GetTitle();
	if (gFlowGraphGeneralPreferences.showNodeIDs)
		title.AppendFormat(" (ID=%d)", pNode->GetId());

	CDisplayString* pTitleString = pList->Add<CDisplayString>()
	                               ->SetFont(&pAssets->font)
	                               ->SetText(title)
	                               ->SetHitEvent(eSOID_Title);

	// background and title fill
	const bool bDebugNode = pFlowNode ? (pFlowNode->GetCategory() == EFLN_DEBUG) : false;
	const bool bHasError = (strcmp(pNode->GetClassName(), "MissingNode") == 0) || (pNode->GetMissingPortCount() > 0);
	pTitleString->SetBrush(pNode->IsSelected() ? &pAssets->brushTitleTextSelected : &pAssets->brushTitleTextUnselected);
	pTitleRect->SetFilled(bHasError? &pAssets->brushNodeTitleError
		: bDebugNode ? &pAssets->brushDebugNodeTitle : &pAssets->brushBackgroundSelected);
	pBackgroundRect->SetFilled(bHasError? &pAssets->brushNodeBckgdError
		: bDebugNode ? &pAssets->brushDebugNode : &pAssets->brushBackgroundUnselected);

	bool bAnyHiddenInput = false;
	bool bAnyHiddenOutput = false;

	bool bEntityInputPort = false;
	bool bEntityPortConnected = false;

	// attached entity?
	CDisplayRectangle* pEntityRect = 0;
	CDisplayString* pEntityString = 0;
	CDisplayPortArrow* pEntityEllipse = 0;
	if (pNode->CheckFlag(EHYPER_NODE_ENTITY))
	{
		bEntityInputPort = true;
		pEntityRect = pList->Add<CDisplayRectangle>()
		              ->SetStroked(pNode->IsSelected() ? &pAssets->penOutlineSelected : &pAssets->penOutline)
		              ->SetHitEvent(eSOID_FirstInputPort);

		pEntityEllipse = pList->Add<CDisplayPortArrow>()
		                 ->SetFilled(pAssets->brushPortFill[IVariable::UNKNOWN])
		                 ->SetHitEvent(eSOID_FirstInputPort);

		pEntityString = pList->Add<CDisplayString>()
		                ->SetHitEvent(eSOID_FirstInputPort)
		                ->SetFont(&pAssets->font)
		                ->SetText(pNode->GetEntityTitle());

		if (pNode->GetInputs() && (*pNode->GetInputs())[0].nConnected != 0)
			bEntityPortConnected = true;

		if (pNode->IsEntityValid() ||
		    pNode->CheckFlag(EHYPER_NODE_GRAPH_ENTITY) ||
		    pNode->CheckFlag(EHYPER_NODE_GRAPH_ENTITY2) ||
		    bEntityPortConnected)
		{
			pEntityRect->SetFilled(&pAssets->brushBackgroundSelected);
			pEntityString->SetBrush(&pAssets->brushEntityTextValid);
		}
		else
		{
			pEntityRect->SetFilled(&pAssets->brushEntityPortNotConnected);
			pEntityString->SetBrush(&pAssets->brushEntityTextInvalid);
		}
	}

	std::vector<SDefaultRenderPort> inputPorts;
	std::vector<SDefaultRenderPort> outputPorts;
	const CHyperNode::Ports& inputs = *pNode->GetInputs();
	const CHyperNode::Ports& outputs = *pNode->GetOutputs();

	inputPorts.reserve(inputs.size());
	outputPorts.reserve(outputs.size());

	// input ports
	for (size_t i = 0; i < inputs.size(); i++)
	{
		if (bEntityInputPort && i == 0)
			continue;
		const CHyperNodePort& pp = inputs[i];
		if (pp.bVisible || pp.nConnected != 0)
		{
			SDefaultRenderPort pr;

			Gdiplus::Brush* pBrush = pAssets->brushPortFill[pp.pVar->GetType()];

			pr.pBackground = pList->Add<CDisplayRectangle>()
			                 ->SetHitEvent(eSOID_FirstInputPort + i);

			if (pp.bSelected)
			{
				pr.pBackground->SetFilled(&pAssets->brushPortSelected);
			}
			else if (pNode->IsPortActivationModified(&pp))
			{
				bool bIsInitializationPhase;
				pNode->GetAdditionalDebugPortInformation(pp, bIsInitializationPhase);
				pr.pBackground = pList->Add<CDisplayRectangle>()
				                 ->SetFilled(bIsInitializationPhase? &pAssets->brushPortDebugActivatedForInitialization : &pAssets->brushPortDebugActivated)
				                 ->SetHitEvent(eSOID_FirstInputPort + i);
			}

			pr.pPortArrow = pList->Add<CDisplayPortArrow>()
			                ->SetFilled(pBrush)
			                ->SetHitEvent(eSOID_FirstInputPort + i);

			pr.pText = pList->Add<CDisplayString>()
			           ->SetFont(&pAssets->font)
			           ->SetStringFormat(&pAssets->sfInputPort)
			           ->SetBrush(pNode->IsPortActivationModified(&pp) ? &pAssets->brushPortDebugText : &pAssets->brushPortText)
			           ->SetHitEvent(eSOID_FirstInputPort + i)
			           ->SetText(pNode->IsPortActivationModified() ? pNode->GetDebugPortValue(pp) : pNode->GetPortName(pp));

			pr.id = i;
			inputPorts.push_back(pr);
		}
		else
			bAnyHiddenInput = true;
	}
	// output ports
	for (size_t i = 0; i < outputs.size(); i++)
	{
		const CHyperNodePort& pp = outputs[i];

		const bool hasBreakPoint = bIsFlowgraph ? GetIEditorImpl()->GetFlowGraphDebuggerEditor()->HasBreakpoint(pFlowNode, &outputs[i]) : false;
		if (pp.bVisible || pp.nConnected != 0 || hasBreakPoint)
		{
			SDefaultRenderPort pr;

			Gdiplus::Brush* pBrush = pAssets->brushPortFill[pp.pVar->GetType()];

			pr.pBackground = pList->Add<CDisplayRectangle>()
			                 ->SetHitEvent(eSOID_FirstOutputPort + i);

			if (pp.bSelected)
			{
				pr.pBackground->SetFilled(&pAssets->brushPortSelected);
			}

			pr.pPortArrow = pList->Add<CDisplayPortArrow>()
			                ->SetFilled(pBrush)
			                ->SetHitEvent(eSOID_FirstOutputPort + i);

			pr.pText = pList->Add<CDisplayString>()
			           ->SetFont(&pAssets->font)
			           ->SetStringFormat(&pAssets->sfOutputPort)
			           ->SetBrush(&pAssets->brushPortText)
			           ->SetHitEvent(eSOID_FirstOutputPort + i)
			           ->SetText(pNode->GetPortName(pp));
			pr.id = 1000 + i;
			outputPorts.push_back(pr);
		}
		else
			bAnyHiddenOutput = true;
	}

	// calculate size now
	Gdiplus::SizeF size(0, 0);

	Gdiplus::Graphics* pG = pList->GetGraphics();
	Gdiplus::RectF rect = pTitleString->GetTextBounds(pG);
	float titleHeight = rect.Height;
	float curY = rect.Height;
	float width = rect.Width + MINIMIZE_BOX_WIDTH; // Add minimize box size.
	float entityHeight = 0;
	Gdiplus::RectF entityTextRect = rect;

	IFlowGraphDebuggerConstPtr pFlowGraphDebugger = GetIFlowGraphDebuggerPtr();
	const SBreakPoint breakPoint = pFlowGraphDebugger ? pFlowGraphDebugger->GetBreakpoint() : SBreakPoint();

	const bool breakPointHit = (breakPoint.flowGraph != NULL);
	const bool isModule = breakPointHit && (breakPoint.flowGraph->GetType() == IFlowGraph::eFGT_Module);
	const bool isModuleDummy = pFlowNode && isModule && (pFlowNode->GetIFlowGraph()->GetType() == IFlowGraph::eFGT_Module);

	if (pEntityString)
	{
		entityTextRect = pEntityString->GetTextBounds(pG);
		entityTextRect.Offset(0, curY);
		float entityWidth = entityTextRect.Width + PORTS_OUTER_MARGIN;
		width = max(width, entityWidth);
		curY += entityTextRect.Height;
		entityHeight = entityTextRect.Height;

		if (pFlowNode)
		{
			const bool hasBreakPoint = GetIEditorImpl()->GetFlowGraphDebuggerEditor()->HasBreakpoint(pFlowNode, &inputs[0]);

			Gdiplus::RectF rectBreak = entityTextRect;
			rectBreak.Offset(-BREAKPOINT_X_OFFSET, (entityHeight - BREAKPOINT_X_OFFSET) * 0.5f);
			rectBreak.Width = BREAKPOINT_X_OFFSET;
			rectBreak.Height = BREAKPOINT_X_OFFSET;

			const bool bTriggered = (breakPointHit || isModuleDummy) && !breakPoint.addr.isOutput && (pFlowNode->GetFlowNodeId() == breakPoint.addr.node && breakPoint.addr.port == 0);
			if (hasBreakPoint)
			{
				SFlowAddress addr;
				addr.isOutput = false;
				addr.node = pNode->GetFlowNodeId();
				addr.port = 0;

				bool bIsEnabled = false;
				bool bIsTracepoint = false;
				CheckBreakpoint(pFlowNode->GetIFlowGraph(), addr, bIsEnabled, bIsTracepoint);

				if (bIsTracepoint)
				{
					CDisplayTracepoint* pTracePoint = pList->Add<CDisplayTracepoint>();

					if (bIsEnabled)
						pTracePoint->SetFilled(&pAssets->breakPoint);
					else
						pTracePoint->SetStroked(&pAssets->penBreakpointDisabled);

					pTracePoint->SetRect(rectBreak);
				}
				else
				{
					CDisplayBreakpoint* pBreakPoint = pList->Add<CDisplayBreakpoint>();

					if (bIsEnabled)
						pBreakPoint->SetFilled(&pAssets->breakPoint);
					else
						pBreakPoint->SetStroked(&pAssets->penBreakpointDisabled);

					if (bTriggered)
						pBreakPoint->SetFilledArrow(&pAssets->breakPointArrow);

					pBreakPoint->SetTriggered(bTriggered);
					pBreakPoint->SetRect(rectBreak);
				}
			}
			else if (bTriggered)
			{
				CDisplayPortArrow* pBreakArrow = pList->Add<CDisplayPortArrow>();
				pBreakArrow->SetSize(6);
				pBreakArrow->SetFilled(&pAssets->breakPointArrow);
				pBreakArrow->SetRect(rectBreak);
			}
		}
	}

	float portStartY = curY;
	float inputsWidth = 0.0f;
	for (size_t i = 0; i < inputPorts.size(); i++)
	{
		const SDefaultRenderPort& p = inputPorts[i];
		Gdiplus::PointF textLoc(Gdiplus::PointF(PORTS_OUTER_MARGIN, curY));
		p.pText->SetLocation(textLoc);
		rect = p.pText->GetBounds(pG);
		curY += rect.Height;
		inputsWidth = max(inputsWidth, rect.Width);
		rect.Height -= 4.0f;
		rect.Y += 2.0f;
		rect.Width = rect.Height;
		rect.X = 2.0f;
		if (p.pPortArrow)
			p.pPortArrow->SetRect(rect);
		if (p.pRectangle)
			p.pRectangle->SetRect(rect);
		pList->SetAttachRect(p.id, Gdiplus::RectF(0, rect.Y + rect.Height * 0.5f, 0.0f, 0.0f));

		if (pFlowNode)
		{
			const int portIndex = p.id;
			const bool hasBreakPoint = GetIEditorImpl()->GetFlowGraphDebuggerEditor()->HasBreakpoint(pFlowNode, &inputs[portIndex]);

			Gdiplus::RectF rectBreak = rect;
			rectBreak.Offset(-(BREAKPOINT_X_OFFSET + 2.0f), (rect.Height - BREAKPOINT_X_OFFSET) * 0.5f);
			rectBreak.Width = BREAKPOINT_X_OFFSET;
			rectBreak.Height = BREAKPOINT_X_OFFSET;

			const bool bTriggered = !breakPoint.addr.isOutput && (breakPointHit || isModuleDummy) && portIndex == breakPoint.addr.port && breakPoint.addr.node == pFlowNode->GetFlowNodeId();
			if (hasBreakPoint)
			{
				SFlowAddress addr;
				addr.isOutput = false;
				addr.node = pNode->GetFlowNodeId();
				addr.port = portIndex;

				bool bIsEnabled = false;
				bool bIsTracepoint = false;
				CheckBreakpoint(pFlowNode->GetIFlowGraph(), addr, bIsEnabled, bIsTracepoint);

				if (bIsTracepoint)
				{
					CDisplayTracepoint* pTracePoint = pList->Add<CDisplayTracepoint>();

					if (bIsEnabled)
						pTracePoint->SetFilled(&pAssets->breakPoint);
					else
						pTracePoint->SetStroked(&pAssets->penBreakpointDisabled);

					pTracePoint->SetRect(rectBreak);
				}
				else
				{
					CDisplayBreakpoint* pBreakPoint = pList->Add<CDisplayBreakpoint>();

					if (bIsEnabled)
						pBreakPoint->SetFilled(&pAssets->breakPoint);
					else
						pBreakPoint->SetStroked(&pAssets->penBreakpointDisabled);

					if (bTriggered)
						pBreakPoint->SetFilledArrow(&pAssets->breakPointArrow);

					pBreakPoint->SetTriggered(bTriggered);
					pBreakPoint->SetRect(rectBreak);
				}
			}
			else if (bTriggered)
			{
				CDisplayPortArrow* pBreakArrow = pList->Add<CDisplayPortArrow>();
				pBreakArrow->SetSize(6);
				pBreakArrow->SetFilled(&pAssets->breakPointArrow);
				pBreakArrow->SetRect(rectBreak);
			}
		}
	}
	curY = portStartY;
	for (size_t i = 0; i < inputPorts.size(); i++)
	{
		const SDefaultRenderPort& p = inputPorts[i];
		rect = p.pText->GetBounds(pG);
		if (p.pBackground)
			p.pBackground->SetRect(1, curY, PORTS_OUTER_MARGIN + rect.Width, rect.Height);
		curY += rect.Height;
	}
	float height = curY;
	float outputsWidth = 0.0f;
	for (size_t i = 0; i < outputPorts.size(); i++)
	{
		const SDefaultRenderPort& p = outputPorts[i];
		outputsWidth = max(outputsWidth, p.pText->GetBounds(pG).Width);
	}
	width = max(width, inputsWidth + outputsWidth + 3 * PORTS_OUTER_MARGIN);
	curY = portStartY;
	for (size_t i = 0; i < outputPorts.size(); i++)
	{
		const SDefaultRenderPort& p = outputPorts[i];
		Gdiplus::PointF textLoc(width - PORTS_OUTER_MARGIN, curY);
		p.pText->SetLocation(textLoc);
		rect = p.pText->GetBounds(pG);
		if (p.pBackground)
			p.pBackground->SetRect(width - PORTS_OUTER_MARGIN - rect.Width - 3, curY, PORTS_OUTER_MARGIN + rect.Width + 3, rect.Height);
		curY += rect.Height;
		rect.Height -= 4.0f;
		rect.Width = rect.Height;
		rect.Y += 2.0f;
		rect.X = width - PORTS_OUTER_MARGIN * 0.5f - 3;
		if (p.pPortArrow)
			p.pPortArrow->SetRect(rect);
		if (p.pRectangle)
			p.pRectangle->SetRect(rect);
		pList->SetAttachRect(p.id, Gdiplus::RectF(width, rect.Y + rect.Height * 0.5f, 0.0f, 0.0f));

		if (pFlowNode)
		{
			const int portIndex = p.id - 1000;
			bool hasBreakPoint = GetIEditorImpl()->GetFlowGraphDebuggerEditor()->HasBreakpoint(pFlowNode, &outputs[portIndex]);

			Gdiplus::RectF rectBreak = rect;
			rectBreak.Offset(BREAKPOINT_X_OFFSET, (rect.Height - BREAKPOINT_X_OFFSET) * 0.5f);
			rectBreak.Width = BREAKPOINT_X_OFFSET;
			rectBreak.Height = BREAKPOINT_X_OFFSET;

			const bool bTriggered = breakPoint.addr.isOutput && (breakPointHit || isModuleDummy) && portIndex == breakPoint.addr.port && breakPoint.addr.node == pFlowNode->GetFlowNodeId();
			if (hasBreakPoint)
			{
				SFlowAddress addr;
				addr.isOutput = true;
				addr.node = pNode->GetFlowNodeId();
				addr.port = portIndex;

				bool bIsEnabled = false;
				bool bIsTracepoint = false;
				CheckBreakpoint(pFlowNode->GetIFlowGraph(), addr, bIsEnabled, bIsTracepoint);

				if (bIsTracepoint)
				{
					CDisplayTracepoint* pTracePoint = pList->Add<CDisplayTracepoint>();

					if (bIsEnabled)
						pTracePoint->SetFilled(&pAssets->breakPoint);
					else
						pTracePoint->SetStroked(&pAssets->penBreakpointDisabled);

					pTracePoint->SetRect(rectBreak);
				}
				else
				{
					CDisplayBreakpoint* pBreakPoint = pList->Add<CDisplayBreakpoint>();

					if (bIsEnabled)
						pBreakPoint->SetFilled(&pAssets->breakPoint);
					else
						pBreakPoint->SetStroked(&pAssets->penBreakpointDisabled);

					if (bTriggered)
						pBreakPoint->SetFilledArrow(&pAssets->breakPointArrow);

					pBreakPoint->SetTriggered(bTriggered);
					pBreakPoint->SetRect(rectBreak);
				}
			}
			else if (bTriggered)
			{
				CDisplayPortArrow* pBreakArrow = pList->Add<CDisplayPortArrow>();
				pBreakArrow->SetSize(6);
				pBreakArrow->SetFilled(&pAssets->breakPointArrow);
				pBreakArrow->SetRect(rectBreak);
			}
		}
	}
	height = max(height, curY);

	// down arrows if any ports are hidden.
	if (bAnyHiddenInput || bAnyHiddenOutput)
	{
		height += 1;
		// Draw hidden ports gripper.
		if (bAnyHiddenInput)
		{
			CDisplayRectangle* pGrip = pList->Add<CDisplayRectangle>()
			                           ->SetHitEvent(eSOID_InputGripper);

			Gdiplus::RectF rect = Gdiplus::RectF(1, height, PORT_DOWN_ARROW_WIDTH, PORT_DOWN_ARROW_HEIGHT);
			//pGrip->SetColors( pAssets->colorTitleUnselectedA, pAssets->colorTitleUnselectedB );
			if (pNode->IsSelected())
			{
				if (pNode->CheckFlag(EHYPER_NODE_CUSTOM_COLOR1))
					pGrip->SetFilled(&pAssets->brushBackgroundCustomSelected1);
				else
					pGrip->SetFilled(&pAssets->brushBackgroundSelected);
			}
			else
			{
				if (pNode->CheckFlag(EHYPER_NODE_CUSTOM_COLOR1))
					pGrip->SetFilled(&pAssets->brushBackgroundCustomUnselected1);
				else
					pGrip->SetFilled(&pAssets->brushBackgroundUnselected);
			}
			pGrip->SetRect(rect);

			// Draw arrow.
			AddDownArrow(pList, Gdiplus::PointF(rect.X + rect.Width / 2, (rect.Y + rect.GetBottom()) / 2 + 3), &pAssets->penDownArrow);
		}
		if (bAnyHiddenOutput)
		{
			CDisplayRectangle* pGrip = pList->Add<CDisplayRectangle>()
			                           ->SetHitEvent(eSOID_OutputGripper);

			Gdiplus::RectF rect = Gdiplus::RectF(width - PORT_DOWN_ARROW_WIDTH - 1, height, PORT_DOWN_ARROW_WIDTH, PORT_DOWN_ARROW_HEIGHT);
			//pGrip->SetColors( pAssets->colorTitleUnselectedA, pAssets->colorTitleUnselectedB );
			pGrip->SetRect(rect);
			if (pNode->IsSelected())
			{
				if (pNode->CheckFlag(EHYPER_NODE_CUSTOM_COLOR1))
					pGrip->SetFilled(&pAssets->brushBackgroundCustomSelected1);
				else
					pGrip->SetFilled(&pAssets->brushBackgroundSelected);
			}
			else
			{
				if (pNode->CheckFlag(EHYPER_NODE_CUSTOM_COLOR1))
					pGrip->SetFilled(&pAssets->brushBackgroundCustomUnselected1);
				else
					pGrip->SetFilled(&pAssets->brushBackgroundUnselected);
			}

			// Draw arrow.
			AddDownArrow(pList, Gdiplus::PointF(rect.X + rect.Width / 2, (rect.Y + rect.GetBottom()) / 2 + 3), &pAssets->penDownArrow);
		}

		height += PORT_DOWN_ARROW_HEIGHT + 1;
	}

	// resize backing boxes...
	pTitleString->SetRect(0, 0, width - MINIMIZE_BOX_WIDTH, titleHeight);
	pTitleRect->SetRect(0, 0, width, titleHeight);
	pMinimizeArrow->SetRect(width - (MINIMIZE_BOX_WIDTH - 2), 2, 12, min(MINIMIZE_BOX_MAX_HEIGHT, titleHeight - 4));
	curY = titleHeight;
	if (pEntityRect)
	{
		pEntityString->SetRect(PORTS_OUTER_MARGIN, curY, width - PORTS_OUTER_MARGIN, entityTextRect.Height);
		pEntityRect->SetRect(0, curY, width, entityTextRect.Height);
		pEntityEllipse->SetRect(2, curY + 2, entityTextRect.Height - 4, entityTextRect.Height - 4);
		pList->SetAttachRect(0, Gdiplus::RectF(0, curY + entityHeight * 0.5f, 0.0f, 0.0f));
		curY += entityHeight;
	}
	pBackgroundRect->SetRect(0, curY, width, height - curY);
}

//////////////////////////////////////////////////////////////////////////
void CHyperNodePainter_Default::AddDownArrow(CDisplayList* pList, const Gdiplus::PointF& pnt, Gdiplus::Pen* pPen)
{
	Gdiplus::PointF p = pnt;
	p.Y += 0;
	pList->AddLine(Gdiplus::PointF(p.X, p.Y), Gdiplus::PointF(p.X, p.Y), pPen);
	pList->AddLine(Gdiplus::PointF(p.X - 1, p.Y - 1), Gdiplus::PointF(p.X + 1, p.Y - 1), pPen);
	pList->AddLine(Gdiplus::PointF(p.X - 2, p.Y - 2), Gdiplus::PointF(p.X + 2, p.Y - 2), pPen);
	pList->AddLine(Gdiplus::PointF(p.X - 3, p.Y - 3), Gdiplus::PointF(p.X + 3, p.Y - 3), pPen);
	pList->AddLine(Gdiplus::PointF(p.X - 4, p.Y - 4), Gdiplus::PointF(p.X + 4, p.Y - 4), pPen);
}

void CHyperNodePainter_Default::CheckBreakpoint(IFlowGraphPtr pFlowGraph, const SFlowAddress& addr, bool& bIsBreakPoint, bool& bIsTracepoint)
{
	IFlowGraphDebuggerConstPtr pFlowGraphDebugger = GetIFlowGraphDebuggerPtr();

	if (pFlowGraphDebugger)
	{
		bIsBreakPoint = pFlowGraphDebugger->IsBreakpointEnabled(pFlowGraph, addr);
		bIsTracepoint = pFlowGraphDebugger->IsTracepoint(pFlowGraph, addr);
	}
}

