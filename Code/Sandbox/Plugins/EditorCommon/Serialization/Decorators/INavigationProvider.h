// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Serialization
{

struct SNavigationContext
{
	string path;
};

struct INavigationProvider
{
	virtual const char* GetIcon(const char* type, const char* path) const = 0;
	virtual const char* GetFileSelectorMaskForType(const char* type) const = 0;
	virtual bool        IsSelected(const char* type, const char* path, int index) const = 0;
	virtual bool        IsActive(const char* type, const char* path, int index) const = 0;
	virtual bool        IsModified(const char* type, const char* path, int index) const = 0;
	virtual bool        Select(const char* type, const char* path, int index) const = 0;
	virtual bool        CanSelect(const char* type, const char* path, int index) const { return false; }
	virtual bool        CanPickFile(const char* type, int index) const                 { return true; }
	virtual bool        CanCreate(const char* type, int index) const                   { return false; }
	virtual bool        Create(const char* type, const char* path, int index) const    { return false; }
	virtual bool        IsRegistered(const char* type) const                           { return false; }
};

}

