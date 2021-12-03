#pragma once

#include "Utility/settings.h"

#include <mutex>
#include <queue>
#include <set>

template <typename T>
class unique_queue : private std::queue<T>
{
	std::set<T>		s;
	std::mutex		protect;

public:
	unique_queue ()
	{
	}

	unique_queue ( unique_queue const & ) = delete;
	unique_queue ( unique_queue && ) = delete;

	void push ( T const &val )
	{
		std::lock_guard	g1 ( protect );

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
			return res;
		}
		return T ();
	}

	bool empty ()
	{
		std::lock_guard	g1 ( protect );

		return std::queue<T>::empty ();
	}
};