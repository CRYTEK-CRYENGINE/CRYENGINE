#include "DeviceProvider.h"

namespace CryVR
{
	namespace VRgineers
	{
		template <class ChildType>
		ChildType Singleton<ChildType>::s_instance;

		DeviceProvider::DeviceProvider()
			: Singleton(), m_deviceInitialized(false)
		{
		}

		DeviceProvider::~DeviceProvider()
		{
		}

		void DeviceProvider::Init()
		{
			std::lock_guard<std::mutex> scopeLock(m_mutex);

			if (!m_deviceInitialized)
			{
				m_device = Device::CreateInstance();
				m_deviceInitialized = true;
			}
		}

		void DeviceProvider::Shutdown()
		{
			std::lock_guard<std::mutex> scopeLock(m_mutex);

			if (m_deviceInitialized)
			{
				delete m_device;
				m_deviceInitialized = false;
			}
		}

		Device* DeviceProvider::GetDevice() const
		{
			std::lock_guard<std::mutex> scopeLock(m_mutex);

			return m_device;
		}
	}
}