#include "SimpleWebSocket.h"
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sstream>
#include <iomanip>
#include <random>
#include <sys/socket.h>      // ADD THIS
#include <netinet/in.h>      // ADD THIS
#include <arpa/inet.h>       // ADD THIS
#include <unistd.h>          // ADD THIS
#include <errno.h>           // ADD THIS
#include <openssl/sha.h>

static std::string base64Encode(const unsigned char* data, size_t len)
{
    static const char* base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::string ret;
    int i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    
    while (len--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for(i = 0; i < 4; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    
    if (i) {
        for(int j = i; j < 3; j++)
            char_array_3[j] = '\0';
        
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        
        for (int j = 0; j < i + 1; j++)
            ret += base64_chars[char_array_4[j]];
        
        while(i++ < 3)
            ret += '=';
    }
    
    return ret;
}

SimpleWebSocket::SimpleWebSocket()
    : m_socket(-1)
    , m_connected(false)
{
}

SimpleWebSocket::~SimpleWebSocket()
{
    disconnect();
}

bool SimpleWebSocket::connect(const std::string& host, int port, const std::string& path)
{
    // Create socket
    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket < 0) {
        return false;
    }
    
    // Set non-blocking
    int flags = fcntl(m_socket, F_GETFL, 0);
    fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);
    
    // Connect
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);
    
    int result = ::connect(m_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    if (result < 0 && errno != EINPROGRESS) {
        close(m_socket);
        m_socket = -1;
        return false;
    }
    
    // Wait for connection with timeout
    struct pollfd pfd;
    pfd.fd = m_socket;
    pfd.events = POLLOUT;
    
    if (poll(&pfd, 1, 5000) <= 0) {  // 5 second timeout
        close(m_socket);
        m_socket = -1;
        return false;
    }
    
    // Do WebSocket handshake
    if (!doHandshake(host, path)) {
        close(m_socket);
        m_socket = -1;
        return false;
    }
    
    m_connected = true;
    return true;
}

bool SimpleWebSocket::doHandshake(const std::string& host, const std::string& path)
{
    // Generate random WebSocket key
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    unsigned char key_bytes[16];
    for (int i = 0; i < 16; i++) {
        key_bytes[i] = dis(gen);
    }
    
    std::string key = base64Encode(key_bytes, 16);
    
    // Build HTTP upgrade request
    std::ostringstream request;
    request << "GET " << path << " HTTP/1.1\r\n";
    request << "Host: " << host << "\r\n";
    request << "Upgrade: websocket\r\n";
    request << "Connection: Upgrade\r\n";
    request << "Sec-WebSocket-Key: " << key << "\r\n";
    request << "Sec-WebSocket-Version: 13\r\n";
    request << "\r\n";
    
    std::string req_str = request.str();
    
    // Send handshake
    ssize_t sent = send(m_socket, req_str.c_str(), req_str.length(), 0);
    if (sent != (ssize_t)req_str.length()) {
        return false;
    }
    
    // Read response
    char buffer[4096];
    ssize_t received = 0;
    
    // Wait for response with timeout
    struct pollfd pfd;
    pfd.fd = m_socket;
    pfd.events = POLLIN;
    
    if (poll(&pfd, 1, 5000) <= 0) {
        return false;
    }
    
    received = recv(m_socket, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) {
        return false;
    }
    
    buffer[received] = '\0';
    
    // Check for 101 Switching Protocols
    std::string response(buffer);
    return response.find("101") != std::string::npos &&
           response.find("Switching Protocols") != std::string::npos;
}

void SimpleWebSocket::disconnect()
{
    if (m_socket >= 0) {
        // Send close frame
        unsigned char close_frame[] = {0x88, 0x00};  // FIN + Close opcode
        send(m_socket, close_frame, 2, 0);
        
        close(m_socket);
        m_socket = -1;
    }
    m_connected = false;
}

bool SimpleWebSocket::sendText(const std::string& message)
{
    if (!m_connected) return false;
    
    size_t len = message.length();
    std::vector<unsigned char> frame;
    
    // Frame format: FIN + text opcode
    frame.push_back(0x81);  // FIN=1, opcode=1 (text)
    
    // Mask bit + payload length
    if (len < 126) {
        frame.push_back(0x80 | len);  // Mask=1
    } else if (len < 65536) {
        frame.push_back(0x80 | 126);
        frame.push_back((len >> 8) & 0xFF);
        frame.push_back(len & 0xFF);
    } else {
        frame.push_back(0x80 | 127);
        for (int i = 7; i >= 0; i--) {
            frame.push_back((len >> (i * 8)) & 0xFF);
        }
    }
    
    // Masking key (random)
    unsigned char mask[4];
    std::random_device rd;
    for (int i = 0; i < 4; i++) {
        mask[i] = rd() & 0xFF;
        frame.push_back(mask[i]);
    }
    
    // Masked payload
    for (size_t i = 0; i < len; i++) {
        frame.push_back(message[i] ^ mask[i % 4]);
    }
    
    // Send frame
    ssize_t sent = send(m_socket, frame.data(), frame.size(), 0);
    return sent == (ssize_t)frame.size();
}

bool SimpleWebSocket::hasData()
{
    if (!m_connected) return false;
    
    struct pollfd pfd;
    pfd.fd = m_socket;
    pfd.events = POLLIN;
    
    return poll(&pfd, 1, 0) > 0;
}

std::string SimpleWebSocket::receiveText()
{
    if (!m_connected || !hasData()) {
        return "";
    }
    
    unsigned char header[2];
    ssize_t received = recv(m_socket, header, 2, 0);
    
    if (received != 2) {
        return "";
    }
    
    bool fin = (header[0] & 0x80) != 0;
    int opcode = header[0] & 0x0F;
    bool masked = (header[1] & 0x80) != 0;
    uint64_t payload_len = header[1] & 0x7F;
    
    // Handle extended payload length
    if (payload_len == 126) {
        unsigned char len_bytes[2];
        if (recv(m_socket, len_bytes, 2, 0) != 2) return "";
        payload_len = (len_bytes[0] << 8) | len_bytes[1];
    } else if (payload_len == 127) {
        unsigned char len_bytes[8];
        if (recv(m_socket, len_bytes, 8, 0) != 8) return "";
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | len_bytes[i];
        }
    }
    
    // Read mask key if present
    unsigned char mask[4] = {0};
    if (masked) {
        if (recv(m_socket, mask, 4, 0) != 4) return "";
    }
    
    // Read payload
    std::vector<unsigned char> payload(payload_len);
    size_t total_received = 0;
    
    while (total_received < payload_len) {
        ssize_t n = recv(m_socket, payload.data() + total_received, 
                        payload_len - total_received, 0);
        if (n <= 0) break;
        total_received += n;
    }
    
    // Unmask if needed
    if (masked) {
        for (size_t i = 0; i < payload_len; i++) {
            payload[i] ^= mask[i % 4];
        }
    }
    
    // Handle opcodes
    if (opcode == 0x1) {  // Text frame
        return std::string(payload.begin(), payload.end());
    } else if (opcode == 0x8) {  // Close frame
        m_connected = false;
    } else if (opcode == 0x9) {  // Ping
        // Send pong
        unsigned char pong[] = {0x8A, 0x00};  // FIN + Pong
        send(m_socket, pong, 2, 0);
    }
    
    return "";
}
