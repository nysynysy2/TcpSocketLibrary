#ifndef _NYSY_TCP_LIBRARY_
#define _NYSY_TCP_LIBRARY_
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <cassert>
#include <unistd.h>
#include <tuple>
#include <cstring>
namespace nysy
{
    static const int BUF_SIZE = 1024;

    class TCPServer;
    class TCPClient;

    enum class ConnectionStatus { Success = 0, SystemError, InvalidError, Logout };

    class Connection
    {
        friend class TCPServer;
        friend class TCPClient;

        bool is_closed;
        int com_fd;
    public:
        Connection() :com_fd(), is_closed(false) {}
        ConnectionStatus receive_once(std::string& data)
        {
            data = "";
            char buf[BUF_SIZE]{ 0 };
            int recv_len = ::recv(com_fd, buf, BUF_SIZE, 0);
            if (recv_len == 0)return ConnectionStatus::Logout;
            else if (recv_len == -1)return ConnectionStatus::SystemError;
            data.append(buf, recv_len);
            do
            {
                recv_len = recv(com_fd, buf, BUF_SIZE, MSG_DONTWAIT);
                if (recv_len > 0)data.append(buf, recv_len);
                //else if(recv_len == -1)return ConnectionStatus::Error;
            } while (recv_len > 0);
            return ConnectionStatus::Success;
        }
        ConnectionStatus receive_all(std::string& data)
        {
            data = "";
            int recv_len = 0;
            char buf[BUF_SIZE]{ 0 };
            do
            {
                recv_len = ::recv(com_fd, buf, BUF_SIZE, 0);
                if (recv_len == 0)break;
                if (recv_len == -1)return ConnectionStatus::SystemError;
                data.append(buf, recv_len);
            } while (recv_len > 0);
            return ConnectionStatus::Success;
        }
        ConnectionStatus send(const std::string& data)
        {
            int data_length = data.size();
            int sent_length = 0;
            while (sent_length < data_length)
            {
                auto res = ::send(com_fd, data.c_str() + sent_length, data_length - sent_length, 0);
                if (res == -1)return ConnectionStatus::SystemError;
                else sent_length += res;
            }
            return ConnectionStatus::Success;
        }
        void close_socket()
        {
            if(!is_closed)
            {
                ::close(com_fd);
                is_closed = true;
            }
        }
    };

    class TCPServer
    {
    public:
        TCPServer() :serv_addr(), listen_fd() {};

        ConnectionStatus init(unsigned short port = 80,std::string ip_addr = "0.0.0.0")
        {
            listen_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (listen_fd == -1)return ConnectionStatus::SystemError;  //return if failed to init
            int on = 1;
            if(::setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)) == -1)return ConnectionStatus::SystemError;
            serv_addr.sin_port = ::htons(port); //set socket's port
            serv_addr.sin_family = AF_INET;
            if (::inet_pton(AF_INET, ip_addr.c_str(), &(serv_addr.sin_addr)) != 1)return ConnectionStatus::InvalidError;
            if (::bind(listen_fd, reinterpret_cast<const sockaddr*>(&(serv_addr)),
                sizeof(serv_addr)) == -1)return ConnectionStatus::SystemError;
            return ConnectionStatus::Success;
        }

        ConnectionStatus listen(int flag)
        {
            if (::listen(listen_fd, flag) == -1)return ConnectionStatus::SystemError;
            return ConnectionStatus::Success;
        }

        std::tuple<ConnectionStatus, Connection, sockaddr_in> accept()
        {
            sockaddr_in client_addr{};
            Connection client_connection{};
            int client_size = sizeof(client_addr);
            client_connection.com_fd = ::accept(listen_fd,
                reinterpret_cast<struct sockaddr*>(&(client_addr)),
                reinterpret_cast<socklen_t*>(&client_size));  //block and wait for client to connect
            if (client_connection.com_fd == -1)return std::make_tuple(ConnectionStatus::SystemError, client_connection, client_addr);
            else return std::make_tuple(ConnectionStatus::Success, client_connection, client_addr);
        }

        ~TCPServer() { ::close(listen_fd); }

    private:
        sockaddr_in serv_addr;
        int listen_fd;
    };

    class TCPClient
    {
    public:
        TCPClient() :com_fd(), serv_addr() {}
        ConnectionStatus init(unsigned short port,std::string ip_addr)
        {
            com_fd = ::socket(AF_INET, SOCK_STREAM, 0);
            if (com_fd == -1)return ConnectionStatus::SystemError;
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(port);
            if (::inet_pton(AF_INET, ip_addr.c_str(),
                &(serv_addr.sin_addr)) != 1)return ConnectionStatus::InvalidError;
            return ConnectionStatus::Success;
        }
        std::pair<ConnectionStatus, Connection> connect()
        {
            Connection server_connection{};
            server_connection.com_fd = com_fd;
            ConnectionStatus stat;
            if (::connect(com_fd, (const sockaddr*)&(serv_addr),
                sizeof(serv_addr)) == -1)stat = ConnectionStatus::SystemError;
            else stat = ConnectionStatus::Success;
            return std::make_pair(stat, server_connection);
        }

        /*~TCPClient()
        {
            ::close(com_fd);
        }*/
    private:
        int com_fd;
        sockaddr_in serv_addr;
    };
}//namespace nysy
#endif
