/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Solutions component.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

// Declaration of the QMfcApp classes

#ifndef QMFCAPP_H
#define QMFCAPP_H

#include <QApplication>

#if defined(_AFXDLL) && defined(_MSC_VER)
	#define QTWINMIGRATE_WITHMFC
class CWinApp;
#endif

#if QT_VERSION >= 0x050000
	#include <QAbstractNativeEventFilter>

class PLUGIN_API QMfcAppEventFilter : public QAbstractNativeEventFilter
{
public:
	QMfcAppEventFilter();
	bool nativeEventFilter(const QByteArray& eventType, void* message, long* result);
};
#endif

class PLUGIN_API QMfcApp : public QApplication
{
public:
	static bool pluginInstance(Qt::HANDLE plugin = 0);

#ifdef QTWINMIGRATE_WITHMFC
	static int           run(CWinApp* mfcApp);
	static QApplication* instance(CWinApp* mfcApp);
	QMfcApp(CWinApp* mfcApp, int& argc, char** argv);
#endif
	QMfcApp(int& argc, char** argv);
	~QMfcApp();

	bool        winEventFilter(MSG* msg, long* result);

	static void enterModalLoop();
	static void exitModalLoop();

public:
#ifdef QTWINMIGRATE_WITHMFC
	static char** mfc_argv;
	static int      mfc_argc;
	static CWinApp* mfc_app;
#endif

	int idleCount;
	bool doIdle;
};

#endif // QMFCAPP_H

