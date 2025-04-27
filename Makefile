CXX = g++
CXXFLAGS = -Wall -std=c++11 -pthread

all: echo_client echo_server performance_test

echo_client: echo_client.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

echo_server: echo_server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

performance_test: performance_test.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f echo_client echo_server performance_test performance_results.txt

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

# Performance testing targets
run-performance-test: performance_test
	@if [ "$(IP)" = "" ]; then \
		echo "Usage: make run-performance-test IP=<server_ip> [PORT=<port_number>] [CLIENTS=<num_clients>] [MSGS=<messages_per_client>]"; \
		echo "Example: make run-performance-test IP=127.0.0.1 PORT=8989 CLIENTS=10 MSGS=50"; \
		exit 1; \
	fi
	./performance_test $(IP) $(PORT) $(CLIENTS) $(MSGS)

.PHONY: all clean run-server run-client run-performance-test run-all-tests 