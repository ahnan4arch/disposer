//-----------------------------------------------------------------------------
// Copyright (c) 2015-2017 Benjamin Buch
//
// https://github.com/bebuch/disposer
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)
//-----------------------------------------------------------------------------
#include <disposer/chain.hpp>
#include <disposer/module_base.hpp>
#include <disposer/create_chain_modules.hpp>

#include <numeric>


namespace disposer{


	chain::chain(
		module_maker_list const& maker_list,
		types::merge::chain const& config_chain,
		id_generator& generate_id,
		std::string const& group
	):
		name(config_chain.name),
		group(group),
		modules_(create_chain_modules(maker_list, config_chain)),
		id_increase_(std::accumulate(
			modules_.cbegin(),
			modules_.cend(),
			std::size_t(1),
			[](std::size_t increase, module_ptr const& module){
				return increase * module->id_increase;
			}
		)),
		generate_id_(generate_id),
		next_run_(0),
		ready_run_(modules_.size()),
		module_mutexes_(modules_.size()),
		enabled_(false),
		exec_calls_count_(0)
		{}


	chain::~chain(){
		disable();
	}


	namespace{


		class exec_call_manager{
		public:
			exec_call_manager(
				std::atomic< std::size_t >& exec_calls_count,
				std::condition_variable& enable_cv
			):
				exec_calls_count_(exec_calls_count),
				enable_cv_(enable_cv){
					++exec_calls_count_;
				}

			~exec_call_manager(){
				--exec_calls_count_;
				enable_cv_.notify_all();
			}

		private:
			std::atomic< std::size_t >& exec_calls_count_;
			std::condition_variable& enable_cv_;
		};



	}


	void chain::exec(){
		if(!enabled_){
			throw std::logic_error("chain '" + name + "' is not enabled");
		}

		exec_call_manager lock(exec_calls_count_, enable_cv_);

		// generate a new id for the exec
		std::size_t const id = generate_id_(id_increase_);

		// generate a unique continuous index for the call
		std::size_t const run = next_run_++;

		// exec any module, call cleanup instead if the module throw
		log([this, id](log_base& os){
			os << "id(" << id << ") chain '" << name << "'";
		}, [this, id, run]{
			try{
				for(std::size_t i = 0; i < modules_.size(); ++i){
					modules_[i]->set_id(chain_key(), id);
					process_module(i, run, [](chain& c, std::size_t i){
						c.modules_[i]->exec(chain_key());
					}, "exec");
				}
			}catch(...){
				// cleanup and unlock all executions
				for(std::size_t i = 0; i < ready_run_.size(); ++i){
					// exec was successful
					if(ready_run_[i] >= run + 1) continue;

					process_module(i, run, [id](chain& c, std::size_t i){
						c.modules_[i]->cleanup(chain_key(), id);
					}, "cleanup");
				}

				// rethrow exception
				throw;
			}
		});
	}


	void chain::enable(){
		std::unique_lock< std::mutex > lock(enable_mutex_);
		if(enabled_) return;

		enable_cv_.wait(lock, [this]{ return exec_calls_count_ == 0; });

		log([this](log_base& os){ os << "chain '" << name << "' enable"; },
			[this]{
				std::size_t i = 0;
				try{
					// enable all modules
					for(; i < modules_.size(); ++i){
						log([this, i](log_base& os){
								os << "chain '" << name << "' module '"
									<< modules_[i]->name << "' enable";
							}, [this, i]{
								modules_[i]->enable(chain_key());
							});
					}
				}catch(...){
					// disable all modules until the one who throw
					for(std::size_t j; j < i; ++j){
						log([this, i, j](log_base& os){
								os << "chain '" << name << "' module '"
									<< modules_[j]->name
									<< "' disable because of exception while "
									<< "enable module '" << modules_[i]->name
									<< "'";
							}, [this, j]{
								modules_[j]->disable(chain_key());
							});
					}

					// rethrow exception
					throw;
				}
			});

		enabled_ = true;
	}


	void chain::disable()noexcept{
		std::unique_lock< std::mutex > lock(enable_mutex_);
		if(!enabled_.exchange(false)) return;

		enable_cv_.wait(lock, [this]{ return exec_calls_count_ == 0; });

		log([this](log_base& os){ os << "chain '" << name << "' disable"; },
			[this]{
				// disable all modules
				for(std::size_t i = 0; i < modules_.size(); ++i){
					log([this, i](log_base& os){
							os << "chain '" << name << "' module '"
								<< modules_[i]->name << "' disable";
						}, [this, i]{
							modules_[i]->disable(chain_key());
						});
				}
			});
	}


	template < typename F >
	void chain::process_module(
		std::size_t const i,
		std::size_t const run,
		F const& action,
		char const* const action_name
	){
		// lock mutex and wait for the previous run to be ready
		std::unique_lock< std::mutex > lock(module_mutexes_[i]);
		module_cv_.wait(lock, [this, i, run]{ return ready_run_[i] == run; });

		// exec or cleanup the module
		log([this, i, action_name](log_base& os){
			os << "id(" << modules_[i]->id << "." << i << ") " << action_name
				<< " chain '" << modules_[i]->chain << "' module '"
				<< modules_[i]->name << "'";
		}, [this, i, &action]{ action(*this, i); });

		// make module ready
		ready_run_[i] = run + 1;
		module_cv_.notify_all();
	}



}
