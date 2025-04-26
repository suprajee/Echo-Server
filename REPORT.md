# Echo Server-Client Implementation Report

## 1. Introduction
This report details the implementation of a multi-threaded Echo Server-Client system with both echo and chat functionalities. The system is designed to handle multiple client connections simultaneously while providing real-time communication capabilities.

## 2. System Architecture

### 2.1 Server Architecture
The server implementation follows a multi-threaded architecture with the following key components:

- **Thread Pool**: Utilizes a fixed-size thread pool (THREAD_POOL_SIZE = 4) to handle client connections
- **Client Management**:
  - Maximum concurrent clients: 5
  - Client queue system for managing incoming connections
  - Thread-safe client tracking using mutexes
- **Communication Modes**:
  - Echo mode: Simple message reflection
  - Chat mode: Direct messaging between clients

### 2.2 Client Architecture
The client implementation features:

- **Interactive Interface**:
  - Raw terminal mode for real-time input
  - Mode switching between echo and chat
  - Thread-safe screen updates
- **Dual-thread Design**:
  - Main thread: Handles user input
  - Receiver thread: Processes incoming messages

## 3. Implementation Details

### 3.1 Server-side Features
1. **Thread Safety Mechanisms**:
   - Mutexes for:
     - Logging (log_mutex)
     - Client name management (name_mutex)
     - Client array access (clients_mutex)
     - Queue operations (queue_mutex)
   - Semaphore for client connection limiting
   - Condition variables for queue synchronization

2. **Client Management**:
   - Dynamic client tracking using maps
   - Name-to-socket mapping
   - Chat session pairing

3. **Message Handling**:
   - Buffer size: 1024 bytes
   - Message formatting and validation
   - Mode-specific message routing

### 3.2 Client-side Features
1. **User Interface**:
   - Real-time input processing
   - Mode-specific prompts
   - Clean terminal management

2. **Communication**:
   - Command-line argument support for IP and port
   - Default port: 8989
   - Asynchronous message reception

## 4. Challenges and Solutions

### 4.1 Concurrency Challenges
1. **Race Conditions**:
   - Challenge: Multiple threads accessing shared resources
   - Solution: Comprehensive mutex implementation for all shared resources

2. **Resource Management**:
   - Challenge: Limited number of concurrent clients
   - Solution: Queue system with semaphore-based admission control

### 4.2 User Experience Challenges
1. **Terminal Management**:
   - Challenge: Maintaining clean terminal state
   - Solution: Proper terminal mode switching and cleanup

2. **Message Synchronization**:
   - Challenge: Ensuring message order and delivery
   - Solution: Thread-safe message queues and mutex-protected operations

## 5. Performance Analysis

### 5.1 Scalability
- Server can handle up to 5 concurrent clients
- Thread pool size of 4 provides efficient resource utilization
- Queue system prevents server overload

### 5.2 Resource Utilization
- Memory usage: O(n) where n is the number of connected clients
- CPU usage: Optimized through thread pool
- Network: Efficient buffer management (1024 bytes)

## 6. Future Improvements
1. **Scalability Enhancements**:
   - Dynamic thread pool sizing
   - Increased maximum client limit
   - Load balancing capabilities

2. **Feature Additions**:
   - File transfer support
   - Group chat functionality
   - Message encryption
   - Persistent chat history

3. **Performance Optimizations**:
   - Message compression
   - Connection pooling
   - Caching mechanisms

## 7. Conclusion
The implemented Echo Server-Client system successfully demonstrates multi-threaded network programming concepts while providing a robust and user-friendly communication platform. The system's architecture ensures thread safety and efficient resource utilization, making it suitable for real-world applications.

## 8. Usage Instructions

### Server
```bash
make run-server
```

### Client
```bash
make run-client IP=<server_ip> [PORT=<port_number>]
```
Example:
```bash
make run-client IP=127.0.0.1 PORT=8989
``` 