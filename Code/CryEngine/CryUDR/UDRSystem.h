// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace Cry
{
	namespace UDR
	{
		class CUDRSystem final : public IUDRSystem
		{
			struct SPrimitiveWithDuration
			{
				SPrimitiveWithDuration(const std::shared_ptr<CRenderPrimitiveBase>& primitive_, const float duration_)
					: primitive(primitive_)
					, duration(duration_)
				{}

				std::shared_ptr<CRenderPrimitiveBase> primitive;
				float                                 duration = 0.0f;
			};

		public:
			CUDRSystem(const CUDRSystem&) = delete;
			CUDRSystem(CUDRSystem&&) = delete;

			// IUDRSystem
			virtual bool                  Initialize() override;
			virtual void                  Reset() override;
			virtual void                  Update(const CTimeValue frameStartTime, const float frameDeltaTime) override;
			
			virtual ITreeManager&         GetTreeManager() override;
			virtual INodeStack&           GetNodeStack() override;
			virtual IRecursiveSyncObject& GetRecursiveSyncObject() override;

			virtual bool                  SetConfig(const SConfig& config) override;
			virtual const SConfig&        GetConfig() const override;
			// ~IUDRSystem

			//! Gets the single existing instance of the UDR system. 
			//! \return Singleton instance of UDR system.
			//! \note Provides access to the UDR System through specific class (CUDRSystem) as opposed to the global environment which provides access through the interface (IUDRSystem). 
			static CUDRSystem&            GetInstance();

			//! Gets the elapsed time since UDR was initialized or reset.
			//! \return Elapsed time.
			//! \note Global Environment and Timer System must be initialized.
			CTimeValue                    GetElapsedTime() const;

			//! Draws the given primitive on the screen for a specified time.
			//! \param pPrimitive Primitive to be rendered on the screen.
			//! \param duration Duration in seconds that the primitive should persist on the screen. Default value is 0.0f which renders the primitive for only one frame.
			//! \note Primitives are added to a queue and will be drawn during the Update function.
			void                          DebugDraw(const std::shared_ptr<CRenderPrimitiveBase>& pPrimitive, const float duration = 0.0f);

			CUDRSystem&                   operator=(const CUDRSystem&) = delete;
			CUDRSystem&                   operator=(CUDRSystem&&) = delete;

		private:

			explicit CUDRSystem();
			~CUDRSystem();

			void                          DebugDrawPrimitivesQueue();

			static void                   CmdDumpRecordings(IConsoleCmdArgs* pArgs);
			static void                   CmdClearRecordingsInLiveTree(IConsoleCmdArgs* pArgs);
			static void                   CmdClearRecordingsInDeserializedTree(IConsoleCmdArgs* pArgs);
			static void                   CmdPrintLiveNodes(IConsoleCmdArgs* pArgs);
			static void                   CmdPrintStatistics(IConsoleCmdArgs* pArgs);

			static void                   HelpCmdClearRecordings(IConsoleCmdArgs* pArgs, ITreeManager::ETreeIndex treeIndex);

		private:

			static CUDRSystem                   s_instance;

			IUDRSystem::SConfig                 m_config;
			CTreeManager                        m_treeManager;
			CNodeStack                          m_nodeStack;
			CRecursiveSyncObject                m_recursiveSyncObject;
			std::vector<SPrimitiveWithDuration> m_primitivesQueue;

			CTimeMetadata                       m_timeMetadataBase;
			CTimeValue                          m_frameStartTime;
			float                               m_frameDeltaTime = 0;
			
		};
	}
}