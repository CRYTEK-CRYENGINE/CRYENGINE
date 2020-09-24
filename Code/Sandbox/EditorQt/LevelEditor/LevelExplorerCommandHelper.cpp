// Copyright 2001-2020 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// Sandbox
#include "Objects/ObjectLayer.h"
#include "Objects/ObjectManager.h"

// EditorCommon
#include <Objects/BaseObject.h>

namespace LevelExplorerCommandHelper
{
namespace Private_LevelExplorerCommandHelper
{
bool GetLastItemState(const std::vector<CObjectLayer*>& layers, std::function<bool(const CObjectLayer*)> layerStateCallBack)
{
	if (!layers.empty())
	{
		return layerStateCallBack(layers[layers.size() - 1]);
	}

	return false;
}

bool GetLastItemState(const std::vector<CObjectLayer*>& layers, const std::vector<CBaseObject*>& objects,
                      std::function<bool(const CObjectLayer*)> layerStateCallBack, std::function<bool(const CBaseObject*)> objectStateCallBack)
{
	if (!objects.empty())
	{
		return objectStateCallBack(objects[objects.size() - 1]);
	}
	else if (!layers.empty())
	{
		return layerStateCallBack(layers[layers.size() - 1]);
	}

	return false;
}

// Returns pair of booleans indicating that a child was found followed by it's state
std::pair<bool, bool> GetLastChildItemState(const std::vector<CObjectLayer*>& layers, std::function<bool(const CBaseObject*)> objectStateCallBack)
{
	if (!layers.empty())
	{
		for (auto ite = layers.rbegin(); ite != layers.rend(); ++ite)
		{
			CObjectLayer* pLayer = *ite;
			std::vector<CBaseObject*> objects;
			GetIEditor()->GetObjectManager()->GetObjects(objects, pLayer);
			if (!objects.empty())
			{
				return std::pair<bool, bool>(true, objectStateCallBack(objects[objects.size() - 1]));
			}

			if (!pLayer->GetChildCount())
				continue;

			for (int i = pLayer->GetChildCount() - 1; i >= 0; --i)
			{
				std::pair<bool, bool> result = GetLastChildItemState({ pLayer->GetChild(i) }, objectStateCallBack);
				if (result.first)
					return result;
			}
		}
	}

	return std::pair<bool, bool>(false, false);
}

bool CheckState(const std::vector<CObjectLayer*>& layers, const std::function<bool(const CObjectLayer*)> layerStateCallBack)
{
	bool lastItemState = GetLastItemState(layers, layerStateCallBack);

	for (const CObjectLayer* pLayer : layers)
	{
		if (lastItemState != layerStateCallBack(pLayer))
			return false;
	}

	return lastItemState;
}

bool CheckState(const std::vector<CObjectLayer*>& layers, const std::vector<CBaseObject*>& objects,
                std::function<bool(const CObjectLayer*)> layerStateCallBack, std::function<bool(const CBaseObject*)> objectStateCallBack)
{
	bool lastItemState = GetLastItemState(layers, objects, layerStateCallBack, objectStateCallBack);

	for (const CBaseObject* pObject : objects)
	{
		if (lastItemState != objectStateCallBack(pObject))
			return false;
	}

	for (const CObjectLayer* pLayer : layers)
	{
		if (lastItemState != layerStateCallBack(pLayer))
			return false;
	}

	return lastItemState;
}

bool CheckChildState(const std::vector<CObjectLayer*>& layers, std::function<bool(const CBaseObject*)> objectStateCallBack)
{
	bool lastChildState = GetLastChildItemState(layers, objectStateCallBack).second;
	for (const CObjectLayer* pLayer : layers)
	{
		std::vector<CBaseObject*> objects;
		GetIEditorImpl()->GetObjectManager()->GetObjects(objects, pLayer);

		for (const CBaseObject* pObject : objects)
		{
			if (lastChildState != objectStateCallBack(pObject))
				return false;
		}

		// Do the same for all child layers
		for (int i = 0; i < pLayer->GetChildCount(); i++)
		{
			if (lastChildState != CheckChildState({ pLayer->GetChild(i) }, objectStateCallBack))
				return false;
		}
	}
	return lastChildState;
}

void SetState(const std::vector<CObjectLayer*>& layers, std::function<void(CObjectLayer*)> layerStateCallBack)
{
	for (CObjectLayer* pLayer : layers)
	{
		layerStateCallBack(pLayer);
	}
}

void SetState(const std::vector<CObjectLayer*>& layers, const std::vector<CBaseObject*>& objects,
              std::function<void(CObjectLayer*)> layerStateCallBack, std::function<void(CBaseObject*)> objectStateCallBack)
{
	for (CBaseObject* pObject : objects)
	{
		objectStateCallBack(pObject);
	}

	for (CObjectLayer* pLayer : layers)
	{
		layerStateCallBack(pLayer);
	}
}

void SetChildState(const std::vector<CObjectLayer*>& layers, std::function<void(CBaseObject*)> objectStateCallBack)
{
	for (CObjectLayer* pLayer : layers)
	{
		CBaseObjectsArray objects;
		GetIEditorImpl()->GetObjectManager()->GetObjects(objects, pLayer);

		for (CBaseObject* pObject : objects)
		{
			objectStateCallBack(pObject);
		}

		// Do the same for all child layers
		for (int i = 0; i < pLayer->GetChildCount(); i++)
		{
			SetChildState({ pLayer->GetChild(i) }, objectStateCallBack);
		}
	}
}

void SetLock(const std::vector<CObjectLayer*>& layers, const std::vector<CBaseObject*>& objects, bool lock)
{
	if (layers.empty() && objects.empty())
		return;

	std::function<void(CObjectLayer*)> layerStateCallBack = [lock](CObjectLayer* pLayer) { pLayer->SetFrozen(lock); };
	std::function<void(CBaseObject*)> objectStateCallBack = [lock](CBaseObject* pObject) { pObject->SetFrozen(lock); };

	SetState(layers, objects, layerStateCallBack, objectStateCallBack);
}
}

void Lock(const std::vector<CObjectLayer*>& layers, const std::vector<CBaseObject*>& objects)
{
	Private_LevelExplorerCommandHelper::SetLock(layers, objects, true);
}

void UnLock(const std::vector<CObjectLayer*>& layers, const std::vector<CBaseObject*>& objects)
{
	Private_LevelExplorerCommandHelper::SetLock(layers, objects, false);
}

bool AreLocked(const std::vector<CObjectLayer*>& layers, const std::vector<CBaseObject*>& objects)
{
	std::function<bool(const CObjectLayer*)> layerStateCallBack = [](const CObjectLayer* pLayer) { return pLayer->IsFrozen(); };
	std::function<bool(const CBaseObject*)> objectStateCallBack = [](const CBaseObject* pObject) { return pObject->IsFrozen(); };

	return Private_LevelExplorerCommandHelper::CheckState(layers, objects, layerStateCallBack, objectStateCallBack);
}

void ToggleLocked(const std::vector<CObjectLayer*>& layers, const std::vector<CBaseObject*>& objects)
{
	Private_LevelExplorerCommandHelper::SetLock(layers, objects, !AreLocked(layers, objects));
}

bool AreVisible(const std::vector<CObjectLayer*>& layers, const std::vector<CBaseObject*>& objects)
{
	std::function<bool(const CObjectLayer*)> layerStateCallBack = [](const CObjectLayer* pLayer) { return pLayer->IsVisible(); };
	std::function<bool(const CBaseObject*)> objectStateCallBack = [](const CBaseObject* pObject) { return pObject->IsVisible(); };

	return Private_LevelExplorerCommandHelper::CheckState(layers, objects, layerStateCallBack, objectStateCallBack);
}

void ToggleVisibility(const std::vector<CObjectLayer*>& layers, const std::vector<CBaseObject*>& objects)
{
	if (layers.empty() && objects.empty())
		return;

	bool visible = AreVisible(layers, objects);

	std::function<void(CObjectLayer*)> layerStateCallBack = [&visible](CObjectLayer* pLayer) { pLayer->SetVisible(!visible); };
	std::function<void(CBaseObject*)> objectStateCallBack = [&visible](CBaseObject* pObject) { pObject->SetVisible(!visible); };

	Private_LevelExplorerCommandHelper::SetState(layers, objects, layerStateCallBack, objectStateCallBack);
}

bool AreChildrenLocked(const std::vector<CObjectLayer*>& layers)
{
	return Private_LevelExplorerCommandHelper::CheckChildState(layers,
	                                                           [](const CBaseObject* pObject) { return pObject->IsFrozen(); });
}

void ToggleChildrenLocked(const std::vector<CObjectLayer*>& layers)
{
	if (layers.empty())
		return;

	bool locked = AreChildrenLocked(layers);

	Private_LevelExplorerCommandHelper::SetChildState(layers,
	                                                  [&locked](CBaseObject* pObject) { return pObject->SetFrozen(!locked); });
}

bool AreChildrenVisible(const std::vector<CObjectLayer*>& layers)
{
	return Private_LevelExplorerCommandHelper::CheckChildState(layers,
	                                                           [](const CBaseObject* pObject) { return pObject->IsVisible(); });
}

void ToggleChildrenVisibility(const std::vector<CObjectLayer*>& layers)
{
	if (layers.empty())
		return;

	bool visible = AreChildrenVisible(layers);

	Private_LevelExplorerCommandHelper::SetChildState(layers,
	                                                  [&visible](CBaseObject* pObject) { return pObject->SetVisible(!visible); });
}

bool AreExportable(const std::vector<CObjectLayer*>& layers)
{
	return Private_LevelExplorerCommandHelper::CheckState(layers, [](const CObjectLayer* pLayer) { return pLayer->IsExportable(); });
}

void ToggleExportable(const std::vector<CObjectLayer*>& layers)
{
	bool exportable = AreExportable(layers);
	std::function<void(CObjectLayer*)> layerStateCallBack = [exportable](CObjectLayer* pLayer) { pLayer->SetExportable(!exportable); };
	Private_LevelExplorerCommandHelper::SetState(layers, layerStateCallBack);
}

bool AreExportableToPak(const std::vector<CObjectLayer*>& layers)
{
	return Private_LevelExplorerCommandHelper::CheckState(layers, [](const CObjectLayer* pLayer) { return pLayer->IsExporLayerPak(); });
}

void ToggleExportableToPak(const std::vector<CObjectLayer*>& layers)
{
	bool exportable = AreExportableToPak(layers);
	std::function<void(CObjectLayer*)> layerStateCallBack = [exportable](CObjectLayer* pLayer) { pLayer->SetExportLayerPak(!exportable); };
	Private_LevelExplorerCommandHelper::SetState(layers, layerStateCallBack);
}

bool AreAutoLoaded(const std::vector<CObjectLayer*>& layers)
{
	return Private_LevelExplorerCommandHelper::CheckState(layers, [](const CObjectLayer* pLayer) { return pLayer->IsDefaultLoaded(); });
}

void ToggleAutoLoad(const std::vector<CObjectLayer*>& layers)
{
	bool autoLoaded = AreAutoLoaded(layers);
	std::function<void(CObjectLayer*)> layerStateCallBack = [autoLoaded](CObjectLayer* pLayer) { pLayer->SetDefaultLoaded(!autoLoaded); };
	Private_LevelExplorerCommandHelper::SetState(layers, layerStateCallBack);
}

bool HavePhysics(const std::vector<CObjectLayer*>& layers)
{
	return Private_LevelExplorerCommandHelper::CheckState(layers, [](const CObjectLayer* pLayer) { return pLayer->HasPhysics(); });
}

void TogglePhysics(const std::vector<CObjectLayer*>& layers)
{
	bool havePhysics = HavePhysics(layers);
	std::function<void(CObjectLayer*)> layerStateCallBack = [havePhysics](CObjectLayer* pLayer){ pLayer->SetHavePhysics(!havePhysics); };
	Private_LevelExplorerCommandHelper::SetState(layers, layerStateCallBack);
}

bool HasPlatformSpecs(const std::vector<CObjectLayer*>& layers, ESpecType type)
{
	std::function<bool(const CObjectLayer*)> layerStateCallBack = [type](const CObjectLayer* pLayer) { return (pLayer->GetSpecs() & type) == type; };
	return Private_LevelExplorerCommandHelper::CheckState(layers, layerStateCallBack);
}

void TogglePlatformSpecs(const std::vector<CObjectLayer*>& layers, ESpecType type)
{
	bool hasSpecs = HasPlatformSpecs(layers, type);
	std::function<void(CObjectLayer*)> layerStateCallBack = [hasSpecs, type](CObjectLayer* pLayer)
	{
		pLayer->SetSpecs(hasSpecs ? pLayer->GetSpecs() & ~type : pLayer->GetSpecs() | type);
	};
	Private_LevelExplorerCommandHelper::SetState(layers, layerStateCallBack);
}
}
