//-----------------------------------------------------------------------------
// Copyright (c) 2015-2017 Benjamin Buch
//
// https://github.com/bebuch/disposer
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)
//-----------------------------------------------------------------------------
#ifndef _disposer__module_base__hpp_INCLUDED_
#define _disposer__module_base__hpp_INCLUDED_

#include "disposer.hpp"
#include "make_data.hpp"
#include "output_base.hpp"
#include "input_base.hpp"
#include "log.hpp"

#include <functional>


namespace disposer{


	/// \brief Class disposer access key
	struct chain_key{
	private:
		/// \brief Constructor
		constexpr chain_key()noexcept = default;
		friend class chain;
	};


	/// \brief Exception class for modules that need input variables
	struct module_not_as_start: std::logic_error{
		module_not_as_start(make_data const& data):
			std::logic_error(
				"module type '" + data.type_name +
				"' can not be used as start of chain '" + data.chain + "'"
			){}
	};


	/// \brief Base class for all disposer modules
	class module_base{
	public:
		/// \brief List of inputs
		using input_list =
			std::vector< std::reference_wrapper< input_base > >;

		/// \brief List of outputs
		using output_list =
			std::vector< std::reference_wrapper< output_base > >;


		/// \brief Constructor with optional outputs
		module_base(
			make_data const& data,
			input_list&& inputs,
			output_list&& outputs = {}
		);

		/// \brief Constructor with optional inputs
		module_base(
			make_data const& data,
			output_list&& outputs,
			input_list&& inputs = {}
		);

		/// \brief Modules are not copyable
		module_base(module_base const&) = delete;

		/// \brief Modules are not movable
		module_base(module_base&&) = delete;


		/// \brief Modules are not copyable
		module_base& operator=(module_base const&) = delete;

		/// \brief Modules are not movable
		module_base& operator=(module_base&&) = delete;


		/// \brief Standard virtual destructor
		virtual ~module_base() = default;


		/// \brief Add a line to the log
		template < typename Log >
		void log(Log&& f)const{
			::disposer::log(module_log(f));
		}

		/// \brief Add a line to the log with linked code block
		template < typename Log, typename Body >
		decltype(auto) log(Log&& f, Body&& body)const{
			return ::disposer::log(module_log(f), static_cast< Body&& >(body));
		}

		/// \brief Add a line to the log with linked code block and catch all
		///        exceptions
		template < typename Log, typename Body >
		decltype(auto) exception_catching_log(Log&& f, Body&& body)const{
			return ::disposer::exception_catching_log(
				module_log(f), static_cast< Body&& >(body));
		}


		/// \brief Set for next exec ID
		void set_id(chain_key, std::size_t id);


		/// \brief Call the actual worker function exec()
		void exec(chain_key){ exec(); }


		/// \brief Call the actual enable() function
		void enable(chain_key){ enable(); }

		/// \brief Call the actual disable() function
		void disable(chain_key)noexcept{ disable(); }


		/// \brief Call input_ready()
		void input_ready(creator_key){ input_ready(); }


		/// \brief Called for a modules wich failed by exception and all
		///        following modules in the chain instead of exec()
		///
		/// Removes all input data whichs ID is less or equal to the actual ID.
		void cleanup(chain_key, std::size_t id)noexcept;


		/// \brief Access to internal inputs_
		input_list& inputs(creator_key){ return inputs_; }

		/// \brief Access to internal outputs_
		output_list& outputs(creator_key){ return outputs_; }


		/// \brief Name of the module type given via class module_declarant
		std::string const type_name;

		/// \brief Name of the process chain in config file section 'chain'
		std::string const chain;

		/// \brief Name of the module in config file section 'module'
		std::string const name;

		/// \brief Position of the module in the process chain
		///
		/// The first module has number 0, the second 1 and so on.
		std::size_t const number;


		/// \brief Number of ID's to output
		///
		/// Most modules process one id by one trigger. Some modules collect
		/// several inputs (with different ID's) to process them together. And
		/// some modules get one input and produce several outputs with
		/// different ID's. With the id_increase you reserve a range of unique
		/// ID's.
		///
		/// Example: id_increase == 4
		///
		/// When your module is triggered with the ID 8, you can put outputs
		/// with the ID's 8, 9, 10 and 11.
		std::size_t const id_increase;

		/// Read only reference to the ID while exec() does run
		std::size_t const& id;


	protected:
		/// \brief The actual worker function called one times per trigger
		virtual void exec() = 0;


		/// \brief Enables the module for exec calls
		///
		/// By default the function does nothing.
		virtual void enable(){}

		/// \brief Disables the module for exec calls
		///
		/// By default the function does nothing.
		virtual void disable()noexcept{}




		/// \brief Called while module creation after the inputs are set
		///
		/// You should enable your outputs in this function.
		virtual void input_ready(){}


	private:
		/// Actual ID while exec() does run
		std::size_t id_;


		/// \brief List of inputs
		input_list inputs_;

		/// \brief List of outputs
		output_list outputs_;


		/// \brief Helper for log message functions
		template < typename Log >
		auto module_log(Log& log)const{
			using log_t = detail::log::extract_log_t< Log >;
			return [&](log_t& os){
				os << "id(" << id << "." << number << ") exec chain '"
					<< chain << "' module '" << name << "': ";
				log(os);
			};
		}
	};


}


#endif
