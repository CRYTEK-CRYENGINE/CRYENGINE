// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __I_AI_MANNEQUIN__H__
#define __I_AI_MANNEQUIN__H__

namespace aiMannequin
{
//////////////////////////////////////////////////////////////////////////
enum EAiMannequinCommandType
{
	eMC_SetTag   = 0,
	eMC_ClearTag = 1,
};

//////////////////////////////////////////////////////////////////////////
struct SCommand
{
	uint16 m_nextOffset;
	uint16 m_type;
};

//////////////////////////////////////////////////////////////////////////
struct STagCommand
	: public SCommand
{
	uint32 m_tagCrc;
};

//////////////////////////////////////////////////////////////////////////
struct SSetTagCommand
	: public STagCommand
{
	static const uint16 Type = eMC_SetTag;
};

//////////////////////////////////////////////////////////////////////////
struct SClearTagCommand
	: public STagCommand
{
	static const uint16 Type = eMC_ClearTag;
};
}

//////////////////////////////////////////////////////////////////////////
template<typename CommandBaseType, uint16 kBufferSize>
class CCommandList
{
public:
	CCommandList()
		: m_usedBufferBytes(0)
	{
	}

	void ClearCommands()
	{
		m_usedBufferBytes = 0;
	}

	const CommandBaseType* GetFirstCommand() const
	{
		return GetCommand(0);
	}

	const CommandBaseType* GetNextCommand(const CommandBaseType* pCommand) const
	{
		assert(pCommand);
		return GetCommand(pCommand->m_nextOffset);
	}

	const CommandBaseType* GetCommand(const uint16 offset) const
	{
		const uint8* pRawCommand = m_buffer + offset;
		const CommandBaseType* pCommand = reinterpret_cast<const CommandBaseType*>(pRawCommand);
		return (offset < m_usedBufferBytes) ? pCommand : NULL;
	}

	template<typename T>
	T* CreateCommand()
	{
		const uint16 commandSizeBytes = sizeof(T);
		const uint16 freeBufferBytes = GetFreeBufferBytes();
		if (freeBufferBytes < commandSizeBytes)
		{
			CRY_ASSERT(false);
			return NULL;
		}

		uint8* pRawCommand = m_buffer + m_usedBufferBytes;
		CommandBaseType* pCommand = reinterpret_cast<CommandBaseType*>(pRawCommand);
		m_usedBufferBytes += commandSizeBytes;

		pCommand->m_nextOffset = m_usedBufferBytes;
		pCommand->m_type = T::Type;

		return reinterpret_cast<T*>(pCommand);
	}

	const uint16 GetBufferSizeBytes() const { return kBufferSize; }
	const uint16 GetUsedBufferBytes() const { return m_usedBufferBytes; }
	const uint16 GetFreeBufferBytes() const { return kBufferSize - m_usedBufferBytes; }

private:
	uint16 m_usedBufferBytes;
	uint8  m_buffer[kBufferSize];
};

//////////////////////////////////////////////////////////////////////////
template<uint32 kBufferSize>
class CAIMannequinCommandList
{
public:
	const aiMannequin::SCommand* CreateSetTagCommand(const uint32 tagCrc)
	{
		aiMannequin::SSetTagCommand* pCommand = m_commandList.template CreateCommand<aiMannequin::SSetTagCommand>();
		if (pCommand)
		{
			pCommand->m_tagCrc = tagCrc;
		}
		return pCommand;
	}

	const aiMannequin::SCommand* CreateClearTagCommand(const uint32 tagCrc)
	{
		aiMannequin::SClearTagCommand* pCommand = m_commandList.template CreateCommand<aiMannequin::SClearTagCommand>();
		if (pCommand)
		{
			pCommand->m_tagCrc = tagCrc;
		}
		return pCommand;
	}

	void ClearCommands()
	{
		m_commandList.ClearCommands();
	}

	const aiMannequin::SCommand* GetFirstCommand() const
	{
		return m_commandList.GetFirstCommand();
	}

	const aiMannequin::SCommand* GetNextCommand(const aiMannequin::SCommand* pCommand) const
	{
		return m_commandList.GetNextCommand(pCommand);
	}

private:
	CCommandList<aiMannequin::SCommand, kBufferSize> m_commandList;
};

#endif
