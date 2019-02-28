// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>

class QAdvancedTreeView;
class CCustomCommand;

class CKeybindEditor : public CDockableWidget
{
	Q_OBJECT

	class KeybindModel;
	class KeybindItemDelegate;
public:
	CKeybindEditor(QWidget* parent = nullptr);
	~CKeybindEditor();

	static QVariant GetState();
	static void     SetState(const QVariant& state);

	static void     SaveKeybindsToFile(const char* filename);
	static void     LoadKeybindsFromFile(const char* filename);

	static void     SaveUserKeybinds();
	static void     LoadUserKeybinds();

	//////////////////////////////////////////////////////////
	// CDockableWidget implementation
	virtual IViewPaneClass::EDockingDirection GetDockingDirection() const override { return IViewPaneClass::DOCK_FLOAT; }
	virtual const char*                       GetPaneTitle() const override        { return "Keyboard Shortcuts"; };
	virtual QRect                             GetPaneRect() override               { return QRect(0, 0, 800, 500); }
	//////////////////////////////////////////////////////////

	virtual void customEvent(QEvent* event) override;
protected:
	static void OnKeybindsChanged();

private:

	void OnContextMenu(const QPoint& pos) const;
	void OnAddCustomCommand() const;
	void OnRemoveCustomCommand(CCustomCommand* command) const;

	KeybindModel* m_model;
	QAdvancedTreeView*    m_treeView;
};

