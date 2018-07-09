// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "InternetSimulatorSocket.h"

#if INTERNET_SIMULATOR

	#include "Network.h"

CInternetSimulatorSocket::SProfileEntry CInternetSimulatorSocket::sm_DefaultProfiles[CInternetSimulatorSocket::DEFAULT_PROFILE_MAX] =
{
	{ 0.0f,  0.0f,  0.00f, 0.04f },                 // DEFAULT_PROFILE_GREAT
	{ 0.0f,  2.0f,  0.01f, 0.05f },                 // DEFAULT_PROFILE_GOOD
	{ 1.0f,  5.0f,  0.04f, 0.08f },                 // DEFAULT_PROFILE_TYPICAL
	{ 2.0f,  8.0f,  0.10f, 0.40f },                 // DEFAULT_PROFILE_IFFY
	{ 5.0f,  12.0f, 0.20f, 0.80f },                 // DEFAULT_PROFILE_BAD
	{ 10.0f, 20.0f, 0.70f, 2.00f },                 // DEFAULT_PROFILE_AWFUL
};

CInternetSimulatorSocket::SProfileEntry* CInternetSimulatorSocket::sm_pProfiles = NULL;
uint32 CInternetSimulatorSocket::sm_nProfiles = 0;

CInternetSimulatorSocket::CInternetSimulatorSocket(IDatagramSocketPtr pChild) : m_pChild(pChild)
{
}

CInternetSimulatorSocket::~CInternetSimulatorSocket()
{
	if (sm_pProfiles && (sm_pProfiles != sm_DefaultProfiles))
	{
		SAFE_DELETE_ARRAY(sm_pProfiles);
		sm_nProfiles = 0;
	}
}

void CInternetSimulatorSocket::SimulatorUpdate(NetTimerId tid, void* p, CTimeValue t)
{
	SPendingSend* pPS = static_cast<SPendingSend*>(p);

	if (!pPS->pThis->IsDead())
	{
		if (ePT_Data == pPS->eType)
			pPS->pThis->m_pChild->Send(pPS->pBuffer, pPS->nLength, pPS->to);
		else if (ePT_Voice == pPS->eType)
			pPS->pThis->m_pChild->SendVoice(pPS->pBuffer, pPS->nLength, pPS->to);
		else
			NetError("[CInternetSimulatorSocket]: Unsupported payload type [%d]", pPS->eType);
	}

	delete[] pPS->pBuffer;
	delete pPS;
}

void CInternetSimulatorSocket::Die()
{
	m_pChild->Die();
}

bool CInternetSimulatorSocket::IsDead()
{
	return m_pChild->IsDead();
}

void CInternetSimulatorSocket::GetSocketAddresses(TNetAddressVec& addrs)
{
	m_pChild->GetSocketAddresses(addrs);
}

static CMTRand_int32 s_InternetSimRandomGen;
ESocketError CInternetSimulatorSocket::Send(const uint8* pBuffer, size_t nLength, const TNetAddress& to)
{
	//local server context can't handle buffering of packets on local connection
	//  buffering can result in packets arriving next frame for object unbound this frame
	//   => will cause disconnect and context destruction
	if (stl::get_if<TLocalNetAddress>(&to))
	{
		return m_pChild->Send(pBuffer, nLength, to);
	}

	g_socketBandwidth.simPacketSends++;

	float randomLoss = 100.0f * s_InternetSimRandomGen.GenerateFloat();
	if (randomLoss < CVARS.net_PacketLossRate)
	{
		g_socketBandwidth.simPacketDrops++;
		return eSE_Ok;
	}

	float fLagRange = CVARS.net_PacketLagMax - CVARS.net_PacketLagMin;
	float fExtraTime = CVARS.net_PacketLagMin;

	if (fLagRange > 0.0f)
	{
		fExtraTime += (fLagRange * s_InternetSimRandomGen.GenerateFloat());
	}

	g_socketBandwidth.simLastPacketLag = (uint32)(fExtraTime * 1000.0f);

	if (fExtraTime <= 0.0f)
	{
		return m_pChild->Send(pBuffer, nLength, to);
	}
	else
	{
		SPendingSend* pPS = new SPendingSend;
		CTimeValue sendTime = g_time + CTimeValue(fExtraTime);
		pPS->eType = ePT_Data;
		pPS->pBuffer = new uint8[nLength];
		memcpy(pPS->pBuffer, pBuffer, nLength);
		pPS->nLength = nLength;
		pPS->to = to;
		pPS->pThis = this;
		NetTimerId timer = ACCURATE_NET_TIMER.ADDTIMER(sendTime, SimulatorUpdate, pPS, "CInternetSimulatorSocket::Send() timer");
		return eSE_Ok;
	}
}

ESocketError CInternetSimulatorSocket::SendVoice(const uint8* pBuffer, size_t nLength, const TNetAddress& to)
{
	if (stl::get_if<TLocalNetAddress>(&to))
	{
		return m_pChild->SendVoice(pBuffer, nLength, to);
	}

	g_socketBandwidth.simPacketSends++;

	float randomLoss = 100.0f * s_InternetSimRandomGen.GenerateFloat();
	if (randomLoss < CVARS.net_PacketLossRate)
	{
		g_socketBandwidth.simPacketDrops++;
		return eSE_Ok;
	}

	float fLagRange = CVARS.net_PacketLagMax - CVARS.net_PacketLagMin;
	float fExtraTime = CVARS.net_PacketLagMin;

	if (fLagRange > 0.0f)
	{
		fExtraTime += (fLagRange * s_InternetSimRandomGen.GenerateFloat());
	}

	g_socketBandwidth.simLastPacketLag = (uint32)(fExtraTime * 1000.0f);

	if (fExtraTime <= 0.0f)
	{
		return m_pChild->SendVoice(pBuffer, nLength, to);
	}
	else
	{
		SPendingSend* pPS = new SPendingSend;
		CTimeValue sendTime = g_time + CTimeValue(fExtraTime);
		pPS->eType = ePT_Voice;
		pPS->pBuffer = new uint8[nLength];
		memcpy(pPS->pBuffer, pBuffer, nLength);
		pPS->nLength = nLength;
		pPS->to = to;
		pPS->pThis = this;
		NetTimerId timer = ACCURATE_NET_TIMER.ADDTIMER(sendTime, SimulatorUpdate, pPS, "CInternetSimulatorSocket::SendVoice() timer");
		return eSE_Ok;
	}
}

void CInternetSimulatorSocket::RegisterListener(IDatagramListener* pListener)
{
	m_pChild->RegisterListener(pListener);
}

void CInternetSimulatorSocket::UnregisterListener(IDatagramListener* pListener)
{
	m_pChild->UnregisterListener(pListener);
}

CRYSOCKET CInternetSimulatorSocket::GetSysSocket()
{
	return m_pChild->GetSysSocket();
}

void CInternetSimulatorSocket::SetProfile(EProfile profile)
{
	CryLog("[LagProfiles] %u lag profiles available.", sm_nProfiles);

	switch (profile)
	{
	case PROFILE_PERFECT:
		{
			CNetCVars::Get().net_PacketLossRate = 0.0f;
			CNetCVars::Get().net_PacketLagMin = 0.0f;
			CNetCVars::Get().net_PacketLagMax = 0.0f;
			CryLog("[LagProfiles] Using Perfect profile settings.");
		}
		break;
	case PROFILE_FATAL:
		{
			CNetCVars::Get().net_PacketLossRate = 100.0f;
			CNetCVars::Get().net_PacketLagMin = 60.0f;
			CNetCVars::Get().net_PacketLagMax = 60.0f;
			CryLog("[LagProfiles] Using Fatal profile settings.");
		}
		break;
	case PROFILE_RANDOM:
		{
			profile = (EProfile)(s_InternetSimRandomGen.GenerateUint32() % sm_nProfiles);
			CryLog("[LagProfiles] Picking Random profile %d of %u.", profile, sm_nProfiles);
			// deliberate fall through to default case
		}
	default:
		{
			if ((profile > PROFILE_PERFECT) && (profile < (int)sm_nProfiles))
			{
				SProfileEntry* pEntry = &sm_pProfiles[profile];

				float fLossRange = pEntry->fLossMax - pEntry->fLossMin;
				float fLoss = pEntry->fLossMin + (fLossRange * s_InternetSimRandomGen.GenerateFloat());

				CNetCVars::Get().net_PacketLossRate = fLoss;
				CNetCVars::Get().net_PacketLagMin = pEntry->fLagMin;
				CNetCVars::Get().net_PacketLagMax = pEntry->fLagMax;
				CryLog("[LagProfiles] Using Profile %d settings. (%.2f%% chance of packet loss, %.2f-%.2f sec variable lag)", profile, fLoss, pEntry->fLagMin, pEntry->fLagMax);
			}
			else
			{
				CryLogAlways("[LagProfiles] Invalid profile index.");
			}
		}
		break;
	}
}

void CInternetSimulatorSocket::LoadXMLProfiles(const char* pFileName)
{
	if (sm_pProfiles && (sm_pProfiles != sm_DefaultProfiles))
	{
		SAFE_DELETE_ARRAY(sm_pProfiles);
	}

	sm_pProfiles = &sm_DefaultProfiles[0];
	sm_nProfiles = DEFAULT_PROFILE_MAX;

	if (pFileName[0] != 0)
	{
		XmlNodeRef rootNode = gEnv->pSystem->LoadXmlFromFile(PathUtil::GetGameFolder() + "/Scripts/Network/" + pFileName);
		if (rootNode)
		{
			uint32 nProfileCount = 0;

			for (int i = 0; i < rootNode->getChildCount(); i++)
			{
				XmlNodeRef profileNode = rootNode->getChild(i);
				if (profileNode->isTag("Profile"))
				{
					nProfileCount++;
				}
			}

			if (nProfileCount > 0)
			{
				sm_pProfiles = new SProfileEntry[nProfileCount];
				sm_nProfiles = nProfileCount;
				SProfileEntry* pProfile = sm_pProfiles;

				for (int i = 0; i < rootNode->getChildCount(); i++)
				{
					XmlNodeRef profileNode = rootNode->getChild(i);
					if (profileNode->isTag("Profile"))
					{
						memcpy(pProfile, &sm_DefaultProfiles[DEFAULT_PROFILE_TYPICAL], sizeof(SProfileEntry));

						profileNode->getAttr("minPacketLoss", pProfile->fLossMin);
						profileNode->getAttr("maxPacketLoss", pProfile->fLossMax);
						profileNode->getAttr("minPacketLag", pProfile->fLagMin);
						profileNode->getAttr("maxPacketLag", pProfile->fLagMax);

						pProfile++;
					}
				}

				CryLog("[LagProfiles] \"%s\" file parsed. %u lag profiles loaded.", pFileName, sm_nProfiles);
				return;
			}
		}
	}

	CryLog("[LagProfiles] Could not load \"%s\". %u default profiles activated.", pFileName, sm_nProfiles);
}

#endif
