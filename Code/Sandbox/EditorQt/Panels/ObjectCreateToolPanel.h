// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <EditorFramework/Editor.h>
#include "IEditorImpl.h"

#include "FileDialogs/FileDialogsCommon.h"

class QWidget;
class QVBoxLayout;
class QStackedLayout;
class QGridLayout;
class QPreviewWidget;
class QToolButton;

class CCreateObjectButtons : public QWidget
{
	Q_OBJECT
public:
	CCreateObjectButtons(QWidget* pParent = nullptr);
	~CCreateObjectButtons();

	void AddButton(const char* type, const std::function<void()>& onClicked);
private:

	QGridLayout* m_layout;
};

class CObjectCreateToolPanel : public CDockableEditor
{
	Q_OBJECT
public:
	CObjectCreateToolPanel(QWidget* pParent = nullptr);
	~CObjectCreateToolPanel();

	virtual const char* GetEditorName() const { return "Create Object"; }

protected:
	void OnBackButtonPressed();
	virtual void mousePressEvent(QMouseEvent* pMouseEvent) override;
	virtual bool eventFilter(QObject* pWatched, QEvent* pEvent) override;

private:
	void         OnObjectTypeSelected(const char* type);
	void         OnObjectSubTypeSelected(const char* type, QWidget* parent);
	void         Animate(QWidget* pIn, QWidget* pOut, bool bReverse = false);

	QWidget*     CreateWidgetOrStartCreate(const char* type);
	QWidget*     CreateWidgetContainer(QWidget* pChild, const char* type);

	void         StartCreation(CObjectClassDesc* cls, const char* file);
	void         AbortCreateTool();

	void         UpdatePreviewWidget(QString path, QPreviewWidget* previewWidget, QToolButton* showPreviewButton);

	QStackedLayout*       m_stacked;
	CCreateObjectButtons* m_pTypeButtonPanel;
	std::vector<string>   m_typeToStackIndex;
};

