#ifndef INETSTREAM_HPP_
#define INETSTREAM_HPP_
// C++
#include <sstream>
#include <optional>
#include <vector>
#include <array>
#include <type_traits>
#include <chrono>
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


// users may override these
#ifndef MAX_INET_CONNECTIONS
#define MAX_INET_CONNECTIONS 10
#endif
#ifndef MAX_INET_TIMEOUT_MS
#define MAX_INET_TIMEOUT_MS 1
#endif
// set to 6 to use IPv6 instead of IPv4
#ifndef INETSTREAM_IPV
#define INETSTREAM_IPV 4
#endif

namespace inet {
    inline void* get_in_addr(sockaddr* sa) {
	if (sa->sa_family == AF_INET) {
	    return &(((sockaddr_in*)sa)->sin_addr);
	}
	return &(((sockaddr_in6*)sa)->sin6_addr);
    }
    typedef unsigned char byte;
    
    enum class protocol {
	TCP, UDP
    };
    // "type" traits
    template <protocol P> struct is_tcp_prot { static constexpr const bool value = std::false_type::value; };
    template <> struct is_tcp_prot<protocol::TCP> { static constexpr const bool value = std::true_type::value; };
    template <protocol P> struct is_udp_prot { static constexpr const bool value = std::false_type::value; };
    template <> struct is_udp_prot<protocol::UDP> { static constexpr const bool value = std::true_type::value; };
    // forward decl
    template <protocol P> class server;
    template <protocol P> class client;
    template <protocol P>
    class inetstream {
    public:
	inetstream() = delete;
	inetstream(const inetstream<P>&) = delete;
	inetstream(inetstream<P>&& other) : _socket_fd {other._socket_fd},
					    _send_buf {std::move(other._send_buf)},
					    _recv_buf {std::move(other._recv_buf)} {
	    other._socket_fd = -1;
	    _read_pos = _recv_buf.begin();
	}
	~inetstream() { if (_socket_fd != -1) { close(_socket_fd); } }
	/**
	 * push data onto the stream
	 *
	 */
	template <typename T>
	typename std::enable_if<!std::is_same<T, uint16_t>::value
				&& !std::is_same<T, uint32_t>::value
				&& !std::is_same<T, uint64_t>::value
				&& !std::is_same<T, int>::value
				&& !std::is_same<T, std::string>::value
				&& !std::is_same<T, const char[]>::value
				&& !std::is_same<T, const char*>::value, inetstream<P>&>::type
	operator<<(T t) {
	    unsigned char chars[sizeof(T)];
	    std::memcpy(&chars[0], &t, sizeof(T));
	    for (std::size_t s {0}; s < sizeof t; ++s) {
		this->_send_buf.push_back(chars[s]);
	    }
	    return *this;
	}
	/** 
	 * retrieve data from the stream
	 *
	 */
	template <typename T>
	typename std::enable_if<!std::is_same<T, uint16_t>::value
				&& !std::is_same<T, uint32_t>::value
				&& !std::is_same<T, uint64_t>::value
				&& !std::is_same<T, int>::value
				&& !std::is_same<T, std::string>::value
				&& !std::is_same<T, const char[]>::value
				&& !std::is_same<T, const char*>::value, void>::type
	operator>>(T& t) {
	    if (this->size() < sizeof(T)) {
		std::stringstream ss;
		ss << "tried to read " << sizeof(T) << " bytes into variable but only got "
		   << this->size() << " bytes";
		throw std::runtime_error {ss.str()};
	    }
	    unsigned char chars[sizeof(T)];
	    for (std::size_t i {0}; i < sizeof(T); ++i) {
		chars[i] = *(this->_read_pos)++;
	    }
	    std::memcpy(&t, &chars[0], sizeof(T));
	}
	inetstream<P>& operator<< (uint16_t us) {
	    constexpr std::size_t sz = sizeof us;
	    // no undefined behaviour, since aliased type is unsigned char
	    union {
		uint16_t i;
		byte b[sz]; 
	    } u {0};
	    u.i = htons(us);
	    for (std::size_t s {0}; s < sz; ++s) {
		this->_send_buf.push_back(u.b[s]);
	    }
	    return *this;
	}
	inetstream<P>& operator<< (uint32_t ul) {
	    constexpr std::size_t sz = sizeof ul;
	    union {
		uint32_t i;
		byte b[sz];
	    } u {0};
	    u.i = htonl(ul);
	    for (std::size_t s {0}; s < sz; ++s) {
		this->_send_buf.push_back(u.b[s]);
	    }
	    return *this;
	}
	inetstream<P>& operator<< (uint64_t ull) {
	    constexpr std::size_t sz = sizeof ull;
	    union {
		uint64_t i;
		byte b[sz];
	    } u {0};
	    u.i  = static_cast<uint64_t>(htonl((ull & 0xffffffff00000000) >> 32)) << 32;
	    u.i |= htonl(ull & 0x00000000ffffffff);
	    for (std::size_t s {0}; s < sz; ++s) {
		this->_send_buf.push_back(u.b[s]);
	    }
	    return *this;
	}
	inetstream<P>& operator<< (int in) {
	    static_assert(sizeof(int) == 2 || sizeof(int) == 4 || sizeof(int) == 8,
			  "size of int not supported");
	    switch (sizeof(int)) {
	    case 2:
		*this << static_cast<uint16_t>(in);
		break;
	    case 4:
		*this << static_cast<uint32_t>(in);
		break;
	    case 8:
		*this << static_cast<uint64_t>(in);
		break;
	    }
	    return *this;
	}
	inetstream<P>& operator<< (std::string s) {
	    for (const char c : s) {
		this->_send_buf.push_back(c);
	    }
	    return *this;
	}
	inetstream<P>& operator<< (const char* p) {
	    std::size_t sz = std::strlen(p);
	    for (std::size_t i {0}; i < sz; ++i) {
		this->_send_buf.push_back(p[i]);
	    }
	    return *this;
	}
	void operator>> (uint16_t& us) {
	    constexpr std::size_t sz = sizeof us;
	    if (this->size() < sz) {
		std::stringstream ss;
		ss << "tried to read " << sz << " bytes into variable but only got "
		   << this->size() << " bytes";
		throw std::runtime_error {ss.str()};
	    }
	    union {
		uint16_t i;
		byte b[sz];
	    } u {0};
	    for (std::size_t s = 0; s < sz; ++s) {
		u.b[s] = *this->_read_pos++;
	    }
	    us = ntohs(u.i);
	}
	void operator>> (uint32_t& ul) {
	    constexpr std::size_t sz = sizeof ul;
	    if (this->size() < sz) {
		std::stringstream ss;
		ss << "tried to read " << sz << " bytes into variable but only got "
		   << this->size() << " bytes";
		throw std::runtime_error {ss.str()};
	    }
	    union {
		uint32_t i;
		byte b[sz];
	    } u {0};
	    for (std::size_t s = 0; s < sz; ++s) {
		u.b[s] = *this->_read_pos++;
	    }
	    ul = ntohl(u.i);
	}
	void operator>> (uint64_t& ull) {
	    constexpr std::size_t sz = sizeof ull;
	    if (this->size() < sz) {
		std::stringstream ss;
		ss << "tried to read " << sz << " bytes into variable but only got "
		   << this->size() << " bytes";
		throw std::runtime_error {ss.str()};
	    }
	    uint32_t tmp {0};
	    uint64_t utmp {0};
	    // lsb
	    *this >> tmp;
	    utmp = tmp;
	    // msb
	    *this >> tmp;
	    // bitshift operators already take care of ntohl stuff
	    ull = utmp | static_cast<uint64_t>(tmp) << 32;
	}
	void operator>> (int& in) {
	    static_assert(sizeof(int) == 2 || sizeof(int) == 4 || sizeof(int) == 8,
			  "size of int not supported");
	    uint64_t i64;
	    uint32_t i32;
	    uint16_t i16;
	    switch (sizeof(int)) {
	    case 2:
		*this >> i16;
		in = static_cast<int>(i16);
		break;
	    case 4:
		*this >> i32;
		in = static_cast<int>(i32);
		break;
	    case 8:
		*this >> i64;
		in = static_cast<int>(i64);
		break;
	    }
	}
	void operator>> (std::string& s) {
	    s.clear();
	    while (this->size() > 0) {
		byte b = *(this->_read_pos)++;
		if (b != 0) {
		    s += b;
		}
		else {
		    break;
		}
	    }
	}
	/**					       
	 * sends data pushed into the stream over the network to remote
	 *
	 */
	void send() {
	    std::chrono::milliseconds timeout {MAX_INET_TIMEOUT_MS};
	    auto t_end = std::chrono::system_clock::now() + timeout;
	    auto total = _send_buf.size();
	    int sent_this_iter = 0;
	    do {
		sent_this_iter = ::send(_socket_fd, &_send_buf[_send_buf.size() - total], total, 0);
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
	/**
	 * receives network data, populating the stream with data
	 *
	 * @param s number of bytes to try and receive
	 * @return the number of bytes received
	 */
	template <protocol T = P>
	typename std::enable_if<is_tcp_prot<T>::value, std::size_t>::type
	recv(std::size_t sz) {
	    std::chrono::milliseconds timeout {MAX_INET_TIMEOUT_MS};
	    auto t_end = std::chrono::system_clock::now() + timeout;
	    // cache read_pos pointer, since _recv_buf may realloc
	    auto read_offset_ = _read_pos - _recv_buf.begin(); 
	    auto tmp_size = _recv_buf.size();
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
		_recv_buf.insert(_recv_buf.end(), buf.begin(), buf.begin() + read);
		if (read == 0) {
		    break;
		}
	    } while (std::chrono::system_clock::now() < t_end);
	    _read_pos = _recv_buf.begin() + read_offset_;
	    return _recv_buf.size() - tmp_size;
	}    
	template <protocol T = P>
	typename std::enable_if<is_udp_prot<T>::value, std::size_t>::type
	recv() {/*TODO: implement*/ return 0;}
	bool empty() const { return size() == 0; }
	void clear() { _send_buf.clear(); _recv_buf.clear(); _read_pos = _recv_buf.begin(); }
	std::size_t size() const { return _recv_buf.end() - _read_pos; }
    private:
	inetstream(int socket_fd) : _socket_fd {socket_fd}, _read_pos {_recv_buf.begin()} {}
	friend class server<P>;
	friend class client<P>;
	int _socket_fd;
	std::vector<byte> _send_buf;
	std::vector<byte> _recv_buf;
	std::vector<byte>::iterator _read_pos;
    };

    template <protocol P>
    class server {
    public:
	template <protocol T = P, typename std::enable_if<is_tcp_prot<T>::value, int>::type* = nullptr>
	server(unsigned short Port) : _port {Port} {
	    addrinfo* servinfo, hints;
	    std::memset(&hints, 0, sizeof hints);
	    if (INETSTREAM_IPV == 4) {
		hints.ai_family = AF_INET;
	    }
	    else if (INETSTREAM_IPV == 6) {
		hints.ai_family = AF_INET6;
	    }
	    else {
		hints.ai_family = AF_UNSPEC;
	    }
	    hints.ai_socktype = SOCK_STREAM;
	    hints.ai_flags = AI_PASSIVE;
	    char port[6] = {0};
	    std::stringstream ss;
	    ss << _port;
	    ss >> port;
	    port[5] = 0;
	    int rv = getaddrinfo(NULL, port, &hints, &servinfo);
	    if (rv != 0) {
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
	template <protocol T = P, typename std::enable_if<is_udp_prot<T>::value, int>::type* = nullptr>
	server(unsigned short Port) : _port {Port}, _close_sockfd {true} {
	    addrinfo* servinfo, hints;
	    std::memset(&hints, 0, sizeof hints);
	    if (INETSTREAM_IPV == 4) {
		hints.ai_family = AF_INET;
	    }
	    else if (INETSTREAM_IPV == 6) {
		hints.ai_family = AF_INET6;
	    }
	    else {
		hints.ai_family = AF_UNSPEC;
	    }
	    hints.ai_socktype = SOCK_DGRAM;
	    hints.ai_flags = AI_PASSIVE;
	    char port[6] = {0};
	    std::stringstream ss;
	    ss << _port;
	    ss >> port;
	    port[5] = 0;
	    int rv = getaddrinfo(NULL, port, &hints, &servinfo);
	    if (rv != 0) {
		throw std::system_error {rv, std::system_category(), gai_strerror(rv)};
	    }
	    addrinfo* p;
	    for (p = servinfo; p != NULL; p = p->ai_next) {
		_socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (_socket_fd == -1) {
		    // swallow error
		    continue;
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
	}
	~server() { if (_close_sockfd) { close(_socket_fd); } }
	// enables "accept" if protocol is TCP
	template <protocol T = P>
	typename std::enable_if<is_tcp_prot<T>::value, inetstream<protocol::TCP>>::type accept() {
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
	    return inetstream<protocol::TCP> {new_fd};
	}
#if defined(__cpp_lib_optional) && (__cpp_lib_optional >= 201606L)
	template <protocol T = P>
	typename std::enable_if<is_tcp_prot<T>::value, std::optional<inetstream<protocol::TCP>> >::type
	accept(std::chrono::milliseconds timeout) {
	    auto now = []() { return std::chrono::system_clock::now(); };
	    auto t_end = now() + timeout;
	    // set own socket back to blocking mode
	    auto reset_flags = [this]() {
		int flags = fcntl(_socket_fd, F_GETFL);
		if (flags == -1)
		    throw std::system_error {errno, std::system_category(), strerror(errno)};
		if (flags & O_NONBLOCK)
		    flags ^= O_NONBLOCK;
		if (fcntl(_socket_fd, F_SETFL, flags) != 0)
		    throw std::system_error {errno, std::system_category(), strerror(errno)};
	    };
	    sockaddr_storage client_addr;
	    socklen_t sin_sz = sizeof client_addr;
	    int new_fd {-1};
	    // set own socket to non-blocking mode
	    if (fcntl(_socket_fd, F_SETFL, O_NONBLOCK) != 0) {
		throw std::system_error {errno, std::system_category(), strerror(errno)};
	    }
	    while (true) {
		// accept non-blocking
		new_fd = ::accept(_socket_fd, (sockaddr*)&client_addr, &sin_sz);
		// check newly created socket
		if (new_fd == -1) {
		    if (errno == EAGAIN || errno == EWOULDBLOCK) {
			if (now() < t_end) {
			    continue;
			}
			reset_flags();
			return {};
		    }
		    reset_flags();
		    throw std::system_error {errno, std::system_category(), strerror(errno)};
		}
		break;
	    }
	    reset_flags();
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
	    // success
	    return inetstream<protocol::TCP>{new_fd};
	}
#endif
	// enables "get_inetstream" if protocol is UDP
	template <protocol T = P>
	typename std::enable_if<is_udp_prot<T>::value, inetstream<protocol::UDP>>::type get_inetstream() {
	    // let inetstream handle closing socket -> transfer ownership
	    _close_sockfd = false;
	    return inetstream<protocol::UDP> {_socket_fd};
	}
    private:
	unsigned short _port;
	int _socket_fd;
	bool _close_sockfd;
    };
    
    template <protocol P>
    class client {
    public:
	template <protocol T = P, typename std::enable_if<is_tcp_prot<T>::value, int>::type* = nullptr>
	client(const std::string& Host, unsigned short Port)
	    : _host {Host}, _port {Port}, _close_sockfd {true} {
	    addrinfo* servinfo, hints;
	    std::memset(&hints, 0, sizeof hints);
	    if (INETSTREAM_IPV == 4) {
		hints.ai_family = AF_INET;
	    }
	    else if (INETSTREAM_IPV == 6) {
		hints.ai_family = AF_INET6;
	    }
	    else {
		hints.ai_family = AF_UNSPEC;
	    }
	    hints.ai_socktype = SOCK_STREAM;
	    char port[6] = {0};
	    std::stringstream ss;
	    ss << _port;
	    ss >> port;
	    port[5] = 0;
	    int rv = getaddrinfo(_host.c_str(), port, &hints, &servinfo);
	    if (rv != 0) {
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
	template <protocol T = P, typename std::enable_if<is_udp_prot<T>::value, int>::type* = nullptr>
	client (const std::string& Host, unsigned short Port)
	    : _host {Host}, _port {Port}, _close_sockfd {true} {
	    addrinfo* servinfo, hints;
	    std::memset(&hints, 0, sizeof hints);
	    if (INETSTREAM_IPV == 4) {
		hints.ai_family = AF_INET;
	    }
	    else if (INETSTREAM_IPV == 6) {
		hints.ai_family = AF_INET6;
	    }
	    else {
		hints.ai_family = AF_UNSPEC;
	    }
	    hints.ai_socktype = SOCK_DGRAM;
	    char port[6] = {0};
	    std::stringstream ss;
	    ss << _port;
	    ss >> port;
	    port[5] = 0;
	    int rv = getaddrinfo(_host.c_str(), port, &hints, &servinfo);
	    if (rv != 0) {
		throw std::system_error {rv, std::system_category(), gai_strerror(rv)};
	    }
	    addrinfo* p;
	    for (p = servinfo; p != NULL; p = p->ai_next) {
		_socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (_socket_fd == -1) {
		    // swallow error
		    continue;
		}
		break;
	    }
	    if (p == NULL) {
		freeaddrinfo(servinfo);
		throw std::system_error {errno, std::system_category(), strerror(errno)};
	    }
	    /// TODO: need to keep servinfo around until closing socket, since sendto
	    /// relies on servinfo->ai_addr, servinfo->ai_addrlen, ...
	    freeaddrinfo(servinfo);
	}
	~client() { if(_close_sockfd) { close(_socket_fd); } }
	// enables "connect" if protocol is TCP
	template <protocol T = P>
	typename std::enable_if<is_tcp_prot<T>::value, inetstream<protocol::TCP>>::type
	connect() {
	    _close_sockfd = false;
	    return inetstream<protocol::TCP> {_socket_fd};
	}
	// enables "get_inetstream" if protocol is UDP
	template <protocol T = P>
	typename std::enable_if<is_udp_prot<T>::value, inetstream<protocol::UDP>>::type
	get_inetstream() {
	    // TODO: investigate if this is sufficient
	    _close_sockfd = false;
	    return inetstream<protocol::UDP> {_socket_fd};
	}
    private:
	std::string _host;
	unsigned short _port;
	int _socket_fd;
	bool _created_inetstream;
	bool _close_sockfd;
    };
} // namespace inet
#endif
