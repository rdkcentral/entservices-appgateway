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
                    // If fully sent or we’re not in a partial-send state, notify and pop
                    if ((_offset == 0) || (loaded != length)) {
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
                , _adminLock()
                , _offset(0)
            {
            }

            DeserializerImpl(ParentClass& parent, ALLOCATOR allocator)
                : _parent(parent)
                , _factory(allocator)
                , _current()
                , _adminLock()
                , _offset(0)
            {
            }

            bool IsIdle() const {
                return (_current.IsValid() == false);
            }

            // One-shot entry: parse current buffer once.
            uint16_t Deserialize(const uint8_t* stream, const uint16_t length) {
                uint16_t loaded = 0;

                // FIX 4: Thread-safe offset management prevents race conditions when
                // multiple TCP frames arrive for large payloads across concurrent callbacks.
                _adminLock.Lock();

                if (_current.IsValid() == false) {
                    _current = Core::ProxyType<INTERFACE>(_factory.Element(EMPTY_STRING));
                    _offset = 0;
                }

                if (_current.IsValid() == true) {
                    loaded = Deserialize(_current, stream, length);

                    // Log deserialization progress for debugging large payloads
                    fprintf(stderr, "[FASIL] [StreamJSONOneShot::Deserialize] Bytes loaded: %u, Current offset: %u, Stream length: %u\n", 
                            loaded, _offset, length);

                    // Only deliver message when deserialization is complete (offset explicitly reset by callee).
                    // This ensures multi-frame payloads (e.g., 200KB across 25+ TCP packets) are fully
                    // assembled before delivery. The previous logic incorrectly delivered incomplete
                    // messages when loaded != length, causing subsequent requests to be corrupted.
                    if (_offset == 0) {
                        fprintf(stderr, "[FASIL] [StreamJSONOneShot::Deserialize] DESERIALIZATION COMPLETE - Delivering message\n");
                        fprintf(stderr, "[FASIL] [StreamJSONOneShot::Deserialize] Final offset value: %u (0 = complete)\n", _offset);
                        _adminLock.Unlock();
                        _parent.Received(_current);
                        _adminLock.Lock();
                        _current.Release();
                    } else {
                        fprintf(stderr, "[FASIL] [StreamJSONOneShot::Deserialize] Deserialization in progress - Offset: %u (awaiting more frames)\n", _offset);
                    }
                }

                _adminLock.Unlock();
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
            mutable Core::CriticalSection _adminLock; 
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

