// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "PreferencesDialog.h"
#include "Preferences.h"

#include "Controls/QObjectTreeWidget.h"
#include "QAdvancedTreeView.h"
#include "QtUtil.h"

#include <QAdvancedPropertyTree.h>

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSplitter>

struct SPreferencePagesContainer
{
	SPreferencePagesContainer(const std::vector<SPreferencePage*>& preferences)
		: m_preferences(preferences)
	{
	}

	virtual ~SPreferencePagesContainer() {}

	virtual bool  Serialize(yasli::Archive& ar)
	{
		for (SPreferencePage* pPreferencePage : m_preferences)
			pPreferencePage->Serialize(ar);

		return true;
	}

private:
	std::vector<SPreferencePage*> m_preferences;
};

QPreferencePage::QPreferencePage(std::vector<SPreferencePage*> preferences, const char* path, QWidget* pParent/* = nullptr*/)
	: QWidget(pParent)
	, m_path(path)
{
	QVBoxLayout* pMainLayout = new QVBoxLayout();
	pMainLayout->setMargin(0);
	pMainLayout->setSpacing(0);

	QAdvancedPropertyTree* pPropertyTree = new QAdvancedPropertyTree("Properties", pParent);
	pPropertyTree->setShowContainerIndices(true);
	SPreferencePagesContainer* pContainer = new SPreferencePagesContainer(preferences);
	pPropertyTree->attach(yasli::Serializer(*pContainer));
	pPropertyTree->expandAll();
	connect(pPropertyTree, &QPropertyTree::signalChanged, this, &QPreferencePage::OnPropertyChanged);
	pMainLayout->addWidget(pPropertyTree);

	QHBoxLayout* pActionLayout = new QHBoxLayout();
	pActionLayout->setSpacing(0);
	pActionLayout->setContentsMargins(0, 5, 0, 0);
	pActionLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
	QPushButton* pResetButton = new QPushButton("Reset to Default");
	connect(pResetButton, &QPushButton::clicked, this, &QPreferencePage::OnResetToDefault);
	pActionLayout->addWidget(pResetButton);
	pMainLayout->addLayout(pActionLayout);

	setLayout(pMainLayout);
}

QPreferencePage::QPreferencePage(SPreferencePage* pPreferencePage, QWidget* pParent /*= nullptr*/)
	: QWidget(pParent)
	, m_pPreferencePage(pPreferencePage)
	, m_path(pPreferencePage->GetPath())
{
	QVBoxLayout* pMainLayout = new QVBoxLayout();
	pMainLayout->setMargin(0);
	pMainLayout->setSpacing(0);

	QAdvancedPropertyTree* pPropertyTree = new QAdvancedPropertyTree("Properties", pParent);
	pPropertyTree->setShowContainerIndices(true);
	pPropertyTree->attach(yasli::Serializer(*pPreferencePage));
	pPropertyTree->expandAll();
	connect(pPropertyTree, &QPropertyTree::signalChanged, this, &QPreferencePage::OnPropertyChanged);
	pMainLayout->addWidget(pPropertyTree);

	QHBoxLayout* pActionLayout = new QHBoxLayout();
	pActionLayout->setSpacing(0);
	pActionLayout->setContentsMargins(0, 5, 0, 0);
	pActionLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
	QPushButton* pResetButton = new QPushButton("Reset to Default");
	connect(pResetButton, &QPushButton::clicked, this, &QPreferencePage::OnResetToDefault);
	pActionLayout->addWidget(pResetButton);
	pMainLayout->addLayout(pActionLayout);

	setLayout(pMainLayout);
}

void QPreferencePage::OnPropertyChanged()
{
	GetIEditor()->GetPreferences()->Save();
}

void QPreferencePage::OnResetToDefault()
{
	GetIEditor()->GetPreferences()->Reset(m_path);
}

QPreferencesDialog::QPreferencesDialog(QWidget* pParent /*= nullptr*/)
	: CDockableWidget(pParent)
{
	QVBoxLayout* pMainLayout = new QVBoxLayout();

	m_pTreeView = new QObjectTreeWidget();
	m_pTreeView->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);

	const char* szGeneral = "General";
	CPreferences* pPreferences = GetIEditor()->GetPreferences();
	const std::map<string, std::vector<SPreferencePage*>>& preferences = pPreferences->GetPages();
	for (auto ite = preferences.begin(); ite != preferences.end(); ++ite)
	{
		auto moduleIdx = ite->first.find_last_of("/");
		string module = ite->first.Mid(moduleIdx +1, ite->first.size() - moduleIdx);
		m_pTreeView->AddEntry(ite->first, ite->first, module == szGeneral ? "aGeneral" : "z" + module);
	}
	m_pTreeView->ExpandAll();

	// Create empty preference page container
	m_pContainer = new QContainer();
	m_pSplitter = new QSplitter(Qt::Horizontal);
	m_pSplitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	m_pSplitter->addWidget(m_pTreeView);
	m_pSplitter->addWidget(m_pContainer);
	pMainLayout->addWidget(m_pSplitter);
	m_pSplitter->setSizes({ m_pTreeView->sizeHint().width(), GetPaneRect().width() });

	m_pTreeView->signalOnClickFile.Connect(this, &QPreferencesDialog::OnPreferencePageSelected);
	pPreferences->signalSettingsReset.Connect(this, &QPreferencesDialog::OnPreferencesReset);

	setLayout(pMainLayout);
}

QPreferencesDialog::~QPreferencesDialog()
{
	m_pTreeView->signalOnClickFile.DisconnectObject(this);
	GetIEditor()->GetPreferences()->signalSettingsReset.DisconnectObject(this);
}

void QPreferencesDialog::OnPreferencePageSelected(const char* path)
{
	m_pContainer->SetChild(GetIEditor()->GetPreferences()->GetPageWidget(path));
	m_currPath = path;
}

void QPreferencesDialog::OnPreferencesReset()
{
	m_pContainer->SetChild(GetIEditor()->GetPreferences()->GetPageWidget(m_currPath));
}

