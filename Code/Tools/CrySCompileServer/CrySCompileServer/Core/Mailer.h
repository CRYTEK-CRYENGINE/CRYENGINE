// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <winsock.h>
#include <string>
#include <set>
#include <list>

class CSMTPMailer
{
public:
	typedef std::string tstr;
	typedef std::set<tstr> tstrcol;
  typedef std::pair<std::string, std::string> tattachment;
  typedef std::list<tattachment> tattachlist;

	static const int DEFAULT_PORT = 25;
	static volatile long 			ms_OpenSockets;

public:
	CSMTPMailer(const tstr& username, const tstr& password, const tstr& server, int port = DEFAULT_PORT);
	~CSMTPMailer();

	bool Send(const tstr& from, const tstrcol& to, const tstrcol& cc, const tstrcol& bcc, const tstr& subject, const tstr& body, const tattachlist& attachments);
	const char* GetResponse() const;

	static volatile long 			GetOpenSockets() { return ms_OpenSockets; }

private:
	void ReceiveLine(SOCKET connection);
	void SendLine(SOCKET connection, const char* format, ...) const;
	void SendRaw(SOCKET connection, const char* data, size_t dataLen) const;
	void SendFile(SOCKET connection, const tattachment &file, const char* boundary) const;

	SOCKET Open(const char* host, unsigned short port,	sockaddr_in& serverAddress);

	void AddReceivers(SOCKET connection, const tstrcol& receivers);
	void AssignReceivers(SOCKET connection, const char* receiverTag, const tstrcol& receivers);
	void SendAttachments(SOCKET connection, const tattachlist& attachments, const char* boundary);

	bool IsEmpty(const tstrcol& col) const;

private:
	tstr m_server;
	tstr m_username;
	tstr m_password;
	int m_port;

	bool m_winSockAvail;
	tstr m_response;
};
