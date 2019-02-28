// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CONFIGURATIONHISTORY_H__
#define __CONFIGURATIONHISTORY_H__

#pragma once

#include "History.h"

class CConfigurationHistory : public CHistory
{
public:
	CConfigurationHistory(CContextView* pView);

	virtual bool ReadCurrentValue(const SReceiveContext& ctx, bool commit);
	virtual void HandleEvent(const SHistoricalEvent& event);

private:
	virtual bool NeedToSync(const SSyncContext& ctx, CSyncContext* pSyncContext, const CRegularlySyncedItem& item);
	virtual bool CanSync(const SSyncContext& ctx, CSyncContext* pSyncContext, const CRegularlySyncedItem& item);
	virtual bool SendCurrentValue(const SSendContext& ctx, CSyncContext* pSyncContext, uint32& newValue);
};

#endif
