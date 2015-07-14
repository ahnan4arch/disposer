//-----------------------------------------------------------------------------
// Copyright (c) 2015 Benjamin Buch
//
// https://github.com/bebuch/disposer
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)
//-----------------------------------------------------------------------------
#ifndef _disposer_log_tag_hpp_INCLUDED_
#define _disposer_log_tag_hpp_INCLUDED_

#include "time_to_string.hpp"

#include <boost/type_index.hpp>


namespace disposer{


	class log_tag{
	public:
		log_tag():
			start_(std::chrono::system_clock::now()),
			body_(false),
			exception_(false)
			{}

		void pre(){
			auto end = std::chrono::system_clock::now();

			time_to_string(os_, start_);

			if(body_){
				os_ << " ( " << std::setfill(' ') << std::setprecision(3) << std::setw(12) << std::chrono::duration< double, std::milli >(end - start_).count() << "ms ) ";
			}else{
				os_ << " ( no content     ) ";
			}
		}

		void post(){
			if(exception_) os_ << " (failed)";
			os_ << exception_text_ << std::endl;
		}

		void failed(){
			exception_ = true;
		}

		void set_exception(std::exception const& error){
			exception_text_ = " (exception catched: [" + boost::typeindex::type_id_runtime(error).pretty_name() + "] " + error.what() + ")";
		}

		void unknown_exception(){
			exception_text_ = " (unknown exception catched)";
		}

		void have_body(){
			body_ = true;
		}

		void exec()const{
			std::clog << os_.str();
		}

		template < typename T >
		friend log_tag& operator<<(log_tag& log, T&& data){
			log.os_ << static_cast< T&& >(data);
			return log;
		}

	protected:
		std::ostringstream os_;
		std::chrono::system_clock::time_point const start_;
		std::string exception_text_;
		bool body_;
		bool exception_;
	};


}

#endif
