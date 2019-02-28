// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <Qt>
#include <QString>

#if QT_VERSION <= 0x050000

	#ifndef Q_DECL_OVERRIDE
		#define Q_DECL_OVERRIDE
	#endif

	#ifndef QStringLiteral
		#define QStringLiteral(str) QString::fromUtf8(str)
	#endif

	#define QGuiApplication QApplication

#endif
