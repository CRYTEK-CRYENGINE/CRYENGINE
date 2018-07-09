// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Script/IScriptElement.h>
#include <CrySchematyc2/Script/IScriptFile.h>
#include <CrySchematyc2/Services/ILog.h>

#include "Script/ScriptExtensionMap.h"

namespace Schematyc2
{
	template <typename INTERFACE> class CScriptElementBase : public INTERFACE
	{
	public:

		CScriptElementBase(EScriptElementType type, IScriptFile& file)
			: m_type(type)
			, m_file(file)
			, m_flags(EScriptElementFlags::None)
			, m_pNewFile(nullptr)
			, m_pParent(nullptr)
			, m_pFirstChild(nullptr)
			, m_pLastChild(nullptr)
			, m_pPrevSibling(nullptr)
			, m_pNextSibling(nullptr)
		{}

		~CScriptElementBase()
		{
			if(m_pParent)
			{
				m_pParent->DetachChild(*this);
			}
			for(IScriptElement* pChild = m_pFirstChild; pChild; )
			{
				IScriptElement* pNextChild = pChild->GetNextSibling();
				pChild->SetParent(nullptr);
				pChild->SetPrevSibling(nullptr);
				pChild->SetNextSibling(nullptr);
				pChild = pNextChild;
			}
		}

		// IScriptElement

		virtual EScriptElementType GetElementType() const override
		{
			return m_type;
		}

		virtual IScriptFile& GetFile() override
		{
			return m_file;
		}

		virtual const IScriptFile& GetFile() const override
		{
			return m_file;
		}

		virtual void SetElementFlags(EScriptElementFlags flags) override
		{
			m_flags = flags;
		}

		virtual EScriptElementFlags GetElementFlags() const override
		{
			return m_flags;
		}

		virtual void SetNewFile(INewScriptFile* pNewFile) override
		{
			m_pNewFile = pNewFile;
		}

		virtual INewScriptFile* GetNewFile() override
		{
			return m_pNewFile;
		}

		virtual const INewScriptFile* GetNewFile() const override
		{
			return m_pNewFile;
		}

		virtual void AttachChild(IScriptElement& child) override
		{
			IScriptElement* pPrevParent = child.GetParent();
			if(pPrevParent)
			{
				pPrevParent->DetachChild(child);
			}

			child.SetParent(this);
			child.SetPrevSibling(m_pLastChild);

			if(m_pFirstChild)
			{
				m_pLastChild->SetNextSibling(&child);
			}
			else
			{
				m_pFirstChild = &child;
			}
			m_pLastChild = &child;
		}

		virtual void DetachChild(IScriptElement& child) override
		{
			IScriptElement* pParent = child.GetParent();
			SCHEMATYC2_SYSTEM_ASSERT(pParent == this);
			if(pParent == this)
			{
				IScriptElement* pPrevSibling = child.GetPrevSibling();
				IScriptElement* pNextSibling = child.GetNextSibling();

				if(&child == m_pFirstChild)
				{
					SCHEMATYC2_SYSTEM_ASSERT(!pPrevSibling);
					m_pFirstChild = pNextSibling;
				}
				else
				{
					SCHEMATYC2_SYSTEM_ASSERT(pPrevSibling);
					pPrevSibling->SetNextSibling(pNextSibling);
				}

				if(&child == m_pLastChild)
				{
					SCHEMATYC2_SYSTEM_ASSERT(!pNextSibling);
					m_pLastChild = pPrevSibling;
				}
				else
				{
					SCHEMATYC2_SYSTEM_ASSERT(pNextSibling);
					pNextSibling->SetPrevSibling(pPrevSibling);
				}

				child.SetParent(nullptr);
				child.SetPrevSibling(nullptr);
				child.SetNextSibling(nullptr);
			}
		}

		virtual void SetParent(IScriptElement* pParent) override
		{
			m_pParent = pParent;
		}

		virtual void SetPrevSibling(IScriptElement* pPrevSibling) override
		{
			m_pPrevSibling = pPrevSibling;
		}

		virtual void SetNextSibling(IScriptElement* pNextSibling) override
		{
			m_pNextSibling = pNextSibling;
		}
		
		virtual IScriptElement* GetParent() override
		{
			return m_pParent;
		}

		virtual const IScriptElement* GetParent() const override
		{
			return m_pParent;
		}

		virtual IScriptElement* GetFirstChild() override
		{
			return m_pFirstChild;
		}

		virtual const IScriptElement* GetFirstChild() const override
		{
			return m_pFirstChild;
		}

		virtual IScriptElement* GetLastChild() override
		{
			return m_pLastChild;
		}

		virtual const IScriptElement* GetLastChild() const override
		{
			return m_pLastChild;
		}

		virtual IScriptElement* GetPrevSibling() override
		{
			return m_pPrevSibling;
		}

		virtual const IScriptElement* GetPrevSibling() const override
		{
			return m_pPrevSibling;
		}

		virtual IScriptElement* GetNextSibling() override
		{
			return m_pNextSibling;
		}

		virtual const IScriptElement* GetNextSibling() const override
		{
			return m_pNextSibling;
		}

		virtual IScriptExtensionMap& GetExtensions() override
		{
			return m_extensions;
		}

		virtual const IScriptExtensionMap& GetExtensions() const override
		{
			return m_extensions;
		}

		virtual void Refresh(const SScriptRefreshParams& params) override
		{
			m_extensions.Refresh(params);
		}

		virtual void Serialize(Serialization::IArchive& archive) override
		{
			if(archive.isOutput())
			{
				EScriptElementType elementType = static_cast<const INTERFACE*>(this)->GetElementType();
				archive(elementType, "elementType");
			}

			archive(m_extensions, "extensions");
		}

		virtual void RemapGUIDs(IGUIDRemapper& guidRemapper) override
		{
			m_extensions.RemapGUIDs(guidRemapper);
		}

		// ~IScriptElement

		inline void AddExtension(const IScriptExtensionPtr& pExtension)
		{
			m_extensions.AddExtension(pExtension);
		}

	private:

		EScriptElementType  m_type;
		IScriptFile&        m_file;
		EScriptElementFlags m_flags;
		INewScriptFile*     m_pNewFile;
		IScriptElement*     m_pParent;
		IScriptElement*     m_pFirstChild;
		IScriptElement*     m_pLastChild;
		IScriptElement*     m_pPrevSibling;
		IScriptElement*     m_pNextSibling;
		CScriptExtensionMap m_extensions;
	};
}
