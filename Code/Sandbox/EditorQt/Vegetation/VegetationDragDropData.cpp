// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

#include "VegetationDragDropData.h"

namespace Private_VegetationDragDropData
{
const auto kObjectListIdentifier = "VegetationObjectList";
const auto kGroupListIdentifier = "VegetationGroupList";
}

QByteArray CVegetationDragDropData::GetObjectListData() const
{
	return GetCustomData(Private_VegetationDragDropData::kObjectListIdentifier);
}

QByteArray CVegetationDragDropData::GetGroupListData() const
{
	return GetCustomData(Private_VegetationDragDropData::kGroupListIdentifier);
}

void CVegetationDragDropData::SetObjectListData(const QByteArray& data)
{
	SetCustomData(Private_VegetationDragDropData::kObjectListIdentifier, data);
}

void CVegetationDragDropData::SetGroupListData(const QByteArray& data)
{
	SetCustomData(Private_VegetationDragDropData::kGroupListIdentifier, data);
}

bool CVegetationDragDropData::HasObjectListData() const
{
	return HasCustomData(Private_VegetationDragDropData::kObjectListIdentifier);
}

bool CVegetationDragDropData::HasGroupListData() const
{
	return HasCustomData(Private_VegetationDragDropData::kGroupListIdentifier);
}

QString CVegetationDragDropData::GetObjectListMimeFormat()
{
	return GetMimeFormatForType(Private_VegetationDragDropData::kObjectListIdentifier);
}

QString CVegetationDragDropData::GetGroupListMimeFormat()
{
	return GetMimeFormatForType(Private_VegetationDragDropData::kGroupListIdentifier);
}

