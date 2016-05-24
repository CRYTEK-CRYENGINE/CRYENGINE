// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QFrame>
#include <QMenu>
#include <QListWidget>

#include "ATLControlsModel.h"
#include "AudioControl.h"
#include <IAudioSystemEditor.h>

class QPropertyTree;

namespace ACE
{
class QConnectionsWidget;

class CInspectorPanel : public QFrame, public IATLControlModelListener
{
	Q_OBJECT
public:
	explicit CInspectorPanel(CATLControlsModel* pATLModel);
	~CInspectorPanel();
	void Reload();

public slots:
	void SetSelectedControls(const ControlList& selectedControls);

private:
	void UpdateConnectionData();

	//////////////////////////////////////////////////////////
	// IAudioSystemEditor implementation
	/////////////////////////////////////////////////////////
	virtual void OnControlModified(ACE::CATLControl* pControl) override;
	//////////////////////////////////////////////////////////

	CATLControlsModel*  m_pATLModel;
	QConnectionsWidget* m_pConnectionList;
	QPropertyTree*      m_pPropertyTree;
	bool                m_bSupressUpdates;
};
}
