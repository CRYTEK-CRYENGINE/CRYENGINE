// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct IMaterial;
class CUsedResources;

// Temporary interface for IEditorMaterial that doesn't offer much functionality apart from legacy support for the database item methods.
// This should be replaced by a properly featured access to the material and its parameters once the material system is made anew.
struct IEditorMaterial
{
	virtual ~IEditorMaterial() {}
	virtual void AddRef() const = 0;
	virtual void Release() const = 0;
	virtual const string& GetName() const = 0;
	virtual string GetFullName() const = 0;
	virtual bool IsDummy() const = 0;

	//! Please note that these flags may be different from the IMaterial flags
	virtual int  GetFlags() const = 0;
	virtual void DisableHighlight() = 0;

	virtual void GatherUsedResources(CUsedResources& resources) = 0;

	virtual struct IMaterial* GetMatInfo(bool bUseExistingEngineMaterial = false) = 0;
	virtual int GetSubMaterialCount() const = 0;
	virtual bool IsMultiSubMaterial() const = 0;
};


