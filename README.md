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

ConnectionStatus Connection::receive_once(std::string& data);  //receive the data once the sender send the data
ConnectionStatus Connection::receive_all(std::string& data);  //receive the data after the sender close the connection
ConnectionStatus Connection::send(const std::string& data);
```
