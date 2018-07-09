// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QWidget>

class QAbstractButton;
class CEditorDialog;

class CFilePreviewContainer
	: public QWidget
{
	Q_OBJECT

public:
	CFilePreviewContainer(QWidget* parent = nullptr);

	QAbstractButton* GetToggleButton() const;

	static bool      IsPreviewablePath(const QString& filePath);

	void             AddPersonalizedPropertyTo(CEditorDialog* dialog);
	void             Clear();
	void             SetPath(const QString& filePath);

signals:
	void Enabled();

private:
	QAbstractButton* CreateToggleButton(QWidget* parent);

private:
	QAbstractButton* m_pToggleButton;
	QWidget*         m_pPreviewWidget;
};

