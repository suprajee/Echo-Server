#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <atomic>
#include <fstream>
#include <iomanip>

#define BUFFER_SIZE 1024
#define DEFAULT_PORT 8989

std::atomic<int> successful_connections(0);
std::atomic<int> failed_connections(0);
std::atomic<int> total_messages_sent(0);
std::atomic<int> total_messages_received(0);

struct TestResults {
    int num_clients;
    int messages_per_client;
    double connection_time;
    double message_latency;
    int successful_conns;
    int failed_conns;
    int messages_sent;
    int messages_received;
};

void simulate_client(const std::string& server_ip, int port, int client_id, int num_messages, TestResults& results) {
    std::cout << "Client " << client_id << " starting connection..." << std::endl;
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cout << "Client " << client_id << " failed to create socket" << std::endl;
        failed_connections++;
        return;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);

    auto start_time = std::chrono::high_resolution_clock::now();
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cout << "Client " << client_id << " failed to connect" << std::endl;
        failed_connections++;
        close(sock);
        return;
    }

    auto connect_time = std::chrono::high_resolution_clock::now();
    auto connection_duration = std::chrono::duration_cast<std::chrono::microseconds>(connect_time - start_time);
    std::cout << "Client " << client_id << " connected in " << connection_duration.count() << " microseconds" << std::endl;
    results.connection_time += connection_duration.count();
    
    successful_connections++;

    // Send client name
    std::string client_name = "test_client_" + std::to_string(client_id);
    std::cout << "Client " << client_id << " sending name: " << client_name << std::endl;
    send(sock, client_name.c_str(), client_name.length(), 0);

    // Wait for server response
    char buffer[BUFFER_SIZE];
    int bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        std::cout << "Client " << client_id << " received response: " << buffer << std::endl;
    }

    // Send messages and measure latency
    for (int i = 0; i < num_messages; i++) {
        auto msg_start = std::chrono::high_resolution_clock::now();
        
        std::string message = "Test message " + std::to_string(i) + " from client " + std::to_string(client_id);
        send(sock, message.c_str(), message.length(), 0);
        total_messages_sent++;

        // Receive echo
        int bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes > 0) {
            total_messages_received++;
            auto msg_end = std::chrono::high_resolution_clock::now();
            auto msg_duration = std::chrono::duration_cast<std::chrono::microseconds>(msg_end - msg_start);
            results.message_latency += msg_duration.count();
        }

        // Small delay between messages
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "Client " << client_id << " finished sending messages" << std::endl;
    close(sock);
}

void run_performance_test(const std::string& server_ip, int port, int num_clients, int messages_per_client) {
    std::vector<std::thread> client_threads;
    TestResults results = {0};
    results.num_clients = num_clients;
    results.messages_per_client = messages_per_client;

    auto test_start = std::chrono::high_resolution_clock::now();

    // Create and start client threads
    for (int i = 0; i < num_clients; i++) {
        client_threads.emplace_back(simulate_client, server_ip, port, i, messages_per_client, std::ref(results));
    }

    // Wait for all clients to finish
    for (auto& thread : client_threads) {
        thread.join();
    }

    auto test_end = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(test_end - test_start);

    // Calculate final results
    results.successful_conns = successful_connections;
    results.failed_conns = failed_connections;
    results.messages_sent = total_messages_sent;
    results.messages_received = total_messages_received;
    
    if (results.successful_conns > 0) {
        results.connection_time /= results.successful_conns;
    }
    if (results.messages_received > 0) {
        results.message_latency /= results.messages_received;
    }

    // Save results to file
    std::ofstream results_file("performance_results.txt");
    results_file << "Performance Test Results\n";
    results_file << "======================\n";
    results_file << "Number of clients: " << results.num_clients << "\n";
    results_file << "Messages per client: " << results.messages_per_client << "\n";
    results_file << "Total test duration: " << total_duration.count() << " microseconds\n";
    results_file << "Successful connections: " << results.successful_conns << "\n";
    results_file << "Failed connections: " << results.failed_conns << "\n";
    results_file << "Average connection time: " << std::fixed << std::setprecision(2) << results.connection_time << " microseconds\n";
    results_file << "Average message latency: " << std::fixed << std::setprecision(2) << results.message_latency << " microseconds\n";
    results_file << "Total messages sent: " << results.messages_sent << "\n";
    results_file << "Total messages received: " << results.messages_received << "\n";
    results_file.close();

    // Print results to console
    std::cout << "\nPerformance Test Results:\n";
    std::cout << "======================\n";
    std::cout << "Number of clients: " << results.num_clients << "\n";
    std::cout << "Messages per client: " << results.messages_per_client << "\n";
    std::cout << "Total test duration: " << total_duration.count() << " microseconds\n";
    std::cout << "Successful connections: " << results.successful_conns << "\n";
    std::cout << "Failed connections: " << results.failed_conns << "\n";
    std::cout << "Average connection time: " << std::fixed << std::setprecision(2) << results.connection_time << " microseconds\n";
    std::cout << "Average message latency: " << std::fixed << std::setprecision(2) << results.message_latency << " microseconds\n";
    std::cout << "Total messages sent: " << results.messages_sent << "\n";
    std::cout << "Total messages received: " << results.messages_received << "\n";
    std::cout << "\nDetailed results have been saved to 'performance_results.txt'\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <server_ip> [port] [num_clients] [messages_per_client]\n";
        std::cout << "Example: " << argv[0] << " 127.0.0.1 8989 10 100\n";
        return 1;
    }

    std::string server_ip = argv[1];
    int port = (argc > 2) ? std::stoi(argv[2]) : DEFAULT_PORT;
    int num_clients = (argc > 3) ? std::stoi(argv[3]) : 5;
    int messages_per_client = (argc > 4) ? std::stoi(argv[4]) : 50;

    std::cout << "Starting performance test with:\n";
    std::cout << "Server IP: " << server_ip << "\n";
    std::cout << "Port: " << port << "\n";
    std::cout << "Number of clients: " << num_clients << "\n";
    std::cout << "Messages per client: " << messages_per_client << "\n\n";

    run_performance_test(server_ip, port, num_clients, messages_per_client);

    return 0;
} 