// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Wrapper for QViewport usable from Qt Creator's UI Designer
#include <QViewPort.h>

class QViewportWrapper : public QViewport
{
public:
	QViewportWrapper(QWidget* pParent)
		: QViewport(gEnv, pParent)
	{
		SetOrbitMode(true);
	}
};

