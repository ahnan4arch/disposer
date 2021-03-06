//-----------------------------------------------------------------------------
// Copyright (c) 2015-2017 Benjamin Buch
//
// https://github.com/bebuch/disposer
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)
//-----------------------------------------------------------------------------
#ifndef _disposer__id_generator__hpp_INCLUDED_
#define _disposer__id_generator__hpp_INCLUDED_

#include <atomic>


namespace disposer{


	/// \brief Generator for unique ID's
	class id_generator{
	public:
		/// \brief Initialize the counter with 0
		id_generator(): next_id(0){}


		/// \brief id_generators are not copyable
		id_generator(id_generator const&) = delete;

		/// \brief id_generators are not movable
		id_generator(id_generator&&) = delete;


		/// \brief id_generators are not copyable
		id_generator& operator=(id_generator const&) = delete;

		/// \brief id_generators are not movable
		id_generator& operator=(id_generator&&) = delete;


		/// \brief Get the next ID and increase the counter by increase
		std::size_t operator()(std::size_t increase){
			return next_id.fetch_add(increase);
		}


	private:
		/// \brief The counter
		std::atomic< std::size_t > next_id;
	};


}


#endif
