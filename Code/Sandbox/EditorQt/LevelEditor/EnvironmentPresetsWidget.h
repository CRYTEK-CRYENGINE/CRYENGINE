// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Cry3DEngine/ITimeOfDay.h>
#include <QWidget>

class QPropertyTreeLegacy;

class CEnvironmentPresetsWidget : public QWidget, public ITimeOfDay::IListener
{
public:
	struct IPresetsSerializer
	{
		virtual ~IPresetsSerializer() {}
		virtual void Init() = 0;
		virtual void Serialize(Serialization::IArchive& ar) = 0;
	};
public:
	CEnvironmentPresetsWidget(QWidget* pParent = nullptr);
	virtual ~CEnvironmentPresetsWidget();
	void         OnBeginUndo();
	void         OnEndUndo(bool acceptUndo);
	void         BeforeSerialization(Serialization::IArchive& ar);
	void         AfterSerialization(Serialization::IArchive& ar);

	virtual void OnChange(const EChangeType changeType, const char* const szPresetName) override;

private:
	QPropertyTreeLegacy*                      m_pPropertyTree = nullptr;
	bool                                m_ignoreEvent = false;
	std::unique_ptr<IPresetsSerializer> m_pPresetsSerializer;
};
