// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.ServiceModel;

namespace Statoscope
{
	class WCF
	{
		[ServiceContract]
		interface IStatoscopeRPC
		{
			[OperationContract]
			void LoadLog(string logURI);
		}

		[ServiceBehavior(InstanceContextMode = InstanceContextMode.Single)]
		class CStatoscopeRPC : IStatoscopeRPC
		{
			StatoscopeForm m_ssForm;

			public CStatoscopeRPC(StatoscopeForm ssForm)
			{
				m_ssForm = ssForm;
			}

			public void LoadLog(string logURI)
			{
				System.Console.WriteLine("Received RPC to load: " + logURI);
				m_ssForm.OpenLog(logURI);
			}
		}

		static ServiceHost s_serviceHost;
		const string PipeAddress = "net.pipe://localhost/Statoscope";
		const string PipeServiceName = "StatoscopeRPC";
		const string PipeEndpointAddress = PipeAddress + "/" + PipeServiceName;

		public static void CreateServiceHost(StatoscopeForm ssForm)
		{
			try
			{
				s_serviceHost = new ServiceHost(new CStatoscopeRPC(ssForm), new Uri[] { new Uri(PipeAddress) });
				s_serviceHost.AddServiceEndpoint(typeof(IStatoscopeRPC), new NetNamedPipeBinding(), PipeServiceName);
				s_serviceHost.Open();
			}
			catch (AddressAlreadyInUseException)
			{
			}
		}

		public static bool CheckForExistingToolProcess(string logURI)
		{
			try
			{
				ChannelFactory<IStatoscopeRPC> ssRPCFactory = new ChannelFactory<IStatoscopeRPC>(new NetNamedPipeBinding(), new EndpointAddress(PipeEndpointAddress));
				IStatoscopeRPC ssRPCProxy = ssRPCFactory.CreateChannel();
				ssRPCProxy.LoadLog(logURI);
			}
			catch (EndpointNotFoundException)
			{
				return false;
			}

			return true;
		}
	}
}
