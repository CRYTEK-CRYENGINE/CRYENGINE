// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CompositeDatagramSocket.h"

CCompositeDatagramSocket::CCompositeDatagramSocket()
{
}

CCompositeDatagramSocket::~CCompositeDatagramSocket()
{
}

void CCompositeDatagramSocket::AddChild(IDatagramSocketPtr child)
{
	stl::push_back_unique(m_children, child);
}

void CCompositeDatagramSocket::GetSocketAddresses(TNetAddressVec& addrs)
{
	for (ChildVecIter iter = m_children.begin(); iter != m_children.end(); ++iter)
		(*iter)->GetSocketAddresses(addrs);
}

ESocketError CCompositeDatagramSocket::Send(const uint8* pBuffer, size_t nLength, const TNetAddress& to)
{
	ESocketError err = eSE_Ok;
	for (ChildVecIter iter = m_children.begin(); iter != m_children.end(); ++iter)
	{
		ESocketError childErr = (*iter)->Send(pBuffer, nLength, to);
		if (childErr > err)
			err = childErr;
	}
	return err;
}

ESocketError CCompositeDatagramSocket::SendVoice(const uint8* pBuffer, size_t nLength, const TNetAddress& to)
{
	ESocketError err = eSE_Ok;
	for (ChildVecIter iter = m_children.begin(); iter != m_children.end(); ++iter)
	{
		ESocketError childErr = (*iter)->SendVoice(pBuffer, nLength, to);
		if (childErr > err)
			err = childErr;
	}
	return err;
}

void CCompositeDatagramSocket::RegisterListener(IDatagramListener* pListener)
{
	for (ChildVecIter iter = m_children.begin(); iter != m_children.end(); ++iter)
	{
		(*iter)->RegisterListener(pListener);
	}
}

void CCompositeDatagramSocket::UnregisterListener(IDatagramListener* pListener)
{
	for (ChildVecIter iter = m_children.begin(); iter != m_children.end(); ++iter)
	{
		(*iter)->UnregisterListener(pListener);
	}
}

void CCompositeDatagramSocket::Die()
{
	for (ChildVecIter iter = m_children.begin(); iter != m_children.end(); ++iter)
		(*iter)->Die();
}

bool CCompositeDatagramSocket::IsDead()
{
	for (ChildVecIter iter = m_children.begin(); iter != m_children.end(); ++iter)
		if ((*iter)->IsDead())
			return true;
	return false;
}

CRYSOCKET CCompositeDatagramSocket::GetSysSocket()
{
	for (ChildVecIter iter = m_children.begin(); iter != m_children.end(); ++iter)
		if ((*iter)->GetSysSocket() != CRY_INVALID_SOCKET)
			return (*iter)->GetSysSocket();
	return CRY_INVALID_SOCKET;
}

void CCompositeDatagramSocket::RegisterBackoffAddress(const TNetAddress& addr)
{
	for (ChildVecIter iter = m_children.begin(); iter != m_children.end(); ++iter)
		(*iter)->RegisterBackoffAddress(addr);
}

void CCompositeDatagramSocket::UnregisterBackoffAddress(const TNetAddress& addr)
{
	for (ChildVecIter iter = m_children.begin(); iter != m_children.end(); ++iter)
		(*iter)->UnregisterBackoffAddress(addr);
}
