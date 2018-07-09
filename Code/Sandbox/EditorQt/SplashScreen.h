// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <QSplashScreen>

class SplashScreen : public QSplashScreen
{

public:
	SplashScreen(const QPixmap& pixmap = QPixmap(), Qt::WindowFlags f = 0);
	SplashScreen(QWidget* parent, const QPixmap& pixmap = QPixmap(), Qt::WindowFlags f = 0);
	~SplashScreen();

	static void  SetVersion(const Version& v);
	static void  SetText(const char* text);

protected:
	virtual void drawContents(QPainter* painter) override;

private:
	void         SetInfo(const char* text);
	void         init();

	QString            version;
	QString            m_text;

	static SplashScreen* s_pLogoWindow;
};

