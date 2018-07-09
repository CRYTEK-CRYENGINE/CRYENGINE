// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NetProfile.h"

#if NET_PROFILE_ENABLE || NET_MINI_PROFILE || INTERNET_SIMULATOR
SSocketBandwidth g_socketBandwidth;
#endif

#if NET_PROFILE_ENABLE

	#include <CrySystem/IConsole.h>
	#include <CryGame/IGameFramework.h>
	#include <CrySystem/ITimer.h>
	#include <CrySystem/ISystem.h>
	#include <CrySystem/TimeValue.h>
	#include <CryString/CryFixedString.h>
	#include "Protocol/PacketRateCalculator.h"
	#include <CrySystem/Profilers/IPerfHud.h>
	#include <CryExtension/CryCreateClassInstance.h>

	#include "NetCVars.h"

	#define NET_PROFILE_MAX_ENTRIES 1024

	#define ONE_K_BITS              1000

	#define NUM_TABS                11
	#define TAB_SIZE                8
	#define ROOT_INDEX              0

SNetProfileStackEntry g_netProfileNull;
SNetProfileStackEntry* g_netProfileCurrent = &g_netProfileNull;

CNetProfileStackEntryList g_UnusedProfileList(NET_PROFILE_MAX_ENTRIES);
static SNetProfileStackEntry* s_netProfileRoot = NULL;

static bool s_initialised = false;
static int s_netProfileLogging = 0;
static int s_netProfileBudgetLogging = 0;
static bool s_budgetExceeded = false;
static int s_netProfileWorstNumChannels = 0;
static int s_netProfileShowSocketMeasurements = 0;
static int s_netProfileEnable = 0;
static int s_netProfileUntouchedDelay = 1;
static int s_netProfileNumRemoteClients = 0;
static bool s_netProfileCountReadBits = false;

CNetProfileStackEntryList::CNetProfileStackEntryList(uint32 numItems)
{
	m_pArray = new SNetProfileStackEntry[numItems];
	if (m_pArray)
	{
		for (uint32 i = 0; i < numItems - 1; i++)
		{
			m_pArray[i].ClearAll();
			m_pArray[i].m_next = &m_pArray[i + 1];
		}
		m_pArray[numItems - 1].m_next = NULL;
		m_pFirst = m_pArray;
	}
}

CNetProfileStackEntryList::~CNetProfileStackEntryList()
{
	SAFE_DELETE_ARRAY(m_pArray);
}

SNetProfileStackEntry* CNetProfileStackEntryList::Pop()
{
	SNetProfileStackEntry* pRet = m_pFirst;

	if (m_pFirst)
	{
		m_pFirst = m_pFirst->m_next;

		//-- Sanity tests to make sure the item is blank
		assert(pRet->m_parent == NULL);
		assert(pRet->m_child == NULL);
		pRet->m_next = NULL;
		pRet->ClearAll();
	}

	return pRet;
}

void CNetProfileStackEntryList::Push(SNetProfileStackEntry* pItem)
{
	//-- Sanity tests to make sure we're returning an unused item to the free list
	assert(pItem->m_parent == NULL);
	assert(pItem->m_next == NULL);

	//-- pushing an item will push all its children.
	while (pItem->m_child)
	{
		SNetProfileStackEntry* pChild = pItem->m_child;
		pItem->m_child = pChild->m_next;
		pChild->m_parent = NULL;
		pChild->m_next = NULL;
		Push(pChild);
	}

	pItem->m_next = m_pFirst;
	m_pFirst = pItem;
}

void netProfileInitEntry(SNetProfileStackEntry* parent, SNetProfileStackEntry* child, const char* name, float budget, bool rmi)
{
	assert(parent && child && name);

	child->ClearAll();
	child->m_name = name;
	child->m_budget = budget;
	child->m_rmi = rmi;

	child->m_next = parent->m_child; // link to our sibling
	parent->m_child = child;         // make new child the first link
	child->m_parent = parent;        // set parent
}

void netProfileRemoveUntouchedNodes(SNetProfileStackEntry* parent)
{
	parent->ClearStatistics();

	//-- Any child which has untouched counter > s_netProfileUntouchedDelay will be removed
	SNetProfileStackEntry* previous = NULL;
	SNetProfileStackEntry* current = parent->m_child;
	SNetProfileStackEntry* next = NULL;
	while (current)
	{
		next = current->m_next;

		if (current->m_untouchedCounter > (uint32)s_netProfileUntouchedDelay)
		{
			if (previous == NULL)
			{
				//-- start of child list.
				parent->m_child = next;
			}
			else
			{
				previous->m_next = next;
			}

			current->m_parent = NULL;
			current->m_next = NULL;
			g_UnusedProfileList.Push(current);
		}
		else
		{
			netProfileRemoveUntouchedNodes(current);
			previous = current;
		}

		current = next;
	}
}

void netProfileStartProfile()
{
	//-- any children with a untouched time > s_netProfileUntouchedDelay should be removed
	netProfileRemoveUntouchedNodes(s_netProfileRoot);

	s_netProfileRoot->ClearStatistics();
	s_netProfileRoot->m_name = "root";
	s_netProfileRoot->m_budget = NO_BUDGET; // we now check the socket budget as a more reliable measurement

	g_netProfileCurrent = s_netProfileRoot;

	s_budgetExceeded = false;
}

void netProfileInitialise(bool isMultiplayer)
{
	if (!isMultiplayer)
		return; // don't do this for single player

	if (!s_initialised)
	{
		s_netProfileRoot = g_UnusedProfileList.Pop();
		assert(s_netProfileRoot);
		s_netProfileRoot->ClearAll();
		s_netProfileRoot->m_name = "root";
		s_netProfileRoot->m_budget = NO_BUDGET; // we now check the socket budget as a more reliable measurement

		g_netProfileNull.ClearAll();
		g_netProfileNull.m_parent = &g_netProfileNull;

		netProfileStartProfile();
		netProfileRegisterWithPerfHUD();

		s_initialised = true;

		memset(&g_socketBandwidth.bandwidthStats, 0, sizeof(SBandwidthStats));
	}
}

void netProfileRegisterWithPerfHUD()
{
	ICryPerfHUDPtr pPerfHUD;
	CryCreateClassInstanceForInterface(cryiidof<ICryPerfHUD>(), pPerfHUD);
	minigui::IMiniGUIPtr pGUI;
	CryCreateClassInstanceForInterface(cryiidof<minigui::IMiniGUI>(), pGUI);

	if (pPerfHUD && pGUI)
	{
		minigui::IMiniCtrl* pNetworkMenu = pPerfHUD->CreateMenu("Networking");

		if (pNetworkMenu)
		{
			pPerfHUD->CreateInfoMenuItem(pNetworkMenu, "Network Profile", netProfileRenderStats, minigui::Rect(200, 300, 675, 375));
		}
	}
}

void netProfileShutDown()
{
	if (s_initialised)
	{
		g_UnusedProfileList.Push(s_netProfileRoot);
		s_netProfileRoot = NULL;
		s_initialised = false;
	}
}

void netProfileCountReadBits(bool count)
{
	s_netProfileCountReadBits = count;
}

bool netProfileGetChildFromCurrent(const char* name, SNetProfileStackEntry** ioEntry, bool rmi)
{
	SNetProfileStackEntry* child = g_netProfileCurrent->m_child;  // get the first child of the current profile
	bool result = false;

	while (child)
	{
		if (child->m_name == name)
		{
			assert(child->m_rmi == rmi);
			*ioEntry = child;
			result = true;
			break;
		}
		child = child->m_next;
	}

	return result;
}

void netProfileRegisterBeginCall(const char* name, SNetProfileStackEntry** ioEntry, float budget, bool rmi)
{
	ASSERT_PRIMARY_THREAD;

	SNetProfileStackEntry* entry = g_UnusedProfileList.Pop();
	if (!entry)
	{
		return;  // sets as g_netProfileNULL
	}
	netProfileInitEntry(g_netProfileCurrent, entry, name, budget, rmi);

	*ioEntry = entry;

	assert(entry);
	assert(entry != g_netProfileCurrent);
}

void netProfileBeginFunction(SNetProfileStackEntry* entry, bool read)
{
	assert(entry != &g_netProfileNull);

	if (!read)
	{
		entry->m_calls++;
		entry->m_tickCalls += entry->m_rmi ? 1 : s_netProfileNumRemoteClients;
	}
	else if (s_netProfileCountReadBits)
	{
		entry->m_tickCalls += entry->m_rmi ? 1 : (s_netProfileNumRemoteClients - 1);
	}

	entry->m_parent = g_netProfileCurrent;
	g_netProfileCurrent = entry;
}

void netProfileEndFunction()
{
	assert(g_netProfileCurrent != &g_netProfileNull);
	assert(g_netProfileCurrent != s_netProfileRoot);

	g_netProfileCurrent = g_netProfileCurrent->m_parent;
}

void netProfileSumBits(SNetProfileStackEntry* entry)
{
	SNetProfileStackEntry* child = entry->m_child;
	entry->m_rmibits = entry->m_rmi ? entry->m_bits : 0;

	while (child)
	{
		netProfileSumBits(child);

		entry->m_rmibits += child->m_rmi ? child->m_bits : 0;
		entry->m_bits += child->m_bits;
		entry->m_untouchedCounter = min(entry->m_untouchedCounter, child->m_untouchedCounter);
		child = child->m_next;
	}
}

void calculateEntryStats(SNetProfileStackEntry* entry, uint32 parentWriteBits, int numChannels)
{
	SNetProfileStackEntry* child = NULL;

	for (child = entry->m_child; child; child = child->m_next)
	{
		calculateEntryStats(child, entry->m_bits, numChannels);
	}

	float one1024th = 1.0f / float(ONE_K_BITS);

	SNetProfileCount* counts = &entry->counts;

	float floatTotalBits = float(entry->m_bits);
	float floatRMIBits = float(entry->m_rmibits);
	float bitsTokbits = floatTotalBits * one1024th;
	uint32 serialisedBits = entry->m_bits - entry->m_rmibits;
	float floatSerialisedBits = float(serialisedBits);

	counts->m_callsAv.AddSample(entry->m_calls);
	counts->m_seqbitsAv.AddSample(serialisedBits);
	counts->m_rmibitsAv.AddSample(entry->m_rmibits);
	counts->m_payloadAv.AddSample(bitsTokbits);

	float onWireOneSec = entry->m_rmi ? bitsTokbits : ((floatSerialisedBits * float(numChannels)) + floatRMIBits) * one1024th;
	counts->m_onWireAv.AddSample(onWireOneSec);

	counts->m_worst = entry->m_rmi ? bitsTokbits : ((floatSerialisedBits * float(s_netProfileWorstNumChannels)) + floatRMIBits) * one1024th;
	counts->m_worstAv = entry->m_rmi ? bitsTokbits : ((counts->m_seqbitsAv.GetAverage() * float(s_netProfileWorstNumChannels)) + counts->m_rmibitsAv.GetAverage()) * one1024th;
	counts->m_heir = parentWriteBits > 0 ? (floatTotalBits / float(parentWriteBits)) * 100.f : 0.f;
	counts->m_self = s_netProfileRoot->m_bits > 0 ? (floatTotalBits / float(s_netProfileRoot->m_bits)) * 100.f : 0.f;

	entry->m_untouchedCounter++;
}

void netProfileEndProfile()
{
	netProfileSumBits(s_netProfileRoot);

	IGameFramework* pFramework = gEnv->pGameFramework;
	INetNub* pNub = NULL;
	int numChannels = 0;

	if (pFramework)
	{
		if (gEnv->bServer)
		{
			pNub = pFramework->GetServerNetNub();
		}
		else
		{
			pNub = pFramework->GetClientNetNub();
		}

		if (pNub)
		{
			numChannels = pNub->GetNumChannels();
		}
		if (gEnv->bServer && !gEnv->IsDedicated())
		{
			// Listen servers have a channel to themselves, ignore it for onwire bandwidth calculations
			numChannels--;
		}
	}

	for (SNetProfileStackEntry* entry = s_netProfileRoot->m_child; entry; entry = entry->m_next)
	{
		calculateEntryStats(entry, s_netProfileRoot->m_bits, numChannels);
	}

	// fix up root stats
	SNetProfileStackEntry* child = s_netProfileRoot->m_child;
	SNetProfileCount* counts = &s_netProfileRoot->counts;

	float one1024th = 1.0f / float(ONE_K_BITS);
	float floatTotalBits = float(s_netProfileRoot->m_bits);
	float floatRMIBits = float(s_netProfileRoot->m_rmibits);
	float bitsTokbits = floatTotalBits * one1024th;
	uint32 serialisedBits = s_netProfileRoot->m_bits - s_netProfileRoot->m_rmibits;
	float floatSerialisedBits = float(serialisedBits);

	counts->m_callsAv.AddSample(1);
	counts->m_seqbitsAv.AddSample(serialisedBits);
	counts->m_rmibitsAv.AddSample(s_netProfileRoot->m_rmibits);
	counts->m_payloadAv.AddSample(bitsTokbits);
	counts->m_self = 100.f;
	counts->m_heir = 100.f;

	float onWireOneSec = s_netProfileRoot->m_rmi ? bitsTokbits : ((floatSerialisedBits * float(numChannels)) + floatRMIBits) * one1024th;
	counts->m_onWireAv.AddSample(onWireOneSec);

	counts->m_worst = s_netProfileRoot->m_rmi ? bitsTokbits : ((floatSerialisedBits * float(s_netProfileWorstNumChannels)) + floatRMIBits) * one1024th;
	counts->m_worstAv = s_netProfileRoot->m_rmi ? bitsTokbits : ((counts->m_seqbitsAv.GetAverage() * float(s_netProfileWorstNumChannels)) + counts->m_rmibitsAv.GetAverage()) * one1024th;
}

void netProfileCheckBudgets(SNetProfileStackEntry* entry)
{
	SNetProfileStackEntry* child = entry->m_child;

	// don't check root, we cover this with the socket measurements
	if ((entry != s_netProfileRoot) && (entry->m_budget > NO_BUDGET) && (entry->counts.m_worst > entry->m_budget))
	{
		NetQuickLog(true, 10.f, "%s has exceeded its budget of %.2f k/bits by %.2f k/bits", entry->m_name.c_str(), entry->m_budget, (entry->counts.m_worst - entry->m_budget));
		s_budgetExceeded = true;
	}

	while (child)
	{
		netProfileCheckBudgets(child);
		child = child->m_next;
	}
}

FILE* netProfileOpenLogFile(const char* name, int* firstTime)
{
	FILE* fout = NULL;

	if (*firstTime)
	{
		fout = gEnv->pCryPak->FOpen(name, "w+");
		*firstTime=0;
	}
	else
	{
		fout = gEnv->pCryPak->FOpen(name, "a+");
//		fout = fopen(name, "a+");
	}

	return fout;
}

static void netProfilePrintEntry(FILE* file, const SNetProfileStackEntry* entry, int depth)
{
	CryFixedStringT<1024> stringBufferTemp;
	CryFixedStringT<1024> stringBuffer;
	const SNetProfileCount* counts = &entry->counts;

	if (depth >= 0)
	{
		for (int i = 0; i < depth; i++)
		{
			stringBuffer.append(".");
		}
	}

	stringBufferTemp.Format("%s", entry->m_name.c_str());
	stringBuffer.append(stringBufferTemp);

	int doTabs = NUM_TABS - (stringBuffer.length() / TAB_SIZE);
	for (int j = 0; j < doTabs; j++)
	{
		stringBuffer.append("\t");
	}

	stringBufferTemp.Format("%7u\t%7u/%7u\t%15.2f\t%15.2f\t%15.2f\t| %5.0f\t%7.0f/%7.0f\t%15.2f\t%15.2f\t%15.2f\t| %7.2f\t%7.2f",
	                        counts->m_callsAv.GetLast(), counts->m_seqbitsAv.GetLast(), counts->m_rmibitsAv.GetLast(), counts->m_payloadAv.GetLast(), counts->m_onWireAv.GetLast(), counts->m_worst,
	                        counts->m_callsAv.GetAverage(), counts->m_seqbitsAv.GetAverage(), counts->m_rmibitsAv.GetAverage(), counts->m_payloadAv.GetAverage(), counts->m_onWireAv.GetAverage(), counts->m_worstAv,
	                        counts->m_heir, counts->m_self);
	stringBuffer.append(stringBufferTemp);

	if (depth >= 0)
	{
		stringBufferTemp.Format("\t\t}}}%d\n", depth + 1);
		stringBuffer.append(stringBufferTemp);
	}
	else
	{
		stringBuffer.append("\n");
	}

	fprintf(file, "%s", stringBuffer.c_str());
}

void netProfileFormatSocketView(FILE* file)
{
	CryFixedStringT<1024> stringBuffer;
	char tabs[NUM_TABS + 1] = { '\0' };

	int doTabs = NUM_TABS - (strlen("socket") / TAB_SIZE);
	for (int i = 0; i < doTabs; i++)
	{
		tabs[i] = '\t';
	}

	stringBuffer.Format("socket%s%.0f\t%.2f\t%.0f\t%.2f\t%d\t%d\t%.2f\n\n", tabs,
	                    g_socketBandwidth.bandwidthStats.m_1secAvg.m_totalBandwidthSent, g_socketBandwidth.bandwidthStats.m_1secAvg.m_totalBandwidthSent / float(ONE_K_BITS),
	                    g_socketBandwidth.sizeDisplayRx, g_socketBandwidth.sizeDisplayRx / float(ONE_K_BITS),
	                    g_socketBandwidth.bandwidthStats.m_total.m_totalPacketsSent,
	                    g_socketBandwidth.numDisplayNetTicks, gEnv->pTimer->GetFrameRate());

	fprintf(file, "%s", stringBuffer.c_str());
}

void netProfileFormatFlatView(FILE* file)
{
	SNetProfileStackEntry* entry = NULL;

	netProfilePrintEntry(file, s_netProfileRoot, -1);

	for (entry = s_netProfileRoot->m_child; entry; entry = entry->m_next)
	{
		netProfilePrintEntry(file, entry, -1);
	}
}

void netProfileFormatHierarchyView(FILE* file, SNetProfileStackEntry* root, int depth)
{
	SNetProfileStackEntry* entry = NULL;

	netProfilePrintEntry(file, root, depth);

	for (entry = root->m_child; entry; entry = entry->m_next)
	{
		netProfileFormatHierarchyView(file, entry, depth + 1);
	}
}

void netProfileWriteLogFiles()
{
	static int firstTimeLogging = 1;
	static int firstTimeBudgetLogging = 1;
	static ICVar* pLogging = gEnv->pConsole->GetCVar("net_profile_logging");
	static ICVar* pBudgetLogging = gEnv->pConsole->GetCVar("net_profile_budget_logging");
	const char* header[9] = { "LAST", "AVERAGE", "Calls", "Seqbits/RMIbits", "Payload(Kbits)", "OnWire(Kbits)", "Worst(Kbits)", "Heir%", "Self%" };
	char tabs[NUM_TABS + 1] = { 0 };
	FILE* fout = NULL;

	for (int i = 0; i < NUM_TABS; i++)
	{
		tabs[i] = '\t';
	}

	if (pLogging->GetIVal() != 0)
	{
		static ICVar* pName = gEnv->pConsole->GetCVar("net_profile_logname");
		fout = netProfileOpenLogFile((string("%USER%/") + pName->GetString()).c_str(), &firstTimeLogging);

		if (fout)
		{
			fprintf(fout, "%sbitsTx\tsent\tbitsRx\trecv\tnumPackets\n", tabs);
			netProfileFormatSocketView(fout);

			fprintf(fout, "%s%s\t\t\t\t\t\t\t\t\t| %s\t\t\t\t\t\t\t\t\t|\n", tabs, header[0], header[1]);
			fprintf(fout, "%s%s\t%s\t%s\t%s\t%s\t| %s\t%s\t%s\t%s\t%s\t| %s\t\t%s\n", tabs, header[2], header[3], header[4], header[5], header[6],
			        header[2], header[3], header[4], header[5], header[6], header[7], header[8]);
			netProfileFormatFlatView(fout);

			fprintf(fout, "\n\n");

			fprintf(fout, "%s%s\t\t\t\t\t\t\t\t\t| %s\t\t\t\t\t\t\t\t\t|\n", tabs, header[0], header[1]);
			fprintf(fout, "%s%s\t%s\t%s\t%s\t%s\t| %s\t%s\t%s\t%s\t%s\t| %s\t\t%s\n", tabs, header[2], header[3], header[4], header[5], header[6],
			        header[2], header[3], header[4], header[5], header[6], header[7], header[8]);
			netProfileFormatHierarchyView(fout, s_netProfileRoot, 0);
			fprintf(fout, "\n");

			fclose(fout);
			fout = NULL;
		}
	}

	if (s_budgetExceeded && pBudgetLogging->GetIVal() != 0)
	{
		static ICVar* pName = gEnv->pConsole->GetCVar("net_profile_budget_logname");
		fout = netProfileOpenLogFile(pName->GetString(), &firstTimeBudgetLogging);

		if (fout)
		{
			fprintf(fout, "%sbitsTx\tsent\tbitsRx\trecv\n", tabs);
			netProfileFormatSocketView(fout);

			fprintf(fout, "%s%s%s%s%s%s%s%s%s%s\n", tabs, header[0], header[1], header[2], header[3], header[4], header[5], header[6], header[7], header[8]);
			netProfileFormatHierarchyView(fout, s_netProfileRoot, 0);
			fprintf(fout, "\n");

			fclose(fout);
			fout = NULL;
		}
	}
}

void netProfileAddBits(uint32 bits, bool read)
{
	ASSERT_PRIMARY_THREAD;
	assert(g_netProfileCurrent != s_netProfileRoot);
	SNetProfileStackEntry* entry = g_netProfileCurrent;

	if (!read)
	{
		entry->m_tickBits += entry->m_rmi ? bits : (bits * s_netProfileNumRemoteClients);
		entry->m_bits += bits;
		entry->m_untouchedCounter = 0;
	}
	else if (s_netProfileCountReadBits)
	{
		entry->m_tickBits += entry->m_rmi ? bits : (bits * (s_netProfileNumRemoteClients - 1));
	}
}

void netProfileClearTickBits(SNetProfileStackEntry* pNode)
{
	if (pNode)
	{
		SNetProfileStackEntry* child = pNode->m_child;
		while (child)
		{
			netProfileClearTickBits(child);

			child->ClearTickBits();
			child = child->m_next;
		}
	}
}

bool netProfileSocketMeasurementTick()
{
	bool shouldDumpLogs = false;
	CTimeValue when = gEnv->pTimer->GetAsyncTime();

	#define   AVERAGE_PERIOD (1.f)  // 1 means don't average, and we probably want that when dumping to file
	#define   UPDATE_PERIOD  (1.f / AVERAGE_PERIOD)

	g_socketBandwidth.numNetTicks++;

	netProfileClearTickBits(s_netProfileRoot);

	if ((when - g_socketBandwidth.last) > UPDATE_PERIOD)
	{
		g_socketBandwidth.bandwidthUsedAmountTx.AddSample(g_socketBandwidth.periodStats.m_totalBandwidthSent);
		g_socketBandwidth.bandwidthStats.m_1secAvg.m_totalBandwidthSent = (uint64)(g_socketBandwidth.periodStats.m_totalBandwidthSent * AVERAGE_PERIOD);
		g_socketBandwidth.bandwidthStats.m_10secAvg.m_totalBandwidthSent = (uint64)(g_socketBandwidth.bandwidthUsedAmountTx.GetAverage() * AVERAGE_PERIOD);
		g_socketBandwidth.numPacketsSent.AddSample(g_socketBandwidth.periodStats.m_totalPacketsSent);
		g_socketBandwidth.bandwidthStats.m_1secAvg.m_totalPacketsSent = g_socketBandwidth.periodStats.m_totalPacketsSent;
		g_socketBandwidth.bandwidthStats.m_10secAvg.m_totalPacketsSent = (int)g_socketBandwidth.numPacketsSent.GetAverage();

		g_socketBandwidth.lobbyBandwidthUsedAmountTx.AddSample(g_socketBandwidth.periodStats.m_lobbyBandwidthSent);
		g_socketBandwidth.bandwidthStats.m_1secAvg.m_lobbyBandwidthSent = (uint64)(g_socketBandwidth.periodStats.m_lobbyBandwidthSent * AVERAGE_PERIOD);
		g_socketBandwidth.bandwidthStats.m_10secAvg.m_lobbyBandwidthSent = (uint64)(g_socketBandwidth.lobbyBandwidthUsedAmountTx.GetAverage() * AVERAGE_PERIOD);
		g_socketBandwidth.numLobbyPacketsSent.AddSample(g_socketBandwidth.periodStats.m_lobbyPacketsSent);
		g_socketBandwidth.bandwidthStats.m_1secAvg.m_lobbyPacketsSent = g_socketBandwidth.periodStats.m_lobbyPacketsSent;
		g_socketBandwidth.bandwidthStats.m_10secAvg.m_lobbyPacketsSent = (int)g_socketBandwidth.numLobbyPacketsSent.GetAverage();

		g_socketBandwidth.seqBandwidthUsedAmountTx.AddSample(g_socketBandwidth.periodStats.m_seqBandwidthSent);
		g_socketBandwidth.bandwidthStats.m_1secAvg.m_seqBandwidthSent = (uint64)(g_socketBandwidth.periodStats.m_seqBandwidthSent * AVERAGE_PERIOD);
		g_socketBandwidth.bandwidthStats.m_10secAvg.m_seqBandwidthSent = (uint64)(g_socketBandwidth.seqBandwidthUsedAmountTx.GetAverage() * AVERAGE_PERIOD);
		g_socketBandwidth.numSeqPacketsSent.AddSample(g_socketBandwidth.periodStats.m_seqPacketsSent);
		g_socketBandwidth.bandwidthStats.m_1secAvg.m_seqPacketsSent = g_socketBandwidth.periodStats.m_seqPacketsSent;
		g_socketBandwidth.bandwidthStats.m_10secAvg.m_seqPacketsSent = (int)g_socketBandwidth.numSeqPacketsSent.GetAverage();

		g_socketBandwidth.bandwidthStats.m_1secAvg.m_fragmentBandwidthSent = (uint64)(g_socketBandwidth.periodStats.m_fragmentBandwidthSent * AVERAGE_PERIOD);
		g_socketBandwidth.bandwidthStats.m_1secAvg.m_fragmentPacketsSent = g_socketBandwidth.periodStats.m_fragmentPacketsSent;

		g_socketBandwidth.bandwidthUsedAmountRx.AddSample(g_socketBandwidth.periodStats.m_totalBandwidthRecvd);
		g_socketBandwidth.bandwidthStats.m_1secAvg.m_totalBandwidthRecvd = (uint64)(g_socketBandwidth.periodStats.m_totalBandwidthRecvd * AVERAGE_PERIOD);
		g_socketBandwidth.bandwidthStats.m_10secAvg.m_totalBandwidthRecvd = (uint64)(g_socketBandwidth.bandwidthUsedAmountRx.GetAverage() * AVERAGE_PERIOD);

		g_socketBandwidth.numPacketsRecv.AddSample(g_socketBandwidth.periodStats.m_totalPacketsRecvd);
		g_socketBandwidth.bandwidthStats.m_1secAvg.m_totalPacketsRecvd = g_socketBandwidth.periodStats.m_totalPacketsRecvd;
		g_socketBandwidth.bandwidthStats.m_10secAvg.m_totalPacketsRecvd = (uint64)(g_socketBandwidth.numPacketsRecv.GetAverage() * AVERAGE_PERIOD);

		g_socketBandwidth.avgValueRx = g_socketBandwidth.bandwidthUsedAmountRx.GetAverage() * AVERAGE_PERIOD;
		g_socketBandwidth.sizeDisplayRx = g_socketBandwidth.periodStats.m_totalBandwidthRecvd * AVERAGE_PERIOD;

		g_socketBandwidth.last = when;
		g_socketBandwidth.numDisplayNetTicks = g_socketBandwidth.numNetTicks;
		g_socketBandwidth.numNetTicks = 0;

		memset(&g_socketBandwidth.periodStats, 0, sizeof(SBandwidthStatsSubset));

		float kbits = g_socketBandwidth.bandwidthStats.m_1secAvg.m_totalBandwidthSent / float(ONE_K_BITS);
		float socketBudget = (gEnv->bServer) ? CNetCVars::Get().net_availableBandwidthServer : CNetCVars::Get().net_availableBandwidthClient;

		if (kbits > socketBudget)
		{
			NetQuickLog(true, 10.f, "root has exceeded its budget of %.2f k/bits by %.2f k/bits", socketBudget, (kbits - socketBudget));
			s_budgetExceeded = true;
		}

		shouldDumpLogs = TRUE;
	}

	static ICVar* pSocketMeasurements = gEnv->pConsole->GetCVar("net_profile_show_socket_measurements");
	if (pSocketMeasurements->GetIVal() != 0)
	{
	#define BANDWIDTH_DEBUG_RENDER_START_X (70.f)
	#define BANDWIDTH_DEBUG_RENDER_START_Y (500.f)

		netProfileRenderStats(BANDWIDTH_DEBUG_RENDER_START_X, BANDWIDTH_DEBUG_RENDER_START_Y);
	}

	return shouldDumpLogs;
}

#include <CryRenderer/IRenderAuxGeom.h>

void netProfileRenderStats(float x, float y)
{
	const float lineHeight = 22.f;

	float col2[] = { 1.f, 1.f, 1.f, 1.f };

	x += 10.f;
	y += 15.f;

	IRenderAuxText::Draw2dLabel(x, y, 2, col2, false, "Num Packets Sent %d  %.2fk  Avg(10s) %.2fk",
	                       g_socketBandwidth.bandwidthStats.m_1secAvg.m_totalPacketsSent,
	                       g_socketBandwidth.bandwidthStats.m_1secAvg.m_totalBandwidthSent / float(ONE_K_BITS),
	                       g_socketBandwidth.bandwidthStats.m_10secAvg.m_totalBandwidthSent / float(ONE_K_BITS));
	y += lineHeight;
	IRenderAuxText::Draw2dLabel(x, y, 2, col2, false, "Received %.2fk  Avg(10s) %.2fk", g_socketBandwidth.sizeDisplayRx / float(ONE_K_BITS), g_socketBandwidth.avgValueRx / float(ONE_K_BITS));
}

void netProfileTick()
{
	ASSERT_PRIMARY_THREAD;

	bool dumpLogs = netProfileSocketMeasurementTick();

	if (netProfileIsInitialised())
	{
		assert(g_netProfileCurrent == s_netProfileRoot);
		netProfileClearTickBits(s_netProfileRoot);

		if (dumpLogs)
		{
			netProfileEndProfile();
			netProfileCheckBudgets(s_netProfileRoot);
			netProfileWriteLogFiles();
			netProfileStartProfile();
		}
	}

	s_netProfileNumRemoteClients = 0;
	if (gEnv->pGameFramework)
	{
		{
			INetNub* pNub = NULL;
			if (gEnv->bServer)
			{
				pNub = gEnv->pGameFramework->GetServerNetNub();
			}
			else
			{
				pNub = gEnv->pGameFramework->GetClientNetNub();
			}

			if (pNub)
			{
				if (gEnv->bServer && !gEnv->IsDedicated())
				{
					// Listen servers have a channel to themselves, don't count it as a remote client
					s_netProfileNumRemoteClients = pNub->GetNumChannels() - 1;
				}
				else
				{
					s_netProfileNumRemoteClients = pNub->GetNumChannels();
				}
			}
		}
	}
}

bool netProfileIsInitialised()
{
	return (s_netProfileEnable) ? s_initialised : false;
}

SNetProfileStackEntry* netProfileGetNullProfile()
{
	return &g_netProfileNull;
}

const SNetProfileStackEntry* netProfileGetProfileTreeRoot()
{
	return s_netProfileRoot;
}

void netProfileFlattenTreeRecursive(ProfileLeafList& list, SNetProfileStackEntry* pNode, string concatName)
{
	if (pNode)
	{
		concatName += pNode->m_name.c_str();
		concatName += "/";

		if (pNode->m_child)
		{
			SNetProfileStackEntry* pEntry = NULL;
			for (pEntry = pNode->m_child; pEntry; pEntry = pEntry->m_next)
			{
				netProfileFlattenTreeRecursive(list, pEntry, concatName);
			}
		}
		else
		{
			//-- no child, must be a leaf
			SProfileInfoStat leaf;
			leaf.m_name = concatName;
			leaf.m_totalBits = pNode->m_tickBits;
			leaf.m_calls = pNode->m_tickCalls;
			leaf.m_rmi = pNode->m_rmi;

			list.push_back(leaf);
		}
	}
}

void netProfileFlattenTreeToLeafList(ProfileLeafList& list)
{
	if (s_netProfileRoot)
	{
		string concatName = "";
		netProfileFlattenTreeRecursive(list, s_netProfileRoot, concatName);
	}
}

void netProfileRegisterCVars()
{
	REGISTER_STRING_DEV_ONLY("net_profile_logname", "profile_net.log", VF_NULL, "log name for net profiler");
	REGISTER_CVAR2_DEV_ONLY("net_profile_logging", &s_netProfileLogging, 0, VF_NULL, "enable/disable logging");
	REGISTER_STRING_DEV_ONLY("net_profile_budget_logname", "profile_net_budget.log", VF_NULL, "budget log name for net profiler");
	REGISTER_CVAR2_DEV_ONLY("net_profile_budget_logging", &s_netProfileBudgetLogging, 0, VF_NULL, "enable/disable budget logging");
	REGISTER_CVAR2_DEV_ONLY("net_profile_worst_num_channels", &s_netProfileWorstNumChannels, 15, VF_NULL, "maximum number of channels expected to connect");
	REGISTER_CVAR2_DEV_ONLY("net_profile_show_socket_measurements", &s_netProfileShowSocketMeasurements, 0, VF_NULL, "show bandwidth socket measurements on screen");
	REGISTER_CVAR2_DEV_ONLY("net_profile_enable", &s_netProfileEnable, 0, VF_NULL, "enable/disable net profile feature");
	REGISTER_CVAR2_DEV_ONLY("net_profile_untouched_delay", &s_netProfileUntouchedDelay, 5, VF_NULL, "number of seconds to hold untouched profile entries before ditching");
}

#endif
