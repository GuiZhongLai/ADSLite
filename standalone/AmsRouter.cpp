#include "AmsRouter.h"
#include "AmsAddr.h"
#include "AmsNetId.h"

#include <exception>
#include <memory>
#include <new>
#include <string>

AmsRouter::AmsRouter(AmsNetId netId) : localAddr(netId)
{
}

uint16_t AmsRouter::OpenPort()
{
	std::lock_guard<std::recursive_mutex> lock(mutex);

	for (uint16_t i = 0; i < NUM_PORTS_MAX; ++i)
	{
		if (!ports[i].IsOpen())
		{
			return ports[i].Open(PORT_BASE + i);
		}
	}
	return 0;
}

long AmsRouter::ClosePort(uint16_t port)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);
	if ((port < PORT_BASE) || (port >= PORT_BASE + NUM_PORTS_MAX))
	{
		return ADSERR_CLIENT_PORTNOTOPEN;
	}
	ports[port - PORT_BASE].Close();
	return 0;
}

long AmsRouter::GetLocalAddress(uint16_t port, AmsAddr *pAddr)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);
	if ((port < PORT_BASE) || (port >= PORT_BASE + NUM_PORTS_MAX))
	{
		return ADSERR_CLIENT_PORTNOTOPEN;
	}

	if (ports[port - PORT_BASE].IsOpen())
	{
		memcpy(&pAddr->netId, &localAddr, sizeof(localAddr));
		pAddr->port = port;
		return 0;
	}
	return ADSERR_CLIENT_PORTNOTOPEN;
}

void AmsRouter::SetLocalAddress(AmsNetId netId)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);
	localAddr = netId;
}

long AmsRouter::GetTimeout(uint16_t port, uint32_t &timeout)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);
	if ((port < PORT_BASE) || (port >= PORT_BASE + NUM_PORTS_MAX))
	{
		return ADSERR_CLIENT_PORTNOTOPEN;
	}

	timeout = ports[port - PORT_BASE].tmms;
	return 0;
}

long AmsRouter::SetTimeout(uint16_t port, uint32_t timeout)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);
	if ((port < PORT_BASE) || (port >= PORT_BASE + NUM_PORTS_MAX))
	{
		return ADSERR_CLIENT_PORTNOTOPEN;
	}

	ports[port - PORT_BASE].tmms = timeout;
	return 0;
}

long AmsRouter::AddRoute(AmsNetId ams, const std::string &host)
{
	/**
	 * DNS查找非常耗时，所以先做查找，然后再使用结果。
	 * 查找与建连都放在 try 内：失败时清理 connection_attempts 并返回错误码。
	 * 异常不得离开本函数，否则会穿过 C API 并 std::terminate 宿主进程。
	 */
	std::unique_lock<std::recursive_mutex> lock(mutex, std::defer_lock);
	bool attemptTracked = false;
	auto releaseAttempt = [&]()
	{
		if (!attemptTracked)
		{
			return;
		}
		try
		{
			if (!lock.owns_lock())
			{
				lock.lock();
			}
			connection_attempts.erase(ams);
			connection_attempt_events.notify_all();
			attemptTracked = false;
		}
		catch (...)
		{
		}
	};
	struct AttemptGuard
	{
		decltype(releaseAttempt) &release;
		~AttemptGuard() { release(); }
	} attemptGuard{releaseAttempt};

	try
	{
		auto hostAddresses = bhf::ads::GetListOfAddresses(host, "48898");
		lock.lock();
		AwaitConnectionAttempts(ams, lock);
		const auto oldConnection = GetConnection(ams);
		if (oldConnection && !oldConnection->IsConnectedTo(hostAddresses.get()))
		{
			/**
			 * 这个AmsNetId已经有了一条路由，不同的IP。
			 * 必须首先删除旧的路由！
			 */
			return ROUTERERR_PORTALREADYINUSE;
		}

		for (const auto &conn : connections)
		{
			if (conn->IsConnectedTo(hostAddresses.get()))
			{
				conn->refCount++;
				mapping[ams] = conn;
				routeHosts[ams] = host;
				return 0;
			}
		}

		connection_attempts[ams] = {};
		attemptTracked = true;
		lock.unlock();

		/* new 必须写在成员函数里：AmsRouter 私有继承 Router，make_shared 的上下文无法转换。 */
		std::unique_ptr<AmsConnection> owned(new AmsConnection{*this, hostAddresses.get()});
		auto new_connection = std::shared_ptr<AmsConnection>(std::move(owned));
		lock.lock();
		connection_attempts.erase(ams);
		attemptTracked = false;
		connection_attempt_events.notify_all();

		connections.push_back(new_connection);
		AmsConnection *const inserted = connections.back().get();
		/** 如果之前没有设置本地AmsNetId，则创建一个 */
		if (AmsNetIdHelper::isEmpty(localAddr))
		{
			localAddr = AmsNetIdHelper::create(inserted->ownIp);
		}
		inserted->refCount++;
		mapping[ams] = connections.back();
		routeHosts[ams] = host;
		return !inserted->ownIp;
	}
	catch (const std::bad_alloc &)
	{
		return GLOBALERR_NO_MEMORY;
	}
	catch (const std::exception &)
	{
		return GLOBALERR_TARGET_PORT;
	}
	catch (...)
	{
		return GLOBALERR_TARGET_PORT;
	}
}

void AmsRouter::DelRoute(const AmsNetId &ams)
{
	/* Dropped after the lock so ~AmsConnection (join receiver) does not run while mutex is held.
	 * An in-flight AdsRequest keeps its own shared_ptr, so this does not free a live request. */
	std::shared_ptr<AmsConnection> routeConn;
	std::shared_ptr<AmsConnection> deadConn;
	{
		std::unique_lock<std::recursive_mutex> lock(mutex);

		AwaitConnectionAttempts(ams, lock);

		auto route = mapping.find(ams);
		if (route != mapping.end())
		{
			routeConn = route->second;
			routeHosts.erase(ams);
			if (0 == --routeConn->refCount)
			{
				mapping.erase(route);
				deadConn = DeleteIfLastConnection(routeConn.get());
			}
		}
	}
}

std::shared_ptr<AmsConnection> AmsRouter::GetConnection(const AmsNetId &amsDest)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);
	const auto it = mapping.find(amsDest);
	if (it != mapping.end())
	{
		return it->second;
	}
	return {};
}

long AmsRouter::AdsRequest(AmsRequest &request)
{
	if (request.bytesRead)
	{
		*request.bytesRead = 0;
	}

	/**
	 * 整个请求（含重连重试）持有连接。DelRoute / InvalidateRouteConnection
	 * 只摘掉路由映射；只要这里还握着 shared_ptr，连接就不会析构，Write/sendto 不会用到已释放对象。
	 * 重连完成前同时持有旧连接和新连接。
	 */
	const std::shared_ptr<AmsConnection> ads = GetConnection(request.destAddr.netId);
	if (!ads)
	{
		return GLOBALERR_MISSING_ROUTE;
	}

	const auto timeout = ports[request.port - Router::PORT_BASE].tmms;
	const auto status = ads->AdsRequest(request, timeout);
	if (!request.allowReconnectRetry || !IsRecoverableTransportFailure(status))
	{
		return status;
	}

	std::string host;
	try
	{
		{
			std::lock_guard<std::recursive_mutex> lock(mutex);
			const auto hostIt = routeHosts.find(request.destAddr.netId);
			if (hostIt == routeHosts.end())
			{
				return status;
			}
			host = hostIt->second;
		}

		InvalidateRouteConnection(request.destAddr.netId, ads.get());
		const auto reconnectStatus = AddRoute(request.destAddr.netId, host);
		if (reconnectStatus)
		{
			return reconnectStatus;
		}
	}
	catch (const std::bad_alloc &)
	{
		return GLOBALERR_NO_MEMORY;
	}
	catch (const std::exception &)
	{
		return GLOBALERR_TARGET_PORT;
	}
	catch (...)
	{
		return GLOBALERR_TARGET_PORT;
	}

	const std::shared_ptr<AmsConnection> retryConnection = GetConnection(request.destAddr.netId);
	if (!retryConnection)
	{
		return GLOBALERR_MISSING_ROUTE;
	}

	const auto retryStatus = retryConnection->AdsRequest(request, timeout);
	return retryStatus;
}

void AmsRouter::InvalidateRouteConnection(const AmsNetId &ams, const AmsConnection *expectedConnection)
{
	std::shared_ptr<AmsConnection> routeConn;
	std::shared_ptr<AmsConnection> deadConn;
	{
		std::unique_lock<std::recursive_mutex> lock(mutex);
		auto route = mapping.find(ams);
		if (route == mapping.end())
		{
			return;
		}

		routeConn = route->second;
		if (expectedConnection && routeConn.get() != expectedConnection)
		{
			return;
		}

		mapping.erase(route);
		if (routeConn && routeConn->refCount > 0)
		{
			--routeConn->refCount;
		}
		deadConn = DeleteIfLastConnection(routeConn.get());
	}
}

bool AmsRouter::IsRecoverableTransportFailure(long status) const
{
	return status == -1;
}

void AmsRouter::AwaitConnectionAttempts(const AmsNetId &ams, std::unique_lock<std::recursive_mutex> &lock)
{
	connection_attempt_events.wait(lock, [&]()
								   { return connection_attempts.find(ams) == connection_attempts.end(); });
}

std::shared_ptr<AmsConnection> AmsRouter::DeleteIfLastConnection(const AmsConnection *conn)
{
	std::shared_ptr<AmsConnection> dead;
	if (!conn)
	{
		return dead;
	}
	for (const auto &r : mapping)
	{
		if (r.second.get() == conn)
		{
			return dead;
		}
	}
	for (auto it = connections.begin(); it != connections.end(); ++it)
	{
		if (conn == it->get())
		{
			std::shared_ptr<AmsConnection> removed = std::move(*it);
			connections.erase(it);
			return removed;
		}
	}
	return dead;
}
