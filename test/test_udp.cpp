#include "Catch2/include/catch.hpp"

#define INET_USE_DEFAULT_SIGUSR1_HANDLER true
#include "../inetstream.hpp"

#include <thread>
#include <chrono>

TEST_CASE("test creating udp server and getting inetstream") {
    inet::server<inet::protocol::UDP> server {1337};
    auto istr = server.get_inetstream();
}
TEST_CASE("test creating udp client and getting inetstream") {
    inet::client<inet::protocol::UDP> client {"127.0.0.1", 1337};
    auto istr = client.get_inetstream();
}
TEST_CASE("test server -> client message") {
    std::cout << "line " << __LINE__ << std::endl;
    std::thread t {[] {
    std::cout << "line " << __LINE__ << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds{30});
    std::cout << "line " << __LINE__ << std::endl;
	inet::client<inet::protocol::UDP> client {"127.0.0.1", 1337};
    std::cout << "line " << __LINE__ << std::endl;
	auto istr = client.get_inetstream();
    std::cout << "line " << __LINE__ << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds{50});
    std::cout << "line " << __LINE__ << std::endl;
	istr.recv();
    std::cout << "line " << __LINE__ << std::endl;
	REQUIRE(istr.size() == 4);
    std::cout << "line " << __LINE__ << std::endl;
	int i {};
    std::cout << "line " << __LINE__ << std::endl;
	istr >> i;
    std::cout << "line " << __LINE__ << std::endl;
	REQUIRE(i == 42);
    std::cout << "line " << __LINE__ << std::endl;
    }};
    std::cout << "line " << __LINE__ << std::endl;
    inet::server<inet::protocol::UDP> server {1337};
    std::cout << "line " << __LINE__ << std::endl;
    auto istr = server.get_inetstream();
    std::cout << "line " << __LINE__ << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    std::cout << "line " << __LINE__ << std::endl;
    istr << 42;
    std::cout << "line " << __LINE__ << std::endl;
    istr.send();
    std::cout << "line " << __LINE__ << std::endl;
    t.join();
    std::cout << "line " << __LINE__ << std::endl;
}
