// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "AssetDragDropLineEdit.h"

#include "AssetSystem/AssetType.h"
#include "DragDrop.h"

#include <QEvent>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>

#include <CrySerialization/Decorators/Resources.h>
#include <CrySerialization/Decorators/ResourceSelector.h>

#include "IResourceSelectorHost.h"

namespace Private_ResourcePickers
{
/*
   -dragState of type string
   - "none" No drag is in progress
   - "valid" Drag is in progress and the asset being dragged can be accepted by this line edit
   - "invalid" Drag is in progress and the asset being dragged can NOT be accepted by this line edit
 */
constexpr const char* s_dragStateName = "dragState";

/*
   - isInDragArea of type boolean
   - false dragging is outside the widget area
   - true dragging is inside the widget area
 */
constexpr const char* s_isInDragArea = "isInDragArea";

/*! Checks if the current drag drop data are storing a valid asset
   \return One valid asset or nullptr if no assets are found
 */
CAsset* GetValidAssetForSelector(const CDragDropData& dragDropData, const SStaticResourceSelectorEntry* pSelector)
{
	//If we have a valid selector and we are dropping assets
	if (pSelector && dragDropData.HasCustomData("Assets"))
	{
		//Extract an array of assets
		QVector<quintptr> tmp;
		QByteArray byteArray = dragDropData.GetCustomData("Assets");
		QDataStream stream(byteArray);
		stream >> tmp;

		//If we have more than one asset this operation is not valid
		if (tmp.size() != 1)
		{
			return nullptr;
		}

		std::vector<CAsset*> assets;

		std::transform(tmp.begin(), tmp.end(), std::back_inserter(assets), [](quintptr pAssetPointer)
		{
			return reinterpret_cast<CAsset*>(pAssetPointer);
		});

		CAsset* pAsset = assets[0];
		//Finally make sure the asset type and the selector type are matching
		if (pAsset->GetType()->GetTypeName() == pSelector->GetTypeName())
		{
			return pAsset;
		}
	}

	return nullptr;
}
}

CAssetDragDropLineEdit::CAssetDragDropLineEdit()
	: m_pSelector(nullptr)
{
	setProperty(Private_ResourcePickers::s_dragStateName, "none");
	setProperty(Private_ResourcePickers::s_isInDragArea, false);

	style()->unpolish(this);
	style()->polish(this);

	setAcceptDrops(true);

	CDragDropData::signalDragStart.Connect(this, &CAssetDragDropLineEdit::DragDropStart);
	CDragDropData::signalDragEnd.Connect(this, &CAssetDragDropLineEdit::DragDropEnd);
}

void CAssetDragDropLineEdit::dragEnterEvent(QDragEnterEvent* pEvent)
{

	const CDragDropData* pDragDropData = CDragDropData::FromMimeData(pEvent->mimeData());

	CAsset* pValidAsset = Private_ResourcePickers::GetValidAssetForSelector(*pDragDropData, m_pSelector);

	setProperty(Private_ResourcePickers::s_isInDragArea, true);
	style()->unpolish(this);
	style()->polish(this);

	//We only accept the action if we have a valid asset
	if (pValidAsset)
	{
		pEvent->acceptProposedAction();
	}

	//We still accept the event so that the leave and move events are sent to the widget
	pEvent->accept();
}

void CAssetDragDropLineEdit::dragMoveEvent(QDragMoveEvent* pEvent)
{
	const CDragDropData* pDragDropData = CDragDropData::FromMimeData(pEvent->mimeData());

	CAsset* pValidAsset = Private_ResourcePickers::GetValidAssetForSelector(*pDragDropData, m_pSelector);

	if (pValidAsset)
	{
		pEvent->acceptProposedAction();
	}
}

void CAssetDragDropLineEdit::dropEvent(QDropEvent* pEvent)
{
	const CDragDropData* pDragDropData = CDragDropData::FromMimeData(pEvent->mimeData());

	CAsset* pValidAsset = Private_ResourcePickers::GetValidAssetForSelector(*pDragDropData, m_pSelector);

	//If we are dropping a valid asset set the line edit, notify listeners of the asset selection and finally accept undo
	if (pValidAsset)
	{
		setText(QString(pValidAsset->GetFile(0)));
		signalValidAssetDropped(*pValidAsset);
		pEvent->acceptProposedAction();
	}
}

void CAssetDragDropLineEdit::dragLeaveEvent(QDragLeaveEvent* pEvent)
{
	setProperty(Private_ResourcePickers::s_isInDragArea, false);
	style()->unpolish(this);
	style()->polish(this);
}

void CAssetDragDropLineEdit::DragDropStart(QMimeData* pMimeData)
{
	if (Private_ResourcePickers::GetValidAssetForSelector(*(qobject_cast<CDragDropData*>(pMimeData)), m_pSelector))
	{
		setProperty(Private_ResourcePickers::s_dragStateName, "valid");
		style()->unpolish(this);
		style()->polish(this);
	}
	else
	{
		setProperty(Private_ResourcePickers::s_dragStateName, "invalid");
		style()->unpolish(this);
		style()->polish(this);
	}
}

void CAssetDragDropLineEdit::DragDropEnd(QMimeData* pMimeData)
{
	setProperty(Private_ResourcePickers::s_isInDragArea, false);
	setProperty(Private_ResourcePickers::s_dragStateName, "none");
	style()->unpolish(this);
	style()->polish(this);
}