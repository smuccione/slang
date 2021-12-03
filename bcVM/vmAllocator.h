#pragma once
 
template <typename T>
class vmAllocator : public std::allocator<T>
{
	public:

	vmInstance   *instance = 0;
	size_t		  age = 0;

	using value_type = T;

	template<typename _Tp1>
	struct rebind
	{
		typedef vmAllocator<_Tp1> other;
	};

	T *allocate ( size_t n, const void *hint = 0 )
	{
		return (T*)instance->om->allocGen ( n * sizeof ( T ),  age );
	}

	void set ( size_t age )
	{
		this->age = age;
	}

	void deallocate ( T *p, size_t n )
	{
		// no deallocation
	}

	vmAllocator ( vmInstance *instance, size_t age ) noexcept : instance ( instance ), age ( age )
	{}

	vmAllocator ( const vmAllocator &right ) noexcept : instance ( right.instance ), age ( right.age )
	{
	}

	vmAllocator ( vmAllocator &&right ) noexcept
	{
		*this = std::move ( right );
	}

	template <typename U>
	vmAllocator ( const vmAllocator<U> &right ) noexcept
	{
		instance = right.instance;
		age = right.age;
	}

	vmAllocator &operator = ( const vmAllocator &right ) noexcept
	{
		instance = right.instance;
		age = right.age;
		return *this;
	}

	vmAllocator &operator = ( vmAllocator &&right ) noexcept
	{
		std::swap ( instance, right.instance );
		std::swap ( age, right.age );
		return *this;
	}

	~vmAllocator ()
	{}
};

template <typename T, typename U>
inline bool operator == ( const vmAllocator<T>&, const vmAllocator<U>& )
{
	return true;
}

template <typename T, typename U>
inline bool operator != ( const vmAllocator<T>&, const vmAllocator<U>& )
{
	return false;
}


