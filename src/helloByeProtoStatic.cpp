#include "helloByeProto.hpp"

#include <mutex>

/*
 * ===================================
 * Define Client config singleton
 * ===================================
 *
 */

HelloBye::HelloByeClientConfig *HelloBye::HelloByeClientConfig::instance = nullptr;
std::mutex HelloBye::HelloByeClientConfig::mtx;
