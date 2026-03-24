#pragma once

#include <string>

std::string getSubnetPrefix(const std::string &ip);
std::string getLocalIpForTarget(const std::string &target_ip);
