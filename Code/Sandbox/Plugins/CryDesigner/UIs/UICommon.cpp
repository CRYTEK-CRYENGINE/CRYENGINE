// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UICommon.h"
#include "DesignerPanel.h"
#include "DesignerEditor.h"
#include "Tools/BaseTool.h"
#include "Material/Material.h"

#include <QPushButton>
#include <QFrame>

namespace Designer
{

QMaterialComboBox::QMaterialComboBox(QWidget* pParent)
	: QWidget(pParent)
	, m_pComboBox(new QMenuComboBox)
	, signalCurrentIndexChanged(m_pComboBox->signalCurrentIndexChanged)
{
	m_pComboBox->SetMultiSelect(false);
	m_pComboBox->SetCanHaveEmptySelection(false);

	auto pLayout = new QVBoxLayout;
	pLayout->setSpacing(0);
	pLayout->setContentsMargins(0, 0, 0, 0);
	pLayout->addWidget(m_pComboBox);
	setLayout(pLayout);
}

void QMaterialComboBox::SetCurrentIndex(int idx)
{
	m_pComboBox->SetChecked(idx, true);
}

int QMaterialComboBox::GetCurrentIndex() const
{
	return m_pComboBox->GetCheckedItem();
}

int QMaterialComboBox::GetItemCount() const
{
	return m_pComboBox->GetItemCount();
}

void QMaterialComboBox::FillWithSubMaterials()
{
	m_pComboBox->Clear();

	CBaseObject* pObject = DesignerSession::GetInstance()->GetBaseObject();
	if (!pObject)
		return;

	CMaterial* pMaterial = (CMaterial*)pObject->GetMaterial();
	if (!pMaterial)
		return;

	for (int i = 1, iSubMatID(pMaterial->GetSubMaterialCount()); i <= iSubMatID; ++i)
	{
		CMaterial* pSubMaterial = pMaterial->GetSubMaterial(i - 1);
		if (!pSubMaterial)
			continue;
		m_pComboBox->AddItem(QString("%1.%2").arg(i).arg(pSubMaterial->GetFullName().GetBuffer()), QVariant(i - 1));
	}
	m_pComboBox->SetChecked(0);
}

int QMaterialComboBox::GetSubMatID()
{
	return m_pComboBox->GetCheckedItem();
}

void QMaterialComboBox::SetSubMatID(CBaseObject* pObj, int nID)
{
	if (pObj)
	{
		CMaterial* pMaterial = (CMaterial*)pObj->GetMaterial();
		if (!pMaterial || nID >= pMaterial->GetSubMaterialCount() || nID < 0)
		{
			m_pComboBox->SetChecked(0);
		}
		else
		{
			m_pComboBox->SetChecked(nID);
		}
	}
	else
	{
		m_pComboBox->SetChecked(0);
	}
}

bool QMaterialComboBox::SelectMaterial(CBaseObject* pObj, CMaterial* pMaterial)
{
	if (pMaterial == NULL || pObj == NULL)
		return false;
	CMaterial* pParentMat = pMaterial->GetParent();
	if (pParentMat && pParentMat == pObj->GetMaterial())
	{
		for (int i = 0, nSubMaterialCount(pParentMat->GetSubMaterialCount()); i < nSubMaterialCount; ++i)
		{
			if (pMaterial == pParentMat->GetSubMaterial(i))
			{
				SetSubMatID(pObj, i);
				return true;
			}
		}
	}
	return false;
}

QFrame* CreateHorizontalLine()
{
	QFrame* pFrame = new QFrame;
	pFrame->setFrameShape(QFrame::HLine);
	pFrame->setFrameShadow(QFrame::Sunken);
	return pFrame;
}

}

