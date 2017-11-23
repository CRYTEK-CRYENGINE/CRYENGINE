#pragma once

#include "IPlatformServer.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			class CServer
				: public IServer
			{
			public:
				CServer(bool bLocal);
				virtual ~CServer();

				// IServer
				virtual IServer::Identifier GetIdentifier() const override;

				virtual uint32 GetPublicIP() const override;
				virtual const char* GetPublicIPString() const override;

				virtual uint16 GetPort() const override;

				virtual bool IsLocal() const override { return m_bLocal; }

				virtual bool AuthenticateUser(uint32 clientIP, char* authData, int authDataLength, IUser::Identifier &userId) override;
				virtual void SendUserDisconnect(IUser::Identifier userId) override;
				// ~IServer

			protected:
				bool m_bLocal;
			};
		}
	}
}