// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct IObjectLayerManager
{
	//! Find layer by layer GUID.
	virtual IObjectLayer* FindLayer(CryGUID guid) const = 0;

	//! Search for layer by name.
	virtual IObjectLayer* FindLayerByFullName(const string& layerFullName) const = 0;

	//! Search for layer by name.
	virtual IObjectLayer* FindLayerByName(const string& layerName) const = 0;
	
	//! Get this layer is current.
	virtual IObjectLayer*  GetCurrentLayer() const = 0;
};


