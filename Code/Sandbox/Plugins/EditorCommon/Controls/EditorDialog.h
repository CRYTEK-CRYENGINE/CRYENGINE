// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"
#include "EditorFramework/PersonalizationManager.h"
#include "QToolWindowManager/QToolWindowCustomWrapper.h"

#include <QDialog>

/**
 * Dialog base class
 *
 * TODO : Handle buttons generically, so they are always placed the same and named the same across all dialogs
 */
class EDITOR_COMMON_API CEditorDialog : public QDialog
{
	Q_OBJECT;
public:
	//! dialogNameId must be unique in the codebase, as this will be used as the key to save its settings and personalization
	CEditorDialog(const QString& dialogNameId, QWidget* parent = nullptr, bool saveSize = true);

	//! Convenience method to store shared properties in personalization manager
	void AddPersonalizedSharedProperty(const QString& propName, std::function<QVariant()>& store, std::function<void(const QVariant&)>& restore);
	//! Convenience method to store project properties in personalization manager
	void AddPersonalizedProjectProperty(const QString& propName, std::function<QVariant()>& store, std::function<void(const QVariant&)>& restore);

	//! Convenience method to store shared properties in personalization manager
	template<typename Store, typename Restore>
	void AddPersonalizedSharedProperty(const QString& propName, Store& store, Restore& restore)
	{
		AddPersonalizedSharedProperty(propName, function_cast<QVariant()>(store), function_cast<void(const QVariant&)>(restore));
	}

	//! Convenience method to store project properties in personalization manager
	template<typename Store, typename Restore>
	void AddPersonalizedProjectProperty(const QString& propName, Store& store, Restore& restore)
	{
		AddPersonalizedProjectProperty(propName, function_cast<QVariant()>(store), function_cast<void(const QVariant&)>(restore));
	}

	const QString& GetDialogName() const { return m_dialogNameId; }

	static QVariantMap s_config;

	//! Prevents the dialog from being resized, must be used after all the layout has been filled up
	void SetResizable(bool resizable);

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

	//! Hides the close button and prevents closing using ESC. Only call this once.
	void SetDoNotClose();

	//! Must be called after being shown //TODO : this should be saved and always work...
	void SetTitle(const QString& title);

protected:
	virtual void changeEvent(QEvent*) override;
	virtual void showEvent(QShowEvent* event) override;
	virtual bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
	virtual void keyPressEvent(QKeyEvent* keyEvent) override;

	bool             m_layoutWrapped;
	QCustomTitleBar* m_titleBar;
	QGridLayout*     m_grid;
	bool             m_saveSize;
	bool			 m_resizable;
	bool			 m_canClose;
	QString			 m_dialogNameId;
};

