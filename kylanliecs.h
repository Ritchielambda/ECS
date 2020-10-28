#pragma once
#pragma once
#include<unordered_map>
#include <functional>
#include<vector>
#include<algorithm>
#include<stdint.h>
#include<type_traits>


/////////////////////////////////////////////////
//masco about tick type
#ifndef ECS_TICK_TYPE
#define ECS_TICK_TYPE ECS::DefaultTickData
#endif

#ifndef ECS_ALLOCATOR_TYPE
#define ECS_ALLOCATOR_TYPE std::allocator<ECS::Entity>
#endif


#ifndef ECS_NO_RTTI




#include<typeindex>
#include<typeinfo>
#define  ECS_TYPE_IMPLEMENTATION

#else
#define ECS_TYPE_IMPLEMENTATION\
	ECS::TypeIndex ECS::Internal::TypeRegistry::nextIndex = 1;\
	\
	ECS_DEFINE_TYPE(ECS::Events::OnEntityCreated);\
	ECS_DEFINE_TYPE(ECS::Events::OnEntityDestroyed);\

//////////////////////////////////////////////////////////////////////////
// CODE //
//////////////////////////////////////////////////////////////////////////
#endif // !ECS_NO_RTTI

namespace ECS
{
#ifndef ECS_NO_RTTI
	typedef std::type_index TypeIndex;
#define ECS_DECLARE_TYPE
#define ECS_DEFINE_TYPE(name)
	template<typename T>
	TypeIndex getTypeIndex()
	{
		return std::type_index(typeid(T));
	}
#else
	typedef uint32_t TypeIndex;
	namespace Internal
	{
		class TypeRegistry
		{
		public:
			TypeRegistry()
			{
				index = nextIndex;
				++nextIndex;
			}
			TypeIndex getIndex() const
			{
				return index;
			}
		private:
			static TypeIndex nextIndex;
			TypeIndex index;



		};
	}

#define ECS_DECLATE_TYPE public: static ECS::Internal::TypeRegistry __ecs_type_reg
#define ECS_DEFINE_TYPE(name) ECS::Internal::TypeRegistry name::__ecs__type_reg

#endif


	class World;
	class Entity;

	typedef float DefaultTickData;
	typedef ECS_ALLOCATOR_TYPE Allocator;

	namespace Internal
	{
		template<typename... Types>
		class EntityComponentView;

		class EntityView;

		struct BaseComponentContainer
		{
		public:
			virtual ~BaseComponentContainer() {}

			virtual void destroy(World * world) = 0;

			virtual void removed(Entity* ent) = 0;

		};
		class BaseEventSubscriber
		{
		public:
			virtual ~BaseEventSubscriber() {};
		};

		template<typename... Types>
		class EntityComponentIterator
		{
		public:
			EntityComponentIterator(World * world, size_t index, bool bIsEnd, bool bIncludePendingDestroy);

			size_t getIndex() const {
				return index;
			}
			bool isEnd() const;
			bool includePendingDestroy() const
			{
				return bIncludePendingDestroy;
			}
			World* getWorld() const {
				return world;
			}
			Entity* get() const;

			Entity* operator*() const {
				return get();
			}

			bool operator==(const EntityComponentIterator<Types...>& other) const
			{
				if (world != other.world)
					return false;

				if (isEnd())
					return other.isEnd();

				return index == other.index;
			}

			bool operator!=(const EntityComponentIterator<Types...>& other) const {
				if (world != other.world)
					return true;
				if (isEnd())
					return !other.isEnd();
				return index != other.index;

			}
			EntityComponentIterator<Type...>& operator ++();

		private:
			bool bIsEnd = false;
			size_t index;
			class ECS::World* world;
			bool bIncludePendingDestroy;
		};

		template<typename...Types>
		class EntityComponentView
		{
		public:
			EntityComponentView(const EntityComponentIterator<Types...>& first, const EntityComponentIterator<Types...>& last);

			const EntityComponentIterator(Types...) & begin() const
			{
				return firstItr;

			}
			const EntityComponentIterator(Types...) & end() const
			{
				return lastItr;
			}
		private:
			EntityComponentIterator<Types...> firstItr;
			EntityComponentIterator<Types...> lastItr;
		};
	}


	template<typename T>
	class ComponentHandle
	{
	public:
		ComponentHandle()
			:component(nullptr)
		{
		}
		T* operator->() const
		{
			return component;
		}
		operator bool() const
		{
			return isValid();
		}
		T& get()
		{
			return component != nullptr;
		}
	private:
		T* component;
	};

	class EntitySystem
	{
	public:
		virtual ~EntitySystem() {}

		virtual void configure(World * world)
		{

		}
		virtual void unconfigure(World* world)
		{

		}
#ifdef ECS_TICK_TYPE_VOID
		virtual void tick(World * world)
#else
		virtual void tick(World *world, ECS_TICK_TYPE data)
#endif
		{

		}
	};

	template<typename T>
	class EventSubscriber :public Internal::BaseEventSubscriber
	{
	public:
		virtual ~EventSubscriber() {}

		virtual void receive(World* world, const T& event) = 0;
	};

	namespace Events
	{
		struct OnEntityCreated
		{
			ECS_DECLARE_TYPE;
			Entity* entity;
		};

		struct OnEntityDestroyed
		{
			ECS_DECLARE_TYPE;

			Entity *entity;
		};

		template<typename T>
		struct OnComponentAssigned
		{
			ECS_DECLARE_TYPE;
			Entity* entity;
			ComponentHandle<T> component;
		};

		template<typename T>
		struct OnComponentRemoved
		{
			ECS_DECLARE_TYPE;
			Entity* entity;
			ComponentHandle<T> component;
		};

#ifdef ECS_NO_RTTI

		template<typename T>
		ECS_DEFINE_TYPE(ECS::Events::OnComponentAssigned<T>);

		template<typename T>
		ECS_DEFINE_TYPE(ECS::Events::OnComponentRemoved<T>);
#endif
	}


	class Entity
	{
	public:
		friend class World;

		const static size_t InvalidEntityId = 0;


		Entity(World* world, size_t id)
			:world(world), id(id)
		{
		}
		~Entity()
		{
			removeAll();
		}
		World* getWorld() const
		{
			return world;

		}
		template<typename T>
		bool has() const
		{
			auto index = getTypeIndex<T>();
			return components.find(index) != components.end();
		}


		template<typename T, typename V, typename...Types>
		bool has() const
		{
			return has<T>() && has<V, Types..>();
		}


		template<typename T, typename...Args>
		ComponentHandle<T> assign(Args&&... args);

		template<typename T>
		bool remove()
		{
			auto found = components.find(getTypeIndex<T>());
			if (found != components.end())
			{
				// to check
				found->second->removed(this);
				found->second->bPendigDestroy(world);
				components.erase(found);
				return true;
			}
			return false;
		}

		void removeAll()
		{
			for (auto pair : components)
			{
				pair.second->removed(this);
				pair.second->destroy(world);
			}
			components.clear();

		}

		template<typename T>
		ComponentHandle<T> get();

		template<typename...Types>
		bool with(typename std::common_type<std::function<void(ComponentHandle<Types>...)>>::type view)
		{
			if (!has<Types...>())
				return false;
			view(get<Types>...)// to check
				return true;
		}

		size_t getEntityId()const
		{
			return id;
		}
		bool isPendingDestroy() const
		{
			return bPendingDestroy;
		}
	private:
		std::unordered_map<TypeIndex, Internal::BaseComponentContainer*> components;
		World* world;

		size_t id;
		bool bPendingDestroy = false;
	};

	class World
	{

	};


}













