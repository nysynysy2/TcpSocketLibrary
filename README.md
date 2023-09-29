# TcpSocketLibrary


A Header-only C++ TCP Socket Library

class:

```cpp
class TCPClient;
class TCPServer;
class Connection;
enum class ConnectionStatus{Success = 0, SystenError, InvalidError, Logout};
```

functions:

```cpp
ConnectionStatus TCPServer::init(unsigned short port = 80,std::string ip="0.0.0.0");
ConnectionStatus TCPClient::init(unsigned short port,std::string ip);

ConnectionStatus TCPServer::listen(int back_log);
std::tuple<ConnectionStatus,Connection,sockaddr_in> TCPServer::accept();

std::pair<ConnectionStatus,Connection> TCPClient::connect();

ConnectionStatus Connection::receive_once(std::string& data);  //receive the data once the sender send the data
ConnectionStatus Connection::receive_all(std::string& data);  //receive the data after the sender close the connection
ConnectionStatus Connection::send(const std::string& data);
```
