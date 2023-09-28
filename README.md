# TcpSocketLibrary


A TCP Socket Library(With a threadpool)

class:

```cpp
TCPClient
TCPServer
Connection
```

functions:

```cpp
TCPServer::init(std::string ip,short port);
TCPClient the same

TCPServer::listen(int back_log);
std::tuple<ConnectionStatus,Connection,sockaddr_in> TCPServer::accept();

std::pair<ConnectionStatus,Connection> TCPClient::connect();

ConnectionStatus Connection::receive(std::string& data);
ConnectionStatus Connection::send(const std::string& data);
```
