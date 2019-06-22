// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../Common/SharedData.h"
#include <QWidget>

class QFilteringPanel;
class QPushButton;

namespace ACE
{
namespace Impl
{
namespace PortAudio
{
class CFilterProxyModel;
class CItemModel;
class CTreeView;
class CImpl;

class CDataPanel final : public QWidget
{
public:

	CDataPanel() = delete;
	CDataPanel(CDataPanel const&) = delete;
	CDataPanel(CDataPanel&&) = delete;
	CDataPanel& operator=(CDataPanel const&) = delete;
	CDataPanel& operator=(CDataPanel&&) = delete;

	explicit CDataPanel(CImpl const& impl);
	virtual ~CDataPanel() override;

	void Reset();
	void OnBeforeReload();
	void OnAfterReload();
	void OnSelectConnectedItem(ControlId const id);
	void OnFileImporterOpened();
	void OnFileImporterClosed();

private:

	// QObject
	virtual bool eventFilter(QObject* pObject, QEvent* pEvent) override;
	// ~QObject

	void OnContextMenu(QPoint const& pos);
	void PlayEvent();
	void StopEvent();
	void OnImportFiles();
	void ClearFilters();

	CImpl const&             m_impl;
	CFilterProxyModel* const m_pFilterProxyModel;
	CItemModel* const        m_pModel;
	CTreeView* const         m_pTreeView;
	QPushButton* const       m_pImportButton;
	QFilteringPanel*         m_pFilteringPanel;
};
} // namespace PortAudio
} // namespace Impl
} // namespace ACE
