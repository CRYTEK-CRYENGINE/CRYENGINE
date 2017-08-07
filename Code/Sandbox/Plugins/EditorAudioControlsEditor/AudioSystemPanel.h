// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QFrame>
#include <ACETypes.h>

class CElidedLabel;

namespace ACE
{
class QAudioSystemModelProxyFilter;
class QAudioSystemModel;
class CAudioAdvancedTreeView;

class CAudioSystemPanel final : public QFrame
{
	Q_OBJECT

public:

	CAudioSystemPanel();
	virtual ~CAudioSystemPanel() override;

	void SetAllowedControls(EItemType type, bool bAllowed);
	void Reset();

private slots:

	void ShowControlsContextMenu(QPoint const& pos);

signals:

	void ImplementationSettingsChanged();

private:

	bool                          m_allowedATLTypes[EItemType::eItemType_NumTypes];
	QAudioSystemModelProxyFilter* m_pModelProxy;
	QAudioSystemModel*            m_pModel;
	CElidedLabel*                 m_pImplNameLabel;
	CAudioAdvancedTreeView*       m_pTreeView;
	QString                       m_filter;
};
} // namespace ACE
