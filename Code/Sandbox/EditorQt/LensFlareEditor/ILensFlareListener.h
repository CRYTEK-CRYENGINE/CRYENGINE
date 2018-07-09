// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CLensFlareItem;
class CLensFlareElement;

class ILensFlareChangeItemListener
{
public:
	virtual void OnLensFlareChangeItem(CLensFlareItem* pLensFlareItem) = 0;
	virtual void OnLensFlareDeleteItem(CLensFlareItem* pLensFlareItem) = 0;
};

class ILensFlareChangeElementListener
{
public:
	virtual void OnLensFlareChangeElement(CLensFlareElement* pLensFlareElement) = 0;
};

