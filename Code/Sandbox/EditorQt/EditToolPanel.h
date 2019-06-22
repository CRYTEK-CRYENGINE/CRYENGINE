// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QWidget>

#include "IEditorImpl.h"

class QPropertyTreeLegacy;
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

class QEditToolPanel : public QWidget
{
public:
	QEditToolPanel(QWidget* parent = nullptr);
	virtual ~QEditToolPanel() override;

	void OnPreEditToolChanged();
	void OnEditToolChanged();

protected:
	void         ReloadProperties(CEditTool* pTool);
	virtual void SetTool(CEditTool* pTool);
	virtual bool CanEditTool(CEditTool* pTool) = 0;

protected:
	QPropertyTreeLegacy*      m_pPropertyTree;
	SEditToolSerializer m_toolSerializer;
};
