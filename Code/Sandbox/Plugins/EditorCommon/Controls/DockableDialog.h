// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "Controls/EditorDialog.h"
#include "Controls/SandboxWindowing.h"

class IPane;

//! Used to spawn a dockable as a dialog. This is used in very rare cases as you lose the docking capabilities, but it enables treating
//! a dockable pane as a dialog which does not influence the layout or the docking system
//! See its typical usage with the default content browser or the find window
//! Note: this only works with Qt panes
class EDITOR_COMMON_API CDockableDialog : public QSandboxWindow
{
	Q_OBJECT;
public:
	CDockableDialog(const QString& dialogNameId, const char* paneClassId, bool saveSize = true);
	~CDockableDialog();

	IPane* GetPane() { return m_pane; }

	template<typename T>
	T* GetPaneT()
	{
		static_assert(std::is_base_of<IPane, T>::value, "GetPaneT can only be called with IPane derived types");
		return static_cast<T*>(GetPane());
	}

	//! Shows the dialog and waits until the dialog exits. Returns true if the dialog succeeded (accepted), false if it was closed or cancelled.
	bool Execute();

	//! Brings the dialog window to the front, being on top of all other windows
	void Raise();

	//! Sets visible and show on top of other windows. Default position is in the center of the main monitor.
	void Popup();
	void Popup(const QPoint &position, const QSize& size);

	//! This will position the window much like a tool window is expected, and cascade them, so several dialogs can be layed out without overlap.
	//! Note that the default dialog position is the center of the screen
	void SetPosCascade();

	//! Hides the dialog when it is finished by any means. Only call this once.
	void SetHideOnClose();

	//! Must be called after being shown //TODO : this should be saved and always work...
	void SetTitle(const QString& title);

	//! Saves state of this dialog in personalization
	void SaveState();

protected:
	bool             m_layoutWrapped;
	QCustomTitleBar* m_titleBar;
	QGridLayout*     m_grid;
	bool             m_saveSize;
	bool			 m_resizable;
	QString			 m_dialogNameId;
	void closeEvent(QCloseEvent* e) override;
	void hideEvent(QHideEvent* e) override;


private:
	IPane* m_pane;
};

//TODO : save/restore layout in personalization
//TODO : only hide on close
