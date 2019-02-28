// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ILOADINGMESSAGEPROVIDER_H__
#define __ILOADINGMESSAGEPROVIDER_H__

class CLoadingMessageProviderListNode;

class ILoadingMessageProvider
{
	public:
	virtual	~ILoadingMessageProvider(){}
	ILoadingMessageProvider(CLoadingMessageProviderListNode * node);
	virtual int GetNumMessagesProvided() const = 0;
	virtual string GetMessageNum(int n) const = 0;
};

class CLoadingMessageProviderListNode
{
	private:
	const ILoadingMessageProvider * m_messageProvider;
	static CLoadingMessageProviderListNode * s_first;
	static CLoadingMessageProviderListNode * s_last;
	CLoadingMessageProviderListNode * m_next;
	CLoadingMessageProviderListNode * m_prev;

	public:
	void Init(const ILoadingMessageProvider * messageProvider);
	~CLoadingMessageProviderListNode();
	static string GetRandomMessage();
	static void ListAll();
};

#endif // __ILOADINGMESSAGEPROVIDER_H__
