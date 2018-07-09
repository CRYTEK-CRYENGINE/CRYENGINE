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
	, m_pDialogDict(new CLegacyOpenDlgModel)
	, m_pDialogDictWidget(new CDictionaryWidget)
{
	QVBoxLayout* layout = new QVBoxLayout();
	setLayout(layout);

	QPushButton* closeBtn = new QPushButton(tr("&Close"));
	connect(closeBtn, &QPushButton::clicked, [this]() { close(); });

	layout->addWidget(m_pDialogDictWidget);
	layout->addWidget(closeBtn);

	m_pDialogDictWidget->AddDictionary(*m_pDialogDict);
}

CLegacyOpenDlg::~CLegacyOpenDlg()
{
	delete m_pDialogDict;
	m_pDialogDictWidget->deleteLater();
}

CDictionaryWidget* CLegacyOpenDlg::GetDialogDictWidget() const
{
	return m_pDialogDictWidget;
}

} //namespace SchematycEd
} //namespace Cry


