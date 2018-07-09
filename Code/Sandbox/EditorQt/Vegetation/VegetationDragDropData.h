// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "DragDrop.h"

class CVegetationDragDropData : public CDragDropData
{
	Q_OBJECT

public:
	QByteArray     GetObjectListData() const;
	QByteArray     GetGroupListData() const;

	void           SetObjectListData(const QByteArray& data);
	void           SetGroupListData(const QByteArray& data);

	bool           HasObjectListData() const;
	bool           HasGroupListData() const;

	static QString GetObjectListMimeFormat();
	static QString GetGroupListMimeFormat();
};

