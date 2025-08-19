#pragma once

#include "AmsNetId.h"
#include "AmsConnection.h"

#include <unordered_set>

class AmsRouter : Router
{
public:
	AmsRouter(AmsNetId netId = AmsNetId{});

	uint16_t OpenPort();
	long ClosePort(uint16_t port);
	long GetLocalAddress(uint16_t port, AmsAddr *pAddr);
	void SetLocalAddress(AmsNetId netId);
	long GetTimeout(uint16_t port, uint32_t &timeout);
	long SetTimeout(uint16_t port, uint32_t timeout);

	long AddRoute(AmsNetId ams, const std::string &host);
	void DelRoute(const AmsNetId &ams);
	AmsConnection *GetConnection(const AmsNetId &amsDest);
	long AdsRequest(AmsRequest &request);

private:
	AmsNetId localAddr;
	std::recursive_mutex mutex;
	std::condition_variable_any connection_attempt_events;
	std::map<AmsNetId, std::tuple<>> connection_attempts;
	std::unordered_set<std::unique_ptr<AmsConnection>> connections;
	std::map<AmsNetId, AmsConnection *> mapping;

	std::array<AmsPort, NUM_PORTS_MAX> ports;

	void AwaitConnectionAttempts(const AmsNetId &ams, std::unique_lock<std::recursive_mutex> &lock);
	void DeleteIfLastConnection(const AmsConnection *conn);
};