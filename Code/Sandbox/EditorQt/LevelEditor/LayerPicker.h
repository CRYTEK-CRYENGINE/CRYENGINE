// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "Controls/EditorDialog.h"

class QAdvancedTreeView;
class CObjectLayer;

//TODO : does not allow to select folders.
class CLayerPicker : public CEditorDialog
{
public:
	CLayerPicker();

	void          SetSelectedLayer(CObjectLayer* layer);
	CObjectLayer* GetSelectedLayer() const { return m_selectedLayer; }

protected:
	virtual QSize sizeHint() const override { return QSize(400, 500); }

private:

	void OnOk();
	void SelectLayerIndex(const QModelIndex& index);

	CObjectLayer*      m_selectedLayer;
	QAdvancedTreeView* m_treeView;
};

