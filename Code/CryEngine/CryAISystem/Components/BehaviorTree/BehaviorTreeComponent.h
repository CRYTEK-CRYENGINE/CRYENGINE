// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

class CEntityAIBehaviorTreeComponent final : public IEntityComponent
{
private:

	// - just a wrapper around a file name without extension
	// - has a custom serializer that offers the user all available behavior trees as a UI list
	class CBehaviorTreeName
	{
	public:

		static void       ReflectType(Schematyc::CTypeDesc<CBehaviorTreeName>& desc);
		bool              Serialize(Serialization::IArchive& archive);
		const char*       GetFilePathWithoutExtension() const { return m_filePathWithoutExtension.c_str(); }
		bool              operator==(const CBehaviorTreeName& rhs) const { return m_filePathWithoutExtension == rhs.m_filePathWithoutExtension; }

	private:

		string            m_filePathWithoutExtension;
	};

public:

	CEntityAIBehaviorTreeComponent();

	// IEntityComponent
	virtual void          OnShutDown() override;
	virtual uint64        GetEventMask() const override;
	virtual void          ProcessEvent(const SEntityEvent& event) override;
	// ~IEntityComponent

	static void           ReflectType(Schematyc::CTypeDesc<CEntityAIBehaviorTreeComponent>& desc);
	static void           Register(Schematyc::IEnvRegistrar& registrar);

private:

	void                  EnsureBehaviorTreeIsRunning();
	void                  EnsureBehaviorTreeIsStopped();

	void                  SchematycFunction_SendEvent(const Schematyc::CSharedString& eventName);
	template <class TValue>
	void                  SchematycFunction_SetBBKeyValue(const Schematyc::CSharedString& key, const TValue& value);

private:

	CBehaviorTreeName     m_behaviorTreeName;
	bool                  m_bBehaviorTreeShouldBeRunning;
	bool                  m_bBehaviorTreeIsRunning;
};
