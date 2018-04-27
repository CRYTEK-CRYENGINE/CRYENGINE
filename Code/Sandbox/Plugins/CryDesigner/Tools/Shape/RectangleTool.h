// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ShapeTool.h"

namespace Designer
{
//! The rectangle parameter structure which has parameters displayed on the property sheet on the right.
struct RectangleParameter
{
	float m_Width;
	float m_Depth;

	RectangleParameter() : m_Width(0), m_Depth(0)
	{
	}

	void Serialize(Serialization::IArchive& ar)
	{
		if (ar.isEdit())
		{
			ar(LENGTH_RANGE(m_Width), "Width", "Width");
			ar(LENGTH_RANGE(m_Depth), "Depth", "Depth");
		}
	}
};

//! The third phase raising the height is for the Box tool not the Rectangle tool.
//! FYI The Box tool is inherited from the Rectangle Tool so EBoxPhase enum is here to be used both in Rectangle tool and Box tool.
enum EBoxPhase
{
	//! Places the starting point of the rectangle.
	eBoxPhase_PlaceStartingPoint,
	//! Draws the rectangle.
	eBoxPhase_DrawRectangle,
	//! Increase the height of the box.
	eBoxPhase_RaiseHeight,
	//! The drawn rectangle or box is modified by changing the width, depth or height etc on the property sheet in eBoxPhase_Done phase
	eBoxPhase_Done,
};

//! The Rectangle Tool is one of the Shape Tools to draw a rectangle on a surface.
//! It needs basically interactions with the inputs from the mouse, keyboard and UIs.
class RectangleTool : public ShapeTool
{
public:
	RectangleTool(EDesignerTool tool) : ShapeTool(tool)
	{
	}

	//! When switches to this tool, Enter() method is called.
	void Enter() override;

	//! When switches to the another tool, Leave() method is called.
	//! DesignerEditor::SwitchTool() calls the Leave() method of the current tool and then the Enter() method of the new tool. And then the new tool becomes the current tool.
	void Leave() override;

	//! Called when LMB is down.
	bool OnLButtonDown(CViewport* view, UINT nFlags, CPoint point) override;

	//! Called when LMB is up.
	bool OnLButtonUp(CViewport* view, UINT nFlags, CPoint point) override;

	//! Called when the mouse moves.
	bool OnMouseMove(CViewport* view, UINT nFlags, CPoint point) override;

	//! Called when a key is down.
	bool OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags) override;

	//! Called when an EditorNotifyEvent event is sent.
	void OnEditorNotifyEvent(EEditorNotifyEvent event) override;

	//! Displays informations needed for the Rectangle tool.
	void Display(DisplayContext& dc) override;

	//! Returns if the current phase is the first phase for creating an primitive shape.
	//! In this tool the first phase is eBoxPhase_PlaceStartingPoint.
	bool IsPhaseFirstStepOnPrimitiveCreation() const override;

	//! Returns if the seamless selection is possible. The seamless selection is possible when the phase is eBoxPhase_PlaceStartingPoint.
	bool EnabledSeamlessSelection() const override;

	//! Called when any parameters on the property sheet are changed.
	void OnChangeParameter(bool continuous) override;

	//! Serializes the rectangle parameters to display them on the property sheet and save/load them on the Hard Disc.
	virtual void Serialize(Serialization::IArchive& ar) override;

protected:
	//! The drawn rectangle with this tool is registered
	virtual void Register();

	//! Adds the drawn rectangle polygon to the shelf 1 of the model and compile the shelf 1 to render it.
	virtual void UpdateShape(bool bUpdateUIs = true);

	//! This is same as the UpdateShape() in the Rectangle Tool.
	virtual void UpdateShapeWithBoundaryCheck(bool bUpdateUIs = true);

	//! How to deal with the LMB event when the phase is eBoxPhase_PlaceStartingPoint.
	virtual void OnLButtonDownAboutPlaceStartingPointPhase(CViewport* view, UINT nFlags, CPoint point);

	//! The editing phase.
	EBoxPhase m_Phase;

	//! The beginning and ending points of the rectangle.
	BrushVec3 m_v[2];

	//! The rectangle parameter.
	RectangleParameter m_RectangleParameter;

	//! The rectangle polygon created from the m_v[2]
	PolygonPtr m_pRectPolygon;
};
}

