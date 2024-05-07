#include <thread>
#include <chrono>

#define CATCH_CONFIG_MAIN
#include "Catch2/include/catch.hpp"
// this prevents failing a test case in test_Tcp
#define INET_USE_DEFAULT_SIGUSR1_HANDLER true
#include "../inetstream.hpp"

TEST_CASE("memory client") {
    inet::client<inet::protocol::TCP> c {"127.0.0.1", 1337};
}
TEST_CASE("memory server") {
    inet::server<inet::protocol::TCP> s {1337};
}
TEST_CASE("memory \"client\"") {
    inet::client<inet::protocol::UDP> c {"127.0.0.1", 1338};
}
TEST_CASE("memory \"server\"") {
    inet::server<inet::protocol::UDP> s {1338};
}
TEST_CASE("memory connected") {
    std::thread t1 {[] {
	std::this_thread::sleep_for(std::chrono::milliseconds{10});
	inet::client<inet::protocol::TCP> c {"127.0.0.1", 1339};
	auto istr = c.connect();
    }};
    inet::server<inet::protocol::TCP> s {1339};
    auto istr = s.accept();
    t1.join();
}
TEST_CASE("memory \"connected\"") {
    std::thread t1 {[] {
	std::this_thread::sleep_for(std::chrono::milliseconds{10});
	inet::client<inet::protocol::UDP> c {"127.0.0.1", 1340};
	auto istr = c.get_inetstream();
    }};
    inet::server<inet::protocol::UDP> s {1340};
    auto istr = s.get_inetstream();
    t1.join();
}
