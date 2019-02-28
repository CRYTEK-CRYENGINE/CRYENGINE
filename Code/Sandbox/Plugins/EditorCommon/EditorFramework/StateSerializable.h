// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QtPlugin>
#include "EditorCommonAPI.h"

//! This is to represent that an object (generally a widget) is serializable into a QVariantMap state.
//! Mostly used for layout but can be used for other practical save/load or copy/paste type of applications
class EDITOR_COMMON_API IStateSerializable
{
public:
	virtual QVariantMap GetState() const = 0;
	virtual void        SetState(const QVariantMap& state) = 0;
};
Q_DECLARE_INTERFACE(IStateSerializable, "EditorCommon/IStateSerializable");

