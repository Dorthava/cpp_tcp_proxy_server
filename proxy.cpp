#include "proxy.h"

ProxyServer::Socket::Socket() : sock_addr_() {
  sockfd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sockfd_ != -1) {
    memset(&sock_addr_, 0, sizeof(sock_addr_));
  }
}

ProxyServer::Socket::Socket(Socket &&other) noexcept : sock_addr_() {
  sockfd_ = other.sockfd_;
  other.sockfd_ = -1;
  memcpy(&sock_addr_, &other.sock_addr_, sizeof(sock_addr_));
}

ProxyServer::Socket::~Socket() {
  if (sockfd_ != -1) {
    shutdown(sockfd_, SHUT_RDWR);
    close(sockfd_);
  }
}

int ProxyServer::Socket::SetBind(const std::string &address, int port) {
  sock_addr_.sin_family = AF_INET;
  sock_addr_.sin_port = htons(port);
  sock_addr_.sin_addr.s_addr = inet_addr(address.c_str());
  return bind(sockfd_, reinterpret_cast<sockaddr *>(&sock_addr_),
              sizeof(sock_addr_));
}

int ProxyServer::Socket::SetConnect(const std::string &address, int port) {
  sock_addr_.sin_family = AF_INET;
  sock_addr_.sin_port = htons(port);
  sock_addr_.sin_addr.s_addr = inet_addr(address.c_str());

  return connect(sockfd_, reinterpret_cast<sockaddr *>(&sock_addr_),
                 sizeof(sock_addr_));
}

bool ProxyServer::Socket::SetAccept(int proxy_socket) {
  socklen_t address_length = sizeof(sock_addr_);
  if ((sockfd_ = accept(proxy_socket, reinterpret_cast<sockaddr *>(&sock_addr_),
                        &address_length)) == -1) {
    std::cout << "Ошибка при попытке принять соединение от клиента. ";
  }
  return sockfd_ != -1;
}

int ProxyServer::Socket::GetDescriptor() const { return sockfd_; }

sockaddr_in ProxyServer::Socket::GetAddress() const noexcept {
  return sock_addr_;
}

ProxyServer::ProxyServer(int proxy_port) : all_sockets_(), server_state_(true) {
  if (!CreateProxyServer(proxy_port)) {
    server_state_ = false;
  }
}

bool ProxyServer::CreateProxyServer(int proxy_port) {
  Socket proxy_socket;
  if (proxy_socket.GetDescriptor() == -1) {
    perror("\"Ошибка при попытке проинициализировать сокет. (Сокет "
           "прокси-сервера).\n");
    return false;
  }

  if (proxy_socket.SetBind("127.0.0.1", proxy_port) == -1) {
    perror("Ошибка при привязке сокета к конкретному адресу и порту. (Сокет "
           "прокси-сервера).\n");
    return false;
  }

  if (listen(proxy_socket.GetDescriptor(), SOMAXCONN) == -1) {
    perror("Невозможно установить сокет в режим прослушивания. (Сокет "
           "прокси-сервера).\n");
    return false;
  }

  all_sockets_.push_back(std::move(proxy_socket));
  return true;
}

bool ProxyServer::ConnectionServer(const std::string &server_address,
                                   int server_port) {
  Socket server_socket;
  if (server_socket.GetDescriptor() == -1) {
    perror("Ошибка при попытке проинициализировать сокет. (Сокет сервера "
           "назначения).\n");
    return false;
  }

  if (server_socket.SetConnect(server_address, server_port) == -1) {
    perror("Невозможно подключиться к серверу назначения. (Сокет сервера "
           "назначения).\n");
    return false;
  }

  all_sockets_.push_back(std::move(server_socket));
  return true;
}

void ProxyServer::AcceptConnection(const std::string &server_address,
                                   int server_port) {
  Socket slave_socket;

  if (!ConnectionServer(server_address, server_port)) {
    std::cout << "Завершение работы..." << std::endl;
  }

  if (slave_socket.SetAccept(all_sockets_.front().GetDescriptor())) {
    std::cout << "Удалось подключить клиента: " << GetSocketName(slave_socket)
              << " к прокси-серверу." << std::endl;
    all_sockets_.push_back(std::move(slave_socket));
  } else {
    std::cout << GetSocketName(slave_socket) << std::endl;
  }
}

bool ProxyServer::HandleServer(std::list<Socket>::iterator &server_iter) {
  auto client_iter = server_iter;
  ++client_iter;
  char buffer[1024];
  memset(buffer, 0, 1024);

  ssize_t bytes_read;
  if ((bytes_read = recv(server_iter->GetDescriptor(), buffer, sizeof(buffer),
                         0)) == -1) {
    std::cout << "Ошибка при попытке прочитать данные конечного сервера."
              << std::endl;
    return false;
  }

  if (send(client_iter->GetDescriptor(), buffer, bytes_read, 0) == -1) {
    std::cout << "Ошибка при попытке передать данные на сервер." << std::endl;
    return false;
  }

  return true;
}

bool ProxyServer::HandleClient(std::list<Socket>::iterator &client_iter,
                               const std::list<std::string> &keywords,
                               std::ofstream &file) {
  auto server_iter = client_iter;
  --server_iter;
  char buffer[1024];
  memset(buffer, 0, 1024);

  ssize_t bytes_read =
      recv(client_iter->GetDescriptor(), buffer, sizeof(buffer), MSG_NOSIGNAL);
  if (bytes_read <= 0) {
    if (bytes_read == -1) {
      std::cout << "Ошибка при чтении данных с клиента." << std::endl;
    }
    return false;
  }

  std::string words(buffer, bytes_read);
  // Если передана команда "stop;" с клиента - работа прокси-сервера
  // завершается;
  if (words.find("stop") != std::string::npos) {
    server_state_ = false;
  }

  for (auto it = words.begin(); it != words.end();) {
    if (!std::isprint(*it) || *it == '\n') {
      it = words.erase(it);
    } else {
      ++it;
    }
  }
  bool result = false;
  for (const std::string &keyword : keywords) {
    if (words.find(keyword) != std::string::npos) {
      result = true;
    }
  }
  if (result) {
    if (words[0] == 'Q')
      words.erase(words.begin());
    file << "Запрос от клиента: " << GetSocketName((*client_iter))
         << ". SQL-запрос: \"" << words << '\"' << std::endl;
    std::cout << "Запрос от клиента: " << GetSocketName((*client_iter))
              << ". SQL-запрос: \"" << words << '\"' << std::endl;
  }

  if (send(server_iter->GetDescriptor(), buffer, bytes_read, 0) == -1) {
    std::cout << "Ошибка при отправке данных от клиента: "
              << GetSocketName(*client_iter) << " к конечному серверу."
              << std::endl;
    return false;
  }
  return true;
}

std::string ProxyServer::GetSocketName(const Socket &sockfd) {
  std::string result = inet_ntoa(sockfd.GetAddress().sin_addr);
  result.insert(0, 1, '[');
  int clientPort = ntohs(sockfd.GetAddress().sin_port);
  result += ":" + std::to_string(clientPort) + "]";
  return result;
}

void ProxyServer::RunServer(const std::string &server_address,
                            int server_port) {
  const std::list<std::string> &keywords = {
      "SELECT", "UPDATE", "DELETE", "INSERT", "CREATE", "DROP",     "RENAME",
      "ALTER",  "GRANT",  "REVOKE", "DENY",   "COMMIT", "ROLLBACK", "BEGIN"};
  struct pollfd poll_sockets[POLL_SIZE];
  std::ofstream file;
  file.open("file.log");
  if (!file.is_open()) {
    std::cout << "Не удалось открыть .log файл." << std::endl;
  }
  while (server_state_) {
    unsigned int index = 0;
    for (auto &iter : all_sockets_) {
      poll_sockets[index].fd = iter.GetDescriptor();
      poll_sockets[index++].events = POLLIN;
    }
    if (poll(poll_sockets, all_sockets_.size(), -1) == -1) {
      std::cout << "Ошибка при работе функции poll()." << std::endl;
      break;
    }

    size_t poll_size = all_sockets_.size();
    auto iter = all_sockets_.begin();
    for (size_t i = 0; i != poll_size; ++i) {
      if (poll_sockets[i].revents & POLLIN) {
        bool check_result = true;
        if (i == 0) {
          AcceptConnection(server_address, server_port);
        } else if ((i % 2) == 1) {
          check_result = HandleServer(iter);
        } else {
          check_result = HandleClient(iter, keywords, file);
        }
        if (!check_result) {
          iter = all_sockets_.erase(iter);
          if (i % 2) {
            iter = all_sockets_.erase(iter);
          } else {
            iter = all_sockets_.erase(--iter);
          }
          --iter;
        }
      }
      ++iter;
    }
  }
  file.close();
}
