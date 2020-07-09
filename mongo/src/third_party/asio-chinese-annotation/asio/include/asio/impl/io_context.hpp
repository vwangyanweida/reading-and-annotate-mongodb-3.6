//
// impl/io_context.hpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_IMPL_IO_CONTEXT_HPP
#define ASIO_IMPL_IO_CONTEXT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/completion_handler.hpp"
#include "asio/detail/executor_op.hpp"
#include "asio/detail/fenced_block.hpp"
#include "asio/detail/handler_type_requirements.hpp"
#include "asio/detail/recycling_allocator.hpp"
#include "asio/detail/service_registry.hpp"
#include "asio/detail/throw_error.hpp"
#include "asio/detail/type_traits.hpp"

#include "asio/detail/push_options.hpp"

namespace asio {

template <typename Service>
inline Service& use_service(io_context& ioc)
{
  // Check that Service meets the necessary type requirements.
  (void)static_cast<execution_context::service*>(static_cast<Service*>(0));
  (void)static_cast<const execution_context::id*>(&Service::id);

  return ioc.service_registry_->template use_service<Service>(ioc);
}

template <>
inline detail::io_context_impl& use_service<detail::io_context_impl>(
    io_context& ioc)
{
  return ioc.impl_;
}

} // namespace asio

#include "asio/detail/pop_options.hpp"

#if defined(ASIO_HAS_IOCP)
# include "asio/detail/win_iocp_io_context.hpp"
#else
# include "asio/detail/scheduler.hpp"
#endif

#include "asio/detail/push_options.hpp"

namespace asio {

inline io_context::executor_type
io_context::get_executor() ASIO_NOEXCEPT
{
  return executor_type(*this);
}

#if defined(ASIO_HAS_CHRONO)

//mongo::transport::ServiceExecutorAdaptive::_workerThreadRoutine�����ߵ�����
template <typename Rep, typename Period>
std::size_t io_context::run_for(
    const chrono::duration<Rep, Period>& rel_time)
{
  return this->run_until(chrono::steady_clock::now() + rel_time);
}

template <typename Clock, typename Duration>
//mongo::transport::ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_for
std::size_t io_context::run_until(
    const chrono::time_point<Clock, Duration>& abs_time)
{
  std::size_t n = 0;
  while (this->run_one_until(abs_time))
    if (n != (std::numeric_limits<std::size_t>::max)())
      ++n;
  return n;
}

template <typename Rep, typename Period>
//mongodb��ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_one_for->
std::size_t io_context::run_one_for(
    const chrono::duration<Rep, Period>& rel_time)
{
  return this->run_one_until(chrono::steady_clock::now() + rel_time);
}

template <typename Clock, typename Duration>
//mongo::transport::ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_for->io_context::run_one_until

//mongodb��ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_one_for->io_context::run_one_until
//->schedule::wait_one
//��������schedule::wait_one��ʱ���������abs_time���ʱ���
std::size_t io_context::run_one_until(
    const chrono::time_point<Clock, Duration>& abs_time) //abs_time��ʾ���иú������ڵľ����¼���
{
  typename Clock::time_point now = Clock::now();
  while (now < abs_time)
  {
    //�ڱ�ѭ���л���Ҫ���ж��
    typename Clock::duration rel_time = abs_time - now;
	//���һ��ѭ��ִ��1s��Ҳ����schedule::wait_one��൥��wait 1s
    if (rel_time > chrono::seconds(1))
      rel_time = chrono::seconds(1);

    asio::error_code ec;
	//schedule::wait_one �Ӳ��������л�ȡ��Ӧ��operationִ�У������ȡ��operation��ִ�гɹ��򷵻�1�����򷵻�0
    std::size_t s = impl_.wait_one(
        static_cast<long>(chrono::duration_cast<
          chrono::microseconds>(rel_time).count()), ec);
    asio::detail::throw_error(ec);

	//�Ѿ���ȡִ����һ��operation����ֱ�ӷ��أ��������ѭ������schedule::wait_one��ȡoperationִ��
    if (s || impl_.stopped())
      return s;

	//���¼��㵱ǰʱ��
    now = Clock::now();
  }

  return 0;
}

#endif // defined(ASIO_HAS_CHRONO)

#if !defined(ASIO_NO_DEPRECATED)

inline void io_context::reset()
{
  restart();
}

//mongodb��ServiceExecutorAdaptive::schedule����->io_context::dispatch(ASIO_MOVE_ARG(CompletionHandler) handler)
template <typename CompletionHandler>
ASIO_INITFN_RESULT_TYPE(CompletionHandler, void ())
io_context::dispatch(ASIO_MOVE_ARG(CompletionHandler) handler)
{
  // If you get an error on the following line it means that your handler does
  // not meet the documented type requirements for a CompletionHandler.
  ASIO_COMPLETION_HANDLER_CHECK(CompletionHandler, handler) type_check;

  async_completion<CompletionHandler, void ()> init(handler);

  //scheduler::can_dispatch  ���߳��Ѿ����뵽�˹����̶߳��У�ֱ��ִ��handler
  if (impl_.can_dispatch()) //���̼߳���ִ�и�handler
  {
    detail::fenced_block b(detail::fenced_block::full);
	//ֱ������handler
    asio_handler_invoke_helpers::invoke(
        init.completion_handler, init.completion_handler);
  }
  else
  {
    // Allocate and construct an operation to wrap the handler.
    //����complete handler,completion_handler�̳л���operation 
    typedef detail::completion_handler<
      typename handler_type<CompletionHandler, void ()>::type> op;
    typename op::ptr p = { detail::addressof(init.completion_handler),
      op::ptr::allocate(init.completion_handler), 0 };
    p.p = new (p.v) op(init.completion_handler); //��ȡop������Ҳ����handler�ص�

    ASIO_HANDLER_CREATION((*this, *p.p,
          "io_context", this, 0, "dispatch"));

	
	//mongodb��ServiceExecutorAdaptive::schedule����->io_context::dispatch(ASIO_MOVE_ARG(CompletionHandler) handler)
	//->scheduler::do_dispatch
    impl_.do_dispatch(p.p); //scheduler::do_dispatch op���
    p.v = p.p = 0;
  }

  return init.result.get();
}

template <typename CompletionHandler>
ASIO_INITFN_RESULT_TYPE(CompletionHandler, void ())
/*
//accept��Ӧ��״̬�������������
//TransportLayerASIO::_acceptConnection->basic_socket_acceptor::async_accept
//->start_accept_op->epoll_reactor::post_immediate_completion

//��ͨread write��Ӧ��״̬�������������
//mongodb��ServiceExecutorAdaptive::schedule����->io_context::post(ASIO_MOVE_ARG(CompletionHandler) handler)
//->scheduler::post_immediate_completion
//mongodb��ServiceExecutorAdaptive::schedule����->io_context::dispatch(ASIO_MOVE_ARG(CompletionHandler) handler)
//->scheduler::do_dispatch

//��ͨ��дread write��Ӧ��״̬�������������
//ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_for->scheduler::wait_one
//->scheduler::do_wait_one����
//mongodb��ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_one_for
//->io_context::run_one_until->schedule::wait_one
		|
		|1.�Ƚ���״̬���������(Ҳ����mongodb��TransportLayerASIO._workerIOContext	TransportLayerASIO._acceptorIOContext��ص�����)
		|2.��ִ�в���1��Ӧ����������������յ���TransportLayerASIO::_acceptConnection��TransportLayerASIO::ASIOSourceTicket::fillImpl��
		|  TransportLayerASIO::ASIOSinkTicket::fillImpl���������Ӵ��������ݶ�д�¼�epollע��(�����ͷ����)�����Ӧ�ص�
		|
		\|/
//accept��Ӧ��������epoll�¼�ע������:reactive_socket_service_base::start_accept_op->reactive_socket_service_base::start_op
//������epoll�¼�ע������:reactive_descriptor_service::async_read_some->reactive_descriptor_service::start_op->epoll_reactor::start_op
//д����epoll�¼�ע������:reactive_descriptor_service::async_write_some->reactive_descriptor_service::start_op->epoll_reactor::start_op
*/

//reactive_socket_service_base::start_accept_op	
//mongodb accept�첽������������: 
//TransportLayerASIO::_acceptConnection->basic_socket_acceptor::async_accept->reactive_socket_service::async_accept(���ﹹ��reactive_socket_accept_op_base��������epoll��ȡ�����ӵ�handler�ص�Ҳ�������do_complete��ִ��)
//->reactive_socket_service_base::start_accept_op->reactive_socket_service_base::start_op�н���acceptע��

//mongodb�첽��ȡ����:
 //mongodbͨ��TransportLayerASIO::ASIOSession::opportunisticRead->asio::async_read->start_read_buffer_sequence_op->read_op::operator
 //->basic_stream_socket::async_read_some->reactive_socket_service_base::async_receive(���ﹹ��reactive_socket_recv_op��������epoll�����ݼ����ȡ��һ������mongo���ĵ�handler�ص�Ҳ�������do_complete��ִ��)
 //->reactive_socket_service_base::start_op�н���EPOLL�¼�ע��
//mongodbͬ����ȡ����:
 //mongodb��opportunisticRead->asio:read->detail::read_buffer_sequence->basic_stream_socket::read_some->basic_stream_socket::read_some
 //reactive_socket_service_base::receive->socket_ops::sync_recv(����ֱ�Ӷ�ȡ����)

//write�����첽��������: 
 //mongodb��ͨ��opportunisticWrite->asio::async_write->start_write_buffer_sequence_op->detail::write_op() 
 //->basic_stream_socket::async_write_some->reactive_socket_service_base::async_send(���ﹹ��reactive_socket_send_op_base��������epollд���ݼ����ȡ��һ������mongo���ĵ�handler�ص�Ҳ�������do_complete��ִ��)
 //->reactive_socket_service_base::start_op->reactive_socket_service_base::start_op�н���EPOLL�¼�ע��
//writeͬ��������������:
 //ͬ��д����asio::write->write_buffer_sequence->basic_stream_socket::write_some->reactive_socket_service_base::send->socket_ops::sync_send(������������ͬ������)



//mongodb��ServiceExecutorAdaptive::schedule����->io_context::post(ASIO_MOVE_ARG(CompletionHandler) handler)
//->scheduler::post_immediate_completion
io_context::post(ASIO_MOVE_ARG(CompletionHandler) handler)
{
  // If you get an error on the following line it means that your handler does
  // not meet the documented type requirements for a CompletionHandler.
  ASIO_COMPLETION_HANDLER_CHECK(CompletionHandler, handler) type_check;

  async_completion<CompletionHandler, void ()> init(handler);

  bool is_continuation =   //mongodb  asio::async_read(), asio::async_write(), asio::async_connect(), Ĭ�Ϸ���true
    asio_handler_cont_helpers::is_continuation(init.completion_handler);

  // Allocate and construct an operation to wrap the handler.
  //����complete handler,completion_handler�̳л���operation 
  typedef detail::completion_handler<
    typename handler_type<CompletionHandler, void ()>::type> op;
  typename op::ptr p = { detail::addressof(init.completion_handler),
      op::ptr::allocate(init.completion_handler), 0 };
  p.p = new (p.v) op(init.completion_handler);

  ASIO_HANDLER_CREATION((*this, *p.p,
        "io_context", this, 0, "post"));

  //scheduler::post_immediate_completion
  impl_.post_immediate_completion(p.p, is_continuation);
  p.v = p.p = 0;

  return init.result.get();
}

template <typename Handler>
#if defined(GENERATING_DOCUMENTATION)
unspecified
#else
inline detail::wrapped_handler<io_context&, Handler>
#endif
io_context::wrap(Handler handler)
{
  return detail::wrapped_handler<io_context&, Handler>(*this, handler);
}

#endif // !defined(ASIO_NO_DEPRECATED)

inline io_context&
io_context::executor_type::context() const ASIO_NOEXCEPT
{
  return io_context_;
}

inline void
io_context::executor_type::on_work_started() const ASIO_NOEXCEPT
{
  io_context_.impl_.work_started();//scheduler::work_started
}

inline void
io_context::executor_type::on_work_finished() const ASIO_NOEXCEPT
{
  io_context_.impl_.work_finished(); //scheduler::work_finished
}

template <typename Function, typename Allocator>
void io_context::executor_type::dispatch(
    ASIO_MOVE_ARG(Function) f, const Allocator& a) const
{
  typedef typename decay<Function>::type function_type;

  // Invoke immediately if we are already inside the thread pool.
  if (io_context_.impl_.can_dispatch())
  {
    // Make a local, non-const copy of the function.
    function_type tmp(ASIO_MOVE_CAST(Function)(f));

    detail::fenced_block b(detail::fenced_block::full);
    asio_handler_invoke_helpers::invoke(tmp, tmp);
    return;
  }

  // Allocate and construct an operation to wrap the function.
  typedef detail::executor_op<function_type, Allocator, detail::operation> op;
  typename op::ptr p = { detail::addressof(a), op::ptr::allocate(a), 0 };
  p.p = new (p.v) op(ASIO_MOVE_CAST(Function)(f), a);

  ASIO_HANDLER_CREATION((this->context(), *p.p,
        "io_context", &this->context(), 0, "post"));

  io_context_.impl_.post_immediate_completion(p.p, false);
  p.v = p.p = 0;
}

template <typename Function, typename Allocator>
void io_context::executor_type::post(
    ASIO_MOVE_ARG(Function) f, const Allocator& a) const
{
  typedef typename decay<Function>::type function_type;

  // Allocate and construct an operation to wrap the function.
  typedef detail::executor_op<function_type, Allocator, detail::operation> op;
  typename op::ptr p = { detail::addressof(a), op::ptr::allocate(a), 0 };
  p.p = new (p.v) op(ASIO_MOVE_CAST(Function)(f), a);

  ASIO_HANDLER_CREATION((this->context(), *p.p,
        "io_context", &this->context(), 0, "post"));

  io_context_.impl_.post_immediate_completion(p.p, false);
  p.v = p.p = 0;
}

template <typename Function, typename Allocator>
void io_context::executor_type::defer(
    ASIO_MOVE_ARG(Function) f, const Allocator& a) const
{
  typedef typename decay<Function>::type function_type;

  // Allocate and construct an operation to wrap the function.
  typedef detail::executor_op<function_type, Allocator, detail::operation> op;
  typename op::ptr p = { detail::addressof(a), op::ptr::allocate(a), 0 };
  p.p = new (p.v) op(ASIO_MOVE_CAST(Function)(f), a);

  ASIO_HANDLER_CREATION((this->context(), *p.p,
        "io_context", &this->context(), 0, "defer"));

  io_context_.impl_.post_immediate_completion(p.p, true);
  p.v = p.p = 0;
}

inline bool
io_context::executor_type::running_in_this_thread() const ASIO_NOEXCEPT
{
  return io_context_.impl_.can_dispatch();
}

#if !defined(ASIO_NO_DEPRECATED)
//mongodb�е�TransportLayerASIO::start()���ã���ʾlistener�̴߳���io_context�����accept�¼�
inline io_context::work::work(asio::io_context& io_context)
  : io_context_impl_(io_context.impl_)
{
  io_context_impl_.work_started();
}

inline io_context::work::work(const work& other)
  : io_context_impl_(other.io_context_impl_)
{
  io_context_impl_.work_started();
}

inline io_context::work::~work()
{//scheduler::post_immediate_completion
  io_context_impl_.work_finished();
}

inline asio::io_context& io_context::work::get_io_context()
{
  return static_cast<asio::io_context&>(io_context_impl_.context());
}

inline asio::io_context& io_context::work::get_io_service()
{
  return static_cast<asio::io_context&>(io_context_impl_.context());
}
#endif // !defined(ASIO_NO_DEPRECATED)

inline asio::io_context& io_context::service::get_io_context()
{
  return static_cast<asio::io_context&>(context());
}

#if !defined(ASIO_NO_DEPRECATED)
inline asio::io_context& io_context::service::get_io_service()
{
  return static_cast<asio::io_context&>(context());
}
#endif // !defined(ASIO_NO_DEPRECATED)

} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_IMPL_IO_CONTEXT_HPP