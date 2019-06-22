// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "IPropertyTreeWidget.h"
#include "AssetSystem/Asset.h"
#include <CrySerialization/Decorators/Resources.h>
#include <CrySerialization/Decorators/ResourceSelector.h>

#include <IResourceSelectorHost.h>
#include <QLineEdit>

struct SStaticResourceSelectorEntry;

class QLineEdit;
class QMimeData;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QDragLeaveEvent;

//! Widget that accepts drag & drop of a valid asset. Validity is determined by a resource selector.
//! The asset path (first asset file) is copied in the line edit.
class CAssetDragDropLineEdit : public QLineEdit
{
	Q_OBJECT

public:
	CAssetDragDropLineEdit();
	void SetResourceSelector(const SStaticResourceSelectorEntry* pSelector) { m_pSelector = pSelector; }
	//! Sent when a valid asset is dropped
	CCrySignal<void(CAsset&)> signalValidAssetDropped;

protected:
	void dragEnterEvent(QDragEnterEvent* pEvent) override;
	void dragMoveEvent(QDragMoveEvent* pEvent) override;
	void dropEvent(QDropEvent* pEvent) override;
	void dragLeaveEvent(QDragLeaveEvent* pEvent) override;

private:
	//Start of a global drag operation
	void DragDropStart(QMimeData* pMimeData);
	//End of a global drag operation
	void DragDropEnd(QMimeData* pMimeData);

private:
	//We use the selector resource type to validate the type of the asset being dropped
	const SStaticResourceSelectorEntry* m_pSelector;
};
