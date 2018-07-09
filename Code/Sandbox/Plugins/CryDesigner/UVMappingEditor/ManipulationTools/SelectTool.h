// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../BaseTool.h"
#include "../UVElement.h"

namespace Designer {
namespace UVMapping
{

enum ESelectionFlag
{
	eSelectionFlag_Vertex  = BIT(0),
	eSelectionFlag_Edge    = BIT(1),
	eSelectionFlag_Polygon = BIT(2),
	eSelectionFlag_Island  = BIT(3)
};

struct RectangleSelectionContext : public _i_reference_target_t
{
	RectangleSelectionContext(bool multipleselection, const Vec3& _start);

	void Draw(DisplayContext& dc);
	void Select(ESelectionFlag flag, const SMouseEvent& me);

	Vec3            start;
	Vec3            end;

	UVElementSetPtr m_pSelectedElements;
};

class SelectTool : public BaseTool
{
public:
	SelectTool(EUVMappingTool tool);

	virtual void OnLButtonDown(const SMouseEvent& me) override;
	virtual void OnLButtonUp(const SMouseEvent& me) override;
	virtual void OnMouseMove(const SMouseEvent& me) override;

	virtual void Display(DisplayContext& dc) override;

protected:

	bool           IsRectSelection()        { return m_pRectangleSelectionContext ? true : false; }
	bool           IsUVCursorSelected()     { return m_bUVCursorSelected; }

	ESelectionFlag GetSelectionFlag() const { return m_SelectionFlag; }

private:

	struct SelectionContext
	{
		bool                                   multipleSelection;
		UVElementSetPtr                        elementSet;
		_smart_ptr<RectangleSelectionContext>* rectSelectionContext;
		UVElement                              uvElement;
		const SMouseEvent*                     mouseEvent;
	};

	void InitSelection();
	void ChooseSelectionType(const SelectionContext& sc);

	_smart_ptr<RectangleSelectionContext> m_pRectangleSelectionContext;
	ESelectionFlag                        m_SelectionFlag;
	bool m_bUVCursorSelected;
};

}
}

