// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{

		//===================================================================================
		//
		// CInputParameterID
		//
		// - unique ID of a single input parameter based on a 4-character string
		// - the ID is only unique among all the parameters of a single function/generator/evaluator, but not "globally"
		//
		//===================================================================================

		class CInputParameterID
		{
		public:

			static CInputParameterID   CreateFromString(const char (&idAsFourCharacterString)[5]);
			static CInputParameterID   CreateEmpty();

			bool                       IsEmpty() const;
			void                       ToString(char (&idAsFourCharacterString)[5]) const;
			bool                       operator==(const CInputParameterID& rhs) const;

		private:

			explicit                   CInputParameterID(const char (&idAsFourCharacterString)[5]);

		private:

			uint32                     m_id;
		};

		inline CInputParameterID::CInputParameterID(const char (&idAsFourCharacterString)[5])
		{
			m_id =
				(uint32)idAsFourCharacterString[0] |
				((uint32)idAsFourCharacterString[1] << 8) |
				((uint32)idAsFourCharacterString[2] << 16) |
				((uint32)idAsFourCharacterString[3] << 24);
		}

		inline CInputParameterID CInputParameterID::CreateFromString(const char (&idAsFourCharacterString)[5])
		{
			return CInputParameterID(idAsFourCharacterString);
		}

		inline CInputParameterID CInputParameterID::CreateEmpty()
		{
			return CInputParameterID("    ");
		}

		inline bool CInputParameterID::IsEmpty() const
		{
			static const CInputParameterID emptyParameterID = CreateEmpty();
			return m_id == emptyParameterID.m_id;
		}

		inline void CInputParameterID::ToString(char (&idAsFourCharacterString)[5]) const
		{
			idAsFourCharacterString[0] = (char)m_id;
			idAsFourCharacterString[1] = (char)(m_id >> 8);
			idAsFourCharacterString[2] = (char)(m_id >> 16);
			idAsFourCharacterString[3] = (char)(m_id >> 24);
			idAsFourCharacterString[4] = '\0';
		}

		inline bool CInputParameterID::operator==(const CInputParameterID& rhs) const
		{
			return m_id == rhs.m_id;
		}

		//===================================================================================
		//
		// IInputParameterRegistry
		//
		//===================================================================================

		struct IInputParameterRegistry
		{
			struct SParameterInfo
			{
				explicit                        SParameterInfo(const char* _szName, const CInputParameterID& _id, const Shared::CTypeInfo& _type, size_t _offset, const char* _szDescription);

				const char*                     szName;         // for displaying purpose only; may be changed at will
				CInputParameterID               id;             // unique ID of this parameter; it's only unique among the parameters in this registry (i. e. not globally unique)
				const Shared::CTypeInfo&        type;           // data type of the parameter's value
				size_t                          offset;         // memory offset of where the parameter is located in the SParams struct of the according generator/function/evaluator class
				const char*                     szDescription;  // short description of what the parameter represents
			};

			virtual                             ~IInputParameterRegistry() {}
			virtual size_t                      GetParameterCount() const = 0;
			virtual SParameterInfo              GetParameter(size_t index) const = 0;
		};

		inline IInputParameterRegistry::SParameterInfo::SParameterInfo(const char* _szName, const CInputParameterID& _id, const Shared::CTypeInfo& _type, size_t _offset, const char* _szDescription)
			: szName(_szName)
			, id(_id)
			, type(_type)
			, offset(_offset)
			, szDescription(_szDescription)
		{}

	}
}
