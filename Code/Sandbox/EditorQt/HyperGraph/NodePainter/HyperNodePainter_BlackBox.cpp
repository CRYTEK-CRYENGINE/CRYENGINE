// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HyperNodePainter_BlackBox.h"

#include "HyperNodePainter_Default.h"
#include "HyperGraph/Nodes/BlackBoxNode.h"
#include "HyperGraph/HyperGraph.h"

namespace
{

struct SBlackBoxAssets
{
	SBlackBoxAssets() :
		titleFont(L"Tahoma", 14.0f),
		font(L"Tahoma", 10.0f),
		brushBackground(Gdiplus::Color(0, 0, 0)),
		brushBackgroundSelected(Gdiplus::Color(64, 64, 64)),
		brushBackgroundTransparent(Gdiplus::Color(0, 0, 0, 128)),
		brushBackgroundSelectedTransparent(Gdiplus::Color(64, 64, 64, 128)),
		brushText(Gdiplus::Color(255, 255, 255)),
		brushTextSelected(Gdiplus::Color(255, 255, 255)),
		penBorder(Gdiplus::Color(0, 0, 0)),
		penBackgroundSeeThrough(Gdiplus::Color(128, 128, 128, 128)),
		penMinimize(Gdiplus::Color(255, 255, 255)),
		brushMinimizeArrow(Gdiplus::Color(255, 255, 255))
	{
		sf.SetAlignment(Gdiplus::StringAlignmentCenter);
		sfOut.SetAlignment(Gdiplus::StringAlignmentFar);
		sfIn.SetAlignment(Gdiplus::StringAlignmentNear);
	}

	Gdiplus::Font         titleFont;
	Gdiplus::Font         font;
	Gdiplus::SolidBrush   brushBackground;
	Gdiplus::SolidBrush   brushBackgroundSelected;
	Gdiplus::SolidBrush   brushBackgroundTransparent;
	Gdiplus::SolidBrush   brushBackgroundSelectedTransparent;
	Gdiplus::SolidBrush   brushText;
	Gdiplus::SolidBrush   brushTextSelected;
	Gdiplus::SolidBrush   brushMinimizeArrow;
	Gdiplus::StringFormat sf;
	Gdiplus::StringFormat sfOut;
	Gdiplus::StringFormat sfIn;
	Gdiplus::Pen          penBorder;
	Gdiplus::Pen          penBackgroundSeeThrough;
	Gdiplus::Pen          penMinimize;
};

}

struct SDefaultRenderPortBB
{
	SDefaultRenderPortBB() : pPortArrow(0), pRectangle(0), pText(0), pBackground(0) {}
	CDisplayPortArrow* pPortArrow;
	CDisplayRectangle* pRectangle;
	CDisplayString*    pText;
	CDisplayRectangle* pBackground;
	int                id;
};

void CHyperNodePainter_BlackBox::Paint(CHyperNode* pNode, CDisplayList* pList)
{
	static SBlackBoxAssets* pAssets = 0;
	if (!pAssets)
		pAssets = new SBlackBoxAssets();

	CBlackBoxNode* pBB = (CBlackBoxNode*)pNode;
	bool collapsed = pBB->IsMinimized();
	Gdiplus::RectF bbRect = pNode->GetRect();
	float minX, minY;
	minX = bbRect.X;
	minY = bbRect.Y;
	float maxX, maxY;
	maxX = minX;
	maxY = minY;

	CDisplayRectangle* pBackground = pList->Add<CDisplayRectangle>()
	                                 ->SetHitEvent(eSOID_ShiftFirstOutputPort)
	                                 ->SetStroked(&pAssets->penBorder);
	CDisplayRectangle* pBackgroundCollapsed = NULL;
	if (!collapsed)
	{
		pBackgroundCollapsed = pList->Add<CDisplayRectangle>()
		                       ->SetHitEvent(eSOID_ShiftFirstOutputPort)
		                       ->SetStroked(&pAssets->penBorder);
	}

	CDisplayMinimizeArrow* pMinimizeArrow = pList->Add<CDisplayMinimizeArrow>()
	                                        ->SetStroked(&pAssets->penMinimize)
	                                        ->SetFilled(&pAssets->brushMinimizeArrow)
	                                        ->SetHitEvent(eSOID_Minimize);
	CString text = pNode->GetName();
	text.Replace("\\n", "\r\n");

	CDisplayString* pString = pList->Add<CDisplayString>()
	                          ->SetHitEvent(eSOID_ShiftFirstOutputPort)
	                          ->SetText(text)
	                          ->SetBrush(&pAssets->brushText)
	                          ->SetFont(&pAssets->titleFont)
	                          ->SetStringFormat(&pAssets->sf);

	if (pNode->IsSelected())
	{
		pString->SetBrush(&pAssets->brushTextSelected);
		pBackground->SetFilled(collapsed ? (&pAssets->brushBackgroundSelected) : (&pAssets->brushBackgroundSelectedTransparent));
		if (!collapsed)
			pBackgroundCollapsed->SetFilled(&pAssets->brushBackgroundSelectedTransparent);
	}
	else
	{
		pBackground->SetFilled(collapsed ? (&pAssets->brushBackground) : (&pAssets->brushBackgroundTransparent));
		if (!collapsed)
			pBackgroundCollapsed->SetFilled(&pAssets->brushBackgroundTransparent);
	}

	pBackground->SetHitEvent(eSOID_Title);

	//******************************************************

	Gdiplus::Graphics* pG = pList->GetGraphics();

	std::vector<SDefaultRenderPortBB> inputPorts;
	std::vector<SDefaultRenderPortBB> outputPorts;
	std::vector<CHyperNodePort*> inputPortsPtr;
	std::vector<CHyperNodePort*> outputPortsPtr;

	inputPorts.reserve(10);
	outputPorts.reserve(10);

	CHyperGraph* pGraph = (CHyperGraph*)pNode->GetGraph();

	Gdiplus::RectF rect = pString->GetBounds(pList->GetGraphics());

	float width = rect.Width + 32;
	float curY = 30;
	float height = curY;
	float outputsWidth = 0.0f;
	float inputsWidth = 0.0f;

	std::vector<CHyperNode*>* pNodes = pBB->GetNodesSafe();

	for (int i = 0; i < pNodes->size(); ++i)
	{
		CHyperNode* pNextNode = (*pNodes)[i];

		const CHyperNode::Ports& inputs = *pNextNode->GetInputs();
		const CHyperNode::Ports& outputs = *pNextNode->GetOutputs();

		if (!collapsed)
		{
			Gdiplus::RectF nodeRect = pNextNode->GetRect();
			if (nodeRect.X < minX)
				minX = nodeRect.X;
			if (nodeRect.Y < minY)
				minY = nodeRect.Y;
			if (nodeRect.X + nodeRect.Width > maxX)
				maxX = nodeRect.X + nodeRect.Width;
			if (nodeRect.Y + nodeRect.Height > maxY)
				maxY = nodeRect.Y + nodeRect.Height;
		}

		// output ports
		for (size_t i = 0; i < outputs.size(); i++)
		{
			const CHyperNodePort& pp = outputs[i];
			bool wasActivated = pBB->PortActive(&pp);
			if (pp.nConnected != 0 || wasActivated)
			{
				std::vector<CHyperEdge*> edges;
				if (pp.nConnected > 1)
					pGraph->FindEdges(pNextNode, edges);
				else
					edges.push_back(pGraph->FindEdge(pNextNode, (CHyperNodePort*)(&pp)));

				for (int i = 0; i < edges.size(); ++i)
				{
					const CHyperEdge* pEdge = edges[i];
					if ((pEdge && !pBB->IncludesNode(pEdge->nodeIn)) || (!pEdge && wasActivated))
					{
						SDefaultRenderPortBB pr;

						Gdiplus::SolidBrush* pBrush = &pAssets->brushText;
						pr.pPortArrow = pList->Add<CDisplayPortArrow>()
						                ->SetFilled(pBrush)
						                ->SetHitEvent(eSOID_FirstOutputPort + i);
						//->SetStroked( &pAssets->penPort );

						CString text = pNextNode->GetTitle() + CString(":") + pNextNode->GetPortName(pp);

						pr.pText = pList->Add<CDisplayString>()
						           ->SetFont(&pAssets->font)
						           ->SetStringFormat(&pAssets->sfOut)
						           ->SetBrush(&pAssets->brushText)
						           ->SetHitEvent(eSOID_FirstOutputPort + i)
						           ->SetText(text);
						pr.id = 1000 + i;
						outputPorts.push_back(pr);
						outputPortsPtr.push_back((CHyperNodePort*)(&pp));
					}
				}
			}
		}

		// input ports
		for (size_t i = 0; i < inputs.size(); i++)
		{
			const CHyperNodePort& pp = inputs[i];
			bool wasActivated = pBB->PortActive(&pp);
			if (pp.nConnected != 0 || wasActivated)
			{
				std::vector<CHyperEdge*> edges;
				if (pp.nConnected > 1)
					pGraph->FindEdges(pNextNode, edges);
				else
					edges.push_back(pGraph->FindEdge(pNextNode, (CHyperNodePort*)(&pp)));

				for (int i = 0; i < edges.size(); ++i)
				{
					const CHyperEdge* pEdge = edges[i];
					if ((pEdge && !pBB->IncludesNode(pEdge->nodeOut)) || (!pEdge && wasActivated))
					{
						SDefaultRenderPortBB pr;

						Gdiplus::SolidBrush* pBrush = &pAssets->brushText;
						pr.pPortArrow = pList->Add<CDisplayPortArrow>()
						                ->SetFilled(pBrush)
						                ->SetHitEvent(eSOID_FirstInputPort + i);

						CString text = pNextNode->GetTitle() + CString(":") + pNextNode->GetPortName(pp);

						pr.pText = pList->Add<CDisplayString>()
						           ->SetFont(&pAssets->font)
						           ->SetStringFormat(&pAssets->sfIn)
						           ->SetBrush(&pAssets->brushText)
						           ->SetHitEvent(eSOID_FirstInputPort + i)
						           ->SetText(text);
						pr.id = 1000 + i;
						inputPorts.push_back(pr);
						inputPortsPtr.push_back((CHyperNodePort*)(&pp));
					}
				}
			}
		}

		//**********************************************************

#define BB_PORTS_OUTER_MARGIN 6

		for (size_t i = 0; i < outputPorts.size(); i++)
		{
			const SDefaultRenderPortBB& p = outputPorts[i];
			outputsWidth = max(outputsWidth, p.pText->GetBounds(pG).Width);
		}
		width = max(width, outputsWidth + 3 * BB_PORTS_OUTER_MARGIN);
		for (size_t i = 0; i < inputPorts.size(); i++)
		{
			const SDefaultRenderPortBB& p = inputPorts[i];
			inputsWidth = max(inputsWidth, p.pText->GetBounds(pG).Width);
		}
		width = max(width, inputsWidth + 3 * BB_PORTS_OUTER_MARGIN);

		//drawing ports

		for (size_t i = 0; i < inputPorts.size(); i++)
		{
			const SDefaultRenderPortBB& p = inputPorts[i];
			Gdiplus::PointF textLoc(BB_PORTS_OUTER_MARGIN + 6, curY);
			p.pText->SetLocation(textLoc);
			rect = p.pText->GetBounds(pG);
			if (p.pBackground)
				p.pBackground->SetRect(rect.Width + 3, curY, BB_PORTS_OUTER_MARGIN + rect.Width + 3, rect.Height);
			curY += rect.Height;
			rect.Height -= 4.0f;
			rect.Width = rect.Height;
			rect.Y += 2.0f;
			rect.X = 0;

			pBB->SetPort(inputPortsPtr[i], Gdiplus::PointF(rect.X, rect.Y + rect.Height * 0.5f));

			if (p.pPortArrow)
				p.pPortArrow->SetRect(rect);
			if (p.pRectangle)
				p.pRectangle->SetRect(rect);
			pList->SetAttachRect(p.id, Gdiplus::RectF(width, rect.Y + rect.Height * 0.5f, 0.0f, 0.0f));
		}
		height = max(height, curY);

		curY = 30;

		for (size_t i = 0; i < outputPorts.size(); i++)
		{
			const SDefaultRenderPortBB& p = outputPorts[i];
			Gdiplus::PointF textLoc(width * 2.0f - BB_PORTS_OUTER_MARGIN, curY);
			p.pText->SetLocation(textLoc);
			rect = p.pText->GetBounds(pG);
			if (p.pBackground)
				p.pBackground->SetRect(width - BB_PORTS_OUTER_MARGIN - rect.Width - 3, curY, BB_PORTS_OUTER_MARGIN + rect.Width + 3, rect.Height);
			curY += rect.Height;
			rect.Height -= 4.0f;
			rect.Width = rect.Height;
			rect.Y += 2.0f;
			rect.X = width * 2.0f - BB_PORTS_OUTER_MARGIN;

			pBB->SetPort(outputPortsPtr[i], Gdiplus::PointF(rect.X, rect.Y + rect.Height * 0.5f));

			if (p.pPortArrow)
				p.pPortArrow->SetRect(rect);
			if (p.pRectangle)
				p.pRectangle->SetRect(rect);
			pList->SetAttachRect(p.id, Gdiplus::RectF(width, rect.Y + rect.Height * 0.5f, 0.0f, 0.0f));
		}
		height = max(height, curY);
	}

	//****************************************************************

	rect.Width = width * 2.0f + 5;
	rect.Height = height + 6.0f;
	rect.X = rect.Y = 0.0f;
	pBackground->SetRect(rect);
	pMinimizeArrow->SetRect(rect.Width - (MINIMIZE_BOX_WIDTH - 2), 2, 12, MINIMIZE_BOX_MAX_HEIGHT);
	//output text
	pString->SetLocation(Gdiplus::PointF(rect.Width * 0.5f, 3.0f));
	pBB->SetNormalBounds(rect);
	if (!collapsed)
	{
		rect.X = minX - bbRect.X;
		rect.Y = minY - bbRect.Y;
		if (maxX < pNode->GetPos().X + rect.Width)
			maxX = pNode->GetPos().X + rect.Width;
		if (maxY < pNode->GetPos().Y + rect.Height)
			maxY = pNode->GetPos().Y + rect.Height;
		rect.Width = fabsf(minX - maxX);
		rect.Height = fabsf(minY - maxY);
		rect.X -= 10.0f;
		rect.Y -= 10.0f;
		rect.Width += 20.0f;
		rect.Height += 20.0f;
		pBackgroundCollapsed->SetRect(rect);
	}
}

