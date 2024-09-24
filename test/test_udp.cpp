#include "Catch2/include/catch.hpp"

#define INET_USE_DEFAULT_SIGUSR1_HANDLER true
#include "../inetstream.hpp"

#include <thread>
#include <chrono>

TEST_CASE("test client -> server message") {
	std::thread t {[] {
		inet::server<inet::protocol::UDP> server {1337};
		std::this_thread::sleep_for(std::chrono::milliseconds{3});
		auto istr = server.get_inetstream();
		istr.recv();
		REQUIRE(istr.size() == 4);
		int i {};
		istr >> i;
		REQUIRE(i == 42);
	}};
	try {
		inet::client<inet::protocol::UDP> client {"127.0.0.1", 1337};
		auto istr = client.get_inetstream();
		istr << 42;
		std::this_thread::sleep_for(std::chrono::milliseconds{50});
		istr.send();
		t.join();
	}
	catch (const std::exception& e)
	{
		std::cout << "caught exception: " << e.what() << std::endl;
	}
}

// server cannot send to client, this test will loop endlessly
/*
TEST_CASE("server -> client") {
	std::thread t {[] {
		inet::client<inet::protocol::UDP> client {"127.0.0.1", 1337};
		auto istr = client.get_inetstream();
		do {
			if (istr.select(std::chrono::milliseconds {100}))
				istr.recv();
		} while (istr.size() != 4);
		REQUIRE(istr.size() == 4);
		int i {};
		istr >> i;
		REQUIRE(i == 42);
	}};
	try {
		inet::server<inet::protocol::UDP> server {1337};
		auto istr = server.get_inetstream();
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		istr << 42;
		istr.send();
		std::this_thread::sleep_for(std::chrono::milliseconds{50});
		istr.clear();
		t.join();
	}
	catch (const std::exception& e)
	{
		std::cout << "caught exception: " << e.what() << std::endl;
	}
}
*/
