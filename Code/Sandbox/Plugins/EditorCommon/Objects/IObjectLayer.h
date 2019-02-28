// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct IObjectLayer
{
	//! Get layer name.
	virtual const string& GetName() const = 0;

	//! Get layer name including its parent names.
	virtual string GetFullName() const = 0;

	//! Get GUID assigned to this layer.
	virtual CryGUID GetGUID() const = 0;

	virtual ~IObjectLayer() {}
	virtual void SetModified(bool isModified = true) = 0;
	virtual void SetVisible(bool isVisible, bool bRecursive = true) = 0;
	virtual void SetFrozen(bool isFrozen, bool bRecursive = true) = 0;
	virtual bool IsVisible(bool bRecursive = true) const = 0;
	virtual bool IsFrozen(bool bRecursive = true) const = 0;

	virtual IObjectLayer* GetParentIObjectLayer() const = 0;
};


