// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "PlaygroundDockable.h"

#include "QtViewPane.h"

#include <QLabel>

//Uncomment this to register and use the Playground Dockable
//REGISTER_VIEWPANE_FACTORY(CPlaygroundDockable, "Playground", "", false);

CPlaygroundDockable::CPlaygroundDockable()
	: CDockableEditor()
{
	//Sample code, replace with your test
	auto someTestWidget = new QLabel("Playground Dockable");
	someTestWidget->setAlignment(Qt::AlignCenter);
	someTestWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	SetContent(someTestWidget);
}

CPlaygroundDockable::~CPlaygroundDockable()
{

}

