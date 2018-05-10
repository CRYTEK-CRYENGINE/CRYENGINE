// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "UICommon.h"
#include "Tools/Shape/CubeEditor.h"

class QMenuComboBox;

class QPushButton;
class QCheckBox;

namespace Designer
{

class QMaterialComboBox;

class CubeEditorPanel : public QWidget, public IBasePanel
{
	Q_OBJECT
public:
	CubeEditorPanel(CubeEditor* pCubeEditor);

	QWidget* GetWidget() override { return this; }
	void     Update() override;

public Q_SLOTS:
	void UpdateCubeSize(int);

protected:

	void SelectEditMode(ECubeEditorMode editMode);

	CubeEditor*             m_pCubeEditor;
	std::vector<BrushFloat> m_CubeSizeList;

	QPushButton*            m_pAddButton;
	QPushButton*            m_pRemoveButton;
	QPushButton*            m_pPaintButton;

	QMenuComboBox*          m_pBrushSizeComboBox;
	QCheckBox*              m_pMergeSideCheckBox;
};
}

