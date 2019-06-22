// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "ResourcePicker.h"
#include "AssetDragDropLineEdit.h"
#include "AssetSystem/Asset.h"
#include "AssetSystem/AssetType.h"

#include <CrySerialization/Decorators/Resources.h>
#include <CrySerialization/Decorators/ResourceSelector.h>

#include "IResourceSelectorHost.h"
#include "CryIcon.h"

#include <IEditor.h>
#include <IUndoManager.h>
#include <QBoxLayout>
#include <QEvent>
#include <QLineEdit>
#include <QToolButton>

namespace Private_ResourcePickers
{

class ResourceSelectionCallback : public IResourceSelectionCallback
{
public:
	virtual void SetValue(const char* newValue) override
	{
		if (m_onValueChanged)
		{
			m_onValueChanged(newValue);
		}
	}

	void SetValueChangedCallback(const std::function<void(const char*)>& onValueChanged)
	{
		m_onValueChanged = onValueChanged;
	}

private:
	std::function<void(const char*)> m_onValueChanged;
};
}

CResourcePicker::CResourcePicker()
	: m_pSelector(nullptr)
{
	m_pLineEdit = new CAssetDragDropLineEdit();

	m_pLayout = new QHBoxLayout();
	m_pLayout->addWidget(m_pLineEdit);
	m_pLayout->setMargin(0);

	setLayout(m_pLayout);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	// needs to be lambda because of default param
	connect(m_pLineEdit, &QLineEdit::editingFinished, [this]() { OnValueChanged(m_pLineEdit->text().toStdString().c_str()); });
	//Register to asset drop event from line picker
	m_pLineEdit->signalValidAssetDropped.Connect([this](CAsset& newAsset)
	{
		OnValueChanged(newAsset.GetFile(0));
	});
}

bool CResourcePicker::event(QEvent* pEvent)
{
	if (pEvent->type() == QEvent::ToolTip)
	{
		if (m_pSelector->ShowTooltip(m_context, m_pLineEdit->text().toStdString().c_str()))
		{
			return true;
		}
	}

	return QWidget::event(pEvent);
}

void CResourcePicker::OnValueChanged(const char* newValue)
{
	//If we are confirming a continuous change (aka pressing ok on the resource selector) skip validation because the data
	//has already been validated in the continuous step
	if (!IsContinuousChange())
	{
		//in case of continuous change m_previousValue is already set to the last value in the resource picker, so it's fine to go ahead and submit and undo. If we are not doing continuous edit we need to actually skip repetition
		if (m_previousValue == newValue)
		{
			return;
		}

		//Run validation check
		SResourceValidationResult validatedPath = m_pSelector->ValidateValue(m_context, newValue, m_previousValue.toStdString().c_str());

		//An empty validatedResource is not valid, but a set to an empty state needs to be allowed
		if (!validatedPath.isValid && !validatedPath.validatedResource.empty())
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Invalid %s: %s", m_context.typeName, newValue);
			m_pLineEdit->setText(m_previousValue);
			if (m_pLineEdit->hasFocus())
			{
				m_pLineEdit->selectAll();
			}

			//we don't want an undo to be registered here as it will be just undoing to the same value (m_previousValue)
			return;
		}
	}

	m_previousValue = newValue;

	OnChanged();

	if (m_pLineEdit->hasFocus())
	{
		m_pLineEdit->selectAll();
	}
}

void CResourcePicker::OnValueContinuousChange(const char* newValue)
{
	m_pLineEdit->setText(newValue);

	if (m_previousValue == newValue)
	{
		return;
	}

	// Run validation check
	SResourceValidationResult validatedPath = m_pSelector->ValidateValue(m_context, newValue, m_previousValue.toStdString().c_str());

	if (!validatedPath.isValid)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Invalid %s: %s", m_context.typeName, newValue);
		m_pLineEdit->setText(m_previousValue);
		if (m_pLineEdit->hasFocus())
		{
			m_pLineEdit->selectAll();
		}
		return;
	}

	OnContinuousChanged();

	//If we are in continuous edit it means that we are using the resource picker. The picker sets the string in the line edit to an always valid value
	//and since we are using the picker the lined edit actually stores the previous value
	m_previousValue = m_pLineEdit->text();

	if (m_pLineEdit->hasFocus())
	{
		m_pLineEdit->selectAll();
	}
}

void CResourcePicker::OnPick()
{
	CRY_ASSERT(m_pSelector);

	Private_ResourcePickers::ResourceSelectionCallback callback;
	callback.SetValueChangedCallback([this](const char* newValue) { OnValueContinuousChange(newValue); });
	m_context.callback = &callback;

	SResourceSelectionResult result = m_pSelector->SelectResource(m_context, m_pLineEdit->text().toStdString().c_str());

	if (result.selectionAccepted)
	{
		m_pLineEdit->setText(result.selectedResource.c_str());
		OnValueChanged(result.selectedResource.c_str());
	}
	else
	{
		m_previousValue = result.selectedResource.c_str();
		OnDiscarded();
	}
}

void CResourcePicker::OnEdit()
{
	m_pSelector->EditResource(m_context, m_pLineEdit->text().toStdString().c_str());
}

void CResourcePicker::OnPickLegacy()
{
	m_context.useLegacyPicker = true;
	OnPick();
	m_context.useLegacyPicker = false;
}

void CResourcePicker::Init(Serialization::IResourceSelector* pSelector, const yasli::Archive& ar)
{
	// If we have a valid selector and the type hasn't changed for this row, then return
	if (m_pSelector && strcmp(pSelector->resourceType, m_context.typeName) == 0)
	{
		return;
	}

	m_type = pSelector->resourceType;
	m_pSelector = GetIEditor()->GetResourceSelectorHost()->GetSelector(m_type.c_str());

	m_context.resourceSelectorEntry = m_pSelector;
	m_context.typeName = m_type.c_str();
	m_context.parentWidget = QWidget::window();     // TODO: This used to be the root widget for the property tree
	m_context.pCustomParams = pSelector->pCustomParams;

	Serialization::TypeID contextTypeID = m_pSelector->GetContextType();
	if (contextTypeID)
	{
		m_context.contextObject = ar.contextByType(contextTypeID);
		m_context.contextObjectType = contextTypeID;
	}

	// Remove all buttons if we're reusing this row and create on demand based on the requirements of the selector
	while (m_pLayout->count() > 1)
	{
		QLayoutItem* pItem = m_pLayout->itemAt(1);
		m_pLayout->removeItem(pItem);
		delete pItem;
	}

	if (m_pSelector->HasLegacyPicker())
	{
		AddButton("icons:General/LegacyResourceSelectorPicker.ico", "Legacy Open", &CResourcePicker::OnPickLegacy);
	}

	if (m_pSelector->CanEdit())
	{
		AddButton("icons:General/Edit_Asset.ico", "Edit", &CResourcePicker::OnEdit);
	}

	AddButton("icons:General/Folder.ico", "Open", &CResourcePicker::OnPick);

	m_pLineEdit->SetResourceSelector(m_pSelector);
}

void CResourcePicker::AddButton(const char* szIconPath, const char* szToolTip, void (CResourcePicker::* pCallback)())
{
	QToolButton* pButton = new QToolButton();
	pButton->setIcon(CryIcon(szIconPath));
	pButton->setToolTip(szToolTip);
	m_pLayout->addWidget(pButton);
	connect(pButton, &QToolButton::clicked, this, pCallback);
}

void CResourcePicker::SetValue(void* pValue, const yasli::TypeID& type, const yasli::Archive& ar)
{
	//Note that when we get here we have no way of knowing the previous value, the pSelector holds the new value and the line edit too as we might have modified it with a new (possibly invalid) path
	if (type != yasli::TypeID::get<Serialization::IResourceSelector>())
	{
		return;
	}

	Serialization::IResourceSelector* pSelector = (Serialization::IResourceSelector*)pValue;

	Init(pSelector, ar);

	m_pLineEdit->setText(pSelector->GetValue());
}

void CResourcePicker::GetValue(void* pValue, const yasli::TypeID& type) const
{
	if (type != yasli::TypeID::get<Serialization::IResourceSelector>())
	{
		return;
	}

	Serialization::IResourceSelector* pSelector = (Serialization::IResourceSelector*)pValue;
	pSelector->SetValue(m_pLineEdit->text().toStdString().c_str());
}

void CResourcePicker::Serialize(Serialization::IArchive& ar)
{
	string lineEditText = m_pLineEdit->text().toStdString().c_str();
	ar(lineEditText, "text", "Text");
	QString newLineEditTextValue = lineEditText;

	if (newLineEditTextValue != m_previousValue && ar.isInput())
	{
		m_pLineEdit->setText(newLineEditTextValue);
		OnValueChanged(lineEditText);
	}
}

void CResourcePicker::SetMultiEditValue()
{
	m_pLineEdit->setText("");
	m_pLineEdit->setPlaceholderText("Multiple Values");
}

namespace Serialization
{
REGISTER_PROPERTY_WIDGET(IResourceSelector, CResourcePicker);
}
