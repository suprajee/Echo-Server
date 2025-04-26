CXX = g++
CXXFLAGS = -Wall -std=c++11

all: echo_client echo_server

echo_client: echo_client.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

echo_server: echo_server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f echo_client echo_server

# Run targets with example usage
run-server: echo_server
	./echo_server

run-client: echo_client
	@if [ "$(IP)" = "" ]; then \
		echo "Usage: make run-client IP=<server_ip> [PORT=<port_number>]"; \
		echo "Example: make run-client IP=127.0.0.1 PORT=8989"; \
		exit 1; \
	fi
	./echo_client $(IP) $(PORT)

.PHONY: all clean run-server run-client 