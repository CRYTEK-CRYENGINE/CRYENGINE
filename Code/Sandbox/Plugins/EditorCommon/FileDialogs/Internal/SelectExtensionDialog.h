// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#include "Controls/EditorDialog.h"

#include <QString>
#include <QStringList>

#include <memory>

class CSelectExtensionDialog
	: public CEditorDialog
{
	Q_OBJECT
public:
	explicit CSelectExtensionDialog(QWidget* parent = nullptr);
	~CSelectExtensionDialog();

	const QString& GetFilenameWithExtension() const;

	void           SetFilename(const QString& filename);
	void           SetExtensions(const QStringList& extensions);
	void           SetPreferred(const QString& extension);
	bool           IsValid() const; //returns false if there is nothing to choose from

	// QDialog interface
public slots:
	virtual int exec() override;

private:
	struct Implementation;
	std::unique_ptr<Implementation> p;
};

