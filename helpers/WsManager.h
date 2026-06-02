/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 Metrological
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <mutex>
#include <memory>
#include <cctype>
#include <cstring>
#include <plugins/plugins.h>
#include "UtilsLogging.h"
#include "WebSocketLink.h"


// TODO: Remove once IsNullValue() in core/JSON.h is fixed
// and replace StreamJSONOneShotType with StreamJSONType
#include "StreamJSONOneShot.h"

#define DEFAULT_SOCKET_ADDRESS "127.0.0.1"
using namespace WPEFramework;

inline std::string TrimWhitespace(const std::string& input)
{
    size_t start = 0;
    while ((start < input.size()) && (std::isspace(static_cast<unsigned char>(input[start])) != 0)) {
        start++;
    }

    size_t end = input.size();
    while ((end > start) && (std::isspace(static_cast<unsigned char>(input[end - 1])) != 0)) {
        end--;
    }

    return input.substr(start, end - start);
}

inline bool IsHexDigit(const char value)
{
    return (((value >= '0') && (value <= '9')) ||
            ((value >= 'a') && (value <= 'f')) ||
            ((value >= 'A') && (value <= 'F')));
}

inline bool LooksLikeJsonStringLiteral(const std::string& value)
{
    if ((value.size() < 2) || (value.front() != '"') || (value.back() != '"')) {
        return false;
    }

    bool escaped = false;
    for (size_t i = 1; i + 1 < value.size(); ++i) {
        const char ch = value[i];

        if ((static_cast<unsigned char>(ch) < 0x20) || ((ch == '"') && (escaped == false))) {
            return false;
        }

        if (escaped) {
            if (ch == 'u') {
                if (i + 4 >= value.size() - 1) {
                    return false;
                }
                for (size_t offset = 1; offset <= 4; ++offset) {
                    if (false == IsHexDigit(value[i + offset])) {
                        return false;
                    }
                }
                i += 4;
            } else {
                const char* legalEscapes = "\"\\/bfnrt";
                if (nullptr == std::strchr(legalEscapes, ch)) {
                    return false;
                }
            }
            escaped = false;
            continue;
        }

        if (ch == '\\') {
            escaped = true;
            if (i + 1 >= value.size() - 1) {
                return false;
            }
        }
    }

    return (false == escaped);
}

inline std::string EncodeAsJsonString(const std::string& value)
{
    Core::JSON::VariantContainer holder;
    holder[_T("value")] = value;

    std::string objectText;
    holder.ToString(objectText);

    const size_t separator = objectText.find(':');
    const size_t objectEnd = objectText.rfind('}');
    if ((std::string::npos == separator) || (std::string::npos == objectEnd) || (objectEnd <= separator + 1)) {
        return "\"\"";
    }

    std::string encoded = TrimWhitespace(objectText.substr(separator + 1, objectEnd - separator - 1));

    // Keep compatibility with prior responder output that does not escape '/'.
    size_t pos = 0;
    while ((pos = encoded.find("\\/", pos)) != std::string::npos) {
        encoded.replace(pos, 2, "/");
        ++pos;
    }

    return encoded;
}

inline std::string NormalizeJsonRpcResult(const std::string& result)
{
    const std::string trimmed = TrimWhitespace(result);
    if (true == LooksLikeJsonStringLiteral(trimmed)) {
        return trimmed;
    }

    Core::JSON::VariantContainer parsed;
    if (parsed.FromString(result)) {
        return result;
    }

    LOGWARN("Result payload is not valid JSON value. Sending as quoted string: %s", result.c_str());
    return EncodeAsJsonString(result);
}

class WebSocketConnectionManager
{
public:
    ~WebSocketConnectionManager() {
        if (mChannel) {
            delete mChannel;
            mChannel = nullptr;
        }
    }
    class Config : public Core::JSON::Container
    {
    public:
        Config(const Config &) = delete;
        Config &operator=(const Config &) = delete;
        Config(Config &&) = delete;
        Config &operator=(Config &&) = delete;

        Config(const std::string socketAddress = DEFAULT_SOCKET_ADDRESS)
            : Core::JSON::Container(), Connector(socketAddress)
        {
            Add(_T("connector"), &Connector);
        }
        ~Config() override = default;

    public:
        Core::JSON::String Connector;
    };

    // Forward declarations
    public:
    class WebSocketChannel;

    // WebSocket JSON Object Factory
    class JSONObjectFactory : public Core::FactoryType<Core::JSON::IElement, char *>
    {
    public:
        JSONObjectFactory():Core::FactoryType<Core::JSON::IElement, char *>(), _jsonRPCFactory(5) {}
        JSONObjectFactory(const JSONObjectFactory &) = delete;
        JSONObjectFactory &operator=(const JSONObjectFactory &) = delete;
        ~JSONObjectFactory() = default;

    public:
        static JSONObjectFactory &Instance(){
            static JSONObjectFactory _singleton;
            return (_singleton);
        }
        Core::ProxyType<Core::JSON::IElement> Element(const string &identifier VARIABLE_IS_NOT_USED) {
            Core::ProxyType<Web::JSONBodyType<Core::JSONRPC::Message>> message = _jsonRPCFactory.Element();
            return Core::ProxyType<Core::JSON::IElement>(message);
        }

    private:
        Core::ProxyPoolType<Web::JSONBodyType<Core::JSONRPC::Message>> _jsonRPCFactory;
    };

    // WebSocket Server implementation
    public:
    class WebSocketServer
        : public Core::StreamJSONOneShotType<Web::WebSocket::HWebSocketServerType<Core::SocketStream>, JSONObjectFactory &, Core::JSON::IElement>
    {
    public:
        WebSocketServer() = delete;
        WebSocketServer &operator=(const WebSocketServer &) = delete;
        typedef Core::StreamJSONOneShotType<Web::WebSocket::HWebSocketServerType<Core::SocketStream>, JSONObjectFactory &, Core::JSON::IElement> BaseClass;                

    public:
        WebSocketServer(const SOCKET &connector, const Core::NodeId &remoteNode, Core::SocketServerType<WebSocketServer> *parent) :
         WebSocketServer::BaseClass(
                  5,
                  WebSocketConnectionManager::JSONObjectFactory::Instance(),
                  false, false, false,
                  connector,
                  remoteNode.AnyInterface(),
                  8096, 8096),
        _id(0),
        _parent(static_cast<WebSocketConnectionManager::WebSocketChannel &>(*parent)),
        _queue(10){
            LOGTRACE("Connector value: %d", static_cast<int>(connector));
            LOGTRACE("Remote host: %s", remoteNode.HostAddress().c_str()); 
        }
        ~WebSocketServer() {
            _qLock.Lock();
            _queue.Clear();
            _qLock.Unlock();
            LOGTRACE("WebSocketServer destructed for connectionId: %d", _id);
            
        }

    public:
        void Received(Core::ProxyType<Core::JSON::IElement> &jsonObject){
            
            uint32_t connectionId = _id;

            if (jsonObject.IsValid() == false)
            {
                LOGERR("WebSocketServer: Invalid JSON object received");
            }
            else
            {
                Core::ProxyType<Core::JSONRPC::Message> message = Core::ProxyType<Core::JSONRPC::Message>(jsonObject);
                // Call internal ProcessMessage
                WebSocketConnectionManager::WebSocketServer::ProcessMessage(message, connectionId);
            }
        }
        void Send(Core::ProxyType<Core::JSON::IElement> &jsonObject) {
            if (jsonObject.IsValid() == false)
            {
                LOGERR("WebSocketServer: Invalid JSON object to send");
            }
            else
            {
                WebSocketConnectionManager::WebSocketServer::ToMessage(jsonObject);
            }
        };
        void StateChange(){
            if (this->IsOpen())
            {
                LOGTRACE("Open - OK");
                const std::string &query = Link().Query();
                if (_parent.Interface()._authHandler != nullptr) {
                    bool authResult = _parent.Interface()._authHandler(_id, query);
                    if (!authResult) {
                        LOGERR("Authentication failed for query: %s", query.c_str());
                        this->Close(0);
                        return;
                    }
                    LOGTRACE("Authentication succeeded");
                } else {
                    LOGWARN("No authentication handler set, proceeding without authentication");
                }
            }
            else if(this->IsSuspended())
            {
                LOGTRACE("Closed - %s", this->IsSuspended() ? _T("SUSPENDED") : _T("OK"));
                if (_parent.Interface()._disconnectHandler != nullptr) {
                    _parent.Interface()._disconnectHandler(Id());
                }
            }
        }

        bool IsIdle() const {
            return (true);
        }

        void SendJSONRPCResponse(const std::string &result, int requestId, uint32_t connectionId) {
            Core::ProxyType<Core::JSONRPC::Message> response = Core::ProxyType<Core::JSONRPC::Message>::Create();
                    const std::string normalizedResult = NormalizeJsonRpcResult(result);
                    response->JSONRPC = Core::JSONRPC::Message::DefaultVersion;
                    response->Id = requestId;
                    response->Result = normalizedResult;

                    LOGDBG("[SendJSONRPCResponse] Sending response for requestId=%d, connectionId=%d", requestId, connectionId);
                    LOGDBG("[SendJSONRPCResponse] Response: %s", normalizedResult.c_str());

                    // Send the response back to the WebSocket client
                    this->Submit(Core::ProxyType<Core::JSON::IElement>(response));
        }

    private:
        friend class Core::SocketServerType<WebSocketServer>;

        uint32_t Id() {
            return (_id);
        }

        void Id(const uint32_t id){
            LOGTRACE("Assigning connectionId: %d", id);
            _id = id;

            // Process any pending messages for this connection
            _qLock.Lock();
            while (_queue.Count() > 0) {
                auto message = _queue[0];
                if (message.IsValid()) {
                    LOGDBG("Processing pending message for connectionId: %d", _id);
                    WebSocketConnectionManager::WebSocketServer::ProcessMessage(message, _id);
                }
                _queue.Remove(0, message);
            }
            _qLock.Unlock();
        }

        void ToMessage(const Core::ProxyType<Core::JSON::IElement> &jsonObject) {
            string jsonMessage;
            jsonObject->ToString(jsonMessage);
            LOGTRACE("WebSocket Sent: %s", jsonMessage.c_str());
        }
        void ProcessMessage(Core::ProxyType<Core::JSONRPC::Message> &message, uint32_t connectionId) {
            // Check for message->Id.IsSet()
                    if(!message->Id.IsSet()) {
                        string jsonMessage;
                        message->ToString(jsonMessage);
                        // Log an Error for this usecase
                        LOGERR("Message MUST contain an id field %s", jsonMessage.c_str());
                        return;
                    }

                    int requestId = message->Id.Value();

                    if (_id == 0) {
                        string jsonMessage;
                        message->ToString(jsonMessage);
                        LOGERR("Connection ID Not set adding request to Pending queue %s", jsonMessage.c_str());
                        AddToPending(message);
                        return;
                    }

                    // Extract method name from designator
                    if(!message->Designator.IsSet()) {
                        SendJSONRPCResponse(R"({"error": "Message MUST contain a method field"})", requestId, connectionId);
                        return;
                    }

                    std::string methodName = message->Designator.Value();
                    
                    LOGTRACE("[ProcessMessage] Method: %s, RequestId: %d, ConnectionId: %d",
                            methodName.c_str(), requestId, connectionId);

                    // Extract params once for reuse
                    std::string params = "{}"; // Default empty params
                    if (message->Parameters.IsSet() && !message->Parameters.Value().empty())
                    {
                        params = message->Parameters.Value();
                    }

                    // SYNCHRONOUS PROCESSING
                    try
                    {
                        auto& manager = _parent.Interface();
                        if (manager._messageHandler) {
                            manager._messageHandler(methodName, params, requestId, _id);
                        } else {
                            SendJSONRPCResponse(R"({"error": "Message handler not set"})", requestId, connectionId);
                        }
                    }
                    catch (const std::exception &e)
                    {
                        LOGERR("[ProcessMessage] Exception during synchronous processing: %s", e.what());
                        std::string errorResponse = R"({"error": "Processing exception", "details": ")" + std::string(e.what()) + R"("})";
                        WebSocketConnectionManager::WebSocketServer::SendJSONRPCResponse(errorResponse, requestId, connectionId);
                    }
                    catch (...)
                    {
                        LOGERR("[ProcessMessage] Unknown exception during synchronous processing");
                        WebSocketConnectionManager::WebSocketServer::SendJSONRPCResponse(R"({"error": "Unknown processing exception"})", requestId, connectionId);
                    }

                    #ifdef ENABLE_APP_GATEWAY_AUTOMATION
                    // Forward to automation server after processing the request
                    if (_parent.Interface()._automationId > 0) {
                        AutomationMessage automationMsg;
                        
                        automationMsg.ConnectionId = connectionId;
                        automationMsg.Type = "request";
                        automationMsg.Id = requestId;
                        automationMsg.Method = methodName;
                        automationMsg.Params = params; // Reuse the params variable extracted above
                        
                        string jsonMsg;
                        automationMsg.ToString(jsonMsg);

                        LOGINFO("[Automation] Forwarding request: %s", jsonMsg.c_str());
                        Core::ProxyType<Core::JSONRPC::Message> automationNotif = Core::ProxyType<Core::JSONRPC::Message>::Create();
                        automationNotif->JSONRPC = Core::JSONRPC::Message::DefaultVersion;
                        automationNotif->Designator = "automationUpdate";
                        automationNotif->Parameters = jsonMsg;
                        _parent.Interface().mChannel->Submit(_parent.Interface()._automationId, Core::ProxyType<Core::JSON::IElement>(automationNotif));
                    }
                    #endif
        }

        void FromMessage(Core::JSON::IElement *jsonObject, const Core::ProxyType<Core::JSONRPC::Message> &message){
            jsonObject->FromString(message->Parameters.Value());
        }

        // New Method add message to the _queue
        void AddToPending(Core::ProxyType<Core::JSONRPC::Message>& element)
        {
            _qLock.Lock();
            if (_queue.Count() == 10 ) {
                LOGERR("Queue full for %d processing error for first entry", _id);
                // Remove the first entry
                auto firstElement = _queue[0];
                _queue.Remove(0, firstElement);
                std::string errorResponse = R"({"error": "Message MUST contain a method field"})";
                WebSocketConnectionManager::WebSocketServer::SendJSONRPCResponse(errorResponse, firstElement->Id.Value(), _id);
            }
            //
            _queue.Add(element);
            LOGTRACE("Message queued for connectionId: %d, queue size: %d", _id, static_cast<int>(_queue.Count()));

             _qLock.Unlock();
        }

    private:
        uint32_t _id;
        WebSocketChannel &_parent;
        Core::CriticalSection _qLock;
        Core::ProxyList<Core::JSONRPC::Message> _queue;
    };

    // WebSocket Channel management
    class WebSocketChannel : public Core::SocketServerType<WebSocketServer>
    {
    public:
        WebSocketChannel() = delete;
        WebSocketChannel(const WebSocketChannel &copy) = delete;
        WebSocketChannel &operator=(const WebSocketChannel &) = delete;

    public:
        WebSocketChannel(const WPEFramework::Core::NodeId &remoteNode, WebSocketConnectionManager &parent):
            Core::SocketServerType<WebSocketServer>(remoteNode),
            _parent(parent) {
            Core::SocketServerType<WebSocketConnectionManager::WebSocketServer>::Open(Core::infinite);
        }
        ~WebSocketChannel() {
            Core::SocketServerType<WebSocketServer>::Close(1000);
        }
        WebSocketConnectionManager &Interface() {
            return _parent;
        }

    private:
        WebSocketConnectionManager &_parent;
    };

public:
        // Message handler callback type
    using MessageHandler = std::function<void(const std::string& method, const std::string& params, const uint32_t requestId, const uint32_t connectionId)>;
    using AuthHandler = std::function<bool(const uint32_t connectionId, const std::string& token)>;
    using DisconnectHandler = std::function<void(const uint32_t connectionId)>;

#ifdef ENABLE_APP_GATEWAY_AUTOMATION
    // JSON container classes for automation messages
    class AutomationMessage : public Core::JSON::Container {
    public:
        AutomationMessage() : Core::JSON::Container() {
            Add(_T("connectionId"), &ConnectionId);
            Add(_T("type"), &Type);
            Add(_T("id"), &Id);
            Add(_T("method"), &Method);
            Add(_T("params"), &Params);
            Add(_T("payload"), &Payload);
        }
        Core::JSON::DecUInt32 ConnectionId;
        Core::JSON::String Type;
        Core::JSON::DecUInt32 Id;
        Core::JSON::String Method;
        Core::JSON::String Params;
        Core::JSON::String Payload;
    };

    class ConnectionUpdate : public Core::JSON::Container {
    public:
        ConnectionUpdate() : Core::JSON::Container() {
            Add(_T("connectionId"), &ConnectionId);
            Add(_T("appId"), &AppId);
            Add(_T("connected"), &Connected);
        }
        Core::JSON::DecUInt32 ConnectionId;
        Core::JSON::String AppId;
        Core::JSON::Boolean Connected;
    };
#endif

    // New method to add a message Handler into ProcessMessage method
    void SetMessageHandler(const MessageHandler& handler) { _messageHandler = handler; }

    void SetAuthHandler(const AuthHandler& handler) { _authHandler = handler; }

    void SetDisconnectHandler(DisconnectHandler handler) { _disconnectHandler = handler; }

    // NEW: Setter for automation ID
    void SetAutomationId(uint32_t automationId) { 
        _automationId = automationId; 
        LOGINFO("Automation ID set to: %d", _automationId);
    }

private:
    // Helper method to forward messages to automation server
    void ForwardToAutomation(const std::string& designator, const std::string& payload) {
        #ifdef ENABLE_APP_GATEWAY_AUTOMATION
        if (_automationId > 0) {
            Core::ProxyType<Core::JSONRPC::Message> automationNotif = Core::ProxyType<Core::JSONRPC::Message>::Create();
            automationNotif->JSONRPC = Core::JSONRPC::Message::DefaultVersion;
            automationNotif->Designator = designator;
            automationNotif->Parameters = payload;
            mChannel->Submit(_automationId, Core::ProxyType<Core::JSON::IElement>(automationNotif));
            LOGINFO("[Automation] Forwarded to automation server: %s", payload.c_str());
        }
        #endif
    }

public:
    // Create a new method which can send message to a given connection id using the connection registry
    // Use the SendJSONRPCResponse in Websocket Server to send the message
    bool SendMessageToConnection(const uint32_t connectionId, const std::string &result, const int requestId)
    {
        Core::ProxyType<Core::JSONRPC::Message> response = Core::ProxyType<Core::JSONRPC::Message>::Create();
        response->JSONRPC = Core::JSONRPC::Message::DefaultVersion;
        response->Id = requestId;
        std::string automationPayload = result;

        Core::JSONRPC::Message::Info info;
        if (info.FromString(result) && info.Code.IsSet() && info.Text.IsSet()) {
            response->Error = info;
        } else {
            automationPayload = NormalizeJsonRpcResult(result);
            response->Result = automationPayload;
        }

    LOGTRACE("[SendJSONRPCResponse] Sending response for requestId=%d, connectionId=%d response=%s", requestId, connectionId, automationPayload.c_str());

        // Send the response back to the WebSocket client
        if (nullptr == mChannel) {
            LOGWARN("[SendJSONRPCResponse] mChannel is null, dropping response for requestId=%d, connectionId=%d", requestId, connectionId);
            return false;
        }
        mChannel->Submit(connectionId, Core::ProxyType<Core::JSON::IElement>(response));
        
        #ifdef ENABLE_APP_GATEWAY_AUTOMATION
        // Forward to automation server after sending response
        if (_automationId > 0 && connectionId != _automationId) {
            AutomationMessage automationMsg;
            
            automationMsg.ConnectionId = connectionId;
            automationMsg.Type = "response";
            automationMsg.Id = requestId;
            automationMsg.Payload = automationPayload;
            
            string jsonMsg;
            automationMsg.ToString(jsonMsg);
            ForwardToAutomation("automationUpdate", jsonMsg);
        }
        #endif

        return true;
    }

    bool DispatchNotificationToConnection(const uint32_t connectionId, const std::string &designator, const std::string &payload)
    {
        Core::ProxyType<Core::JSONRPC::Message> event = Core::ProxyType<Core::JSONRPC::Message>::Create();
        event->JSONRPC = Core::JSONRPC::Message::DefaultVersion;
        event->Designator = designator;
        event->Parameters = payload;
        LOGTRACE("Emit Event for method=%s, connectionId=%d params=%s", designator.c_str(), connectionId, payload.c_str());
        if (nullptr == mChannel) {
            LOGWARN("[DispatchNotificationToConnection] mChannel is null, dropping notification for method=%s, connectionId=%d", designator.c_str(), connectionId);
            return false;
        }
        mChannel->Submit(connectionId, Core::ProxyType<Core::JSON::IElement>(event));
        
        #ifdef ENABLE_APP_GATEWAY_AUTOMATION
        // Forward to automation server after sending notification
        if (_automationId > 0 && connectionId != _automationId) {
            AutomationMessage automationMsg;
            
            automationMsg.ConnectionId = connectionId;
            automationMsg.Type = "notification";
            automationMsg.Method = designator;
            automationMsg.Params = payload;
            
            string jsonMsg;
            automationMsg.ToString(jsonMsg);
            ForwardToAutomation("automationUpdate", jsonMsg);
        }
        #endif
        
        return true;
    }

    bool SendRequestToConnection(const uint32_t connectionId, const std::string &designator, const uint32_t requestId, const std::string &params)
    {
        Core::ProxyType<Core::JSONRPC::Message> request = Core::ProxyType<Core::JSONRPC::Message>::Create();
        request->JSONRPC = Core::JSONRPC::Message::DefaultVersion;
        request->Id = requestId;
        request->Designator = designator;
        request->Parameters = params;

        LOGTRACE("Send Request for method=%s, connectionId=%d params=%s", designator.c_str(), connectionId, params.c_str());
        if (nullptr == mChannel) {
            LOGWARN("[SendRequestToConnection] mChannel is null, dropping request for method=%s, connectionId=%d", designator.c_str(), connectionId);
            return false;
        }
        mChannel->Submit(connectionId, Core::ProxyType<Core::JSON::IElement>(request));

        #ifdef ENABLE_APP_GATEWAY_AUTOMATION
        // Forward to automation server after sending request
        if (_automationId > 0 && connectionId != _automationId) {
            AutomationMessage automationMsg;
            
            automationMsg.ConnectionId = connectionId;
            automationMsg.Type = "request";
            automationMsg.Id = requestId;
            automationMsg.Method = designator;
            automationMsg.Params = params;
            
            string jsonMsg;
            automationMsg.ToString(jsonMsg);
            ForwardToAutomation("automationUpdate", jsonMsg);
        }
        #endif

        return true;
    }

    // Method to update connection status to automation server
    void UpdateConnection(uint32_t connectionId, const std::string& appId, bool connected) {
        #ifdef ENABLE_APP_GATEWAY_AUTOMATION
        if (_automationId > 0) {
            ConnectionUpdate updateMsg;
            
            updateMsg.ConnectionId = connectionId;
            updateMsg.AppId = appId;
            updateMsg.Connected = connected;
            
            string jsonMsg;
            updateMsg.ToString(jsonMsg);
            ForwardToAutomation("connectionUpdate", jsonMsg);
        }
        #endif
    }

    // New Method to start websocket channel using NodeId
    bool Start(const Core::NodeId &remoteNode)
    {
        try
        {
            mChannel = new WebSocketChannel(remoteNode, *this);
            if (nullptr == mChannel)
            {
                LOGERR("Failed to create WebSocket channel");
                return false;
            }

            LOGINFO("WebSocket channel started successfully on %s %d", remoteNode.HostAddress().c_str(), remoteNode.PortNumber());
            return true;
        }
        catch (const std::exception &e)
        {
            LOGERR("Exception while starting WebSocket channel: %s", e.what());
            return false;
        }
        catch (...)
        {
            LOGERR("Unknown exception while starting WebSocket channel");
            return false;
        }
    }

    // Close connection for a given connection id
    void Close(const uint32_t connectionId) {
        if (nullptr == mChannel) {
            LOGWARN("[Close] mChannel is null, cannot close connectionId=%d", connectionId);
            return;
        }
        const auto& client = mChannel->Client(connectionId);
        client->Close(0);
    }

private:

    MessageHandler _messageHandler;
    AuthHandler _authHandler;
    DisconnectHandler _disconnectHandler;
    WebSocketChannel *mChannel = nullptr;
    uint32_t _automationId = 0;
};
