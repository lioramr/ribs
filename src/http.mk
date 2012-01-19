TARGET=http.a

SRC=epoll.cpp acceptor.cpp http_server.cpp http_client.cpp http_client_file.cpp mime_types.cpp ringbuf.cpp http_header.cpp

include ../make/ribscpp.mk
