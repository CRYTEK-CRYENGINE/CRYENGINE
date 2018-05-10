// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Control D3D debug runtime output.

#ifndef __D3DDEBUG__H__
#define __D3DDEBUG__H__

#if defined(DX11_ALLOW_D3D_DEBUG_RUNTIME)
enum ESeverityCombination
{
	ESeverity_None = 0,
	ESeverity_Info,
	ESeverity_InfoWarning,
	ESeverity_InfoWarningError,
	ESeverity_All
};

enum { MAX_NUM_DEBUG_MSG_IDS = 32 };

class CD3DDebug
{
public:
	CD3DDebug();
	~CD3DDebug();

	bool Init(ID3D11Device* pD3DDevice);
	void Release();
	void Update(ESeverityCombination muteSeverity, const char* strMuteMsgList, const char* strBreakOnMsgList);

	// To use D3D debug info queue outside of this class,
	// you can push a copy of current settings or empty filter settings onto stack:
	//      m_d3dDebug.GetDebugInfoQueue()->PushCopyOfStorageFilter()  or   PushEmptyStorageFilter()
	//      .... change settings here
	// or push your filter settings directly
	//      m_d3dDebug.GetDebugInfoQueue()->PushStorageFilter(&myFilter);
	// Note that 'PopStorageFilter' should be called before next CD3DDebug::Update,
	// as CD3DDebug::Update modifies filter on top of stack
	ID3D11InfoQueue* GetDebugInfoQueue()
	{
		return m_pd3dDebugQueue;
	}

	string GetLastMessage();

private:
	ID3D11InfoQueue* m_pd3dDebugQueue;

	D3D11_MESSAGE_ID m_arrBreakOnIDsList[MAX_NUM_DEBUG_MSG_IDS];
	UINT             m_nNumCurrBreakOnIDs;

	UINT ParseIDs(const char* strMsgIDList, D3D11_MESSAGE_ID arrMsgList[MAX_NUM_DEBUG_MSG_IDS]) const;
};

#endif // #if defined(DX11_ALLOW_D3D_DEBUG_RUNTIME)

#endif // __D3DDEBUG__H__
