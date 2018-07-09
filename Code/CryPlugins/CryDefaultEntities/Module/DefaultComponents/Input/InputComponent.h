#pragma once

#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/IObject.h>
#include <CrySchematyc/Utils/SharedString.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CryGame/IGameFramework.h>

#include <IActionMapManager.h>

class CPlugin_CryDefaultEntities;

namespace Cry
{
	namespace DefaultComponents
	{
		class CInputComponent
			: public IEntityComponent
		{
		protected:
			friend CPlugin_CryDefaultEntities;
			static void Register(Schematyc::CEnvRegistrationScope& componentScope);

			// IEntityComponent
			virtual void Initialize() final;
			// ~IEntityComponent

		public:
			enum class EKeyboardInputId {};
			enum class EMouseInputId {};
			enum class EXboxInputId {};
			enum class EPS4InputId {};

			CInputComponent() = default;
			virtual ~CInputComponent() {}

			typedef std::function<void(int activationMode, float value)> TActionCallback;

			virtual void RegisterAction(const char* szGroupName, const char* szName, TActionCallback callback)
			{
				IActionMapManager *pActionMapManager = gEnv->pGameFramework->GetIActionMapManager();
				IActionMap* pActionMap = pActionMapManager->GetActionMap(szGroupName);
				if (pActionMap == nullptr)
				{
					pActionMap = pActionMapManager->CreateActionMap(szGroupName);

					m_groups.emplace_back(szGroupName);
				}
				else
				{
					bool bFound = false;

					for (const SGroup& group : m_groups)
					{
						if (!strcmp(group.m_szName, szGroupName))
						{
							bFound = true;
							break;
						}
					}

					if (!bFound)
					{
						m_groups.emplace_back(szGroupName);
					}
				}

				IActionMapAction* pAction = pActionMap->GetAction(ActionId(szName));
				if (pAction != nullptr)
				{
					pActionMap->RemoveAction(ActionId(szName));
				}
				
				pActionMap->CreateAction(ActionId(szName));

				for (SGroup& group : m_groups)
				{
					if (!strcmp(group.m_szName, szGroupName))
					{
						group.AddAction(szName, callback);
					}
				}
			}

			virtual void BindKeyboardAction(Schematyc::CSharedString groupName, Schematyc::CSharedString name, EKeyboardInputId keyId, bool bOnPress = true, bool bOnRelease = true, bool bOnHold = true);
			virtual void BindMouseAction(Schematyc::CSharedString groupName, Schematyc::CSharedString name, EMouseInputId keyId, bool bOnPress = true, bool bOnRelease = true, bool bOnHold = true);
			virtual void BindXboxAction(Schematyc::CSharedString groupName, Schematyc::CSharedString name, EXboxInputId keyId, bool bOnPress = true, bool bOnRelease = true, bool bOnHold = true);
			virtual void BindPS4Action(Schematyc::CSharedString groupName, Schematyc::CSharedString name, EPS4InputId keyId, bool bOnPress = true, bool bOnRelease = true, bool bOnHold = true);
			virtual void BindAction(Schematyc::CSharedString groupName, Schematyc::CSharedString name, EActionInputDevice device, EKeyId keyId, bool bOnPress = true, bool bOnRelease = true, bool bOnHold = true);

			static void ReflectType(Schematyc::CTypeDesc<CInputComponent>& desc)
			{
				desc.SetGUID("{183E3C84-2E08-4506-95FA-B362F7E735E9}"_cry_guid);
				desc.SetEditorCategory("Input");
				desc.SetLabel("Input");
				desc.SetDescription("Exposes support for inputs and action maps");
				//desc.SetIcon("icons:ObjectTypes/object.ico");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::HideFromInspector });
			}

			virtual void RegisterSchematycAction(Schematyc::CSharedString groupName, Schematyc::CSharedString name);

		protected:
			struct SGroup final : public IActionListener
			{
				struct SAction
				{
					const char* szName;
					TActionCallback callback;
				};

				SGroup(const char* szName)
					: m_szName(szName) {}

				SGroup(const SGroup& other)
					: m_szName(other.m_szName)
					, m_actions(other.m_actions)
				{
					if (m_actions.size() > 0)
					{
						gEnv->pGameFramework->GetIActionMapManager()->AddExtraActionListener(this, m_szName);
					}
				}

				virtual ~SGroup()
				{
					if (m_actions.size() > 0)
					{
						gEnv->pGameFramework->GetIActionMapManager()->RemoveExtraActionListener(this, m_szName);
					}
				}

				void AddAction(const char* szName, TActionCallback callback)
				{
					// Delete already existing entries
					for (auto it = m_actions.begin(); it != m_actions.end(); it++)
					{
						if (strcmp(it->szName, szName) == 0)
						{
							m_actions.erase(it);
							break;
						}
					}

					if (m_actions.size() == 0)
					{
						gEnv->pGameFramework->GetIActionMapManager()->AddExtraActionListener(this, m_szName);
					}

					m_actions.emplace_back(SGroup::SAction{ szName, callback });
				}

				// IActionListener
				virtual void OnAction(const ActionId &actionId, int activationMode, float value) override
				{
					for (const SAction& action : m_actions)
					{
						if (!strcmp(action.szName, actionId.c_str()))
						{
							action.callback(activationMode, value);
							break;
						}
					}
				}
				// ~IActionListener

				const char* m_szName;

			protected:
				DynArray<SAction> m_actions;
			};

			DynArray<SGroup> m_groups;
		};
	}
}