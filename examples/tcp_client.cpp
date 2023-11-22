#include "../inetstream.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    inet::client client {"127.0.0.1", 3490};
    auto inetstr = client.connect();
    std::cout << "connected" << std::endl;
    inetstr << 0;
    std::cout << "sending" << std::endl;
    inetstr.send();
    std::cout << "sent" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds{500});
    std::cout << "receiving" << std::endl;
    inetstr.recv();
    std::cout << "received" << std::endl;
    uint32_t answer {42};
    inetstr >> answer;
    std::cout << "the answer is 0x" << std::hex << answer << std::endl;
    return 0;
}
#define INETSTREAM_IMPL
#include "../inetstream.hpp"
