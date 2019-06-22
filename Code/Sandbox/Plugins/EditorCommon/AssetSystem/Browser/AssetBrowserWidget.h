// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AssetBrowser.h"

class CAssetEditor;
class QCommandAction;
class QTimer;

class CAssetBrowserWidget : public CAssetBrowser
{
	Q_OBJECT
	Q_INTERFACES(IStateSerializable)
public:
	CAssetBrowserWidget(CAssetEditor* pOwner);
	virtual ~CAssetBrowserWidget();

	virtual const char* GetEditorName() const override { return "Asset Browser Widget"; }
	virtual void        SetLayout(const QVariantMap& state) override;
	virtual QVariantMap GetLayout() const override;

	bool                IsSyncSelection() const;
	void                SetSyncSelection(bool syncSelection);

protected:
	virtual void UpdatePreview(const QModelIndex& currentIndex) override;
	virtual void OnActivated(CAsset* pAsset) override;

	virtual void showEvent(QShowEvent* pEvent) override;

private:
	CAsset* GetCurrentAsset() const;
	void    OnAssetOpened();
	bool    OnSyncSelection();

private:
	CAssetEditor*           m_pAssetEditor = nullptr;
	QCommandAction*         m_pActionSyncSelection = nullptr;
	std::unique_ptr<QTimer> m_pSyncSelectionTimer;

};
