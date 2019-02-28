// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "QScrollableBox.h"
#include <memory>
#include <vector>

#include "EditorFramework/Inspector.h"

class QScrollArea;
class QVBoxLayout;
class CBaseObject;
class QShowEvent;
class QPropertyTree;

class EDITOR_COMMON_API CObjectPropertyWidget : public QWidget, public IAutoEditorNotifyListener
{
	Q_OBJECT

public:
	typedef std::function<void(CBaseObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)> TSerializationFunc;

	CObjectPropertyWidget(TSerializationFunc serializationFunc);
	virtual ~CObjectPropertyWidget();
	void OnObjectEvent(class CBaseObject* object, int event);
	void OnEditorNotifyEvent(EEditorNotifyEvent event);

public slots:
	void UndoPush();

	void CreatePropertyTrees();

private:
	struct SBaseObjectSerializer;

	typedef std::unique_ptr<QPropertyTree>     PropertyTreePtr;
	typedef std::vector<SBaseObjectSerializer> SerializerList;

	void UnregisterObjects();
	void ReloadPropertyTrees();
	void CleanupDeletedObjects();

	PropertyTreePtr m_propertyTree;
	SerializerList  m_objectSerializers;
	bool            m_bReloadProperties;
	bool            m_bCleanupDeletedObjects;

	TSerializationFunc m_serializationFunc;
};

