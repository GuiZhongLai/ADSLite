#include "AmsRouter.h"
#include "AmsAddr.h"
#include "AmsNetId.h"

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
	 */
	auto hostAddresses = bhf::ads::GetListOfAddresses(host, "48898");
	std::unique_lock<std::recursive_mutex> lock(mutex);
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
			mapping[ams] = conn.get();
			return 0;
		}
	}

	connection_attempts[ams] = {};
	lock.unlock();

	try
	{
		auto new_connection = std::unique_ptr<AmsConnection>(new AmsConnection{*this, hostAddresses.get()});
		lock.lock();
		connection_attempts.erase(ams);
		connection_attempt_events.notify_all();

		auto conn = connections.emplace(std::move(new_connection));
		if (conn.second)
		{
			/** 如果之前没有设置本地AmsNetId，则创建一个 */
			if (AmsNetIdHelper::isEmpty(localAddr))
			{
				localAddr = AmsNetIdHelper::create(conn.first->get()->ownIp);
			}
			conn.first->get()->refCount++;
			mapping[ams] = conn.first->get();
			return !conn.first->get()->ownIp;
		}

		return -1;
	}
	catch (std::exception &e)
	{
		lock.lock();
		connection_attempts.erase(ams);
		connection_attempt_events.notify_all();
		throw e;
	}
}

void AmsRouter::DelRoute(const AmsNetId &ams)
{
	std::unique_lock<std::recursive_mutex> lock(mutex);

	AwaitConnectionAttempts(ams, lock);

	auto route = mapping.find(ams);
	if (route != mapping.end())
	{
		AmsConnection *conn = route->second;
		if (0 == --conn->refCount)
		{
			mapping.erase(route);
			DeleteIfLastConnection(conn);
		}
	}
}

AmsConnection *AmsRouter::GetConnection(const AmsNetId &amsDest)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);
	const auto it = mapping.find(amsDest);
	if (it != mapping.end())
	{
		return it->second;
	}
	return nullptr;
}

long AmsRouter::AdsRequest(AmsRequest &request)
{
	if (request.bytesRead)
	{
		*request.bytesRead = 0;
	}

	auto ads = GetConnection(request.destAddr.netId);
	if (!ads)
	{
		return GLOBALERR_MISSING_ROUTE;
	}
	return ads->AdsRequest(request, ports[request.port - Router::PORT_BASE].tmms);
}

void AmsRouter::AwaitConnectionAttempts(const AmsNetId &ams, std::unique_lock<std::recursive_mutex> &lock)
{
	connection_attempt_events.wait(lock, [&]()
								   { return connection_attempts.find(ams) == connection_attempts.end(); });
}

void AmsRouter::DeleteIfLastConnection(const AmsConnection *conn)
{
	if (conn)
	{
		for (const auto &r : mapping)
		{
			if (r.second == conn)
			{
				return;
			}
		}
		for (auto it = connections.begin(); it != connections.end(); ++it)
		{
			if (conn == it->get())
			{
				connections.erase(it);
				return;
			}
		}
	}
}
