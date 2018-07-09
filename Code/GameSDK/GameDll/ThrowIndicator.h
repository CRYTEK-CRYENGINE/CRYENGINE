// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Iron Sight

-------------------------------------------------------------------------
History:
- 28:10:2005   16:00 : Created by Márcio Martins

*************************************************************************/
#pragma once

#ifndef __THROWINDICATOR_H__
#define __THROWINDICATOR_H__

#include "IronSight.h"

class CThrowIndicator : public CIronSight
{

public:
	CRY_DECLARE_GTI(CThrowIndicator);

	CThrowIndicator();
	virtual ~CThrowIndicator();

	virtual bool StartZoom(bool stayZoomed = false, bool fullZoomout = true, int zoomStep = 1) override;
	virtual void StopZoom() override;
	virtual void ExitZoom(bool force) override;

	virtual bool IsZoomed() const override { return m_indicatorShowing; }

private:

	bool m_indicatorShowing;
};

#endif // __THROWINDICATOR_H__
