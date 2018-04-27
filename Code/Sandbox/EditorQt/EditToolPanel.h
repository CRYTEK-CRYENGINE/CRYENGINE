// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QWidget>

#include "IEditorImpl.h"

class QPropertyTree;
class CEditTool;

struct SEditToolSerializer
{
public:
	SEditToolSerializer()
		: pEditTool(nullptr)
	{}

	void YASLI_SERIALIZE_METHOD(Serialization::IArchive& ar);

	CEditTool* pEditTool;
};

class QEditToolPanel : public QWidget, public IEditorNotifyListener
{
public:
	QEditToolPanel(QWidget* parent = nullptr);
	virtual ~QEditToolPanel() override;

	virtual void OnEditorNotifyEvent(EEditorNotifyEvent e) override;

protected:
	void         ReloadProperties(CEditTool* pTool);
	virtual void SetTool(CEditTool* pTool);
	virtual bool CanEditTool(CEditTool* pTool) = 0;

protected:
	QPropertyTree*      m_pPropertyTree;
	SEditToolSerializer m_toolSerializer;
};

