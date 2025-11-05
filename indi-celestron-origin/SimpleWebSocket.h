#pragma once

#include <string>
#include <vector>
#include <functional>

class SimpleWebSocket
{
public:
    SimpleWebSocket();
    ~SimpleWebSocket();
    
    bool connect(const std::string& host, int port, const std::string& path);
    void disconnect();
    bool isConnected() const { return m_connected; }
    
    bool sendText(const std::string& message);
    std::string receiveText();
    
    // Non-blocking check for data
    bool hasData();
    
    // Callback for received messages
    std::function<void(const std::string&)> onTextMessage;
    
private:
    int m_socket;
    bool m_connected;
    
    bool doHandshake(const std::string& host, const std::string& path);
};
