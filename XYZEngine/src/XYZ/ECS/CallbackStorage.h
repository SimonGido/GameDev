#pragma once
#include "XYZ/Utils/DataStructures/FreeList.h"

namespace XYZ {

	enum class CallbackType
	{
		None,
		ComponentCreate,
		ComponentRemove,
		EntityDestroy
	};

	struct Listener
	{
		void(*Callback)(void*, uint32_t, CallbackType);
		void* Instance;
	};

	class ICallbackStorage
	{
	public:
		virtual ~ICallbackStorage() = default;

		virtual void Execute(uint32_t entity, CallbackType type) = 0;
	};

	template <typename T>
	class CallbackStorage : public ICallbackStorage
	{
	public:
		virtual void Execute(uint32_t entity, CallbackType type) override
		{
			for (auto& listener : m_Listeners)
			{
				listener.Callback(listener.Instance, entity, type);
			}
		}

		void AddListener(void(*callback)(void*, uint32_t, CallbackType), void* instance)
		{
			XYZ_ASSERT(std::invoke([this, instance]() -> bool {
				for (auto it : m_Listeners)
				{
					if (it.Instance == instance)
						return false;
				}
				return true;
				}), "");
			m_Listeners.push_back({ callback, instance});
		}

		void RemoveListener(void *instance)
		{
			for (auto it = m_Listeners.begin(); it != m_Listeners.end(); ++it)
			{
				if (it->Instance == instance)
				{
					m_Listeners.erase(it);
					return;
				}
			}
		}
		
	private:
		std::vector<Listener> m_Listeners;
	};

}