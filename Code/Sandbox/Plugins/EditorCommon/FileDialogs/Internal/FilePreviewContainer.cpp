// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FilePreviewContainer.h"

#include "CryIcon.h"
#include <CrySystem/File/CryFile.h>
#include "EditorCommonInit.h"

#include "Controls/EditorDialog.h"

#include <QFileInfo>

#include <QVBoxLayout>
#include <QToolButton>

CFilePreviewContainer::CFilePreviewContainer(QWidget* parent)
	: QWidget(parent)
	, m_pToggleButton(nullptr)
	, m_pPreviewWidget(nullptr)
{
	hide(); // always start hidden

	auto previewContainerLayout = new QVBoxLayout;
	previewContainerLayout->setContentsMargins(0, 0, 0, 0);
	previewContainerLayout->setSpacing(0);
	setLayout(previewContainerLayout);

	m_pToggleButton = CreateToggleButton(parent);
}

QAbstractButton* CFilePreviewContainer::GetToggleButton() const
{
	return m_pToggleButton;
}

bool CFilePreviewContainer::IsPreviewablePath(const QString& filePath)
{
	const static QStringList validGeometryExtensions = []()
	{
		return QStringList()
		       << QStringLiteral(CRY_SKEL_FILE_EXT)
		       << QStringLiteral(CRY_SKIN_FILE_EXT)
		       << QStringLiteral(CRY_CHARACTER_DEFINITION_FILE_EXT)
		       << QStringLiteral(CRY_ANIM_GEOMETRY_FILE_EXT)
		       << QStringLiteral(CRY_MATERIAL_FILE_EXT)
		       << QStringLiteral(CRY_GEOMETRY_FILE_EXT)
			   << QStringLiteral("dds");
	} ();

	QFileInfo fileInfo(filePath);
	return validGeometryExtensions.contains(fileInfo.suffix());
}

void CFilePreviewContainer::AddPersonalizedPropertyTo(CEditorDialog* pDialog)
{
	static const QString previewProp = QStringLiteral("showPreview");

	pDialog->AddPersonalizedSharedProperty(previewProp, [this]
	{
		return QVariant(m_pToggleButton->isChecked());
	},
	[this](const QVariant& data)
	{
		if (!data.canConvert<bool>())
		{
		  return;
		}
		const auto showPreview = data.toBool();
		m_pToggleButton->setChecked(showPreview);
	});
}

void CFilePreviewContainer::Clear()
{
	if (!m_pPreviewWidget)
	{
		return;
	}
	layout()->removeWidget(m_pPreviewWidget);
	m_pPreviewWidget->deleteLater();
	m_pPreviewWidget = nullptr;
}

void CFilePreviewContainer::SetPath(const QString& filePath)
{
	Clear();

	if (!m_pToggleButton->isChecked() || filePath.isEmpty())
	{
		return; // nothing to show
	}

	m_pPreviewWidget = GetIEditor()->CreatePreviewWidget(filePath, this);
	if (m_pPreviewWidget)
	{
		layout()->addWidget(m_pPreviewWidget);
		show();
	}
}

QAbstractButton* CFilePreviewContainer::CreateToggleButton(QWidget* parent)
{
	auto button = new QToolButton(parent);
	button->setCheckable(true);
	button->setText(tr("Show Preview"));
	button->setIconSize(QSize(16, 16));
	button->setToolTip(tr("Show Preview"));

	auto setChecked = [this, button](bool checked)
	{
		button->setIcon(
		  CryIcon(
		    checked ? ":/icons/General/Visibility_True.ico"
		    : ":/icons/General/Visibility_False.ico"));

		if (checked)
		{
			Enabled();
		}
		else
		{
			hide();
		}
	};

	connect(button, &QAbstractButton::toggled, setChecked);
	setChecked(false);

	return button;
}

