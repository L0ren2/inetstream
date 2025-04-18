#include "Catch2/include/catch.hpp"

#define INET_USE_DEFAULT_SIGUSR1_HANDLER true
#include "../inetstream.hpp"

#include <thread>
#include <chrono>

TEST_CASE("test creating tcp server and getting inetstream") {
	std::thread t {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		inet::client<inet::protocol::TCP> client{"127.0.0.1", 3490};
		auto istr = client.connect();
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
	}};
	REQUIRE_NOTHROW([] {
		inet::server<inet::protocol::TCP> server{3490};
		auto istr = server.accept();
	}());
	t.join();
}
TEST_CASE("communicating between server and client") {
	std::thread t {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		inet::client<inet::protocol::TCP> client{"127.0.0.1", 3491};
		auto istr = client.connect();
		REQUIRE(4 == istr.recv(4));
		uint32_t i;
		istr >> i;
		REQUIRE(i == 0x01020304);
	}};
	inet::server<inet::protocol::TCP> server{3491};
	auto istr = server.accept();
	istr << 0x01020304;
	istr.send();
	t.join();
}
TEST_CASE("communicating between client and server") {
	std::thread t {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		inet::client<inet::protocol::TCP> client{"127.0.0.1", 3492};
		auto istr = client.connect();
		istr << 0x01020304;
		istr.send();
	}};
	inet::server<inet::protocol::TCP> server{3492};
	auto istr = server.accept();
	REQUIRE(4 == istr.recv(4));
	uint32_t i;
	istr >> i;
	REQUIRE(i == 0x01020304);
	t.join();
}
TEST_CASE("same as above but tests uint16") {
	std::thread t {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		inet::client<inet::protocol::TCP> client{"127.0.0.1", 3493};
		auto istr = client.connect();
		REQUIRE(2 == istr.recv(2));
		uint16_t i;
		istr >> i;
		REQUIRE(i == 0x0102);
	}};
	inet::server<inet::protocol::TCP> server{3493};
	auto istr = server.accept();
	istr << static_cast<uint16_t>(0x0102);
	istr.send();
	t.join();
}
TEST_CASE("same as above but tests uint16 vice versa") {
	std::thread t {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		inet::client<inet::protocol::TCP> client{"127.0.0.1", 3494};
		auto istr = client.connect();
		istr << static_cast<uint16_t>(0x0102);
		istr.send();
	}};
	inet::server<inet::protocol::TCP> server{3494};
	auto istr = server.accept();
	REQUIRE(2 == istr.recv(2));
	uint16_t i;
	istr >> i;
	REQUIRE(i == 0x0102);
	t.join();
}
TEST_CASE("same as above but tests uint64") {
	std::thread t {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		inet::client<inet::protocol::TCP> client{"127.0.0.1", 3495};
		auto istr = client.connect();
		REQUIRE(8 == istr.recv(8));
		uint64_t i;
		istr >> i;
		REQUIRE(i == 0x0102030405060708);
	}};
	inet::server<inet::protocol::TCP> server{3495};
	auto istr = server.accept();
	istr << static_cast<uint64_t>(0x0102030405060708);
	istr.send();
	t.join();
}
TEST_CASE("same as above but tests uint64 vice versa") {
	std::thread t {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		inet::client<inet::protocol::TCP> client{"127.0.0.1", 3496};
		auto istr = client.connect();
		istr << static_cast<uint64_t>(0x0102030405060708);
		istr.send();
	}};
	inet::server<inet::protocol::TCP> server{3496};
	auto istr = server.accept();
	REQUIRE(8 == istr.recv(8));
	uint64_t i;
	istr >> i;
	REQUIRE(i == 0x0102030405060708);
	t.join();
}
TEST_CASE("same as above but tests byte") {
	typedef unsigned char byte;
	std::thread t {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		inet::client<inet::protocol::TCP> client{"127.0.0.1", 3497};
		auto istr = client.connect();
		REQUIRE(1 == istr.recv(1));
		byte i;
		istr >> i;
		REQUIRE(i == 0x42);
	}};
	inet::server<inet::protocol::TCP> server{3497};
	auto istr = server.accept();
	istr << static_cast<byte>(0x42);
	istr.send();
	t.join();
}
TEST_CASE("same as above but tests byte vice versa") {
	typedef unsigned char byte;
	std::thread t {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		inet::client<inet::protocol::TCP> client{"127.0.0.1", 3498};
		auto istr = client.connect();
		istr << static_cast<byte>(0x42);
		istr.send();
	}};
	inet::server<inet::protocol::TCP> server{3498};
	auto istr = server.accept();
	REQUIRE(1 == istr.recv(1));
	byte i;
	istr >> i;
	REQUIRE(i == 0x42);
	t.join();
}
TEST_CASE("same as above but tests char*") {
	std::thread t {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		inet::client<inet::protocol::TCP> client{"127.0.0.1", 3499};
		auto istr = client.connect();
		// might recv less than requested
		REQUIRE(istr.recv(sizeof("Hello, World!")) <= sizeof("Hello, World!"));
		std::string s;
		istr >> s;
		REQUIRE(s == "Hello, World!");
	}};
	inet::server<inet::protocol::TCP> server{3499};
	auto istr = server.accept();
	istr << "Hello, World!";
	istr.send();
	t.join();
}
TEST_CASE("same as above but tests char* vice versa") {
	std::thread t {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		inet::client<inet::protocol::TCP> client{"127.0.0.1", 3500};
		auto istr = client.connect();
		istr << "Hello, World!";
		istr.send();
	}};
	inet::server<inet::protocol::TCP> server{3500};
	auto istr = server.accept();
	// might recv less than requested
	REQUIRE(istr.recv(sizeof("Hello, World!")) <= sizeof("Hello, World!"));
	std::string s;
	istr >> s;
	REQUIRE(s == "Hello, World!");
	t.join();
}
TEST_CASE("same as above but tests std::string") {
	std::thread t {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		inet::client<inet::protocol::TCP> client{"127.0.0.1", 3501};
		auto istr = client.connect();
		// might recv less than requested
		REQUIRE(istr.recv(sizeof("Hello, World!")) <= sizeof("Hello, World!"));
		std::string str{};
		istr >> str;
		REQUIRE(str == "Hello, World!");
	}};
	inet::server<inet::protocol::TCP> server{3501};
	auto istr = server.accept();
	istr << std::string{"Hello, World!"};
	istr.send();
	t.join();
}
TEST_CASE("same as above but tests std::string vice versa") {
	std::thread t {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		inet::client<inet::protocol::TCP> client{"127.0.0.1", 3502};
		auto istr = client.connect();
		istr << std::string{"Hello, World!"};
		istr.send();
	}};
	inet::server<inet::protocol::TCP> server{3502};
	auto istr = server.accept();
	// might recv less than requested
	REQUIRE(istr.recv(sizeof("Hello, World!")) <= sizeof("Hello, World!"));
	std::string str{};
	istr >> str;
	REQUIRE(str == "Hello, World!");
	t.join();
}
class widget {
public:
	widget(int x=42, int y=1337) : _x{x}, _y{y} {}
	std::string to_string() {
		std::stringstream ss;
		ss << "widget_str {x: " << this->_x << ", y: " << this->_y << "}";
		return ss.str();
	}
	int x() { return _x; }
	int y() { return _y; }
private:
	int _x, _y;
};
TEST_CASE("same as above but tests widget") {
	std::thread t {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		inet::client<inet::protocol::TCP> client{"127.0.0.1", 3503};
		auto istr = client.connect();
		REQUIRE(istr.recv(sizeof(widget)) <= sizeof(widget));
		widget v;
		REQUIRE(v.x() == 42);
		REQUIRE(v.y() == 1337);
		REQUIRE(v.to_string() == "widget_str {x: 42, y: 1337}");
		istr >> v;
		REQUIRE(v.x() == 1337);
		REQUIRE(v.y() == 42);
		REQUIRE(v.to_string() == "widget_str {x: 1337, y: 42}");
	}};
	inet::server<inet::protocol::TCP> server{3503};
	auto istr = server.accept();
	widget w {1337, 42};
	istr << w;
	istr.send();
	t.join();
}
TEST_CASE("same as above but tests widget vice versa") {
	std::thread t {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		inet::client<inet::protocol::TCP> client{"127.0.0.1", 3504};
		auto istr = client.connect();
		widget w {1337, 42};
		istr << w;
		istr.send();
	}};
	inet::server<inet::protocol::TCP> server{3504};
	auto istr = server.accept();
	REQUIRE(istr.recv(sizeof(widget)) <= sizeof(widget));
	std::string str{};
	widget v;
	REQUIRE(v.x() == 42);
	REQUIRE(v.y() == 1337);
	REQUIRE(v.to_string() == "widget_str {x: 42, y: 1337}");
	istr >> v;
	REQUIRE(v.x() == 1337);
	REQUIRE(v.y() == 42);
	REQUIRE(v.to_string() == "widget_str {x: 1337, y: 42}");
	t.join();
}
TEST_CASE("send and receive multiple messages") {
	std::thread t {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		inet::client<inet::protocol::TCP> client{"127.0.0.1", 3505};
		auto istr = client.connect();
		int i {42};
		istr << i;
		istr.send();
		istr.clear();
		std::this_thread::sleep_for(std::chrono::milliseconds{20});
		REQUIRE(4 == istr.recv(4));
		istr >> i;
		REQUIRE(i == 1337);
		istr.clear();
		istr << 0;
		istr.send();
		istr.clear();
		std::this_thread::sleep_for(std::chrono::milliseconds{20});
		// may recv less than requested
		REQUIRE(istr.recv(sizeof("Hello")) <= sizeof("Hello"));
		std::string s;
		istr >> s;
		REQUIRE(s == "Hello");
		istr.clear();
		std::this_thread::sleep_for(std::chrono::milliseconds{20});
	}};
	inet::server<inet::protocol::TCP> server{3505};
	auto istr = server.accept();
	int j {};
	REQUIRE(4 == istr.recv(4));
	istr >> j;
	REQUIRE(j == 42);
	j = 1337;
	istr.clear();
	istr << j;
	istr.send();
	istr.clear();
	std::this_thread::sleep_for(std::chrono::milliseconds{20});
	// may recv less than requested
	REQUIRE(istr.recv(sizeof("Hello")) <= sizeof("Hello"));
	istr >> j;
	REQUIRE(j == 0);
	istr.clear();
	istr << "Hello";
	istr.send();
	t.join();
}
TEST_CASE("test receiving more than there is to get") {
	std::thread t {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		inet::client<inet::protocol::TCP> client{"127.0.0.1", 3506};
		auto istr = client.connect();
		int i {42};
		istr << i;
		istr.send();
	}};
	inet::server<inet::protocol::TCP> server{3506};
	auto istr = server.accept();
	INFO("should receive 4 bytes even though 8 requested, "
	     "because only 4 are sent");
	REQUIRE(4 == istr.recv(8));
	int i;
	istr >> i;
	REQUIRE(i == 42);
	t.join();
}
TEST_CASE("test receiving twice, then reading all data on stream") {
	std::thread t {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{50});
		inet::client<inet::protocol::TCP> client{"127.0.0.1", 3507};
		auto istr = client.connect();
		int i_1 {42}, i_2 {1337};
		istr << i_1;
		istr.send();
		std::this_thread::sleep_for(std::chrono::milliseconds{
				INET_MAX_RECV_TIMEOUT_MS});
		istr.clear();
		istr << i_2;
		istr.send();
	}};
	inet::server<inet::protocol::TCP> server{3507};
	auto istr = server.accept();
	INFO("should receive 4 bytes even though 8 requested, "
	     "because only 4 are sent");
	REQUIRE(4 == istr.recv(8));
	REQUIRE(istr.size() == 4);
	std::this_thread::sleep_for(std::chrono::milliseconds{
			INET_MAX_RECV_TIMEOUT_MS});
	REQUIRE(4 == istr.recv(8));
	INFO("if this fails, recv sets size incorrectly");
	REQUIRE(istr.size() == 8);
	int i_1, i_2;
	istr >> i_1;
	istr >> i_2;
	REQUIRE(i_1 == 42);
	REQUIRE(i_2 == 1337);
	t.join();
}
TEST_CASE("test receiving twice, but reading one by one") {
	std::thread t {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{50});
		inet::client<inet::protocol::TCP> client{"127.0.0.1", 3507};
		auto istr = client.connect();
		int i_1 {42}, i_2 {1337};
		istr << i_1;
		istr.send();
		std::this_thread::sleep_for(std::chrono::milliseconds{
				INET_MAX_RECV_TIMEOUT_MS});
		istr.clear();
		istr << i_2;
		istr.send();
	}};
	inet::server<inet::protocol::TCP> server{3507};
	auto istr = server.accept();
	INFO("should receive 4 bytes even though 8 requested, "
	     "because only 4 are sent");
	REQUIRE(4 == istr.recv(8));
	REQUIRE(istr.size() == 4);
	int i_1;
	istr >> i_1;
	REQUIRE(i_1 == 42);
	REQUIRE(istr.size() == 0);
	std::this_thread::sleep_for(std::chrono::milliseconds{10});
	REQUIRE(4 == istr.recv(8));
	INFO("if this fails, recv sets size incorrectly");
	REQUIRE(istr.size() == 4);
	int i_2;
	istr >> i_2;
	REQUIRE(i_2 == 1337);
	REQUIRE(istr.size() == 0);
	t.join();
}
TEST_CASE("test chaining leftshift on istr") {
	std::thread t {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{50});
		inet::client<inet::protocol::TCP> client {"127.0.0.1", 3508};
		auto istr = client.connect();
		istr << 1 << 2 << 3 << 4;
		istr.send();
	}};
	inet::server<inet::protocol::TCP> server {3508};
	auto istr = server.accept();
	std::this_thread::sleep_for(std::chrono::milliseconds{5});
	istr.recv(4 * sizeof(int));
	REQUIRE(istr.size() == 4 * sizeof(int));
	for (int i {1}; i < 5; ++i) {
		int j {};
		istr >> j;
		REQUIRE(i == j);
	}
	t.join();
}
TEST_CASE("test default signal handler for leaving accept() call") {
	std::thread t {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		kill(getpid(), SIGUSR1);
	}};
	inet::server<inet::protocol::TCP> server {3509};
	try {
		server.accept();
		// unreachable by design
		REQUIRE(false == true);
	}
	catch (const std::system_error& e) {
		// expect interrupted system call exception
		REQUIRE(e.code().value() == EINTR);
	}
	t.join();
}
TEST_CASE("multiple clients") {
	std::thread t1 {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{50});
		{
			inet::client<inet::protocol::TCP> client1 {"127.0.0.1", 3510};
			auto istr1 = client1.connect();
			istr1 << 1; istr1.send();
			std::this_thread::sleep_for(std::chrono::milliseconds{
					INET_MAX_RECV_TIMEOUT_MS});
			istr1.clear(); istr1.recv(4);
			REQUIRE(istr1.size() == 4);
			int i {0};
			istr1 >> i;
			REQUIRE(i == 42);
			// kill client
		}
	}};
	std::thread t2 {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{100});
		{
			inet::client<inet::protocol::TCP> client2 {"127.0.0.1", 3510};
			auto istr2 = client2.connect();
			istr2 << 2; istr2.send();
			std::this_thread::sleep_for(std::chrono::milliseconds{
					INET_MAX_RECV_TIMEOUT_MS});
			istr2.clear(); istr2.recv(4);
			REQUIRE(istr2.size() == 4);
			int i {0};
			istr2 >> i;
			REQUIRE(i == 1337);
			// kill client
		}
	}};
	inet::server<inet::protocol::TCP> server {3510};
	{
		auto istr1 = server.accept();
		istr1.recv(4);
		REQUIRE(istr1.size() == 4);
		int i {0};
		istr1 >> i;
		REQUIRE(i == 1);
		istr1.clear(); istr1 << 42;
		istr1.send();
	}
	{
		auto istr2 = server.accept();
		istr2.recv(4);
		REQUIRE(istr2.size() == 4);
		int i {0};
		istr2 >> i;
		REQUIRE(i == 2);
		istr2.clear(); istr2 << 1337;
		istr2.send();
	}
	t1.join(); t2.join();
}
TEST_CASE("multiple clients 2") {
	std::thread t1 {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		{
			inet::client<inet::protocol::TCP> client1 {"127.0.0.1", 3511};
			auto istr1 = client1.connect();
			istr1 << 1; istr1.send();
			std::this_thread::sleep_for(std::chrono::milliseconds{
					INET_MAX_RECV_TIMEOUT_MS});
			istr1.clear(); istr1.recv(4);
			REQUIRE(istr1.size() == 4);
			int i {0};
			istr1 >> i;
			REQUIRE(i == 42);
			// kill client
		}
	}};
	std::thread t2 {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{50});
		{
			inet::client<inet::protocol::TCP> client2 {"127.0.0.1", 3511};
			auto istr2 = client2.connect();
			istr2 << 2; istr2.send();
			std::this_thread::sleep_for(std::chrono::milliseconds{
					INET_MAX_RECV_TIMEOUT_MS});
			istr2.clear(); istr2.recv(4);
			REQUIRE(istr2.size() == 4);
			int i {0};
			istr2 >> i;
			REQUIRE(i == 1337);
			// kill client
		}
	}};
	inet::server<inet::protocol::TCP> server {3511};
	{
		auto istr1 = server.accept();
		istr1.recv(4);
		REQUIRE(istr1.size() == 4);
		int i {0};
		istr1 >> i;
		REQUIRE(i == 1);
		istr1.clear(); istr1 << 42;
		istr1.send();
		auto istr2 = server.accept();
		istr2.recv(4);
		REQUIRE(istr2.size() == 4);
		int j {0};
		istr2 >> j;
		REQUIRE(j == 2);
		istr2.clear(); istr2 << 1337;
		istr2.send();
	}
	t1.join(); t2.join();
}
TEST_CASE("server dies before client") {
	std::thread t1 {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		{
			inet::client<inet::protocol::TCP> client1 {"127.0.0.1", 3512};
			auto istr1 = client1.connect();
			istr1 << 1; istr1.send();
			std::this_thread::sleep_for(std::chrono::milliseconds{50});
			istr1.clear(); istr1.recv(4);
			REQUIRE(istr1.size() == 4);
			int i {0};
			istr1 >> i;
			REQUIRE(i == 42);
			std::this_thread::sleep_for(std::chrono::milliseconds{50});
			// kill client
		}
	}};
	{
		inet::server<inet::protocol::TCP> server {3512};
		auto istr1 = server.accept();
		istr1.recv(4);
		REQUIRE(istr1.size() == 4);
		int i {0};
		istr1 >> i;
		REQUIRE(i == 1);
		istr1.clear(); istr1 << 42;
		istr1.send();
	}
	t1.join(); 
}
TEST_CASE("inetstream::select") {
	std::thread t1 {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		{
			inet::client<inet::protocol::TCP> client {"127.0.0.1", 3513};
			auto istr = client.connect();
			std::this_thread::sleep_for(std::chrono::milliseconds{100});
			istr << 1; istr.send();
			std::this_thread::sleep_for(std::chrono::milliseconds{5});
			istr.clear(); istr.recv(4);
			REQUIRE(istr.size() == 4);
			int i {0};
			istr >> i;
			REQUIRE(i == 42);
			// kill client
		}
	}};
	{
		inet::server<inet::protocol::TCP> server {3513};
		auto istr = server.accept();
		int tries = 25; // a bit extra time for scheduling reasons
		do {
			if (istr.select(std::chrono::milliseconds{5}))
				break;
		} while (--tries);
		REQUIRE(tries > 0);
		istr.recv(4);
		REQUIRE(istr.size() == 4);
		int i {0};
		istr >> i;
		REQUIRE(i == 1);
		istr.clear(); istr << 42;
		istr.send();
		std::this_thread::sleep_for(std::chrono::milliseconds{5});
	}
	t1.join();
}
TEST_CASE("early return from recv()") {
	std::thread t1 {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		inet::client<inet::protocol::TCP> client {"127.0.0.1", 3514};
		auto istr = client.connect();
		istr << 1; istr.send();
		istr.clear();
		istr.recv(4);
		REQUIRE(istr.size() == 4);
		int i {0};
		istr >> i;
		REQUIRE(i == 42);
	}};
	using namespace std::chrono;
	auto start_t = system_clock::now();
	{
		inet::server<inet::protocol::TCP> server {3514};
		auto istr = server.accept();
		istr.recv(4);
		REQUIRE(istr.size() == 4);
		int i {0};
		istr >> i;
		REQUIRE(i == 1);
		istr.clear(); istr << 42;
		istr.send();
	}
	t1.join();
	auto end_t = system_clock::now();
	REQUIRE(duration_cast<milliseconds>(end_t - start_t).count()
	        <= INET_MAX_RECV_TIMEOUT_MS);
}
TEST_CASE("test int16") {
	std::thread t1 {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		inet::client<inet::protocol::TCP> client {"127.0.0.1", 3515};
		auto istr = client.connect();
		istr << static_cast<int16_t>(0x1234);
		istr.send();
		std::this_thread::sleep_for(std::chrono::milliseconds {100});
	}};
	inet::server<inet::protocol::TCP> server {3515};
	auto istr =	server.accept();
	istr.select(std::chrono::milliseconds {100});
	int16_t i {};
	istr.recv(2);
	REQUIRE(istr.size() == 2);
	istr >> i;
	REQUIRE(i == 0x1234);
	t1.join();
}

TEST_CASE("test int32") {
	std::thread t1 {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		inet::client<inet::protocol::TCP> client {"127.0.0.1", 3516};
		auto istr = client.connect();
		istr << static_cast<int32_t>(0x12345678);
		istr.send();
		std::this_thread::sleep_for(std::chrono::milliseconds {100});
	}};
	inet::server<inet::protocol::TCP> server {3516};
	auto istr =	server.accept();
	istr.select(std::chrono::milliseconds {100});
	int32_t i {};
	istr.recv(4);
	REQUIRE(istr.size() == 4);
	istr >> i;
	REQUIRE(i == 0x12345678);
	t1.join();
}

TEST_CASE("test int64") {
	std::thread t1 {[] {
		std::this_thread::sleep_for(std::chrono::milliseconds{10});
		inet::client<inet::protocol::TCP> client {"127.0.0.1", 3517};
		auto istr = client.connect();
		istr << static_cast<int64_t>(0x1234567890);
		istr.send();
		std::this_thread::sleep_for(std::chrono::milliseconds {100});
	}};
	inet::server<inet::protocol::TCP> server {3517};
	auto istr =	server.accept();
	istr.select(std::chrono::milliseconds {100});
	int64_t i {};
	istr.recv(8);
	REQUIRE(istr.size() == 8);
	istr >> i;
	REQUIRE(i == 0x1234567890);
	t1.join();
}
TEST_CASE("floating point") {
	std::thread t1 {[] {
		inet::client<inet::protocol::TCP> client {"127.0.0.1", 3260};
		std::this_thread::sleep_for(std::chrono::milliseconds{1});
		auto istr = client.connect();
		istr.recv(sizeof(double) + sizeof(float));
		double d {};
		float f {};
		istr >> d;
		istr >> f;
		const auto ABS = [](double x) { return (x > 0) ? (x) : -(x); };
		const auto ABSf = [](float x) { return (x > 0) ? (x) : -(x); };
		REQUIRE(ABS(d - 3.1415926535) < 0.0000001);
		REQUIRE(ABSf(f - 3.14136f) < 0.0003f);
	}};
	inet::server<inet::protocol::TCP> server {3260};
	auto istr = server.accept();
	double d {3.1415926535};
	float f {42 / 13.37f};
	istr << d;
	istr << f;
	istr.send();
	t1.join();
}
TEST_CASE("non-blocking accept") {
	std::thread t1 {[] {
		std::this_thread::sleep_for(std::chrono::seconds{1});
		inet::client<inet::protocol::TCP> client {"127.0.0.1", 3261};
		auto istr = client.connect();
		istr.recv(sizeof(uint32_t));
		uint32_t i;
		istr >> i;
		REQUIRE(i == 42);
		istr.clear();
		istr << 1337;
		istr.send();
	}};
	inet::server<inet::protocol::TCP> server {3261};
	server.set_nonblocking();
	bool s = server.select(std::chrono::milliseconds{100});
	REQUIRE_FALSE(s);
	if (!server.select(std::chrono::seconds{2})) {
		throw std::runtime_error {"timeout!"};
	}
	auto istr = server.accept();
	istr << 42; istr.send();
	istr.recv(sizeof(uint32_t));
	uint32_t i;
	istr >> i;
	REQUIRE(i == 1337);
	t1.join();
}
