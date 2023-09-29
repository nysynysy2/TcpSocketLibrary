#ifndef _NYSY_TCP_LIBRARY_
#define _NYSY_TCP_LIBRARY_
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <string>
#include <tuple>
#pragma comment(lib, "Ws2_32.lib")
namespace nysy {
	static const int BUF_SIZE = 1024;

	class TCPServer;
	class TCPClient;

	enum class ConnectionStatus {Success = 0, SystemError, InvalidError, Logout};

	class Connection {
		friend class TCPServer;
		friend class TCPClient;

		SOCKET com_fd;
	public:
		ConnectionStatus receive(std::string& data) {
			char buf[BUF_SIZE]{ 0 };
			data = "";
			auto recv_len = ::recv(com_fd,buf,BUF_SIZE,0);
			if (recv_len == SOCKET_ERROR)return ConnectionStatus::SystemError;
			if (recv_len == 0)return ConnectionStatus::Logout;
			data.append(buf,recv_len);
			unsigned long mode = 1;
			bool is_non_blocking = false;
			while (recv_len) {
				auto ioctl_res = ::ioctlsocket(com_fd,FIONBIO,&mode);
				if (ioctl_res == SOCKET_ERROR)return ConnectionStatus::SystemError;
				is_non_blocking = true;
				recv_len = ::recv(com_fd, buf, BUF_SIZE, 0);
				if (recv_len <= 0)break;
				data.append(buf,recv_len);
			}
			mode = 0;
			if (is_non_blocking) {
				auto ioctl_res = ::ioctlsocket(com_fd, FIONBIO, &mode);
				if (ioctl_res == SOCKET_ERROR)return ConnectionStatus::SystemError;
			}
			return ConnectionStatus::Success;
		}
		ConnectionStatus send(const std::string& data) {
			int sent_len = 0;
			int data_len = data.size();
			while (sent_len < data_len) {
				auto sent = ::send(com_fd, data.c_str() + sent_len, data_len - sent_len, 0);
				if (sent == SOCKET_ERROR)return ConnectionStatus::SystemError;
				sent_len += sent;
			}
		}
	};

	class TCPServer {
		friend class TCPClient;
		sockaddr_in serv_info;
		SOCKET listen_fd;
	public:
		TCPServer() :serv_info(), listen_fd() {}
		ConnectionStatus init(std::string ip_addr, short port) {
			listen_fd = ::socket(AF_INET,SOCK_STREAM,0);
			if (listen_fd == INVALID_SOCKET)return ConnectionStatus::SystemError;
			serv_info.sin_family = AF_INET;
			serv_info.sin_port = ::htons(port);
			if (::inet_pton(AF_INET, ip_addr.c_str(), &(serv_info.sin_addr)) != 1)return ConnectionStatus::InvalidError;
			if (::bind(listen_fd, reinterpret_cast<const sockaddr*>(&serv_info), sizeof serv_info) == SOCKET_ERROR)return ConnectionStatus::SystemError;
			return ConnectionStatus::Success;
		}
		ConnectionStatus listen(int backlog) {
			if (::listen(listen_fd, backlog) == SOCKET_ERROR)return ConnectionStatus::SystemError;
			return ConnectionStatus::Success;
		}
		std::tuple<ConnectionStatus, Connection, sockaddr_in> accept() {
			Connection client_connection{};
			sockaddr_in client_info{};
			ConnectionStatus stat = ConnectionStatus::Success;
			int addr_len = sizeof client_info;
			client_connection.com_fd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&client_info), &addr_len);
			if (client_connection.com_fd == INVALID_SOCKET)stat = ConnectionStatus::SystemError;
			return std::make_tuple(stat, client_connection, client_info);
		}

		~TCPServer() { ::closesocket(listen_fd); }
	};
	
	class TCPClient {
		friend class TCPServer;
		sockaddr_in serv_info;
		SOCKET com_fd;
	public:
		TCPClient() :serv_info(), com_fd() {}
		ConnectionStatus init(std::string ip_addr, short port) {
			com_fd = ::socket(AF_INET, SOCK_STREAM, 0);
			if (com_fd == INVALID_SOCKET)return ConnectionStatus::SystemError;
			serv_info.sin_family = AF_INET;
			serv_info.sin_port = ::htons(port);
			if (::inet_pton(AF_INET, ip_addr.c_str(), &(serv_info.sin_addr)) != 1)return ConnectionStatus::InvalidError;
			return ConnectionStatus::Success;
		}
		std::pair<ConnectionStatus, Connection> connect() {
			Connection serv_connection{};
			ConnectionStatus stat = ConnectionStatus::Success;
			auto connect_res = ::connect(com_fd, reinterpret_cast<const sockaddr*>(&serv_info), sizeof serv_info);
			if (connect_res == SOCKET_ERROR)stat = ConnectionStatus::SystemError;
			serv_connection.com_fd = com_fd;
			return std::make_pair(stat,serv_connection);
		}

		~TCPClient() { ::closesocket(com_fd); }
	};
}//namespace nysy
#endif