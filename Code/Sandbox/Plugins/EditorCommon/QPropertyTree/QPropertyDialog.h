// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include <memory>

#include "Controls/EditorDialog.h"

#include <CrySerialization/Forward.h>
#include <CrySerialization/yasli/Config.h>

namespace yasli{
class BinOArchive;
}

class QPropertyTree;
class QBoxLayout;

class EDITOR_COMMON_API QPropertyDialog : public CEditorDialog
{
	Q_OBJECT
public:
	static bool edit(Serialization::SStruct& ser, const char* title, const char* windowStateFilename, QWidget* parent);

	QPropertyDialog(QWidget* parent);
	~QPropertyDialog();

	void        setSerializer(const Serialization::SStruct& ser);
	void        setArchiveContext(Serialization::SContextLink* context);
	void        setWindowStateFilename(const char* windowStateFilename);
	void        setSizeHint(const QSize& sizeHint);
	void        setStoreContent(bool storeContent);

	void        revert();
	QBoxLayout* layout() { return m_layout; }

	void        Serialize(Serialization::IArchive& ar);
protected slots:
	void        onAccepted();
	void        onRejected();

protected:
	QSize sizeHint() const override;
	void  setVisible(bool visible) override;
private:
	QPropertyTree*                          m_propertyTree;
	QBoxLayout*                             m_layout;
	std::unique_ptr<Serialization::SStruct> m_serializer;
	std::unique_ptr<yasli::BinOArchive>     m_backup;
	yasli::string                           m_windowStateFilename;
	QSize m_sizeHint;
	bool  m_storeContent;
};

