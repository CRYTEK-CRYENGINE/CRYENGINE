// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Script/IScriptView.h>
#include <CrySchematyc/Script/IScriptRegistry.h>
#include <CrySchematyc/Script/Elements/IScriptComponentInstance.h>

#include <CrySandbox/CrySignal.h>
#include <CrySerialization/Forward.h>
#include <CryIcon.h>

#include <QVariant>
#include <QString>

class CAbstractDictionary;

namespace CrySchematycEditor {

class CAbstractComponentsModel;

class CComponentItem
{
public:
	CComponentItem(Schematyc::IScriptComponentInstance& componentInstance, CAbstractComponentsModel& model);
	~CComponentItem();

	void                                 SetName(QString name);
	QString                              GetName() const { return m_name; }
	QString                              GetDescription() const;
	CryIcon                              GetIcon() const;

	CAbstractComponentsModel&            GetModel() const    { return m_model; }

	Schematyc::IScriptComponentInstance& GetInstance() const { return m_componentInstance; }

	void                                 Serialize(Serialization::IArchive& archive);

public:
	CCrySignal<void(CComponentItem&)> SignalComponentChanged;

private:
	CAbstractComponentsModel&            m_model;
	Schematyc::IScriptComponentInstance& m_componentInstance;
	QString                              m_name;
};

class CAbstractComponentsModel
{
public:
	virtual uint32                     GetComponentItemCount() const = 0;
	virtual CComponentItem*            GetComponentItemByIndex(uint32 index) const = 0;
	virtual CComponentItem*            CreateComponent(CryGUID typeId, const char* szName) = 0;
	virtual bool                       RemoveComponent(CComponentItem& component) = 0;

	virtual CAbstractDictionary*       GetAvailableComponentsDictionary() = 0;

	virtual Schematyc::IScriptElement* GetScriptElement() const = 0;

public:
	CCrySignal<void(CComponentItem&)> SignalAddedComponentItem;
	CCrySignal<void(CComponentItem&)> SignalRemovedComponentItem;
};

}

