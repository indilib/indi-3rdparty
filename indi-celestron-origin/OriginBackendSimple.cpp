#include "OriginBackendSimple.hpp"
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QDebug>
#include <cmath>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <regex>
#include <ctime>
#include <cstring>

OriginDiscovery::OriginDiscovery()
{
}

OriginDiscovery::~OriginDiscovery()
{
    stopDiscovery();
}

bool OriginDiscovery::startDiscovery()
{
    qDebug() << "Starting telescope discovery...";
    
    // Close existing socket if open
    if (m_udpSocket >= 0)
    {
        close(m_udpSocket);
        m_udpSocket = -1;
    }
    
    // Clear previous discoveries
    m_telescopes.clear();
    
    // Create UDP socket
    m_udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_udpSocket < 0)
    {
        qDebug() << "Failed to create UDP socket:" << strerror(errno);
        return false;
    }
    
    // Set socket to non-blocking mode
    int flags = fcntl(m_udpSocket, F_GETFL, 0);
    fcntl(m_udpSocket, F_SETFL, flags | O_NONBLOCK);
    
    // Enable address reuse
    int reuse = 1;
    if (setsockopt(m_udpSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        qDebug() << "Failed to set SO_REUSEADDR:" << strerror(errno);
    }
    
#ifdef SO_REUSEPORT
    if (setsockopt(m_udpSocket, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0)
    {
        qDebug() << "Failed to set SO_REUSEPORT:" << strerror(errno);
    }
#endif
    
    // Enable broadcast reception
    int broadcast = 1;
    if (setsockopt(m_udpSocket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0)
    {
        qDebug() << "Failed to enable broadcast:" << strerror(errno);
    }
    
    // Bind to port 55555 on all interfaces
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(55555);
    
    if (bind(m_udpSocket, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        qDebug() << "Failed to bind to port 55555:" << strerror(errno);
        close(m_udpSocket);
        m_udpSocket = -1;
        return false;
    }
    
    m_discovering = true;
    m_discoveryStartTime = time(nullptr);
    
    qDebug() << "Listening for telescope broadcasts on port 55555...";
    
    return true;
}

void OriginDiscovery::stopDiscovery()
{
    if (m_udpSocket >= 0)
    {
        close(m_udpSocket);
        m_udpSocket = -1;
    }
    
    m_discovering = false;
    
    qDebug() << "Discovery stopped";
}

void OriginDiscovery::poll()
{
  if (false) qDebug() << "poll()";

    if (!m_discovering || m_udpSocket < 0)
        return;
    
    // Check for timeout (300 seconds)
    time_t now = time(nullptr);
    if (now - m_discoveryStartTime > 300)
    {
        qDebug() << "Discovery timeout after 300 seconds";
        stopDiscovery();
        return;
    }
    
    // Process any pending datagrams
    processPendingDatagrams();
}

void OriginDiscovery::processPendingDatagrams()
{
    char buffer[4096];
    struct sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);

    if (false) qDebug() << "pending datagrams";
    
    // Read all available datagrams
    while (m_udpSocket > 0)
    {
        ssize_t bytesRead = recvfrom(m_udpSocket, buffer, sizeof(buffer) - 1, 0,
                                     (struct sockaddr*)&sender_addr, &sender_len);
        
        if (bytesRead < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // No more data available
                break;
            }
            else
            {
                qDebug() << "recvfrom error:" << strerror(errno);
                break;
            }
        }
        
        if (bytesRead == 0)
            break;
        
        // Null-terminate the data
        buffer[bytesRead] = '\0';
        std::string datagram(buffer, bytesRead);
        
        // Get sender IP
        char sender_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, INET_ADDRSTRLEN);
        
        // Check if this looks like a telescope broadcast
        if (datagram.find("Origin") != std::string::npos && 
            datagram.find("IP Address") != std::string::npos)
        {
            // Extract telescope information
            std::string telescopeIP = extractIPAddress(datagram);
            std::string telescopeModel = extractModel(datagram);

	    if (false) qDebug() << "extract datagram" << telescopeIP.c_str();
	    
            if (telescopeIP.empty())
            {
                // Use sender IP if we couldn't extract it
                telescopeIP = sender_ip;
            }
            
            // Check if we already have this telescope
            bool found = false;
            for (auto& telescope : m_telescopes)
            {
                if (telescope.ipAddress == telescopeIP)
                {
                    telescope.lastSeen = time(nullptr);
                    found = true;
                    break;
                }
            }
            
            if (!found)
            {
                // Add new telescope
                TelescopeInfo info;
                info.ipAddress = telescopeIP;
                info.model = telescopeModel.empty() ? "Celestron Origin" : telescopeModel;
                info.lastSeen = time(nullptr);
                
                m_telescopes.push_back(info);
                
                qDebug() << "Discovered telescope:" << info.ipAddress.c_str() 
                         << "-" << info.model.c_str();

		// Call discovery callback
		if (m_callback) m_callback(info);
            }
        }
    }
}

std::string OriginDiscovery::extractIPAddress(const std::string& datagram)
{
    // Regex to match IP address (e.g., 192.168.1.195)
    std::regex ipRegex("\\b(?:(\\d{1,3}\\.){3}\\d{1,3})\\b");
    std::smatch match;
    
    if (std::regex_search(datagram, match, ipRegex))
    {
        return match.str(0);
    }
    
    return "";
}

std::string OriginDiscovery::extractModel(const std::string& datagram)
{
    // Look for "Identity:" followed by the model name
    size_t identityPos = datagram.find("Identity:");
    if (identityPos != std::string::npos)
    {
        size_t start = identityPos + 9;  // Skip "Identity:"
        size_t end = datagram.find(" ", start);
        
        if (end != std::string::npos && end > start)
        {
            return datagram.substr(start, end - start);
        }
    }
    
    return "";
}

std::vector<OriginDiscovery::TelescopeInfo> OriginDiscovery::getDiscoveredTelescopes() const
{
    return m_telescopes;
}

OriginBackendSimple::OriginBackendSimple()
    : m_webSocket(new SimpleWebSocket())
    , m_connectedPort(80)
    , m_connected(false)
    , m_logicallyConnected(false)
    , m_cameraConnected(false)
    , m_nextSequenceId(2000)
{
}

OriginBackendSimple::~OriginBackendSimple()
{
    disconnectFromTelescope();
    delete m_webSocket;
}

// In OriginBackendSimple.cpp

void OriginBackendSimple::requestImage(const QString& filePath)
{
    qDebug() << "=== IMAGE DOWNLOAD START ===" << QDateTime::currentDateTime().toString();
    qDebug() << "Image notification received:" << filePath;
    
    // Store the path for later download
    m_pendingImagePath = filePath;
    
    // Download the image using simple HTTP GET without QEventLoop
    QString fullPath = QString("http://%1/SmartScope-1.0/dev2/%2")
                       .arg(m_connectedHost, filePath);
    
    qDebug() << "Will download from:" << fullPath;
    
    // Record start time
    auto startTime = std::chrono::steady_clock::now();
    
    // Use synchronous download
    QByteArray imageData = downloadImageSync(fullPath);
    
    // Record end time
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    qDebug() << "=== IMAGE DOWNLOAD COMPLETE ===" << QDateTime::currentDateTime().toString();
    qDebug() << "Download took:" << duration << "ms (" << (duration/1000.0) << "seconds)";
    
    if (!imageData.isEmpty())
    {
        qDebug() << "Downloaded" << imageData.size() << "bytes";
        
        // Call image callback with the data
        if (m_imageCallback)
        {
            m_imageCallback(filePath, imageData, 0, 0, 0);
        }
    }
    else
    {
        qDebug() << "Failed to download image";
    }
    
    qDebug() << "=== IMAGE PROCESSING COMPLETE ===" << QDateTime::currentDateTime().toString();
}

QByteArray OriginBackendSimple::downloadImageSync(const QString& url)
{
    QByteArray result;
    
    // Parse URL
    QUrl qurl(url);
    QString host = qurl.host();
    int port = qurl.port(80);
    QString path = qurl.path();
    
    qDebug() << "Downloading from host:" << host << "port:" << port << "path:" << path;
    
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        qDebug() << "Failed to create socket";
        return result;
    }
    
    // Set socket timeouts
    struct timeval timeout;
    timeout.tv_sec = 60;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    // Set socket to non-blocking for the download loop
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    // Resolve host
    struct hostent *server = gethostbyname(host.toUtf8().constData());
    if (server == nullptr)
    {
        qDebug() << "Failed to resolve host";
        close(sock);
        return result;
    }
    
    // Connect (switch back to blocking for connect)
    fcntl(sock, F_SETFL, flags);
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        qDebug() << "Failed to connect to server";
        close(sock);
        return result;
    }
    
    // Send HTTP GET request
    QString request = QString("GET %1 HTTP/1.1\r\n"
                             "Host: %2\r\n"
                             "Connection: close\r\n"
                             "\r\n").arg(path, host);
    
    send(sock, request.toUtf8().constData(), request.length(), 0);
    
    // Switch to non-blocking for reading
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    // Read response with periodic WebSocket keepalive
    char buffer[65536];
    QByteArray response;
    int totalBytes = 0;
    auto lastKeepalive = std::chrono::steady_clock::now();
    auto lastLog = lastKeepalive;
    
    qDebug() << "Reading response with WebSocket keepalive...";
    
    while (true)
    {
        int bytesRead = recv(sock, buffer, sizeof(buffer), 0);
        
        if (bytesRead > 0)
        {
            response.append(buffer, bytesRead);
            totalBytes += bytesRead;
        }
        else if (bytesRead == 0)
        {
            // Connection closed - done
            break;
        }
        else if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            // Real error
            qDebug() << "Socket error:" << strerror(errno);
            break;
        }
        
        // Check if we should send a keepalive
        auto now = std::chrono::steady_clock::now();
        auto keepaliveElapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastKeepalive).count();
        
        if (keepaliveElapsed >= 5)  // Every 5 seconds
        {
            qDebug() << "Sending WebSocket keepalive (downloaded" << totalBytes << "bytes)";
            
            // Send a lightweight status request to keep WebSocket alive
            sendCommand("GetStatus", "Mount");
            
            // Process any incoming WebSocket messages
            while (m_webSocket->hasData())
            {
                std::string message = m_webSocket->receiveText();
                if (!message.empty())
                {
                    processMessage(message);
                }
            }
            
            lastKeepalive = now;
        }
        
        // Log progress
        auto logElapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastLog).count();
        if (logElapsed >= 5)
        {
            qDebug() << "Downloaded" << totalBytes << "bytes so far...";
            lastLog = now;
        }
        
        // Small sleep to avoid spinning
        usleep(10000);  // 10ms
    }
    
    close(sock);
    
    qDebug() << "Received" << response.size() << "bytes total";
    
    // Parse HTTP response
    int headerEnd = response.indexOf("\r\n\r\n");
    if (headerEnd > 0)
    {
        result = response.mid(headerEnd + 4);
        qDebug() << "Image data size:" << result.size() << "bytes";
    }
    
    return result;
}

void OriginBackendSimple::poll()
{
    if (!m_webSocket)
        return;
    
    static auto lastPollTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPollTime).count();
    
    // Log if poll() wasn't called for a long time
    if (elapsed > 5000)
    {
        qDebug() << "WARNING: poll() was blocked for" << elapsed << "ms";
    }
    lastPollTime = now;
    
    // Check WebSocket connection status
    if (!m_webSocket->isConnected())
    {
        if (m_connected)
        {
            qDebug() << "ERROR: WebSocket disconnected! Will attempt reconnection...";
            m_connected = false;
            
            // Try to reconnect immediately
            if (reconnectWebSocket())
            {
                qDebug() << "Reconnection successful, continuing...";
            }
            else
            {
                qDebug() << "Reconnection failed, will retry on next poll()";
            }
        }
        else
        {
            // Not connected, try to reconnect periodically
            static auto lastReconnectAttempt = std::chrono::steady_clock::now();
            auto timeSinceLastAttempt = std::chrono::duration_cast<std::chrono::seconds>(
                now - lastReconnectAttempt).count();
            
            if (timeSinceLastAttempt >= 5)  // Try every 5 seconds
            {
                qDebug() << "Attempting periodic reconnection...";
                if (reconnectWebSocket())
                {
                    qDebug() << "Periodic reconnection successful!";
                }
                lastReconnectAttempt = now;
            }
        }
        
        return;
    }
    
    if (!m_connected)
        return;
    
    // Check for incoming messages
    int messageCount = 0;
    while (m_webSocket->hasData())
    {
        std::string message = m_webSocket->receiveText();
        if (!message.empty())
        {
            messageCount++;
            if (false) qDebug() << "RECEIVED MESSAGE:" << QString::fromStdString(message);
            processMessage(message);
        }
    }
    
    if (messageCount > 0)
    {
        if (false) qDebug() << "Processed" << messageCount << "messages";
    }
}

bool OriginBackendSimple::connectToTelescope(const QString& host, int port)
{
    m_connectedHost = host;
    m_connectedPort = port;
    
    // Save for reconnection
    m_lastConnectedHost = host;
    m_lastConnectedPort = port;
    
    std::string path = "/SmartScope-1.0/mountControlEndpoint";
    
    qDebug() << "Connecting to Origin at" << host << ":" << port;
    
    if (!m_webSocket->connect(host.toStdString(), port, path))
    {
        qWarning() << "Failed to connect WebSocket";
        return false;
    }
    
    m_connected = true;
    qDebug() << "WebSocket connected";
    
    // Send initial status request
    sendCommand("GetStatus", "Mount");
    
    return true;
}


bool OriginBackendSimple::reconnectWebSocket()
{
    if (!m_autoReconnect)
        return false;
    
    qDebug() << "Attempting to reconnect WebSocket...";
    
    std::string path = "/SmartScope-1.0/mountControlEndpoint";
    
    if (m_webSocket->connect(m_lastConnectedHost.toStdString(), m_lastConnectedPort, path))
    {
        m_connected = true;
        qDebug() << "WebSocket reconnected successfully";
        
        // Re-request status to sync state
        sendCommand("GetStatus", "Mount");
        
        return true;
    }
    else
    {
        qDebug() << "Reconnection failed";
        return false;
    }
}

void OriginBackendSimple::disconnectFromTelescope()
{
    if (m_webSocket)
    {
        m_webSocket->disconnect();
    }
    m_connected = false;
    m_logicallyConnected = false;
}

void OriginBackendSimple::processMessage(const std::string& message)
{
    QString qmsg = QString::fromStdString(message);
    QJsonDocument doc = QJsonDocument::fromJson(qmsg.toUtf8());
    
    if (!doc.isObject())
        return;
    
    QJsonObject obj = doc.object();
    
    // Parse telescope data
    QString source = obj["Source"].toString();
    if (false) qDebug() << "Processing message from:" << source;

    if (source == "Mount")
    {
        // Update mount status
        if (obj.contains("Ra"))
            m_status.raPosition = radiansToHours(obj["Ra"].toDouble());
        if (obj.contains("Dec"))
            m_status.decPosition = radiansToDegrees(obj["Dec"].toDouble());
        if (obj.contains("IsTracking"))
            m_status.isTracking = obj["IsTracking"].toBool();
        if (obj.contains("IsGotoOver"))
            m_status.isSlewing = !obj["IsGotoOver"].toBool();
        
        // Call status callback
        if (m_statusCallback)
            m_statusCallback();
    }
    
    // Handle image notifications
    QString command = obj["Command"].toString();
    QString type = obj["Type"].toString();
    
    if (source == "ImageServer" && command == "NewImageReady" && type == "Notification")
    {
        QString filePath = obj["FileLocation"].toString();
        if (!filePath.isEmpty() && filePath.endsWith(".tiff", Qt::CaseInsensitive))
        {
            requestImage(filePath);
        }
    }
}

void OriginBackendSimple::sendCommand(const QString& command, const QString& destination,
                                     const QJsonObject& params)
{
    QJsonObject jsonCommand;
    jsonCommand["Command"] = command;
    jsonCommand["Destination"] = destination;
    jsonCommand["SequenceID"] = m_nextSequenceId++;
    jsonCommand["Source"] = "INDIDriver";
    jsonCommand["Type"] = "Command";
    
    // Add parameters
    for (auto it = params.begin(); it != params.end(); ++it)
    {
        jsonCommand[it.key()] = it.value();
    }
    
    QJsonDocument doc(jsonCommand);
    QString msgStr = doc.toJson(QJsonDocument::Compact);
    
    if (m_webSocket && m_webSocket->isConnected())
    {
        m_webSocket->sendText(msgStr.toStdString());
        qDebug() << "Sent:" << msgStr;
    }
}

bool OriginBackendSimple::gotoPosition(double ra, double dec)
{
    QJsonObject params;
    params["Ra"] = hoursToRadians(ra);
    params["Dec"] = degreesToRadians(dec);
    
    sendCommand("GotoRaDec", "Mount", params);
    return true;
}

bool OriginBackendSimple::syncPosition(double ra, double dec)
{
    QJsonObject params;
    params["Ra"] = hoursToRadians(ra);
    params["Dec"] = degreesToRadians(dec);
    
    sendCommand("SyncToRaDec", "Mount", params);
    return true;
}

bool OriginBackendSimple::abortMotion()
{
    sendCommand("AbortAxisMovement", "Mount");
    return true;
}

bool OriginBackendSimple::parkMount()
{
    sendCommand("Park", "Mount");
    return true;
}

bool OriginBackendSimple::unparkMount()
{
    sendCommand("Unpark", "Mount");
    return true;
}

bool OriginBackendSimple::setTracking(bool enabled)
{
    QString command = enabled ? "StartTracking" : "StopTracking";
    sendCommand(command, "Mount");
    return true;
}

bool OriginBackendSimple::isTracking() const
{
    return m_status.isTracking;
}

void OriginBackendSimple::setAutoReconnect(bool enable)
{
    m_autoReconnect = enable;
    qDebug() << "Auto-reconnect" << (enable ? "enabled" : "disabled");
}

bool OriginBackendSimple::takeSnapshot(double exposure, int iso)
{
    QJsonObject params;
    params["ExposureTime"] = exposure;
    params["ISO"] = iso;
    
    sendCommand("RunSampleCapture", "TaskController", params);
    return true;
}

bool OriginBackendSimple::abortExposure()
{
    return true;
}

double OriginBackendSimple::hoursToRadians(double hours)
{
    return hours * M_PI / 12.0;
}

double OriginBackendSimple::degreesToRadians(double degrees)
{
    return degrees * M_PI / 180.0;
}

double OriginBackendSimple::radiansToHours(double radians)
{
    return radians * 12.0 / M_PI;
}

double OriginBackendSimple::radiansToDegrees(double radians)
{
    return radians * 180.0 / M_PI;
}
