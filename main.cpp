#include "proxy.h"

int main(int argc, char **argv) {
  if (argc == 4) {
    ProxyServer server{std::stoi(argv[1])};
    server.RunServer(argv[3], std::stoi(argv[2]));
  } else {
    std::cout << "Недостаточно аргументов передано на вход." << std::endl;
    std::cout << "Необходимо передать их в порядке: 1) Порт прокси-сервера, 2) "
                 "Порт сервера назначения, 3) Адрес сервера."
              << std::endl;
  }
  return 0;
}