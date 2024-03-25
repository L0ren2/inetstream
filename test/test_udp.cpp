#include "Catch2/include/catch.hpp"

#define INET_USE_DEFAULT_SIGUSR1_HANDLER true
#include "../inetstream.hpp"

#include <thread>
#include <chrono>

TEST_CASE("test creating udp server and getting inetstream") {
    inet::server<inet::protocol::UDP> server {1337};
    auto istr = server.get_inetstream();
}
