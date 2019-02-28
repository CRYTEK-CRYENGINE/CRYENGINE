// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CItemDebugProxyFactory
		//
		//===================================================================================

		class CItemDebugProxyFactory : public IItemDebugProxyFactory
		{
		public:
			explicit                                       CItemDebugProxyFactory();

			virtual IItemDebugProxy_Sphere&                CreateSphere() override;
			virtual IItemDebugProxy_Path&                  CreatePath() override;
			virtual IItemDebugProxy_AABB&                  CreateAABB() override;

			std::unique_ptr<CItemDebugProxyBase>           GetAndForgetLastCreatedDebugItemRenderProxy();
			void                                           Serialize(Serialization::IArchive& ar);

		private:
			                                               UQS_NON_COPYABLE(CItemDebugProxyFactory);

		private:
			std::unique_ptr<CItemDebugProxyBase>           m_pLastCreatedDebugItemRenderProxy;
		};

	}
}
