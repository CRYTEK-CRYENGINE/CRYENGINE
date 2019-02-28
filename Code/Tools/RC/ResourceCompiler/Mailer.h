// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <winsock2.h>
#include <vector>

class CSMTPMailer
{
public:
	typedef string tstr;
	typedef std::vector<tstr> tstrcol;

	static const int DEFAULT_PORT = 25;

public:
	CSMTPMailer(const tstr& username, const tstr& password, const tstr& server, int port = DEFAULT_PORT);
	~CSMTPMailer();

	bool Send(const tstr& from, const tstrcol& to, const tstrcol& cc, const tstrcol& bcc, const tstr& subject, const tstr& body, const tstrcol& attachments);
	const char* GetResponse() const;

private:
	void ReceiveLine(SOCKET connection);
	void SendLine(SOCKET connection, const char* format, ...) const;
	void SendRaw(SOCKET connection, const char* data, size_t dataLen) const;
	void SendFile(SOCKET connection, const char* filepath, const char* boundary) const;

	SOCKET Open(const char* host, unsigned short port,	sockaddr_in& serverAddress);

	void AddReceivers(SOCKET connection, const tstrcol& receivers);
	void AssignReceivers(SOCKET connection, const char* receiverTag, const tstrcol& receivers);
	void SendAttachments(SOCKET connection, const tstrcol& attachments, const char* boundary);

	bool IsEmpty(const tstrcol& col) const;

private:
	tstr m_server;
	tstr m_username;
	tstr m_password;
	int m_port;

	bool m_winSockAvail;
	tstr m_response;
};