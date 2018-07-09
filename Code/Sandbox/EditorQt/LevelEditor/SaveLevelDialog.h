// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#include "Controls/EditorDialog.h"

#include <memory>

class CSaveLevelDialog : public CEditorDialog
{
	Q_OBJECT

public:
	CSaveLevelDialog(const QString& title = tr("Save Level As"));
	~CSaveLevelDialog();

	// return the levelFile as EnginePath after accept
	QString GetAcceptedLevelFile() const;

	// return the levelFile as UserPath after accept
	QString GetAcceptedUserLevelFile() const;

	// return the levelFile as AbsolutePath after accept
	QString GelAcceptedAbsoluteLevelFile() const;

	// select this file
	// used for the last used file
	void SelectLevelFile(const QString& levelFile);

signals:
	// Triggered after the first paint - used to scroll to the last level in the tree
	void AfterFirstPaint();

	// QWidget interface
protected:
	virtual void paintEvent(QPaintEvent* event) override;

private:
	struct Implementation;
	std::unique_ptr<Implementation> p;
};

