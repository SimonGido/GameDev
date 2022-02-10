#pragma once
#include "XYZ/Scene/SceneEntity.h"
#include "XYZ/Utils/Delegate.h"


#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/compatibility.hpp>

namespace XYZ {


	template <typename T>
	struct KeyFrame
	{
		bool operator <(const KeyFrame<T>& other) const
		{
			return Frame < other.Frame;
		}
		bool operator >(const KeyFrame<T>& other) const
		{
			return Frame > other.Frame;
		}

		T		 Value;
		uint32_t Frame = 0;
	};

	class IProperty
	{
	public:
		virtual ~IProperty() = default;

		virtual void Update(size_t key, uint32_t frame) = 0;
		virtual void SetSceneEntity(const SceneEntity& entity) = 0;

		virtual const SceneEntity& GetSceneEntity()				 const = 0;
		virtual const std::string& GetPath()					 const = 0;
		virtual const std::string& GetValueName()				 const = 0;
		virtual const std::string& GetComponentName()			 const = 0;
		virtual size_t			   GetKeyCount()				 const = 0;
		virtual uint32_t		   GetEndFrame(size_t key)		 const = 0;
		virtual uint32_t		   Length()					     const = 0;
		virtual int64_t			   FindKey(uint32_t frame)	     const = 0;
		virtual bool			   HasKeyAtFrame(uint32_t frame) const = 0;
	};

	template <typename T>
	class Property : public IProperty
	{
	public:	
		Property(const std::string& path);
		virtual ~Property() override;

		Property(const Property<T>& other);
		Property(Property<T>&& other) noexcept;
		Property<T>& operator=(const Property<T>& other);
		Property<T>& operator=(Property<T>&& other) noexcept;

		virtual void Update(size_t key, uint32_t frame) override;
		virtual void SetSceneEntity(const SceneEntity& entity) override;


		virtual const SceneEntity& GetSceneEntity()				 const override { return m_Entity; }
		virtual const std::string& GetPath()					 const override { return m_Path; }
		virtual const std::string& GetValueName()				 const override { return m_ValueName; }
		virtual const std::string& GetComponentName()			 const override { return m_ComponentName; }
		virtual size_t			   GetKeyCount()				 const override { return Keys.size(); }
		virtual uint32_t		   GetEndFrame(size_t key)		 const override { return Keys[key].Frame; }
		virtual uint32_t		   Length()					     const override;
		virtual int64_t			   FindKey(uint32_t frame)	     const override;
		virtual bool			   HasKeyAtFrame(uint32_t frame) const override;

		template <typename ComponentType, uint16_t valIndex>
		void Setup();

		bool AddKeyFrame(const KeyFrame<T>& key);
		T    GetValue(uint32_t frame) const;
	
			 
		std::vector<KeyFrame<T>> Keys;
	private:
		template <typename ComponentType, uint16_t valIndex>
		static T* getReference(SceneEntity& entity);
	
	private:
		T*								   m_Value;
		SceneEntity						   m_Entity;
		
		std::string						   m_Path;
		std::string						   m_ValueName;
		std::string						   m_ComponentName;
	
		
		Delegate<T*(SceneEntity& entity)>  m_GetPropertyReference;	
		uint16_t						   m_ValueIndex  = UINT16_MAX;
		uint16_t						   m_ComponentID = UINT16_MAX;
	};

	template<typename T>
	inline Property<T>::Property(const std::string& path)
		:
		m_Value(nullptr),
		m_Path(path)
	{
	}

	template<typename T>
	inline Property<T>::~Property()
	{
	}
	template<typename T>
	inline Property<T>::Property(const Property<T>& other)
		:
		m_Value(other.m_Value),
		m_Entity(other.m_Entity),
		m_Path(other.m_Path),
		m_ValueName(other.m_ValueName),
		m_ComponentName(other.m_ComponentName),
		m_ValueIndex(other.m_ValueIndex),
		m_ComponentID(other.m_ComponentID),
		m_GetPropertyReference(other.m_GetPropertyReference),
		Keys(other.Keys)
	{
	}
	template<typename T>
	inline Property<T>::Property(Property<T>&& other) noexcept
		:
		m_Value(other.m_Value),
		m_Entity(std::move(other.m_Entity)),
		m_Path(std::move(other.m_Path)),
		m_ValueName(std::move(other.m_ValueName)),
		m_ComponentName(std::move(other.m_ComponentName)),
		m_ValueIndex(other.m_ValueIndex),
		m_ComponentID(other.m_ComponentID),
		m_GetPropertyReference(std::move(other.m_GetPropertyReference)),
		Keys(std::move(other.Keys))
	{
	}
	template<typename T>
	inline Property<T>& Property<T>::operator=(const Property<T>& other)
	{
		m_Value				  = other.m_Value;
		m_Entity			  = other.m_Entity;
		m_Path				  = other.m_Path;
		m_ValueName			  = other.m_ValueName;
		m_ComponentName		  = other.m_ComponentName;
		m_ValueIndex		  = other.m_ValueIndex;
		m_ComponentID		  = other.m_ComponentID;
		m_GetPropertyReference = other.m_GetPropertyReference;
		Keys				  = other.Keys;
		return *this;
	}
	template<typename T>
	inline Property<T>& Property<T>::operator=(Property<T>&& other) noexcept
	{
		m_Value				   = other.m_Value;
		m_Entity			   = std::move(other.m_Entity);
		m_Path				   = std::move(other.m_Path);
		m_ValueName			   = std::move(other.m_ValueName);
		m_ComponentName		   = std::move(other.m_ComponentName);
		m_ValueIndex		   = other.m_ValueIndex;
		m_ComponentID		   = other.m_ComponentID;
		m_GetPropertyReference = std::move(other.m_GetPropertyReference);
		Keys				   = std::move(other.Keys);
		return *this;
	}


	template<typename T>
	inline void Property<T>::SetSceneEntity(const SceneEntity& entity)
	{
		m_Entity = entity;
	}



	template<typename T>
	inline bool Property<T>::AddKeyFrame(const KeyFrame<T>& key)
	{
		for (const auto& k : Keys)
		{
			if (k.Frame == key.Frame)
				return false;
		}
		Keys.push_back(key);
		std::sort(Keys.begin(), Keys.end());
		return true;
	}


	template<typename T>
	inline bool Property<T>::HasKeyAtFrame(uint32_t frame) const
	{
		for (const auto& key : Keys)
		{
			if (key.Frame == frame)
				return true;
		}
		return false;
	}

	template<typename T>
	inline uint32_t Property<T>::Length() const
	{
		if (Keys.empty())
			return 0;
		return Keys.back().Frame;
	}
	template<typename T>
	inline int64_t Property<T>::FindKey(uint32_t frame) const
	{
		if (!Keys.empty())
		{
			for (size_t i = 0; i < Keys.size() - 1; ++i)
			{
				const auto& key = Keys[i];
				const auto& next = Keys[i + 1];
				if (frame >= key.Frame && frame < next.Frame)
					return i;
			}
			return Keys.size() - 1;
		}
		return -1;
	}

	template<typename T>
	inline T Property<T>::GetValue(uint32_t frame) const
	{
		const int64_t current = FindKey(frame);
		if (current == -1)
			return T();

		const int64_t next = current + 1;
		if (next == Keys.size())
			return Keys[current].Value;

		const auto& currKey = Keys[current];
		const auto& nextKey = Keys[next];

		if constexpr (
			   std::is_same_v<T, glm::mat4>
			|| std::is_same_v<T, glm::vec4>
			|| std::is_same_v<T, glm::vec3>
			|| std::is_same_v<T, glm::vec2>
			|| std::is_same_v<T, float>)
		{
			const uint32_t length = nextKey.Frame - currKey.Frame;
			const uint32_t passed = frame - currKey.Frame;
			return glm::lerp(currKey.Value, nextKey.Value, (float)passed / (float)length);
		}
		else if constexpr (std::is_integral_v<T>)
		{
			const uint32_t length = nextKey.Frame - currKey.Frame;
			const uint32_t passed = frame - currKey.Frame;
			return currKey.Value + (nextKey.Value - currKey.Value) * (float)passed / (float)length;
		}
		else
		{
			return currKey.Value;
		}
	}
	
	template<typename T>
	template<typename ComponentType, uint16_t valIndex>
	inline void Property<T>::Setup()
	{
		m_GetPropertyReference.Connect<Property<T>::getReference<ComponentType, valIndex>>();
		m_ValueIndex = valIndex;
		m_ComponentID = Component<ComponentType>::ID();
		m_ComponentName = Reflection<ComponentType>::sc_ClassName;
		m_ValueName = std::string(Reflection<ComponentType>::sc_VariableNames[valIndex]);
	}

	template<typename T>
	template<typename ComponentType, uint16_t valIndex>
	inline T* Property<T>::getReference(SceneEntity& entity)
	{
		if (entity.IsValid() && entity.HasComponent<ComponentType>())
			return reinterpret_cast<T*>(&Reflection<ComponentType>::Get<valIndex>(entity.GetComponent<ComponentType>()));
		return nullptr;
	}

}