#ifndef INETSTREAM_HPP_
#define INETSTREAM_HPP_
// C++
#include <sstream>
#include <iostream>
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
#ifndef INET_MAX_CONNECTIONS
#define INET_MAX_CONNECTIONS 10
#endif

#ifndef INET_MAX_SEND_TIMEOUT_MS
#define INET_MAX_SEND_TIMEOUT_MS 1000
#endif

#ifndef INET_MAX_RECV_TIMEOUT_MS
#define INET_MAX_RECV_TIMEOUT_MS 1000
#endif
// set to 6 to use IPv6 instead of IPv4
#ifndef INET_IPV
#define INET_IPV 4
#endif

#ifndef INET_USE_DEFAULT_SIGUSR1_HANDLER
#define INET_USE_DEFAULT_SIGUSR1_HANDLER false
#endif

#ifndef INET_PRINT_SIGUSR1
#define INET_PRINT_SIGUSR1 false
#endif

namespace inet {
	inline void sigusr1_handler(int signal) {
		if (INET_PRINT_SIGUSR1) {
			std::cout << "inet::sigusr1_handler() was called (signal=" << signal << ")" << std::endl;
		}
	}

	inline void* get_in_addr(sockaddr* sa) {
		if (sa->sa_family == AF_INET) {
			return &((reinterpret_cast<sockaddr_in*>(sa))->sin_addr);
		}
		return &((reinterpret_cast<sockaddr_in6*>(sa))->sin6_addr);
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
	struct addrinfos {
		struct addrinfo* infos, *p;
	};
	template <protocol P> class server;
	template <protocol P> class client;
	template <protocol P>
	class inetstream {
	public:
		inetstream() = delete;
		inetstream(const inetstream<P>&) = delete;
		inetstream(inetstream<P>&& other) : _socket_fd {other._socket_fd} {
			other._socket_fd = -1;
			_addrinfos.infos = other._addrinfos.infos;
			_addrinfos.p = other._addrinfos.p;
			other._addrinfos.infos = other._addrinfos.p = nullptr;
			auto pos = other.size() - 1;
			_send_buf = std::move(other._send_buf);
			_recv_buf = std::move(other._recv_buf);
			_read_pos = _recv_buf.begin() + pos;
		}
		~inetstream() {
			if (_owns) {
				if (_socket_fd != -1) {
					close(_socket_fd);
				}
				freeaddrinfo(_addrinfos.infos);
				_addrinfos.infos = _addrinfos.p = nullptr;
			}
		}
		/**
		 * push data onto the stream
		 *
		 */
		template <typename T>
		typename std::enable_if<!std::is_same<T, uint16_t>::value
		                        && !std::is_same<T, uint32_t>::value
		                        && !std::is_same<T, uint64_t>::value
		                        && !std::is_same<T, int16_t>::value
		                        && !std::is_same<T, int32_t>::value
		                        && !std::is_same<T, int64_t>::value
		                        && !std::is_same<T, float>::value
		                        && !std::is_same<T, double>::value
		                        && !std::is_same<T, std::string>::value
		                        && !std::is_same<typename std::remove_const<T>::type, char[]>::value
		                        && !std::is_same<typename std::remove_const<T>::type, char*>::value,
		                        inetstream<P>&>::type
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
		                        && !std::is_same<T, int16_t>::value
		                        && !std::is_same<T, int32_t>::value
		                        && !std::is_same<T, int64_t>::value
		                        && !std::is_same<T, float>::value
		                        && !std::is_same<T, double>::value
		                        && !std::is_same<T, std::string>::value
		                        && !std::is_same<typename std::remove_const<T>::type, char[]>::value
		                        && !std::is_same<typename std::remove_const<T>::type, char*>::value, void>::type
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
		inetstream<P>& operator<< (int16_t s) {
			return this->operator<<(static_cast<uint16_t>(s));
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
		inetstream<P>& operator<< (int32_t l) {
			return this->operator<<(static_cast<uint32_t>(l));
		}
		inetstream<P>& operator<< (uint64_t ull) {
			constexpr std::size_t sz = sizeof ull;
			union {
				uint64_t i;
				byte b[sz];
			} u {0};
			u.i  = static_cast<uint64_t>(htonl((ull & 0xffffffff00000000) >> 32));
			u.i |= static_cast<uint64_t>(htonl(ull & 0x00000000ffffffff)) << 32;
			for (std::size_t s {0}; s < sz; ++s) {
				this->_send_buf.push_back(u.b[s]);
			}
			return *this;
		}
		inetstream<P>& operator<< (int64_t ll) {
			return this->operator<<(static_cast<uint64_t>(ll));
		}
		inetstream<P>& operator<< (float f) {
			static_assert(sizeof(float) == sizeof(uint32_t), "sizeof float not supported");
			uint32_t ul {};
			std::memcpy(&ul, &f, sizeof(float));
			*this << ul;
			return *this;
		}
		inetstream<P>& operator<< (double d) {
			static_assert(sizeof(double) == sizeof(uint64_t), "sizeof double not supported");
			uint64_t ull {};
			std::memcpy(&ull, &d, sizeof(double));
			*this << ull;
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
		void operator>> (int16_t& s) {
			uint16_t us {};
			this->operator>>(us);
			s = static_cast<int16_t>(us);
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
		void operator>> (int32_t& l) {
			uint32_t ul {};
			this->operator>>(ul);
			l = static_cast<int32_t>(ul);
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
			// msb
			*this >> tmp;
			utmp = static_cast<uint64_t>(tmp) << 32;
			// lsb
			*this >> tmp;
			// bitshift operators already take care of ntohl stuff
			ull = utmp | tmp;
		}
		void operator>> (int64_t& ll) {
			uint64_t ull {};
			this->operator>>(ull);
			ll = static_cast<int64_t>(ull);
		}
		void operator>> (float& f) {
			static_assert(sizeof(float) == sizeof(uint32_t), "sizeof float not supported");
			uint32_t ul {};
			*this >> ul;
			std::memcpy(&f, &ul, sizeof(float));
		}
		void operator>> (double& d) {
			static_assert(sizeof(double) == sizeof(uint64_t), "sizeof double not supported");
			uint64_t ull {};
			*this >> ull;
			std::memcpy(&d, &ull, sizeof(double));
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
		template <protocol T = P>
		typename std::enable_if<is_tcp_prot<T>::value, void>::type
		send() {
			std::chrono::milliseconds timeout {INET_MAX_SEND_TIMEOUT_MS};
			auto t_end = std::chrono::system_clock::now() + timeout;
			auto total = _send_buf.size();
			do {
				int sent_this_iter = 0;
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
		 * sends data pushed into the stream over the network to remote
		 *
		 */
		template <protocol T = P>
		typename std::enable_if<is_udp_prot<T>::value, void>::type
		send() {
			std::chrono::milliseconds timeout {INET_MAX_SEND_TIMEOUT_MS};
			auto t_end = std::chrono::system_clock::now() + timeout;
			auto total = _send_buf.size();
			do {
				int sent_this_iter =
					::sendto(_socket_fd, &_send_buf[_send_buf.size() - total], total, 0,
					         _addrinfos.p->ai_addr, _addrinfos.p->ai_addrlen);
				if (sent_this_iter == -1) {
					throw std::system_error {errno, std::system_category(), strerror(errno)};
				}
				if (std::chrono::system_clock::now() > t_end) {
					throw std::runtime_error {"timeout reached"};
				}
				total -= sent_this_iter;
			} while (total > 0);
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
			std::chrono::milliseconds timeout {INET_MAX_RECV_TIMEOUT_MS};
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
				if (read == 0) {
					break;
				}
				if (read > 0) {
					sz -= read;
				}
				_recv_buf.insert(_recv_buf.end(), buf.begin(), buf.begin() + read);
			} while (sz && std::chrono::system_clock::now() < t_end);
			_read_pos = _recv_buf.begin() + read_offset_;
			return _recv_buf.size() - tmp_size;
		}    
		/** 
		 * receives data from network and stores it in stream
		 *
		 * if data is available to receive, will return early after receiving
		 *
		 * @throws std::system_error if ::recv() encountered an error
		 *
		 * @return number of bytes received
		 */
		template <protocol T = P>
		typename std::enable_if<is_udp_prot<T>::value, std::size_t>::type
		recv() {
			std::chrono::milliseconds timeout {INET_MAX_RECV_TIMEOUT_MS};
			// cache offset because recv may cause _recv_buf to realloc
			auto read_offset_ = _read_pos - _recv_buf.begin(); 
			struct sockaddr_storage remote_addr;
			socklen_t addr_len = sizeof(remote_addr);
			constexpr const std::size_t SZ {1024};
			std::array<unsigned char, SZ> buf;
			int num_recv {0};
			if (this->select(timeout)) {
				num_recv = ::recvfrom(_socket_fd, &buf[0], SZ - 1, 0,
				                      reinterpret_cast<struct sockaddr*>(&remote_addr), &addr_len);
			}
			if (num_recv == -1) {
				throw std::system_error {errno, std::system_category(), strerror(errno)};
			}
			if (num_recv == 0)
				return 0;
			_recv_buf.insert(_recv_buf.end(), buf.begin(), buf.begin() + num_recv);
			_read_pos = _recv_buf.begin() + read_offset_;
			return num_recv;
		}
		bool empty() const { return size() == 0; }
		void clear() { _send_buf.clear(); _recv_buf.clear(); _read_pos = _recv_buf.begin(); }
		std::size_t size() const { return _recv_buf.end() - _read_pos; }
		/** 
		 * blocks while now() < time_of_call + timeout and checks if data can be
		 * recv()-ed
		 *
		 * @param timeout duration for which to wait for data to arrive
		 *
		 * @return early true if data can be recv()-ed or false after timeout
		 * expired otherwise
		 *
		 * @throws std::system_error if ::select() encounters an error
		 *
		 */
		bool select(std::chrono::milliseconds timeout) const {
			struct timeval t {};
			t.tv_sec = timeout.count() / 1000;
			t.tv_usec = (timeout.count() - t.tv_sec * 1000) * 1000;
			fd_set rfds {};
			FD_ZERO(&rfds);
			FD_SET(_socket_fd, &rfds);
			int rv = ::select(_socket_fd + 1, &rfds, nullptr, nullptr, &t);
			if (rv < 0) {
				// error occured
				throw std::system_error {errno, std::system_category(), strerror(errno)};
			}
			// 0 on timeout, 1 if socket is ready to recv
			return static_cast<bool>(rv);
		}
	private:
		inetstream(int socket_fd, addrinfos addrinfos, bool owns) :
			_socket_fd {socket_fd}, _addrinfos {addrinfos}, _read_pos {_recv_buf.begin()}, _owns {owns} {}
		friend class server<P>;
		friend class client<P>;
		int _socket_fd;
		addrinfos _addrinfos;
		std::vector<byte> _send_buf;
		std::vector<byte> _recv_buf;
		std::vector<byte>::iterator _read_pos;
		bool _owns;
	};

	template <protocol P>
	class server {
	public:
		template <protocol T = P, typename std::enable_if<is_tcp_prot<T>::value, int>::type* = nullptr>
		server(unsigned short Port) : _port {Port}, _addrinfos {nullptr, nullptr} {
			if (INET_USE_DEFAULT_SIGUSR1_HANDLER) {
				struct sigaction sa;
				sa.sa_handler = sigusr1_handler;
				sigemptyset(&sa.sa_mask);
				sa.sa_flags = 0;
				if (sigaction(SIGUSR1, &sa, NULL) != 0) {
					throw std::system_error {errno, std::system_category(), strerror(errno)};
				}
			}
			addrinfo hints;
			std::memset(&hints, 0, sizeof hints);
			if (INET_IPV == 4) {
				hints.ai_family = AF_INET;
			}
			else if (INET_IPV == 6) {
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
			int rv = getaddrinfo(NULL, port, &hints, &_addrinfos.infos);
			if (rv != 0) {
				throw std::system_error {rv, std::system_category(), gai_strerror(rv)};
			}
			for (_addrinfos.p = _addrinfos.infos; _addrinfos.p != NULL; _addrinfos.p = _addrinfos.p->ai_next) {
				_socket_fd = socket(_addrinfos.p->ai_family, _addrinfos.p->ai_socktype, _addrinfos.p->ai_protocol);
				if (_socket_fd == -1) {
					// swallow error
					continue;
				}
				int yes = 1; rv = 0;
				rv = setsockopt(_socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
				if (rv == -1) {
					throw std::system_error {errno, std::system_category(), strerror(errno)};
				}
				if (bind(_socket_fd, _addrinfos.p->ai_addr, _addrinfos.p->ai_addrlen) == -1) {
					close(_socket_fd);
					// swallow error
					continue;
				}
				break;
			}
			if (_addrinfos.p == NULL) {
				throw std::system_error {errno, std::system_category(), strerror(errno)};
			}
			if (listen(_socket_fd, INET_MAX_CONNECTIONS) == -1) {
				throw std::system_error {errno, std::system_category(), strerror(errno)};
			}
		}
		template <protocol T = P, typename std::enable_if<is_udp_prot<T>::value, int>::type* = nullptr>
		server(unsigned short Port) : _port {Port}, _addrinfos {nullptr, nullptr} {
			addrinfo hints;
			std::memset(&hints, 0, sizeof hints);
			if (INET_IPV == 4) {
				hints.ai_family = AF_INET;
			}
			else if (INET_IPV == 6) {
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
			int rv = getaddrinfo(NULL, port, &hints, &_addrinfos.infos);
			if (rv != 0) {
				throw std::system_error {rv, std::system_category(), gai_strerror(rv)};
			}
			for (_addrinfos.p = _addrinfos.infos; _addrinfos.p != NULL; _addrinfos.p = _addrinfos.p->ai_next) {
				_socket_fd = socket(_addrinfos.p->ai_family, _addrinfos.p->ai_socktype, _addrinfos.p->ai_protocol);
				if (_socket_fd == -1) {
					// swallow error
					continue;
				}
				if (bind(_socket_fd, _addrinfos.p->ai_addr, _addrinfos.p->ai_addrlen) == -1) {
					close(_socket_fd);
					// swallow error
					continue;
				}
				break;
			}
			if (_addrinfos.p == NULL) {
				freeaddrinfo(_addrinfos.infos);
				throw std::system_error {errno, std::system_category(), strerror(errno)};
			}
		}
		~server() {
			close(_socket_fd); _socket_fd = -1;
			freeaddrinfo(_addrinfos.infos);
			_addrinfos.infos = _addrinfos.p = nullptr;
		}
		/**
		 * set the underlying socket to non-blocking mode.
		 * @throws std::system_error if an error occurs
		 */
		template <protocol T = P>
		typename std::enable_if<is_tcp_prot<T>::value>::type set_nonblocking() {
			if (fcntl(_socket_fd, F_SETFD, O_NONBLOCK)) {
				throw std::system_error {errno, std::system_category(), strerror(errno)};
			}
		}
		/**
		 * if using select() before set_nonblocking() has been called, the
		 * behaviour is undefined.
		 *
		 * blocks while now() < time_of_call + timeout and checks if a client
		 * can be accept()-ed without blocking.
		 *
		 * @param timeout duration for which to wait for client to connect()
		 *
		 * @return early true if client can be accept()-ed or false after
		 * timeout expired otherwise
		 *
		 * @throws std::system_error if ::select() encounters an error
		 *
		 */
		template <protocol T = P>
		typename std::enable_if<is_tcp_prot<T>::value, bool>::type
		select(std::chrono::milliseconds timeout) const {
			struct timeval t {};
			t.tv_sec = timeout.count() / 1000;
			t.tv_usec = (timeout.count() - t.tv_sec * 1000) * 1000;
			fd_set rfds {};
			FD_ZERO(&rfds);
			FD_SET(_socket_fd, &rfds);
			int rv = ::select(_socket_fd + 1, &rfds, nullptr, nullptr, &t);
			if (rv == 0) {
				return false;
			}
			if (rv > 0) {
				return true;
			}
			throw std::system_error {errno, std::system_category(), strerror(errno)};
		}
		/** 
		 * blocks until a client connects. 
		 *
		 * @throws std::system_error if ::accept() was interrupted by a signal
		 * or the connection could not established cleanly
		 *
		 * @return inetstream to the connected client
		 */
		// enables "accept" if protocol is TCP
		template <protocol T = P>
		typename std::enable_if<is_tcp_prot<T>::value, inetstream<protocol::TCP>>::type accept() {
			sockaddr_storage client_addr;
			socklen_t sin_sz = sizeof client_addr;
			int new_fd = ::accept(_socket_fd, reinterpret_cast<sockaddr*>(&client_addr), &sin_sz);
			if (new_fd == -1) {
				throw std::system_error {errno, std::system_category(), strerror(errno)};
			}
			char s[INET6_ADDRSTRLEN];
			const char* rv = inet_ntop(client_addr.ss_family,
			                           get_in_addr(reinterpret_cast<sockaddr*>(&client_addr)),
			                           s, sizeof s);
			if (rv == NULL) {
				close(new_fd);
				throw std::system_error {errno, std::system_category(), strerror(errno)};
			}
			if (fcntl(new_fd, F_SETFL, O_NONBLOCK) != 0) {
				close(new_fd);
				throw std::system_error {errno, std::system_category(), strerror(errno)};
			}
			// connected to s
			return inetstream<protocol::TCP> {new_fd, {nullptr, nullptr}, /*owns*/true};
		}
		/** 
		 *
		 * @return inetstream to this end of the communication
		 */
		// enables "get_inetstream" if protocol is UDP
		template <protocol T = P>
		typename std::enable_if<is_udp_prot<T>::value, inetstream<protocol::UDP>>::type
		get_inetstream() {
			// don't transfer ownership
			return inetstream<protocol::UDP> {_socket_fd, _addrinfos, /*owns*/false};
		}
	private:
		unsigned short _port;
		int _socket_fd;
		addrinfos _addrinfos;
	};
    
	template <protocol P>
	class client {
	public:
		template <protocol T = P, typename std::enable_if<is_tcp_prot<T>::value, int>::type* = nullptr>
		client(const std::string& Host, unsigned short Port)
			: _host {Host}, _port {Port} {}
		template <protocol T = P, typename std::enable_if<is_udp_prot<T>::value, int>::type* = nullptr>
		client (const std::string& Host, unsigned short Port)
			: _host {Host}, _port {Port} {}
		/** 
		 * connect the client to the server specified via the constructor
		 *
		 * @throws std::system_error if connection was not possible for some
		 * reason
		 *
		 * @return inetstream to the connected server
		 */
		// enables "connect" if protocol is TCP
		template <protocol T = P>
		typename std::enable_if<is_tcp_prot<T>::value, inetstream<protocol::TCP>>::type
		connect() {
			addrinfo hints;
			std::memset(&hints, 0, sizeof hints);
			if (INET_IPV == 4) {
				hints.ai_family = AF_INET;
			}
			else if (INET_IPV == 6) {
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
			addrinfo* infos, *p;
			int rv = getaddrinfo(_host.c_str(), port, &hints, &infos);
			if (rv != 0) {
				throw std::system_error {rv, std::system_category(), gai_strerror(rv)};
			}
			for (p = infos; p != NULL; p = p->ai_next) {
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
				freeaddrinfo(infos);
				throw std::system_error {errno, std::system_category(), strerror(errno)};
			}
			char s[INET6_ADDRSTRLEN];
			const char* rvp = inet_ntop(
				p->ai_family,
				get_in_addr(reinterpret_cast<sockaddr*>(p->ai_addr)),
				s, sizeof s);
			if (rvp == NULL) {
				freeaddrinfo(infos);
				throw std::system_error {errno, std::system_category(), strerror(errno)};
			}
			if (fcntl(_socket_fd, F_SETFL, O_NONBLOCK) != 0) {
				freeaddrinfo(infos);
				throw std::system_error {errno, std::system_category(), strerror(errno)};
			}
			return inetstream<protocol::TCP> {_socket_fd, addrinfos {infos, p}, /*owns*/true};
		}
		/** 
		 * @throws std::system_error if communication could not be established
		 * @return inetstream to this end of the communication
		 */
		// enables "get_inetstream" if protocol is UDP
		template <protocol T = P>
		typename std::enable_if<is_udp_prot<T>::value, inetstream<protocol::UDP>>::type
		get_inetstream() {
			addrinfo hints;
			std::memset(&hints, 0, sizeof hints);
			if (INET_IPV == 4) {
				hints.ai_family = AF_INET;
			}
			else if (INET_IPV == 6) {
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
			addrinfo* infos, *p;
			int rv = getaddrinfo(_host.c_str(), port, &hints, &infos);
			if (rv != 0) {
				throw std::system_error {rv, std::system_category(), gai_strerror(rv)};
			}
			for (p = infos; p != NULL; p = p->ai_next) {
				_socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
				if (_socket_fd == -1) {
					// swallow error
					continue;
				}
				break;
			}
			if (p == NULL) {
				freeaddrinfo(infos);
				throw std::system_error {errno, std::system_category(), strerror(errno)};
			}
			return inetstream<protocol::UDP> {_socket_fd, addrinfos {infos, p}, /*owns*/true};
		}
	private:
		std::string _host;
		unsigned short _port;
		int _socket_fd;
	};
} // namespace inet
#endif
