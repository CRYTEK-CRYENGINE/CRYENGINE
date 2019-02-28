// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QAdvancedTreeView.h>

#include <functional>

class CMaterialElement;

class CComboBoxDelegate;
class CSortedMaterialModel;

class QSearchBox;
class QMenu;

class CMaterialView : public QAdvancedTreeView
{
	Q_OBJECT
public:
	typedef std::function<void (QMenu*, CMaterialElement*)> AddMaterialContextMenu;

public:
	CMaterialView(CSortedMaterialModel* pModel, QWidget* pParent = nullptr);
	~CMaterialView();

	void                             SetMaterialContextMenuCallback(const AddMaterialContextMenu& callback);

	CSortedMaterialModel* model() const
	{
		return m_pModel;
	}

private:
	void  CreateMaterialContextMenu(const QPoint& point);

private:
	CSortedMaterialModel* m_pModel;
	std::unique_ptr<CComboBoxDelegate> m_pSubMaterialDelegate;
	std::unique_ptr<CComboBoxDelegate> m_pPhysicalizeDelegate;
	AddMaterialContextMenu             m_addMaterialContextMenu;
};

class CMaterialViewContainer : public QWidget
{
public:
	CMaterialViewContainer(CSortedMaterialModel* pModel, CMaterialView* pView, QWidget* pParent = nullptr);

	CMaterialView* GetView();
private:
	CMaterialView* m_pView;
	QSearchBox*    m_pSearchBox;
};

