// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QFrame>
#include <ACETypes.h>

class QLabel;

namespace ACE
{

class QAudioSystemModelProxyFilter;
class QAudioSystemModel;

class CAudioSystemPanel : public QFrame
{
	Q_OBJECT

public:
	CAudioSystemPanel();
	void SetAllowedControls(EItemType type, bool bAllowed);
	void Reset();

signals:
	void ImplementationSettingsChanged();

private:
	bool                          m_allowedATLTypes[EItemType::eItemType_NumTypes];
	QAudioSystemModelProxyFilter* m_pModelProxy;
	QAudioSystemModel*            m_pModel;
	QLabel*                       m_pImplNameLabel;
};
}
