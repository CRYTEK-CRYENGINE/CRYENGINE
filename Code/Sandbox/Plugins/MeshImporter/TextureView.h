// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QAdvancedTreeView.h>

class CTextureModel;
class CComboBoxDelegate;

class CTextureView : public QAdvancedTreeView
{
public:
	CTextureView(QWidget* pParent = nullptr);

	void SetModel(CTextureModel* pTextureModel);
private:
	void CreateContextMenu(const QPoint& point);

	CTextureModel*                     m_pTextureModel;
	std::unique_ptr<CComboBoxDelegate> m_pComboBoxDelegate;
};
