#ifndef INETSTREAM_HPP_
#define INETSTREAM_HPP_
// C++
#include <sstream>
#include <vector>
// C
#include <cstring>
#include <cerrno>
// System
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>

// users may override these
#ifndef MAX_INET_CONNECTIONS
#define MAX_INET_CONNECTIONS 10
#endif
#ifndef MAX_INET_TIMEOUT_MS
#define MAX_INET_TIMEOUT_MS 1
#endif

namespace inet {
    typedef unsigned char byte;
    
    // forward decl
    class inetstream;
    
    class server {
    public:
	server(unsigned short Port);
	~server();
	inetstream accept();
    private:
	unsigned short _port;
	int _socket_fd;
    };
    
    class client {
    public:
	client(const std::string& Host, unsigned short Port);
	~client();
	inetstream connect();
    private:
	std::string _host;
	unsigned short _port;
	int _socket_fd;
    };
    
    template <typename T> inetstream& operator<<(inetstream&, T);
    template <typename T> void operator>>(inetstream&, T&);
    
    class inetstream {
    public:
	inetstream() = delete;
	inetstream(const inetstream&) = delete;
	inetstream(inetstream&&);
	~inetstream();
	/**
	 * push data onto the stream
	 *
	 */
	template <typename T>
	friend inetstream& operator<<(inetstream&, T);
	/**
	 * retrieve data from the stream
	 *
	 */
	template <typename T>
	friend void operator>>(inetstream&, T&);
	/**
	 * sends data pushed into the stream over the network to remote
	 *
	 */
	void send();
	/**
	 * receives network data, populating the stream with data
	 *
	 * @param s number of bytes to try and receive
	 * @return the number of bytes received
	 */
	std::size_t recv(std::size_t s);
	bool empty() const { return size() == 0; }
	void clear() { _buf.clear(); _read_pos = _buf.begin(); }
	std::size_t size() const { return _buf.end() - _read_pos; }
    private:
	inetstream(int socket_fd);
	friend class server;
	friend class client;
	int _socket_fd;
	std::vector<byte> _buf;
	std::vector<byte>::iterator _read_pos;
    };
    
    template <typename T>
    inetstream& operator<<(inetstream& str, T t) {
	for (std::size_t s {0}; s < sizeof t; ++s) {
	    str._buf.push_back(*(reinterpret_cast<byte*>(&t) + s));
	}
	return str;
    }
    template <> inetstream& operator<< (inetstream&, std::string);
    template <> inetstream& operator<< (inetstream&, const char*);
    template <> inetstream& operator<< (inetstream&, uint16_t);
    template <> inetstream& operator<< (inetstream&, uint32_t);
    template <> inetstream& operator<< (inetstream&, uint64_t);
    template <> inetstream& operator<< (inetstream&, int);
    
    template <typename T>
    void operator>>(inetstream& str, T& t) {
	constexpr std::size_t sz = sizeof t;
	if (str.size() < sz) {
	    std::stringstream ss;
	    ss << "tried to read " << sz << " bytes into variable but only got "
	       << str.size() << " bytes";
	    throw std::runtime_error {ss.str()};
	}
	T tmp {};
	for (std::size_t i {0}; i < sz; ++i) {
	    *(reinterpret_cast<byte*>(&tmp) + i) = *str._read_pos++;
	}
	t = tmp;
    }
    template <> void operator>> (inetstream&, std::string&);
    template <> void operator>> (inetstream&, uint16_t&);
    template <> void operator>> (inetstream&, uint32_t&);
    template <> void operator>> (inetstream&, uint64_t&);
    template <> void operator>> (inetstream&, int&);
    
} // namespace inet
#endif

#ifdef INETSTREAM_IMPL
#undef INETSTREAM_IMPL
// C++
#include <sstream>
#include <chrono>
#include <vector>
#include <array>
// C
#include <cstring>
#include <cerrno>
// System
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>

namespace inet {
    template <> inetstream& operator<< (inetstream& str, std::string s) {
	for (const char c : s) {
	    str._buf.push_back(c);
	}
	return str;
    }
    template <> inetstream& operator<< (inetstream& str, const char* p) {
	std::size_t sz = std::strlen(p);
	for (std::size_t i {0}; i < sz; ++i) {
	    str._buf.push_back(p[i]);
	}
	return str;
    }
    template <> inetstream& operator<< (inetstream& str, uint16_t us) {
	constexpr std::size_t sz = sizeof us;
	union {
	    uint16_t i;
	    byte b[sz];
	} u {0};
	u.i = htons(us);
	for (std::size_t s {0}; s < sz; ++s) {
	    str._buf.push_back(u.b[s]);
	}
	return str;
    }
    template <> inetstream& operator<< (inetstream& str, uint32_t ul) {
	constexpr std::size_t sz = sizeof ul;
	union {
	    uint32_t i;
	    byte b[sz];
	} u {0};
	u.i = htonl(ul);
	for (std::size_t s {0}; s < sz; ++s) {
	    str._buf.push_back(u.b[s]);
	}
	return str;
    }
    template <> inetstream& operator<< (inetstream& str, uint64_t ull) {
	constexpr std::size_t sz = sizeof ull;
	union {
	    uint64_t i;
	    byte b[sz];
	} u {0};
	u.i  = static_cast<uint64_t>(htonl((ull & 0xffffffff00000000) >> 32)) << 32;
	u.i |= htonl(ull & 0x00000000ffffffff);
	for (std::size_t s {0}; s < sz; ++s) {
	    str._buf.push_back(u.b[s]);
	}
	return str;
    }
    template <> inetstream& operator<< (inetstream& str, int in) {
	static_assert(1 < sizeof(int) && sizeof(int) % 2 == 0 && sizeof(int) < 9,
		      "size of int not supported");
	switch (sizeof(int)) {
	case 2:
	    str << static_cast<uint16_t>(in);
	    break;
	case 4:
	    str << static_cast<uint32_t>(in);
	    break;
	case 8:
	    str << static_cast<uint64_t>(in);
	    break;
	}
	return str;
    }

    template <> void operator>> (inetstream& str, std::string& s) {
	s.clear();
	while (str.size() > 0) {
	    byte b = *str._read_pos++;
	    if (b != 0) {
		s += b;
	    }
	    else {
		break;
	    }
	}
    }
    template <> void operator>> (inetstream& str, uint16_t& us) {
	constexpr std::size_t sz = sizeof us;
	if (str.size() < sz) {
	    std::stringstream ss;
	    ss << "tried to read " << sz << " bytes into variable but only got "
	       << str.size() << " bytes";
	    throw std::runtime_error {ss.str()};
	}
	union {
	    uint16_t i;
	    byte b[sz];
	} u {0};
	for (std::size_t s = 0; s < sz; ++s) {
	    u.b[s] = *str._read_pos++;
	}
	us = ntohs(u.i);
    }
    template <> void operator>> (inetstream& str, uint32_t& ul) {
	constexpr std::size_t sz = sizeof ul;
	if (str.size() < sz) {
	    std::stringstream ss;
	    ss << "tried to read " << sz << " bytes into variable but only got "
	       << str.size() << " bytes";
	    throw std::runtime_error {ss.str()};
	}
	union {
	    uint32_t i;
	    byte b[sz];
	} u {0};
	for (std::size_t s = 0; s < sz; ++s) {
	    u.b[s] = *str._read_pos++;
	}
	ul = ntohl(u.i);
    }
    template <> void operator>> (inetstream& str, uint64_t& ull) {
	constexpr std::size_t sz = sizeof ull;
	if (str.size() < sz) {
	    std::stringstream ss;
	    ss << "tried to read " << sz << " bytes into variable but only got "
	       << str.size() << " bytes";
	    throw std::runtime_error {ss.str()};
	}
	uint32_t tmp {0};
	uint64_t utmp {0};
	// lsb
	str >> tmp;
	utmp = tmp;
	// msb
	str >> tmp;
	// bitshift operators already take care of ntohl stuff
	ull = utmp | static_cast<uint64_t>(tmp) << 32;
    }
    template <> void operator>> (inetstream& str, int& in) {
	static_assert(1 < sizeof(int) && sizeof(int) % 2 == 0 && sizeof(int) < 9,
		      "size of int not supported");
	uint64_t i64;
	uint32_t i32;
	uint16_t i16;
	switch (sizeof(int)) {
	case 2:
	    str >> i16;
	    in = static_cast<int>(i16);
	    break;
	case 4:
	    str >> i32;
	    in = static_cast<int>(i32);
	    break;
	case 8:
	    str >> i64;
	    in = static_cast<int>(i64);
	    break;
	}
    }
    void* get_in_addr(sockaddr* sa) {
	if (sa->sa_family == AF_INET) {
	    return &(((sockaddr_in*)sa)->sin_addr);
	}
	return &(((sockaddr_in6*)sa)->sin6_addr);
    }
    server::server(unsigned short Port) : _port {Port} {
	addrinfo* servinfo, hints;
	std::memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	char port[6] = {0};
	std::stringstream ss;
	ss << _port;
	ss >> port;
	port[5] = 0;
	int rv = getaddrinfo(NULL, port, &hints, &servinfo);
	if (rv != 0) {
	    freeaddrinfo(servinfo);
	    throw std::system_error {rv, std::system_category(), gai_strerror(rv)};
	}
	addrinfo* p;
	for (p = servinfo; p != NULL; p = p->ai_next) {
	    _socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
	    if (_socket_fd == -1) {
		// swallow error
		continue;
	    }
	    int yes = 1; rv = 0;
	    rv = setsockopt(_socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	    if (rv == -1) {
		freeaddrinfo(servinfo);
		throw std::system_error {errno, std::system_category(), strerror(errno)};
	    }
	    if (bind(_socket_fd, p->ai_addr, p->ai_addrlen) == -1) {
		close(_socket_fd);
		// swallow error
		continue;
	    }
	    break;
	}
	freeaddrinfo(servinfo);
	if (p == NULL) {
	    throw std::system_error {errno, std::system_category(), strerror(errno)};
	}
	if (listen(_socket_fd, MAX_INET_CONNECTIONS) == -1) {
	    throw std::system_error {errno, std::system_category(), strerror(errno)};
	}
    }
    server::~server() { close(_socket_fd); }
    inetstream server::accept() {
	sockaddr_storage client_addr;
	socklen_t sin_sz = sizeof client_addr;
	int new_fd = ::accept(_socket_fd, (sockaddr*)&client_addr, &sin_sz);
	if (new_fd == -1) {
	    throw std::system_error {errno, std::system_category(), strerror(errno)};
	}
	char s[INET6_ADDRSTRLEN];
	const char* rv = inet_ntop(client_addr.ss_family,
				   get_in_addr((sockaddr*)&client_addr),
				   s, sizeof s);
	if (rv == NULL) {
	    throw std::system_error {errno, std::system_category(), strerror(errno)};
	}
	if (fcntl(new_fd, F_SETFL, O_NONBLOCK) != 0) {
	    throw std::system_error {errno, std::system_category(), strerror(errno)};
	}
	// connected to s
	return inetstream {new_fd};
    }
    
    client::client(const std::string& Host, unsigned short Port)
	: _host {Host}, _port {Port}
    {
	addrinfo* servinfo, hints;
	std::memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	char port[6] = {0};
	std::stringstream ss;
	ss << _port;
	ss >> port;
	port[5] = 0;
	int rv = getaddrinfo(_host.c_str(), port, &hints, &servinfo);
	if (rv != 0) {
	    freeaddrinfo(servinfo);
	    throw std::system_error {rv, std::system_category(), gai_strerror(rv)};
	}
	addrinfo* p;
	for (p = servinfo; p != NULL; p = p->ai_next) {
	    _socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
	    if (_socket_fd == -1) {
		// swallow error
		continue;
	    }
	    if (::connect(_socket_fd, p->ai_addr, p->ai_addrlen) == -1) {
		close(_socket_fd);
		// swallow error
		continue;
	    }
	    break;
	}
	if (p == NULL) {
	    freeaddrinfo(servinfo);
	    throw std::system_error {errno, std::system_category(), strerror(errno)};
	}
	char s[INET6_ADDRSTRLEN];
	const char* rvp = inet_ntop(p->ai_family,
				    get_in_addr((sockaddr*)p->ai_addr),
				    s, sizeof s);
	if (rvp == NULL) {
	    freeaddrinfo(servinfo);
	    throw std::system_error {errno, std::system_category(), strerror(errno)};
	}
	if (fcntl(_socket_fd, F_SETFL, O_NONBLOCK) != 0) {
	    freeaddrinfo(servinfo);
	    throw std::system_error {errno, std::system_category(), strerror(errno)};
	}
	freeaddrinfo(servinfo);
    }
    client::~client() { }
    
    inetstream client::connect() {
	return inetstream {_socket_fd};
    }
    
    inetstream::inetstream(int socket_fd)
	: _socket_fd {socket_fd}, _read_pos {_buf.begin()} {}
    inetstream::inetstream(inetstream&& other)
	: _socket_fd {other._socket_fd}, _buf {std::move(other._buf)} {
	other._socket_fd = -1;
	_read_pos = _buf.begin();
    }
    inetstream::~inetstream() { if (_socket_fd != -1) { close(_socket_fd); } }
    void inetstream::send() {
	std::chrono::milliseconds timeout {MAX_INET_TIMEOUT_MS};
	auto t_end = std::chrono::system_clock::now() + timeout;
	auto total = _buf.size();
	int sent_this_iter = 0;
	do {
	    sent_this_iter = ::send(_socket_fd, &_buf[_buf.size() - total], total, 0);
	    if (sent_this_iter == -1) {
		throw std::system_error {errno, std::system_category(), strerror(errno)};
	    }
	    if (std::chrono::system_clock::now() > t_end) {
		throw std::runtime_error {"timeout reached"};
	    }
	    total -= sent_this_iter;
	} while (total > 0);
	this->clear();
    }
    std::size_t inetstream::recv(std::size_t sz) {
	std::chrono::milliseconds timeout {MAX_INET_TIMEOUT_MS};
	auto t_end = std::chrono::system_clock::now() + timeout;
	// cache read_pos pointer, since _buf may realloc
	auto read_offset_ = _read_pos - _buf.begin(); 
	auto tmp_size = _buf.size();
	constexpr const std::size_t SZ {1024};
	int read {0};
	std::array<unsigned char, SZ> buf;
	do {
	    if (sz < SZ) {
		read = ::recv(_socket_fd, &buf[0], sz, 0);
	    }
	    else {
		read = ::recv(_socket_fd, &buf[0], SZ - 1, 0);
		if (read > 0) {
		    sz -= read;
		}
	    }
	    if (read == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
		    if (std::chrono::system_clock::now() < t_end) {
			// wait until timeout is reached
			continue;
		    }
		    break;
		}
		throw std::system_error {errno, std::system_category(), strerror(errno)};
	    }
	    _buf.insert(_buf.end(), buf.begin(), buf.begin() + read);
	    if (read == 0) {
		break;
	    }
	} while (std::chrono::system_clock::now() < t_end);
	_read_pos = _buf.begin() + read_offset_;
	return _buf.size() - tmp_size;
    }
} // namespace inet
#endif
