// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "QPropertyDialog.h"
#include "Serialization/QPropertyTree/QPropertyTree.h"
#include <CrySerialization/IArchive.h>
#include <CrySerialization/yasli/BinArchive.h>
#include <CrySerialization/yasli/JSONIArchive.h>
#include <CrySerialization/yasli/JSONOArchive.h>
#include <QBoxLayout>
#include <QDialogButtonBox>
#include <QDir>

#ifndef SERIALIZATION_STANDALONE
	#include <CrySystem/File/CryFile.h>
	#include <IEditor.h>
#else
namespace PathUtil
{
string GetParentDirectory(const char* path)
{
	const char* end = strrchr(path, '/');
	if (!end)
		end = strrchr(path, '\\');
	if (end)
		return string(path, end);
	else
		return string();
}
};
#endif

#ifndef SERIALIZATION_STANDALONE
	#include <IEditor.h>
#endif

static string getFullStateFilename(const char* filename)
{
#ifdef SERIALIZATION_STANDALONE
	// use current folder
	return filename;
#else
	string path = GetIEditor()->GetUserFolder();
	if (!path.empty() && path[path.size() - 1] != '\\' && path[path.size() - 1] != '/')
		path.push_back('\\');
	path += filename;
	return path;
#endif
}

bool QPropertyDialog::edit(Serialization::SStruct& ser, const char* title, const char* windowStateFilename, QWidget* parent)
{
	QPropertyDialog dialog(parent);
	dialog.setSerializer(ser);
	dialog.setWindowTitle(QString::fromLocal8Bit(title));
	dialog.setWindowStateFilename(windowStateFilename);

	return dialog.exec() == QDialog::Accepted;
}

QPropertyDialog::QPropertyDialog(QWidget* parent)
	: CEditorDialog("QPropertyDialog")
	, m_sizeHint(440, 500)
	, m_layout(0)
	, m_storeContent(false)
{
	connect(this, SIGNAL(accepted()), this, SLOT(onAccepted()));
	connect(this, SIGNAL(rejected()), this, SLOT(onRejected()));
	setModal(true);
	setWindowModality(Qt::ApplicationModal);

	m_propertyTree = new QPropertyTree(this);
	m_propertyTree->setExpandLevels(1);
	
	PropertyTreeStyle treeStyle(QPropertyTree::defaultTreeStyle());
	treeStyle.propertySplitter = false;
	m_propertyTree->setTreeStyle(treeStyle);

	m_layout = new QBoxLayout(QBoxLayout::TopToBottom);
	setLayout(m_layout);

	m_layout->addWidget(m_propertyTree, 1);
	QDialogButtonBox* buttons = new QDialogButtonBox(this);
	buttons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
	connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));
	m_layout->addWidget(buttons, 0);
}

QPropertyDialog::~QPropertyDialog()
{
}

void QPropertyDialog::revert()
{
	if (m_propertyTree)
		m_propertyTree->revert();
}

void QPropertyDialog::setSerializer(const Serialization::SStruct& ser)
{
	if (!m_serializer)
		m_serializer.reset(new Serialization::SStruct());
	*m_serializer = ser;
}

void QPropertyDialog::setWindowStateFilename(const char* windowStateFilename)
{
	m_windowStateFilename = windowStateFilename;
}

void QPropertyDialog::setSizeHint(const QSize& size)
{
	m_sizeHint = size;
}

void QPropertyDialog::setStoreContent(bool storeContent)
{
	m_storeContent = storeContent;
}

QSize QPropertyDialog::sizeHint() const
{
	return m_sizeHint;
}

void QPropertyDialog::setVisible(bool visible)
{
	QDialog::setVisible(visible);

	if (visible)
	{
		string fullStateFilename = getFullStateFilename(m_windowStateFilename.c_str());
		if (!fullStateFilename.empty())
		{
			yasli::JSONIArchive ia;
			if (ia.load(fullStateFilename.c_str()))
				ia(*this);
		}

		m_backup.reset(new yasli::BinOArchive());
		if (m_serializer && *m_serializer)
		{
			const Serialization::SStruct& ser = *m_serializer;
			(*m_backup)(ser, "backup");
			m_propertyTree->attach(*m_serializer);
		}
	}
}

void QPropertyDialog::onAccepted()
{
	string fullStateFilename = getFullStateFilename(m_windowStateFilename.c_str());
	if (!fullStateFilename.empty())
	{
		yasli::JSONOArchive oa;
		oa(*this);

		QDir().mkdir(QString::fromLocal8Bit(PathUtil::GetParentDirectory(fullStateFilename.c_str()).c_str()));
		oa.save(fullStateFilename.c_str());
	}
}

void QPropertyDialog::onRejected()
{
	if (m_backup.get() && m_serializer.get() && *m_serializer)
	{
		// restore previous object state
		yasli::BinIArchive ia;
		if (ia.open(m_backup->buffer(), m_backup->length()))
		{
			const Serialization::SStruct& ser = *m_serializer;
			ia(ser, "backup");
		}
	}
}

void QPropertyDialog::setArchiveContext(Serialization::SContextLink* context)
{
	m_propertyTree->setArchiveContext(context);
}

void QPropertyDialog::Serialize(Serialization::IArchive& ar)
{
	if (m_storeContent && m_serializer.get())
		ar(*m_serializer, "content");

	QByteArray geometry;
	if (ar.isOutput())
		geometry = saveGeometry();
	std::vector<char> geometryVec(geometry.begin(), geometry.end());
	ar(geometryVec, "geometry");
	if (ar.isInput() && !geometryVec.empty())
		restoreGeometry(QByteArray(geometryVec.data(), (int)geometryVec.size()));

	ar(*m_propertyTree, "propertyTree");
}

