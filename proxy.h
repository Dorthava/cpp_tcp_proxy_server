#ifndef PROXY_H
#define PROXY_H

#define POLL_SIZE 256

#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <list>

class ProxyServer {
public:
  ProxyServer() = delete;
  explicit ProxyServer(int proxy_port);
  ProxyServer(const ProxyServer &other) = delete;
  ProxyServer(ProxyServer &&other) = delete;
  ProxyServer &operator=(const ProxyServer &other) = delete;
  ProxyServer &operator=(ProxyServer &&other) = delete;
  ~ProxyServer() = default;

  void RunServer(const std::string &server_address, int server_port);
  // auto& HandleClient(std::ofstream& file, int sockfd);

private:
  class Socket {
  public:
    Socket();
    Socket(const Socket &other) = delete;
    Socket(Socket &&other) noexcept;
    Socket &operator=(const Socket &other) = delete;
    Socket &operator=(Socket &&other) noexcept = default;
    ~Socket();

    int SetBind(const std::string &address, int port);
    int SetConnect(const std::string &address, int port);
    bool SetAccept(int proxy_socket);

    int GetDescriptor() const;
    sockaddr_in GetAddress() const noexcept;

  private:
    struct sockaddr_in sock_addr_;
    int sockfd_;
  };

  bool CreateProxyServer(int proxy_port);
  bool ConnectionServer(const std::string &server_address, int server_port);
  void AcceptConnection(const std::string &server_address, int server_port);
  bool HandleServer(std::list<Socket>::iterator &server_iter);
  bool HandleClient(std::list<Socket>::iterator &client_iter,
                    const std::list<std::string> &keywords,
                    std::ofstream &file);
  std::string GetSocketName(const Socket &sockfd);

  std::list<Socket> all_sockets_;
  bool server_state_;
};

// #include "proxy.cpp"
#endif // PROXY_H
