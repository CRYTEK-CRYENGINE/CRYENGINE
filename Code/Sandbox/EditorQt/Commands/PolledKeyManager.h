// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QObject>

class CPolledKeyCommand;

//! Manages polled key state by intercepting keyboard events and keeping track of the state of the keys
class CPolledKeyManager : public QObject
{
	Q_OBJECT
public:
	CPolledKeyManager();
	~CPolledKeyManager();

	void Init();

	bool IsKeyDown(const char* commandName) const;

private:

	virtual bool eventFilter(QObject* object, QEvent* event) override;
	void         Update();

	typedef std::pair<CPolledKeyCommand*, QKeySequence> CmdAndKey;
	std::vector<CmdAndKey> m_polledKeyState;
};

