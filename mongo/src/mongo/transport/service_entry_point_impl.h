/**
 *    Copyright (C) 2017 MongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the GNU Affero General Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#pragma once


#include "mongo/base/disallow_copying.h"
#include "mongo/platform/atomic_word.h"
#include "mongo/stdx/condition_variable.h"
#include "mongo/stdx/list.h"
#include "mongo/stdx/mutex.h"
#include "mongo/transport/service_entry_point.h"
#include "mongo/transport/service_state_machine.h"

namespace mongo {
class ServiceContext;

namespace transport {
class Session;
}  // namespace transport

/**
 * A basic entry point from the TransportLayer into a server.
 *
 * The server logic is implemented inside of handleRequest() by a subclass.
 * startSession() spawns and detaches a new thread for each incoming connection
 * (transport::Session).
 */ 
//ServiceContextMongoD->ServiceContext(包含ServiceEntryPoint成员)
//ServiceEntryPointMongod->ServiceEntryPointImpl->ServiceEntryPoint


//_initAndListen->(serviceContext->setServiceEntryPoint)

//class ServiceEntryPointMongod final : public ServiceEntryPointImpl   ServiceEntryPointMongod::ServiceEntryPointImpl
//ServiceEntryPointMongod和ServiceEntryPointMongos继承该类，记录全局的session链接相关信息
class ServiceEntryPointImpl : public ServiceEntryPoint {
    MONGO_DISALLOW_COPYING(ServiceEntryPointImpl);

public:
    //调整最大可用文件描述符fd，_initAndListen中调用执行
    explicit ServiceEntryPointImpl(ServiceContext* svcCtx);

    void startSession(transport::SessionHandle session) final;

    void endAllSessions(transport::Session::TagMask tags) final;

    bool shutdown(Milliseconds timeout) final;

    Stats sessionStats() const final;

    size_t numOpenSessions() const final {
        return _currentConnections.load();
    }

private:
    //一个新链接对应一个ServiceStateMachine保存到ServiceEntryPointImpl._sessions中
    using SSMList = stdx::list<std::shared_ptr<ServiceStateMachine>>;
    using SSMListIterator = SSMList::iterator;

    //赋值ServiceEntryPointImpl::ServiceEntryPointImpl
    //对应ServiceContextMongoD(mongod)或者ServiceContextNoop(mongos)类
    ServiceContext* const _svcCtx; 
    //该成员变量在代码中没有使用
    AtomicWord<std::size_t> _nWorkers;

    mutable stdx::mutex _sessionsMutex;
    stdx::condition_variable _shutdownCondition;
    //一个新链接对应一个ssm保存到ServiceEntryPointImpl._sessions中
    SSMList _sessions;

    size_t _maxNumConnections{DEFAULT_MAX_CONN};
    //当前的总链接数，不包括关闭的链接
    AtomicWord<size_t> _currentConnections{0};
    //所有的链接，包括已经关闭的链接
    AtomicWord<size_t> _createdConnections{0};
};

}  // namespace mongo
