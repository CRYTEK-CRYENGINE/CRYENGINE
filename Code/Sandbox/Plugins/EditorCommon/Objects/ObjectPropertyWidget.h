// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include "EditorFramework/Inspector.h"

#include <memory>
#include <vector>


class CBaseObject;
class QPropertyTreeLegacy;

struct CObjectEvent;

class EDITOR_COMMON_API CObjectPropertyWidget : public QWidget, public IAutoEditorNotifyListener
{
	Q_OBJECT

public:
	typedef std::function<void (CBaseObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)> TSerializationFunc;

	CObjectPropertyWidget(TSerializationFunc serializationFunc);
	virtual ~CObjectPropertyWidget();
	void OnObjectEvent(const CBaseObject* pObject, const CObjectEvent& event);
	void OnEditorNotifyEvent(EEditorNotifyEvent event);

public:
	void OnBeginUndo();
	void OnUndoEnd(bool undoAccepted);

	void CreatePropertyTrees();

private:
	struct SBaseObjectSerializer;

	typedef std::unique_ptr<QPropertyTreeLegacy>     PropertyTreePtr;
	typedef std::vector<SBaseObjectSerializer> SerializerList;

	void UnregisterObjects();
	void ReloadPropertyTrees();
	void CleanupDeletedObjects();

	PropertyTreePtr    m_propertyTree;
	SerializerList     m_objectSerializers;
	bool               m_bReloadProperties;
	bool               m_bCleanupDeletedObjects;

	TSerializationFunc m_serializationFunc;
};
