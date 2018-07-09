// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ILoadingMessageProvider.h"

CLoadingMessageProviderListNode * CLoadingMessageProviderListNode::s_first = NULL;
CLoadingMessageProviderListNode * CLoadingMessageProviderListNode::s_last = NULL;

#if 0
#define LOADING_MESSAGE_PROVIDER_ENABLED 1
#else
#define LOADING_MESSAGE_PROVIDER_ENABLED 0
#endif

ILoadingMessageProvider::ILoadingMessageProvider(CLoadingMessageProviderListNode * node)
{
#if LOADING_MESSAGE_PROVIDER_ENABLED
	node->Init(this);
#endif
}

void CLoadingMessageProviderListNode::Init(const ILoadingMessageProvider * messageProvider)
{
#if LOADING_MESSAGE_PROVIDER_ENABLED
	assert (messageProvider != NULL);

	m_prev = NULL;
	m_messageProvider = messageProvider;

	if (s_first)
	{
		assert (s_first->m_prev == NULL);
		s_first->m_prev = this;
	}
	else
	{
		assert (s_last == NULL);
		s_last = this;
	}

	m_next = s_first;
	s_first = this;
#endif
}

CLoadingMessageProviderListNode::~CLoadingMessageProviderListNode()
{
#if LOADING_MESSAGE_PROVIDER_ENABLED
	if (m_messageProvider)
	{
		if (s_first == this)
		{
			assert (m_prev == NULL);
			s_first = m_next;
		}
		else
		{
			assert (m_prev != NULL);
			assert (m_prev->m_next == this);
			m_prev->m_next = m_next;
			m_prev = NULL;
		}

		if (s_last == this)
		{
			assert (m_next == NULL);
			s_last = m_prev;
		}
		else
		{
			assert (m_next != NULL);
			assert (m_next->m_prev == this);
			if(m_next)
				m_next->m_prev = m_prev;
			m_next = NULL;
		}
	}
#endif
}

string CLoadingMessageProviderListNode::GetRandomMessage()
{
#if LOADING_MESSAGE_PROVIDER_ENABLED
	int totalMessages = 0;

	for (CLoadingMessageProviderListNode * eachOne = s_first; eachOne != NULL; eachOne = eachOne->m_next)
	{
		totalMessages += eachOne->m_messageProvider->GetNumMessagesProvided();
	}

	if (totalMessages)
	{
		// Create our own random number generator here, because if we use the global one we always get the same number returned the first time
		// this code is called...

		CMTRand_int32 generator(gEnv->bNoRandomSeed?0:(uint32)gEnv->pTimer->GetAsyncTime().GetValue());
		int randomMessageNum = generator.Generate() % totalMessages;

		for (CLoadingMessageProviderListNode * eachOne = s_first; eachOne != NULL; eachOne = eachOne->m_next)
		{
			int numMessagesFromMe = eachOne->m_messageProvider->GetNumMessagesProvided();
			if (randomMessageNum < numMessagesFromMe)
			{
				return eachOne->m_messageProvider->GetMessageNum(randomMessageNum);
			}
			randomMessageNum -= numMessagesFromMe;
		}

		assert (0);
	}
#endif
#if defined(_RELEASE)
	return "";
#else
	return "No random loading messages found!";
#endif
}

void CLoadingMessageProviderListNode::ListAll()
{
#if LOADING_MESSAGE_PROVIDER_ENABLED
	for (CLoadingMessageProviderListNode * eachOne = s_first; eachOne != NULL; eachOne = eachOne->m_next)
	{
		if (eachOne->m_prev)
		{
			CryLogAlways (" ");
		}
		int totalMessagesHere = eachOne->m_messageProvider->GetNumMessagesProvided();
		for (int i = 0; i < totalMessagesHere; ++ i)
		{
			CryLogAlways ("%p(%2d) \"%s\"", eachOne, i, eachOne->m_messageProvider->GetMessageNum(i).c_str());
		}
	}
#endif
}