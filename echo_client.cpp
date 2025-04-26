#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <termios.h>

#define BUFFER_SIZE 1024
#define INPUT_MAX 1024

// Global variables
char input_buffer[INPUT_MAX];
int input_pos = 0;
int PORT=8989;
pthread_mutex_t screen_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;  // Mutex for state variables
struct termios orig_term, raw_term;
char current_mode = 'e';  // 'e' for echo, 'c' for chat
bool in_chat = false;     // Whether the user is currently in a chat session

// Reset terminal to original state
void reset_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
}

// Set terminal to raw mode
void set_raw_terminal() {
    tcgetattr(STDIN_FILENO, &raw_term);
    raw_term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw_term);
}

// Clear current line and reset cursor position
void clear_current_line() {
    printf("\r\033[K");  // Carriage return and clear line
    fflush(stdout);
}

// Display the current input buffer
void display_input_prompt() {
    pthread_mutex_lock(&state_mutex);
    if (in_chat) {
        printf("(Chat) > %s", input_buffer);
    } else {
        printf("(%s) > %s", current_mode == 'e' ? "Echo" : "Chat", input_buffer);
    }
    pthread_mutex_unlock(&state_mutex);
    fflush(stdout);
}

// Function to receive messages from server
void* receive_messages(void* socket_ptr) {
    int sock = *(int*)socket_ptr;
    char buffer[BUFFER_SIZE];
    
    while (1) {
        int bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) {
            pthread_mutex_lock(&screen_mutex);
            clear_current_line();
            printf("Server disconnected or error occurred\n");
            pthread_mutex_unlock(&screen_mutex);
            exit(1);
        }
        buffer[bytes] = '\0';
        
        // Clean up trailing whitespace
        while (bytes > 0 && (buffer[bytes-1] == '\n' || buffer[bytes-1] == '\r')) {
            buffer[--bytes] = '\0';
        }
        
        // Check for chat status messages
        pthread_mutex_lock(&state_mutex);
        if (strstr(buffer, "Chat started with") != NULL) {
            in_chat = true;
        } else if (strstr(buffer, "Chat ended") != NULL || strstr(buffer, "has left the chat") != NULL) {
            in_chat = false;
        }
        pthread_mutex_unlock(&state_mutex);
        
        pthread_mutex_lock(&screen_mutex);
        clear_current_line();
        printf("%s\n", buffer);
        display_input_prompt();
        pthread_mutex_unlock(&screen_mutex);
    }
    return NULL;
}

// Send a message to the server
void send_message_to_server(int sock, const char* message) {
    char buffer[BUFFER_SIZE];
    strncpy(buffer, message, BUFFER_SIZE - 2);
    strcat(buffer, "\n");
    send(sock, buffer, strlen(buffer), 0);
}

int main(int argc, char* argv[] ) {
    if(argc <= 1){
        printf("Usage: %s <IP> <PORT>\n", argv[0]);
        return 1;
    }
    else if(argc == 3){
        PORT=atoi(argv[2]);
    }
    int sock;
    struct sockaddr_in server_addr;
    char name[50];
    pthread_t receive_thread;
    int c;
    tcgetattr(STDIN_FILENO, &orig_term);
    atexit(reset_terminal);
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    // TODO: Replace with your server's IP address
    const char* server_ip = argv[1];
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        return -1;
    }
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection Failed");
        return -1;
    }
    
    printf("Connected to server at %s\n", server_ip);
    
    // Get username
    while (1) {
        printf("Enter your name: ");
        fgets(name, sizeof(name), stdin);
        name[strcspn(name, "\n")] = 0; // remove newline
    
        send(sock, name, strlen(name), 0);
    
        char response[BUFFER_SIZE];
        int res_bytes = recv(sock, response, BUFFER_SIZE - 1, 0);
        if (res_bytes <= 0) {
            printf("Disconnected while verifying name.\n");
            close(sock);
            return 1;
        }
        response[res_bytes] = '\0';
    
        if (strstr(response, "Name already exists") != NULL) {
            printf("%s\n", response);
            continue;
        }
    
        printf("%s\n", response); // prints welcome message
        break;
    }
    
    // Create thread to receive messages
    if (pthread_create(&receive_thread, NULL, receive_messages, &sock) != 0) {
        perror("Failed to create thread");
        return -1;
    }
    
    // Main thread handles user input
    printf("You can now start chatting:\n");
    printf("Commands:\n");
    printf("  /startchat - Switch to chat mode\n");
    printf("  /startecho - Switch to echo mode\n");
    printf("  /chat <name> - Request chat with another user\n");
    printf("  /exit - Leave chat\n");
    printf("  /list - Show connected users and their modes\n");
    printf("  /help - Show help\n");
    printf("  /quit - Quit application\n");
    
    // Set terminal to raw mode
    set_raw_terminal();
    
    // Clear input buffer
    memset(input_buffer, 0, INPUT_MAX);
    input_pos = 0;
    
    pthread_mutex_lock(&screen_mutex);
    display_input_prompt();
    pthread_mutex_unlock(&screen_mutex);
    
    while (1) {
        c = getchar();
        
        if (c == '\n' || c == '\r') {  // Enter pressed
            pthread_mutex_lock(&screen_mutex);
            clear_current_line();
            
            // Make a copy of the input buffer
            char temp_buffer[INPUT_MAX];
            strcpy(temp_buffer, input_buffer);
            
            // Clear input buffer
            memset(input_buffer, 0, INPUT_MAX);
            input_pos = 0;
            
            // Only send non-empty messages
            if (strlen(temp_buffer) > 0) {
                // Check for mode switch commands
                if (strcmp(temp_buffer, "/startchat") == 0) {
                    pthread_mutex_lock(&state_mutex);
                    current_mode = 'c';
                    pthread_mutex_unlock(&state_mutex);
                    printf("Switching to chat mode...\n");
                    
                    pthread_mutex_lock(&state_mutex);
                    if (in_chat) {
                        printf("Note: You are currently in a chat session. Use /exit to leave it.\n");
                    }
                    pthread_mutex_unlock(&state_mutex);
                } else if (strcmp(temp_buffer, "/startecho") == 0) {
                    pthread_mutex_lock(&state_mutex);
                    if (in_chat) {
                        printf("Note: You are in a chat session. The chat will end when switching to echo mode.\n");
                    }
                    current_mode = 'e';
                    pthread_mutex_unlock(&state_mutex);
                    printf("Switching to echo mode...\n");
                }
                
                // Display the user's message
                printf("You: %s\n", temp_buffer);
                
                // Send the message to server
                send_message_to_server(sock, temp_buffer);
                
                // Check for quit command
                if (strcmp(temp_buffer, "/quit") == 0) {
                    pthread_mutex_unlock(&screen_mutex);
                    break;
                }
            }
            
            display_input_prompt();
            pthread_mutex_unlock(&screen_mutex);
        }
        else if (c == 127 || c == 8) {  // Backspace
            if (input_pos > 0) {
                pthread_mutex_lock(&screen_mutex);
                input_buffer[--input_pos] = '\0';
                clear_current_line();
                display_input_prompt();
                pthread_mutex_unlock(&screen_mutex);
            }
        }
        else if (c >= 32 && c < 127 && input_pos < INPUT_MAX - 1) {  // Printable character
            pthread_mutex_lock(&screen_mutex);
            input_buffer[input_pos++] = c;
            input_buffer[input_pos] = '\0';
            clear_current_line();
            display_input_prompt();
            pthread_mutex_unlock(&screen_mutex);
        }
    }
    
    // Clean up
    reset_terminal();
    close(sock);
    pthread_cancel(receive_thread);
    pthread_join(receive_thread, NULL);
    
    return 0;
}
