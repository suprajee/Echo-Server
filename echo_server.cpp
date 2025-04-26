#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <map>
#include <string>
#include <iostream>

#define PORT 8989
#define MAX_CLIENTS 5
#define THREAD_POOL_SIZE 4
#define BUFFER_SIZE 1024

using namespace std;

// Global variables
sem_t client_semaphore;               // Semaphore to limit concurrent clients
pthread_mutex_t log_mutex;           // Mutex for thread-safe logging
pthread_mutex_t name_mutex;          // Mutex for name access
pthread_mutex_t clients_mutex;       // Mutex for clients array access

int client_queue[100];               // Queue to hold client sockets
int front = 0, rear = 0;             // Queue pointers
pthread_mutex_t queue_mutex;         // Mutex for queue access
pthread_cond_t queue_cond;           // Condition variable for queue

// Structure to store client information
typedef struct {
    int socket;
    char mode;  // 'e' for echo, 'c' for chat
} ClientInfo;

ClientInfo clients[MAX_CLIENTS];     // Array to store client information
int client_count = 0;                // Number of connected clients

// Chat-specific variables
map<int, string> client_names;           // socket -> name
map<string, int> name_to_socket;         // name -> socket
map<int, int> chatting_with;             // socket -> socket

// Helper function to ensure message ends with exactly one newline
string formatMessage(const string& msg) {
    string result = msg;
    
    // Remove all trailing whitespace including newlines
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ' || result.back() == '\t')) {
        result.pop_back();
    }
    
    // Add exactly one newline
    result += '\n';
    return result;
}

// Logging function
void log_event(const char* msg) {
    pthread_mutex_lock(&log_mutex);
    FILE* log_file = fopen("server_log.txt", "a");
    if (log_file) {
        time_t now = time(NULL);
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(log_file, "[%s] %s\n", time_str, msg);
        fclose(log_file);
    }
    pthread_mutex_unlock(&log_mutex);
}

// Add client socket to queue
void enqueue_client(int client_socket) {
    pthread_mutex_lock(&queue_mutex);
    client_queue[rear++] = client_socket;
    pthread_cond_signal(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);
}

// Remove client socket from queue
int dequeue_client() {
    pthread_mutex_lock(&queue_mutex);
    while (front == rear) {
        pthread_cond_wait(&queue_cond, &queue_mutex);
    }
    int client_socket = client_queue[front++];
    pthread_mutex_unlock(&queue_mutex);
    return client_socket;
}

// Add client to clients array
void add_client(int socket) {
    pthread_mutex_lock(&clients_mutex);
    if (client_count < MAX_CLIENTS) {
        clients[client_count].socket = socket;
        clients[client_count].mode = 'e';  // Default to echo mode
        client_count++;
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Remove client from clients array
void remove_client(int socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket == socket) {
            // Shift remaining clients
            for (int j = i; j < client_count - 1; j++) {
                clients[j] = clients[j + 1];
            }
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Send a formatted message to a client
void send_message(int socket, const string& message) {
    string formatted = formatMessage(message);
    send(socket, formatted.c_str(), formatted.length(), 0);
}

// List all connected clients
void list_connected_clients() {
    printf("=== Connected Clients ===\n");
    for (const auto& entry : name_to_socket) {
        printf("Name: %s | Socket: %d\n", entry.first.c_str(), entry.second);
    }
    printf("=========================\n");
}

// Handle client communication
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_read;
    string client_name = "Unknown";
    bool name_set = false;

    // First, get the client's name
    while (!name_set) {
        bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read <= 0) {
            close(client_socket);
            return;
        }
        buffer[bytes_read] = '\0';
        client_name = string(buffer);
        client_name.erase(client_name.find_last_not_of(" \n\r\t") + 1);
    
        pthread_mutex_lock(&name_mutex);
        if (!name_to_socket.count(client_name)) {
            client_names[client_socket] = client_name;
            name_to_socket[client_name] = client_socket;
            pthread_mutex_unlock(&name_mutex);
            name_set = true;
            break;
        }
        pthread_mutex_unlock(&name_mutex);
        send_message(client_socket, "Name already exists. Please Try another ");
    }
    
    // Add client to the clients array
    add_client(client_socket);
    
    // Send welcome message
    send_message(client_socket, "Welcome to the server!");
    send_message(client_socket, "Type '/startchat' to enter chat mode or '/startecho' to enter echo mode");

    // Log connection
    char log_msg[BUFFER_SIZE];
    snprintf(log_msg, sizeof(log_msg), "Client '%s' connected (socket %d).", client_name.c_str(), client_socket);
    log_event(log_msg);
    printf("%s\n", log_msg);

    list_connected_clients();

    // Main message handling loop
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_read <= 0) break;

        buffer[bytes_read] = '\0';
        // Remove trailing whitespace
        string msg(buffer);
        msg.erase(msg.find_last_not_of(" \n\r\t") + 1);

        // Get client's current mode
        pthread_mutex_lock(&clients_mutex);
        char mode = 'e';
        for (int i = 0; i < client_count; i++) {
            if (clients[i].socket == client_socket) {
                mode = clients[i].mode;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);

        if (mode == 'e') {
            // Echo mode - handle commands and echo back the message
            if (msg == "/list") {
                pthread_mutex_lock(&name_mutex);
                string user_list = "Connected users:";
                for (const auto& entry : name_to_socket) {
                    string mode_str = " (echo)";
                    pthread_mutex_lock(&clients_mutex);
                    for (int i = 0; i < client_count; i++) {
                        if (clients[i].socket == entry.second) {
                            mode_str = clients[i].mode == 'c' ? " (chat)" : " (echo)";
                            break;
                        }
                    }
                    pthread_mutex_unlock(&clients_mutex);
                    user_list += "\n  " + entry.first + mode_str;
                }
                pthread_mutex_unlock(&name_mutex);
                send_message(client_socket, user_list);
            } else if (msg == "/help") {
                string help_text = "Commands:\n"
                                  "  /startchat - Switch to chat mode\n"
                                  "  /startecho - Switch to echo mode\n"
                                  "  /list - Show connected users and their modes\n"
                                  "  /help - Show this help message\n"
                                  "  /quit - Quit application";
                send_message(client_socket, help_text);
            } else if (msg == "/startchat") {
                pthread_mutex_lock(&clients_mutex);
                for (int i = 0; i < client_count; i++) {
                    if (clients[i].socket == client_socket) {
                        clients[i].mode = 'c';
                        break;
                    }
                }
                pthread_mutex_unlock(&clients_mutex);
                send_message(client_socket, "Switched to chat mode. Use /chat <name> to start chatting with someone.");
            } else if (msg == "/startecho") {
                pthread_mutex_lock(&clients_mutex);
                for (int i = 0; i < client_count; i++) {
                    if (clients[i].socket == client_socket) {
                        clients[i].mode = 'e';
                        break;
                    }
                }
                pthread_mutex_unlock(&clients_mutex);
                send_message(client_socket, "Switched to echo mode.");
            } else {
                // Regular echo mode - just echo back the message
                send(client_socket, buffer, bytes_read, 0);
                
                // Log and print message
                char log_msg[BUFFER_SIZE + 50];
                snprintf(log_msg, sizeof(log_msg), "Client '%s' (echo mode): %s", client_name.c_str(), buffer);
                log_event(log_msg);
                printf("Echo from client '%s': %s", client_name.c_str(), buffer);
            }
        } else {
            // Chat mode - handle chat commands
            pthread_mutex_lock(&name_mutex);
            if (chatting_with.count(client_socket)) {
                int peer = chatting_with[client_socket];
                pthread_mutex_unlock(&name_mutex);
                
                if (msg == "/exit") {
                    pthread_mutex_lock(&name_mutex);
                    chatting_with.erase(client_socket);
                    chatting_with.erase(peer);
                    pthread_mutex_unlock(&name_mutex);
                    
                    send_message(client_socket, "Chat ended.");
                    send_message(peer, client_name + " has left the chat.");
                } else if (msg == "/startecho") {
                    // If in a chat, end it first
                    pthread_mutex_lock(&name_mutex);
                    chatting_with.erase(client_socket);
                    chatting_with.erase(peer);
                    pthread_mutex_unlock(&name_mutex);
                    
                    send_message(client_socket, "Chat ended. Switching to echo mode.");
                    send_message(peer, client_name + " has left the chat.");
                    
                    pthread_mutex_lock(&clients_mutex);
                    for (int i = 0; i < client_count; i++) {
                        if (clients[i].socket == client_socket) {
                            clients[i].mode = 'e';
                            break;
                        }
                    }
                    pthread_mutex_unlock(&clients_mutex);
                } else if (!msg.empty()) {
                    string full_msg = client_name + ": " + msg;
                    send_message(peer, full_msg);
                    
                    // Log chat message
                    char log_msg[BUFFER_SIZE + 50];
                    snprintf(log_msg, sizeof(log_msg), "Chat from '%s' to peer: %s", client_name.c_str(), buffer);
                    log_event(log_msg);
                }
            } else {
                pthread_mutex_unlock(&name_mutex);
                
                if (msg.substr(0, 5) == "/chat") {
                    string target_name;
                    if (msg.length() > 6) {
                        target_name = msg.substr(6);
                    }
                    
                    if (target_name.empty()) {
                        send_message(client_socket, "Usage: /chat <name>");
                        continue;
                    }

                    pthread_mutex_lock(&name_mutex);
                    if (name_to_socket.count(target_name)) {
                        int target_socket = name_to_socket[target_name];

                        if (target_socket == client_socket) {
                            send_message(client_socket, "You cannot chat with yourself.");
                        } else if (chatting_with.count(target_socket)) {
                            send_message(client_socket, "Client is already in a chat with someone else.");
                        } else {
                            // Check if target user is in echo mode
                            bool target_in_echo_mode = true;
                            pthread_mutex_lock(&clients_mutex);
                            for (int i = 0; i < client_count; i++) {
                                if (clients[i].socket == target_socket) {
                                    target_in_echo_mode = (clients[i].mode == 'e');
                                    break;
                                }
                            }
                            pthread_mutex_unlock(&clients_mutex);
                            
                            if (target_in_echo_mode) {
                                send_message(client_socket, "Cannot start chat: " + target_name + " is in echo mode. They need to switch to chat mode first.");
                            } else {
                                chatting_with[client_socket] = target_socket;
                                chatting_with[target_socket] = client_socket;
                                pthread_mutex_unlock(&name_mutex);

                                string target_msg = "Chat started with " + client_name + ". Type '/exit' to end.";
                                string requester_msg = "Chat started with " + target_name + ". Type '/exit' to end.";

                                send_message(client_socket, requester_msg);
                                send_message(target_socket, target_msg);
                                
                                // Log chat start
                                char log_msg[BUFFER_SIZE + 50];
                                snprintf(log_msg, sizeof(log_msg), "Chat started between '%s' and '%s'", 
                                        client_name.c_str(), target_name.c_str());
                                log_event(log_msg);
                            }
                        }
                    } else {
                        send_message(client_socket, "Client not found: " + target_name);
                    }
                    pthread_mutex_unlock(&name_mutex);
                } else if (msg == "/list") {
                    pthread_mutex_lock(&name_mutex);
                    string user_list = "Connected users:";
                    for (const auto& entry : name_to_socket) {
                        string mode_str = " (echo)";
                        pthread_mutex_lock(&clients_mutex);
                        for (int i = 0; i < client_count; i++) {
                            if (clients[i].socket == entry.second) {
                                mode_str = clients[i].mode == 'c' ? " (chat)" : " (echo)";
                                break;
                            }
                        }
                        pthread_mutex_unlock(&clients_mutex);
                        user_list += "\n  " + entry.first + mode_str;
                    }
                    pthread_mutex_unlock(&name_mutex);
                    send_message(client_socket, user_list);
                } else if (msg == "/help") {
                    string help_text = "Commands:\n"
                                      "  /chat <name> - Request chat with another user\n"
                                      "  /list - Show connected users\n"
                                      "  /exit - Leave current chat\n"
                                      "  /startecho - Switch to echo mode\n"
                                      "  /quit - Disconnect from server\n"
                                      "  /help - Show this help message";
                    send_message(client_socket, help_text);
                } else if (msg == "/startecho") {
                    if (chatting_with.count(client_socket)) {
                    // End current chat first
                    int peer = chatting_with[client_socket];
                    chatting_with.erase(client_socket);
                    chatting_with.erase(peer);
                    
                    send_message(client_socket, "Chat ended.");
                    send_message(peer, client_name + " has left the chat.");
                    }
                    pthread_mutex_lock(&clients_mutex);
                    for (int i = 0; i < client_count; i++) {
                        if (clients[i].socket == client_socket) {
                            clients[i].mode = 'e';
                            break;
                        }
                    }
                    pthread_mutex_unlock(&clients_mutex);
                    send_message(client_socket, "Switched to echo mode.");
                } else {
                    send_message(client_socket, "You are in chat mode but not chatting with anyone. Use /chat <name> to start a chat or /startecho to switch to echo mode.");
                }
            }
        }
    }

    // Clean up when client disconnects
    pthread_mutex_lock(&name_mutex);
    if (chatting_with.count(client_socket)) {
        int peer = chatting_with[client_socket];
        chatting_with.erase(peer);
        chatting_with.erase(client_socket);
        send_message(peer, client_name + " has disconnected.");
    }
    name_to_socket.erase(client_name);
    client_names.erase(client_socket);
    pthread_mutex_unlock(&name_mutex);

    remove_client(client_socket);
    snprintf(log_msg, sizeof(log_msg), "Client '%s' disconnected (socket %d).", client_name.c_str(), client_socket);
    log_event(log_msg);
    printf("%s\n", log_msg);
    close(client_socket);
}

// Thread function for thread pool
void* thread_function(void* arg) {
    while (1) {
        int client_socket = dequeue_client();
        sem_wait(&client_semaphore); // Limit concurrent clients
        handle_client(client_socket);
        sem_post(&client_semaphore);
    }
    return NULL;
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    sem_init(&client_semaphore, 0, MAX_CLIENTS);
    pthread_mutex_init(&queue_mutex, NULL);
    pthread_mutex_init(&log_mutex, NULL);
    pthread_mutex_init(&name_mutex, NULL);
    pthread_mutex_init(&clients_mutex, NULL);
    pthread_cond_init(&queue_cond, NULL);

    // Create thread pool
    pthread_t threads[THREAD_POOL_SIZE];
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_create(&threads[i], NULL, thread_function, NULL);
    }

    // Create server socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_fd, 10) == 0)
        printf("Server listening on port %d...\n", PORT);
    else {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    log_event("Server started.");

    // Accept clients
    while (1) {
        client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }
        enqueue_client(client_socket);
    }

    // Cleanup (not reachable in this infinite server loop)
    close(server_fd);
    sem_destroy(&client_semaphore);
    pthread_mutex_destroy(&queue_mutex);
    pthread_mutex_destroy(&log_mutex);
    pthread_mutex_destroy(&name_mutex);
    pthread_mutex_destroy(&clients_mutex);
    pthread_cond_destroy(&queue_cond);

    return 0;
}