#pragma once

#include "Utility/settings.h"

#include <mutex>
#include <queue>
#include <set>

template <typename T>
class unique_queue : private std::queue<T>
{
	std::unordered_map<T, bool>	hold;
	std::unordered_set<T>		s;
	std::mutex					protect;

public:
	unique_queue ()
	{
	}

	unique_queue ( unique_queue const & ) = delete;
	unique_queue ( unique_queue && ) = delete;

	void push ( T const &val )
	{
		std::lock_guard	g1 ( protect );

		auto it = hold.find ( val );
		if ( it != hold.end() )
		{
			it->second = true;
			return;
		}

		if ( !s.contains ( val ) )
		{
			s.insert ( val );
			std::queue<T>::push ( val );
		}
	}
	T pop ()
	{
		std::lock_guard	g1 ( protect );

		if ( !std::queue<T>::empty () )
		{
			T res = std::queue<T>::front ();
			s.erase ( std::queue<T>::front () );
			std::queue<T>::pop ();

			hold[res] = false;
			return res;
		}
		return T ();
	}

	void finish ( T const &val )
	{
		std::lock_guard	g1 ( protect );
		auto rescan = hold[val];
		if ( rescan )
		{
			// someone triggered a rescan while we were already scanning so need to do it again.
			s.insert ( val );
			std::queue<T>::push ( val );
		}
		hold.erase ( val );
	}

	bool empty ()
	{
		std::lock_guard	g1 ( protect );

		return std::queue<T>::empty ();
	}

	auto size ()
	{
		std::lock_guard	g1 ( protect );
		return s.size ();
	}
};