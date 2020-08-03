#pragma once

#include "VRgineersDevice.h"

#include <mutex>

namespace CryVR
{
	namespace VRgineers
	{
		template <class ChildType>
		class Singleton
		{
		private:
			friend ChildType;

		private:
			static ChildType s_instance;

		private:
			Singleton(const Singleton&) = delete;

			Singleton(Singleton&&) = delete;

			Singleton& operator=(const Singleton&) = delete;

			Singleton& operator=(Singleton&&) = delete;

			Singleton()
			{
			}

		public:
			static ChildType& GetInstance()
			{
				return s_instance;
			}

			virtual ~Singleton()
			{
			}
		};


		class DeviceProvider : public Singleton<DeviceProvider>
		{
		private:
			friend class Singleton<DeviceProvider>;

		private:
			DeviceProvider();

		public:
			virtual ~DeviceProvider();

		public:
			void Init();

			void Shutdown();

			Device* GetDevice() const;

		private:
			Device* m_device;

			bool m_deviceInitialized;

			mutable std::mutex m_mutex;
		};
	}
}