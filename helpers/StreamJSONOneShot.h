//  Forked one-shot variant of StreamJSONType to workaround issue with streaming JSON parsing
//  of invalid json data that causes infinite loops.
//  Original StreamJSONType tries to parse incoming data in a loop until all data is consumed,
//  which can lead to infinite loops if the data is malformed. This variant only attempts to parse the data once per call.
//  - Parses incoming buffer in a single call, instead of looping.
//  - Public API mirrors the original StreamJSONType.

#pragma once

#include <core/JSON.h>
#include <core/core.h>
#include <plugins/plugins.h>
#include <mutex>
#include <cstdio>
#include <ctime>

// Enable payload logging for debugging large JSON/MessagePack deserialization.
// Disabled by default to avoid unexpected raw payload dumps and file I/O overhead
// in production builds. Define ENABLE_PAYLOAD_LOGGING=1 at build time to enable.
#ifndef ENABLE_PAYLOAD_LOGGING
#define ENABLE_PAYLOAD_LOGGING 0
#endif

namespace WPEFramework {
namespace Core {

    template <typename SOURCE, typename ALLOCATOR, typename INTERFACE /*= Core::JSON::IElement or IMessagePack*/>
    class StreamJSONOneShotType {
    private:
        using ParentClass = StreamJSONOneShotType<SOURCE, ALLOCATOR, INTERFACE>;

        class SerializerImpl {
        public:
            SerializerImpl() = delete;
            SerializerImpl(const SerializerImpl&) = delete;
            SerializerImpl& operator=(const SerializerImpl&) = delete;

            SerializerImpl(ParentClass& parent, const uint8_t slotSize)
                : _parent(parent)
                , _adminLock()
                , _sendQueue(slotSize)
                , _offset(0)
            {
            }

            ~SerializerImpl() {
                _sendQueue.Clear();
            }

            bool IsIdle() const {
                return (_sendQueue.Count() == 0);
            }

            bool Submit(const ProxyType<INTERFACE>& entry) {
                _adminLock.Lock();
                _sendQueue.Add(const_cast<ProxyType<INTERFACE>&>(entry));
                const bool trigger = (_sendQueue.Count() == 1);
                _adminLock.Unlock();
                return trigger;
            }

            uint16_t Serialize(uint8_t* stream, const uint16_t length) const {
                uint16_t loaded = 0;

                _adminLock.Lock();
                if (_sendQueue.Count() > 0) {
                    loaded = Serialize(_sendQueue[0], stream, length);
                    // If fully sent or we're not in a partial-send state, notify and pop
                    if ((0 == _offset) || (length != loaded)) {
                        Core::ProxyType<INTERFACE> current = _sendQueue[0];
                        _parent.Send(current);
                        _sendQueue.Remove(0);
                        _offset = 0;
                    }
                }
                _adminLock.Unlock();

                return loaded;
            }

        private:
            // Overloads for JSON text vs MessagePack
            inline uint16_t Serialize(const Core::ProxyType<Core::JSON::IElement>& source,
                                      uint8_t* stream, const uint16_t length) const {
                return source->Serialize(reinterpret_cast<char*>(stream), length, _offset);
            }

            inline uint16_t Serialize(const Core::ProxyType<Core::JSON::IMessagePack>& source,
                                      uint8_t* stream, const uint16_t length) const {
                return source->Serialize(stream, length, _offset);
            }

        private:
            ParentClass& _parent;
            mutable Core::CriticalSection _adminLock;
            mutable Core::ProxyList<INTERFACE> _sendQueue;
            mutable uint32_t _offset;
        };

        class DeserializerImpl {
        public:
            DeserializerImpl() = delete;
            DeserializerImpl(const DeserializerImpl&) = delete;
            DeserializerImpl& operator=(const DeserializerImpl&) = delete;

            DeserializerImpl(ParentClass& parent, const uint8_t slotSize)
                : _parent(parent)
                , _factory(slotSize)
                , _current()
                , _mutex()
                , _offset(0)
            {
            }

            DeserializerImpl(ParentClass& parent, ALLOCATOR allocator)
                : _parent(parent)
                , _factory(allocator)
                , _current()
                , _mutex()
                , _offset(0)
            {
            }

            bool IsIdle() const {
                std::lock_guard<std::mutex> lock(_mutex);
                return (false == _current.IsValid());
            }

            // One-shot entry: parse current buffer once with RAII-based synchronization.
            uint16_t Deserialize(const uint8_t* stream, const uint16_t length) {
                uint16_t loaded = 0;
                Core::ProxyType<INTERFACE> deliver;

#if ENABLE_PAYLOAD_LOGGING
                // Log incoming payload for debugging large 150KB+ JSON/MessagePack frames.
                // Protected against concurrent Deserialize() calls with once_flag init and mutex writes.
                static std::once_flag payloadFileInitFlag;
                static std::mutex payloadLogMutex;
                static FILE* payloadFile = nullptr;

                std::call_once(payloadFileInitFlag, []() {
                    payloadFile = fopen("/tmp/appgateway_payload.log", "a");
                    if (nullptr != payloadFile) {
                        fprintf(payloadFile, "\n===== StreamJSONOneShot::Deserialize Logging Initialized =====\n");
                        fflush(payloadFile);
                    }
                });

                if ((nullptr != payloadFile) && (0 < length)) {
                    uint32_t currentOffset = 0;
                    bool isParsing = false;
                    {
                        std::lock_guard<std::mutex> logStateLock(_mutex);
                        currentOffset = _offset;
                        isParsing = _current.IsValid();
                    }
                    std::lock_guard<std::mutex> payloadLogLock(payloadLogMutex);
                    time_t now = std::time(nullptr);
                    fprintf(payloadFile, "\n========== INCOMING PAYLOAD (Frame #%u bytes) ==========\n", length);
                    fprintf(payloadFile, "Timestamp: %ld\n", now);
                    fprintf(payloadFile, "Current Offset: %u\n", currentOffset);
                    fprintf(payloadFile, "Is Parsing: %s\n", (true == isParsing) ? "YES" : "NO");
                    fprintf(payloadFile, "------- PAYLOAD DATA (First 1000 bytes) -------\n");

                    // Write first 1000 bytes or entire payload if smaller
                    uint16_t bytesToLog = (1000 < length) ? 1000 : length;
                    fwrite(stream, 1, bytesToLog, payloadFile);

                    if (1000 < length) {
                        fprintf(payloadFile, "\n... (truncated, total %u bytes) ...\n", length);
                    }
                    fprintf(payloadFile, "\n------- END PAYLOAD -------\n");
                    fprintf(payloadFile, "==============================================\n");
                    fflush(payloadFile);
                }
#endif

                // RAII-based mutex synchronization prevents race conditions when
                // multiple TCP frames arrive for large payloads across concurrent callbacks.
                {
                    std::lock_guard<std::mutex> lock(_mutex);

                    if (false == _current.IsValid()) {
                        _current = Core::ProxyType<INTERFACE>(_factory.Element(EMPTY_STRING));
                        _offset = 0;
                    }

                    if (true == _current.IsValid()) {
                        loaded = Deserialize(_current, stream, length);
                        // Deliver message when:
                        // 1. offset == 0: Parser reset (complete JSON was parsed in previous call or we're starting fresh)
                        // 2. loaded < length: Parser found complete JSON and stopped mid-frame (didn't consume all bytes)
                        // 
                        // This ensures large payloads across multiple frames are fully reassembled before delivery.
                        // The critical insight: loaded=bytes_consumed by parser, so if loaded < length, JSON is complete.
                        if ((0 == _offset) || (length != loaded)) {
                            // Detach completed message from shared state while holding the lock.
                            ASSERT(_current.IsValid());
                            // CRITICAL: Reset offset BEFORE release to ensure clean state for next message.
                            // Without this, subsequent messages would start with stale offset value.
                            _offset = 0;
                            deliver = _current;
                            _current.Release();
                        }
                    }
                } // lock_guard automatically releases mutex

#if ENABLE_PAYLOAD_LOGGING
                if (deliver.IsValid() && (nullptr != payloadFile)) {
                    std::lock_guard<std::mutex> payloadLogLock(payloadLogMutex);
                    fprintf(payloadFile, "[DELIVERY] Complete message parsed and delivering to parent handler\n");
                    fflush(payloadFile);
                }
#endif

                if (deliver.IsValid()) {
                    _parent.Received(deliver);
                }

                return loaded;
            }

        private:
            inline uint16_t Deserialize(const Core::ProxyType<Core::JSON::IElement>& source,
                                        const uint8_t* stream, const uint16_t length) {
                return source->Deserialize(reinterpret_cast<const char*>(stream), length, _offset);
            }

            inline uint16_t Deserialize(const Core::ProxyType<Core::JSON::IMessagePack>& source,
                                        const uint8_t* stream, const uint16_t length) {
                return source->Deserialize(stream, length, _offset);
            }

        private:
            ParentClass& _parent;
            ALLOCATOR _factory;
            Core::ProxyType<INTERFACE> _current;
            mutable std::mutex _mutex;
            uint32_t _offset;
        };

        class HandlerType : public SOURCE {
        public:
            HandlerType() = delete;
            HandlerType(const HandlerType&) = delete;
            HandlerType& operator=(const HandlerType&) = delete;

            explicit HandlerType(ParentClass& parent)
                : SOURCE()
                , _parent(parent) {
            }

            template <typename... Args>
            HandlerType(ParentClass& parent, Args... args)
                : SOURCE(args...)
                , _parent(parent) {
            }

            ~HandlerType() override = default;

        public:
            // Pass-through to parent
            uint16_t SendData(uint8_t* dataFrame, const uint16_t maxSendSize) override {
                return _parent.SendData(dataFrame, maxSendSize);
            }

            uint16_t ReceiveData(uint8_t* dataFrame, const uint16_t receivedSize) override {
                return _parent.ReceiveData(dataFrame, receivedSize);
            }

            void StateChange() override {
                _parent.StateChange();
            }

            bool IsIdle() const override {
                return _parent.IsIdle();
            }

        private:
            ParentClass& _parent;
        };

    public:
        StreamJSONOneShotType(const StreamJSONOneShotType&) = delete;
        StreamJSONOneShotType& operator=(const StreamJSONOneShotType&) = delete;

    PUSH_WARNING(DISABLE_WARNING_THIS_IN_MEMBER_INITIALIZER_LIST)
        // Constructor with external allocator
        template <typename... Args>
        StreamJSONOneShotType(uint8_t slotSize, ALLOCATOR& allocator, Args... args)
            : _channel(*this, args...)
            , _serializer(*this, slotSize)
            , _deserializer(*this, allocator) {
        }

        // Constructor with slot-sized factory
        template <typename... Args>
        StreamJSONOneShotType(uint8_t slotSize, Args... args)
            : _channel(*this, args...)
            , _serializer(*this, slotSize)
            , _deserializer(*this, slotSize) {
        }
    POP_WARNING()

        virtual ~StreamJSONOneShotType() {
            _channel.Close(Core::infinite);
        }

    public:
        inline SOURCE& Link() { return _channel; }

        virtual void Received(ProxyType<INTERFACE>& element) = 0;
        virtual void Send(ProxyType<INTERFACE>& element) = 0;
        virtual void StateChange() = 0;

        inline void Submit(const ProxyType<INTERFACE>& element) {
            if (_channel.IsOpen() == true) {
                if (_serializer.Submit(element)) {
                    _channel.Trigger();
                }
            }
        }

        inline uint32_t Open(const uint32_t waitTime) { return _channel.Open(waitTime); }
        inline uint32_t Close(const uint32_t waitTime) { return _channel.Close(waitTime); }
        inline bool IsOpen() const { return _channel.IsOpen(); }
        inline bool IsClosed() const { return _channel.IsClosed(); }
        inline bool IsSuspended() const { return _channel.IsSuspended(); }

    private:
        uint16_t SendData(uint8_t* dataFrame, const uint16_t maxSendSize) {
            return _serializer.Serialize(dataFrame, maxSendSize);
        }

        uint16_t ReceiveData(uint8_t* dataFrame, const uint16_t receivedSize) {
            return _deserializer.Deserialize(&dataFrame[0], receivedSize);
        }

        bool IsIdle() const {
            return (_serializer.IsIdle() && _deserializer.IsIdle());
        }

    private:
        HandlerType   _channel;
        SerializerImpl   _serializer;
        DeserializerImpl _deserializer;
    };

} // namespace Core
} // namespace WPEFramework
