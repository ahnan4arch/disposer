//-----------------------------------------------------------------------------
// Copyright (c) 2015-2017 Benjamin Buch
//
// https://github.com/bebuch/disposer
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)
//-----------------------------------------------------------------------------
#ifndef _disposer__log__hpp_INCLUDED_
#define _disposer__log__hpp_INCLUDED_

#include <boost/hana.hpp>

#include <optional>
#include <memory>


namespace disposer{


	namespace detail{ namespace log{


		/// \brief Implementation of extract_log_t
		template < typename Function >
		struct extract_log_from_function{};

		/// \brief Implementation of extract_log_t
		template < typename F, typename R, typename Log >
		struct extract_log_from_function< R(F::*)(Log&) >{
			using type = Log;
		};

		/// \brief Implementation of extract_log_t
		template < typename F, typename R, typename Log >
		struct extract_log_from_function< R(F::*)(Log&)const >{
			using type = Log;
		};

		/// \brief Implementation of extract_log_t
		template < typename F, typename R, typename Log >
		struct extract_log_from_function< R(F::*)(Log&)volatile >{
			using type = Log;
		};

		/// \brief Implementation of extract_log_t
		template < typename F, typename R, typename Log >
		struct extract_log_from_function< R(F::*)(Log&)volatile const >{
			using type = Log;
		};

		/// \brief Extract type of first Function parameter
		template < typename Function >
		struct extract_log{
			using type =
				typename extract_log_from_function<
					decltype(&Function::operator())
				>::type;
		};

		/// \brief Implementation of extract_log_t
		template < typename Function >
		struct extract_log< Function& >{
			using type = typename extract_log< Function >::type;
		};

		/// \brief Implementation of extract_log_t
		template < typename Log, typename R >
		struct extract_log< R(Log&) >{
			using type = Log;
		};

		/// \brief Implementation of extract_log_t
		template < typename Log, typename R >
		struct extract_log< R(*)(Log&) >{
			using type = Log;
		};

		/// \copydoc extract_log
		template < typename Function >
		using extract_log_t = typename extract_log< Function >::type;


		/// \brief Output the log message
		///
		///   1. Call log.pre() if it exists
		///   2. Call the log function: f(log)
		///   3. Call log.post() if it exists
		///   4. Call log.exec()
		template < typename F, typename Log >
		inline void exec_log(F& f, Log& log){
			auto has_pre = boost::hana::is_valid(
				[](auto&& x)->decltype((void)x->pre()){}
			);

			auto has_post = boost::hana::is_valid(
				[](auto&& x)->decltype((void)x->post()){}
			);

			boost::hana::if_(has_pre(log),
				[](auto& log){ log->pre(); },
				[](auto&){}
			)(log);

			f(*log);

			boost::hana::if_(has_post(log),
				[](auto& log){ log->post(); },
				[](auto&){}
			)(log);

			log->exec();
		}


		/// \brief Call the associated code block
		///
		///   - If no exception appears:
		///       1. return with associated code block result
		///   - If an exception appears:
		///       1. Call log.failed() if it exists
		///       2. Call exec_log
		///       3. rethrow the exception
		template < typename F, typename Body, typename Log >
		inline decltype(auto) exec_body(F& f, Body& body, Log& log)try{
			return body();
		}catch(...){
			auto has_failed = boost::hana::is_valid(
				[](auto&& x)->decltype((void)x->failed()){}
			);

			boost::hana::if_(has_failed(log),
				[](auto& log){ log->failed(); },
				[](auto&){}
			)(log);

			exec_log(f, log);

			throw;
		}


		/// \brief Call the associated code block and catch exceptions
		///
		///   - If no exception appears:
		///       1. return with associated code block result as std::optional
		///   - If an exception appears:
		///       1. exception is derived from std::exception
		///           - yes: Call log.set_exception(exception)
		///           - no: Call log.unknown_exception()
		///       2. return with an empty std::optional
		template < typename Body, typename Log >
		inline auto exec_exception_catching_body(Body& body, Log& log){
			constexpr auto is_void = boost::hana::traits::is_void(
				boost::hana::type_c< decltype(body()) >
			);

			try{
				return boost::hana::if_(is_void,
					[](auto&& body){ body(); return true; },
					[](auto&& body){
						return std::optional< decltype(body()) >(body());
					}
				)(static_cast< Body&& >(body));
			}catch(std::exception const& error){
				log->set_exception(error);
			}catch(...){
				log->unknown_exception();
			}

			return boost::hana::if_(is_void,
				[](auto&&){ return false; },
				[](auto&& body){
					return std::optional< decltype(body()) >();
				}
			)(static_cast< Body&& >(body));

		}


		/// \brief Construct a new log object
		///
		///   - Log::factory exists
		///       - yes: construct by calling Log::factory()
		///       - no: construct by calling standard constructor
		template < typename Log >
		auto make_log(){
			auto has_factory = boost::hana::is_valid(
				[](auto t)->decltype((void)decltype(t)::type::factory()){}
			);

			return boost::hana::if_(has_factory(boost::hana::type_c< Log >),
				[](auto t){ return decltype(t)::type::factory(); },
				[](auto t){
					return std::make_unique< typename decltype(t)::type >();
				}
			)(boost::hana::type_c< Log >);
		}

		/// \brief Check if a type has a exec() function
		auto has_exec = boost::hana::is_valid(
			[](auto&& x)->decltype((void)x->exec()){}
		);

		/// \brief Check if a type has a have_body() function
		auto has_have_body = boost::hana::is_valid(
			[](auto&& x)->decltype((void)x->have_body()){}
		);


	} }


	/// \brief Add a log message without associated code block
	///
	/// Usage Example:
	///
	/// \code{.cpp}
	/// log([](your_log_tag_type& os){ os << "your message"; });
	/// \endcode
	template < typename Log >
	inline void log(Log&& f){
		using log_t = detail::log::extract_log_t< Log >;

		auto log = detail::log::make_log< log_t >();

		static_assert(
			detail::log::has_exec(log),
			"In 'log([](Log& os){ ... })' have 'os.exec()' to be a callable "
			"expression."
		);

		detail::log::exec_log(f, log);
	}

	/// \brief Add a log message with associated code block
	///
	/// Usage Example:
	///
	/// \code{.cpp}
	/// int result = log([](your_log_tag_type& os){ os << "your message"; }, []{
	///      // your code
	///      return 5;
	/// });
	/// \endcode
	template < typename Log, typename Body >
	inline decltype(auto) log(Log&& f, Body&& body){
		using log_t = detail::log::extract_log_t< Log >;

		auto log = detail::log::make_log< log_t >();

		static_assert(
			detail::log::has_exec(log),
			"In 'log([](Log& os){ ... }, []{ ... })' have 'os.exec()' to be a "
			"callable expression."
		);

		boost::hana::if_(detail::log::has_have_body(log),
			[](auto& log){ log->have_body(); },
			[](auto&){}
		)(log);

		return boost::hana::if_(
			boost::hana::traits::is_void(
				boost::hana::type_c< decltype(body()) >
			),
			[](auto&& f, auto&& body, auto& log){
				detail::log::exec_body(f, body, log);
				detail::log::exec_log(f, log);
			},
			[](auto&& f, auto&& body, auto& log)->decltype(auto){
				decltype(auto) result = detail::log::exec_body(f, body, log);
				detail::log::exec_log(f, log);
				return result;
			}
		)(static_cast< Log&& >(f), static_cast< Body&& >(body), log);
	}

	/// \brief Catch all exceptions
	///
	/// Call the function and catch all exceptions throwing by the function.
	/// The name is emited via error_log together with the exception message.
	///
	/// As function the usage of a Lambda function is possible, which captures
	/// all variables by reference. ([&]{/* ... */})
	///
	/// If the Lambda function does not return anything, result will be a bool,
	/// indicating with false whether an exception appeared. Otherwise, the
	/// result will be a type that is convertible to bool. If and only if the
	/// conversion becomes true, accessability to the function result using
	/// member-function result() is permitted. Otherwise, result() will throw
	/// a std::logic_error.
	///
	/// Usage Example:
	///
	/// \code{.cpp}
	/// std::optional< int > result = exception_catching_log(
	///     [](your_log_tag_type& os){ os << "your message"; },
	///     []{
	///         // your code
	///         return 5;
	///     });
	/// \endcode
	template < typename Log, typename Body >
	inline auto exception_catching_log(Log&& f, Body&& body){
		using log_t = detail::log::extract_log_t< Log >;

		auto log = detail::log::make_log< log_t >();


		auto has_set_exception = boost::hana::is_valid(
			[](auto&& x)->decltype(
				(void)x->set_exception(std::declval< std::exception >())
			){}
		);

		auto has_unknown_exception = boost::hana::is_valid(
			[](auto&& x)->decltype((void)x->unknown_exception()){}
		);


		static_assert(
			detail::log::has_exec(log),
			"In 'exception_catching_log([](Log& os){ ... }, []{ ... })' have "
			"'os.exec()' to be a callable expression."
		);

		static_assert(
			has_set_exception(log),
			"In 'exception_catching_log([](Log& os){ ... }, []{ ... })' have "
			"'os.set_exception(std::declval< std::exception >())' to be a "
			"callable expression."
		);

		static_assert(
			has_unknown_exception(log),
			"In 'exception_catching_log([](Log& os){ ... }, []{ ... })' have "
			"'os.unknown_exception()' to be a callable expression."
		);


		boost::hana::if_(detail::log::has_have_body(log),
			[](auto& log){ log->have_body(); },
			[](auto&){}
		)(log);

		auto result = detail::log::exec_exception_catching_body(body, log);
		detail::log::exec_log(f, log);
		return result;
	}



}


#endif
