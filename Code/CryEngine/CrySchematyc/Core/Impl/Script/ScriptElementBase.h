// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Script/IScript.h>
#include <CrySchematyc/Script/IScriptElement.h>
#include <CrySchematyc/SerializationUtils/ISerializationContext.h>
#include <CrySchematyc/Utils/Assert.h>
#include <CrySchematyc/Utils/IGUIDRemapper.h>

#include "Script/ScriptExtensionMap.h"

namespace Schematyc
{
template<typename INTERFACE> class CScriptElementBase : public INTERFACE
{
public:

	inline CScriptElementBase(const ScriptElementFlags& flags = EScriptElementFlags::None)
		: m_flags(flags)
		, m_pScript(nullptr)
		, m_pParent(nullptr)
		, m_pFirstChild(nullptr)
		, m_pLastChild(nullptr)
		, m_pPrevSibling(nullptr)
		, m_pNextSibling(nullptr)
	{}

	inline CScriptElementBase(const CryGUID& guid, const char* szName, const ScriptElementFlags& flags = EScriptElementFlags::None)
		: m_guid(guid)
		, m_name(szName)
		, m_flags(flags)
		, m_pScript(nullptr)
		, m_pParent(nullptr)
		, m_pFirstChild(nullptr)
		, m_pLastChild(nullptr)
		, m_pPrevSibling(nullptr)
		, m_pNextSibling(nullptr)
	{}

	inline ~CScriptElementBase()
	{
		if (m_pScript && (m_pScript->GetRoot() == this))
		{
			m_pScript->SetRoot(nullptr);
		}

		if (m_pParent)
		{
			m_pParent->DetachChild(*this);
		}

		for (IScriptElement* pChild = m_pFirstChild; pChild; )
		{
			IScriptElement* pNextChild = pChild->GetNextSibling();
			pChild->SetParent(nullptr);
			pChild->SetPrevSibling(nullptr);
			pChild->SetNextSibling(nullptr);
			pChild = pNextChild;
		}
	}

	// IScriptElement

	virtual CryGUID GetGUID() const override
	{
		return m_guid;
	}

	virtual void SetName(const char* szName) override
	{
		m_name = szName;
	}

	virtual const char* GetName() const override
	{
		return m_name.c_str();
	}

	virtual EScriptElementAccessor GetAccessor() const override
	{
		for (const IScriptElement* pParent = m_pParent; pParent; pParent = pParent->GetParent())
		{
			switch (pParent->GetType())
			{
			case EScriptElementType::Class:
			case EScriptElementType::State:
				{
					return EScriptElementAccessor::Private;
				}
			}
		}
		return EScriptElementAccessor::Public;
	}

	virtual void SetFlags(const ScriptElementFlags& flags) override
	{
		m_flags = flags;
	}

	virtual ScriptElementFlags GetFlags() const override
	{
		return m_flags;
	}

	virtual void SetScript(IScript* pScript) override
	{
		m_pScript = pScript;
	}

	virtual IScript* GetScript() override
	{
		return m_pScript;
	}

	virtual const IScript* GetScript() const override
	{
		return m_pScript;
	}

	virtual void AttachChild(IScriptElement& child) override
	{
		IScriptElement* pPrevParent = child.GetParent();
		if (pPrevParent)
		{
			pPrevParent->DetachChild(child);
		}

		child.SetParent(this);
		child.SetPrevSibling(m_pLastChild);

		if (m_pFirstChild)
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
		SCHEMATYC_CORE_ASSERT(pParent == this);
		if (pParent == this)
		{
			IScriptElement* pPrevSibling = child.GetPrevSibling();
			IScriptElement* pNextSibling = child.GetNextSibling();

			if (&child == m_pFirstChild)
			{
				SCHEMATYC_CORE_ASSERT(!pPrevSibling);
				m_pFirstChild = pNextSibling;
			}
			else
			{
				SCHEMATYC_CORE_ASSERT(pPrevSibling);
				pPrevSibling->SetNextSibling(pNextSibling);
			}

			if (&child == m_pLastChild)
			{
				SCHEMATYC_CORE_ASSERT(!pNextSibling);
				m_pLastChild = pPrevSibling;
			}
			else
			{
				SCHEMATYC_CORE_ASSERT(pNextSibling);
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

	virtual EVisitStatus VisitChildren(const ScriptElementVisitor& visitor) override
	{
		SCHEMATYC_CORE_ASSERT(visitor);
		if (visitor)
		{
			for (IScriptElement* pChild = m_pFirstChild; pChild; pChild = pChild->GetNextSibling())
			{
				EVisitStatus visitStatus = visitor(*pChild);
				if (visitStatus == EVisitStatus::Recurse)
				{
					visitStatus = pChild->VisitChildren(visitor);
				}
				if (visitStatus == EVisitStatus::Stop)
				{
					return visitStatus;
				}
			}
			return EVisitStatus::Continue;
		}
		return EVisitStatus::Error;
	}

	virtual EVisitStatus VisitChildren(const ScriptElementConstVisitor& visitor) const override
	{
		SCHEMATYC_CORE_ASSERT(visitor);
		if (visitor)
		{
			for (const IScriptElement* pChild = m_pFirstChild; pChild; pChild = pChild->GetNextSibling())
			{
				EVisitStatus visitStatus = visitor(*pChild);
				if (visitStatus == EVisitStatus::Recurse)
				{
					visitStatus = pChild->VisitChildren(visitor);
				}
				if (visitStatus == EVisitStatus::Stop)
				{
					return visitStatus;
				}
			}
			return EVisitStatus::Continue;
		}
		return EVisitStatus::Error;
	}

	virtual IScriptExtensionMap& GetExtensions() override
	{
		return m_extensions;
	}

	virtual const IScriptExtensionMap& GetExtensions() const override
	{
		return m_extensions;
	}

	virtual void EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const override
	{
		m_extensions.EnumerateDependencies(enumerator, type);
	}

	virtual void RemapDependencies(IGUIDRemapper& guidRemapper) override
	{
		m_guid = guidRemapper.Remap(m_guid);

		m_extensions.RemapDependencies(guidRemapper);
	}

	virtual void ProcessEvent(const SScriptEvent& event) override
	{
		m_extensions.ProcessEvent(event);
	}

	virtual void Serialize(Serialization::IArchive& archive) override
	{
		SerializationContext::SetValidatorLink(archive, SValidatorLink(m_guid));

		if (archive.isOutput())
		{
			EScriptElementType elementType = static_cast<const INTERFACE*>(this)->GetType();
			archive(elementType, "elementType");
		}

		switch (SerializationContext::GetPass(archive))
		{
		case ESerializationPass::LoadDependencies:
		case ESerializationPass::Save:
			{
				archive(m_guid, "guid");
				archive(m_name, "name");
				break;
			}
		}
	}

	// ~IScriptElement

protected:

	inline void AddExtension(const IScriptExtensionPtr& pExtension)
	{
		m_extensions.AddExtension(pExtension);
	}

	inline void SerializeExtensions(Serialization::IArchive& archive)
	{
		archive(m_extensions, "extensions");
	}

private:

	CryGUID               m_guid;
	string              m_name;
	ScriptElementFlags  m_flags;
	IScript*            m_pScript;
	IScriptElement*     m_pParent;
	IScriptElement*     m_pFirstChild;
	IScriptElement*     m_pLastChild;
	IScriptElement*     m_pPrevSibling;
	IScriptElement*     m_pNextSibling;
	CScriptExtensionMap m_extensions;
};
} // Schematyc
