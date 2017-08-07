// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "InputParameterRegistry.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{
		namespace Internal
		{

			CInputParameterRegistry::SStoredParameterInfo::SStoredParameterInfo(const char* _szName, const CInputParameterID& _id, const Shared::CTypeInfo& _type, size_t _offset, const char* _szDescription)
				: name(_szName)
				, id(_id)
				, type(_type)
				, offset(_offset)
				, description(_szDescription)
			{
				// nothing
			}

			void CInputParameterRegistry::RegisterParameterType(const char* szParamName, const char (&idAsFourCharacterString)[5], const Shared::CTypeInfo& typeInfo, size_t offset, const char* szDescription)
			{
				const CInputParameterID id = CInputParameterID::CreateFromString(idAsFourCharacterString);

				// prevent duplicates (these checks will also be done by the StartupConsistencyChecker)
				assert(std::find_if(m_parametersInOrder.cbegin(), m_parametersInOrder.cend(), [szParamName](const SStoredParameterInfo& p) { return p.name == szParamName; }) == m_parametersInOrder.cend());
				assert(std::find_if(m_parametersInOrder.cbegin(), m_parametersInOrder.cend(), [id](const SStoredParameterInfo& p) { return p.id == id; }) == m_parametersInOrder.cend());
				assert(std::find_if(m_parametersInOrder.cbegin(), m_parametersInOrder.cend(), [offset](const SStoredParameterInfo& p) { return p.offset == offset; }) == m_parametersInOrder.cend());

				SStoredParameterInfo pi(szParamName, id, typeInfo, offset, szDescription);
				m_parametersInOrder.push_back(pi);
			}

			size_t CInputParameterRegistry::GetParameterCount() const
			{
				return m_parametersInOrder.size();
			}

			IInputParameterRegistry::SParameterInfo CInputParameterRegistry::GetParameter(size_t index) const
			{
				assert(index < m_parametersInOrder.size());
				const SStoredParameterInfo& pi = m_parametersInOrder[index];
				return SParameterInfo(pi.name.c_str(), pi.id, pi.type, pi.offset, pi.description.c_str());
			}

		}
	}
}
