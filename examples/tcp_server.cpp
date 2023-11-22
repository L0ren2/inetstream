#include "../inetstream.hpp"
#include <iostream>
#include <thread>
#include <future>
#include <chrono>

void exec(inet::inetstream& inetstr) {
    uint32_t cmd {};
    inetstr >> cmd;
    inetstr.clear();
    std::cout << "will exec " << cmd << std::endl;
    switch (cmd) {
    case 0: inetstr << 0xffffffff; break;
    case 1: inetstr << 0; break;
    }
}

int main() {
    inet::server serv {3490};
    while (true) {
	auto inetstr = serv.accept();
	std::cout << "connected" << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds{250});
	std::cout << "receiving" << std::endl;
	inetstr.recv();
	std::cout << "received" << std::endl;
	exec(inetstr);
	std::cout << "sending" << std::endl;
	inetstr.send();
	std::cout << "sent" << std::endl;
    }
    return 0;
}

#define INETSTREAM_IMPL
#include "../inetstream.hpp"
