// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: interface definition file for the Crysis remote control system
   -------------------------------------------------------------------------
   History:
   - Created by Lin Luo, November 06, 2006
*************************************************************************/

#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#pragma once

static const uint8 RCONSERVERMSGTYPE_INSESSION = 0;
static const uint8 RCONSERVERMSGTYPE_CHALLENGE = 1;
static const uint8 RCONSERVERMSGTYPE_AUTHORIZED = 2;
static const uint8 RCONSERVERMSGTYPE_AUTHFAILED = 3;
static const uint8 RCONSERVERMSGTYPE_RCONRESULT = 4;
static const uint8 RCONSERVERMSGTYPE_AUTHTIMEOUT = 5;

static const uint8 RCONCLIENTMSGTYPE_MD5DIGEST = 0;
static const uint8 RCONCLIENTMSGTYPE_RCONCOMMAND = 1;

static const uint32 RCON_MAGIC = 0x18181818;

/* cross-platform padding issue.

   SRCONMessageHdr contains variables which aren't aligned to machine word size.
   It is base structure and all RCON network messages are derived from it.

   Since base structure isn't aligned to machine word size so there is
   different memory organization in case of MS VC and GCC under Linux.

   In case of MS VC compiler memory is aligned as follows:
   [SRCONMessageHdr][00 00 00][derived structure]

   While in case of GCC under Linux:
   [SRCONMessageHdr][derived structure][00 00 00]

   So in order to fix it we will force align base structure to have the same
   memory structure under both platforms. */
#pragma pack(push,1)

struct SRCONMessageHdr
{
	SRCONMessageHdr() : magic(0), messageType(255) {}
	SRCONMessageHdr(uint32 m, uint8 t) : magic(m), messageType(t) {}
	uint32 magic; // to filter out accidental erroneous connections
	uint8  messageType;
};

struct SRCONServerMsgChallenge : public SRCONMessageHdr
{
	SRCONServerMsgChallenge() : SRCONMessageHdr(RCON_MAGIC, RCONSERVERMSGTYPE_CHALLENGE) {}
	uint8 challenge[16]; // 16 bytes array
};

struct SRCONServerMsgInSession : public SRCONMessageHdr
{
	SRCONServerMsgInSession() : SRCONMessageHdr(RCON_MAGIC, RCONSERVERMSGTYPE_INSESSION) {}
};

struct SRCONServerMsgAuthorized : public SRCONMessageHdr
{
	SRCONServerMsgAuthorized() : SRCONMessageHdr(RCON_MAGIC, RCONSERVERMSGTYPE_AUTHORIZED) {}
};

struct SRCONServerMsgAuthFailed : public SRCONMessageHdr
{
	SRCONServerMsgAuthFailed() : SRCONMessageHdr(RCON_MAGIC, RCONSERVERMSGTYPE_AUTHFAILED) {}
};

struct SRCONServerMsgRConResult : public SRCONMessageHdr
{
	SRCONServerMsgRConResult() : SRCONMessageHdr(RCON_MAGIC, RCONSERVERMSGTYPE_RCONRESULT), commandId(0), resultLen(0) {}
	uint32 commandId;
	uint32 resultLen; // NOTE: this message is handled on the client side, so security considerations are not that strict as for the server
	//uint8 result[4080]; // null terminated string
};

struct SRCONServerMsgAuthTimeout : public SRCONMessageHdr
{
	SRCONServerMsgAuthTimeout() : SRCONMessageHdr(RCON_MAGIC, RCONSERVERMSGTYPE_AUTHTIMEOUT) {}
};

struct SRCONClientMsgMD5Digest : public SRCONMessageHdr
{
	SRCONClientMsgMD5Digest() : SRCONMessageHdr(RCON_MAGIC, RCONCLIENTMSGTYPE_MD5DIGEST) {}
	uint8 digest[CWhirlpoolHash::DIGESTBYTES]; // 16 bytes array
};

struct SRCONClientMsgRConCommand : public SRCONMessageHdr
{
	SRCONClientMsgRConCommand() : SRCONMessageHdr(RCON_MAGIC, RCONCLIENTMSGTYPE_RCONCOMMAND), commandId(0) { memset(command, 0, sizeof(command)); }
	uint32 commandId;
	uint8  command[256]; // null terminated string
};

#pragma pack(pop)

#endif
