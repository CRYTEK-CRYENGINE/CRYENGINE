// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LegacyOpenDlg.h"

#include <Controls/EditorDialog.h>
#include <Controls/DictionaryWidget.h>

#include <QLayout>
#include <QPushButton>

#include "Util.h"
#include "LegacyOpenDlgModel.h"

//////////////////////////////////////////////////////////////////////////
namespace Cry {
namespace SchematycEd {

CLegacyOpenDlg::CLegacyOpenDlg()
	: CEditorDialog("OpenLegacy", nullptr, false)
	, m_dialogDict(new CLegacyOpenDlgModel)
	, m_dialogDictWidget(new CDictionaryWidget)
{
	QVBoxLayout* layout = new QVBoxLayout();
	setLayout(layout);

	QPushButton* closeBtn = new QPushButton(tr("&Close"));
	connect(closeBtn, &QPushButton::clicked, [this]() { close(); });

	layout->addWidget(m_dialogDictWidget);
	layout->addWidget(closeBtn);

	m_dialogDictWidget->SetDictionary(*m_dialogDict);
}

CLegacyOpenDlg::~CLegacyOpenDlg()
{
	delete m_dialogDict;
	m_dialogDictWidget->deleteLater();
}

CDictionaryWidget* CLegacyOpenDlg::GetDialogDictWidget() const
{
	return m_dialogDictWidget;
}

} //namespace SchematycEd
} //namespace Cry


