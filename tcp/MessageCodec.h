#pragma once
#include <common/base/Types.h> 
#include <common/base/Logging.h>
#include <common/net/Endian.h>
#include <google/protobuf/message.h>
#include <common/net/Buffer.h>
#include "proto/message.pb.h"
#include <google/protobuf/descriptor.h>
#include <zlib.h>  // adler32

using namespace common;
using namespace common::net;

// When serializing, we first compute the byte size, then serialize the message.
// If serialization produces a different number of bytes than expected, we
// call this function, which crashes.  The problem could be due to a bug in the
// protobuf implementation but is more likely caused by concurrent modification
// of the message.  This function attempts to distinguish between the two and
// provide a useful error message.
inline
void ByteSizeConsistencyError(int byte_size_before_serialization,
                              int byte_size_after_serialization,
                              int bytes_produced_by_serialization)
{
  GOOGLE_CHECK_EQ(byte_size_before_serialization, byte_size_after_serialization)
      << "Protocol message was modified concurrently during serialization.";
  GOOGLE_CHECK_EQ(bytes_produced_by_serialization, byte_size_before_serialization)
      << "Byte size calculation and serialization were inconsistent.  This "
         "may indicate a bug in protocol buffers or it may be caused by "
         "concurrent modification of the message.";
  GOOGLE_LOG(FATAL) << "This shouldn't be called if all the sizes are equal.";
}

inline
std::string InitializationErrorMessage(const char* action,
                                       const google::protobuf::MessageLite& message)
{
  // Note:  We want to avoid depending on strutil in the lite library, otherwise
  //   we'd use:
  //
  // return strings::Substitute(
  //   "Can't $0 message of type \"$1\" because it is missing required "
  //   "fields: $2",
  //   action, message.GetTypeName(),
  //   message.InitializationErrorString());

  std::string result;
  result += "Can't ";
  result += action;
  result += " message of type \"";
  result += message.GetTypeName();
  result += "\" because it is missing required fields: ";
  result += message.InitializationErrorString();
  return result;
}

inline
int32_t asInt32(const char* buf)
{
  int32_t be32 = 0;
  ::memcpy(&be32, buf, sizeof(be32));
  return sockets::networkToHost32(be32);
}

class MessageCodec {
public:

    enum ErrorCode
    {
        kNoError = 0,
        kInvalidLength,
        kCheckSumError,
        kInvalidNameLen,
        kUnknownMessageType,
        kParseError,
    };

    const static int kHeaderLen = sizeof(int32_t);
    const static int kMinMessageLen = 2*kHeaderLen + 2; // nameLen + typeName + checkSum
    const static int kMaxMessageLen = 64*1024*1024; // same as codec_stream.h kDefaultTotalBytesLimit
    
    // 编码消息到Buffer
    static void encode(common::net::Buffer* buf, const google::protobuf::Message& message) {

        assert(buf->readableBytes() == 0);

        const std::string& typeName = message.GetTypeName();
        int32_t nameLen = static_cast<int32_t>(typeName.size()+1);
        buf->appendInt32(nameLen);
        buf->append(typeName.c_str(), nameLen);

        // code copied from MessageLite::SerializeToArray() and MessageLite::SerializePartialToArray().
        GOOGLE_DCHECK(message.IsInitialized()) << InitializationErrorMessage("serialize", message);

        #if GOOGLE_PROTOBUF_VERSION > 3009002
            int byte_size = google::protobuf::internal::ToIntSize(message.ByteSizeLong());
        #else
            int byte_size = message.ByteSize();
        #endif
        buf->ensureWritableBytes(byte_size);
        
        uint8_t* start = reinterpret_cast<uint8_t*>(buf->beginWrite());
        uint8_t* end = message.SerializeWithCachedSizesToArray(start);
        if (end - start != byte_size)
        {
            #if GOOGLE_PROTOBUF_VERSION > 3009002
            ByteSizeConsistencyError(byte_size, google::protobuf::internal::ToIntSize(message.ByteSizeLong()), static_cast<int>(end - start));
            #else
            ByteSizeConsistencyError(byte_size, message.ByteSize(), static_cast<int>(end - start));
            #endif
        }
        buf->hasWritten(byte_size);
        int32_t checkSum = static_cast<int32_t>(
                ::adler32(1,
                reinterpret_cast<const Bytef*>(buf->peek()),
                static_cast<int>(buf->readableBytes())));
        buf->appendInt32(checkSum);
        assert(buf->readableBytes() == sizeof nameLen + nameLen + byte_size + sizeof checkSum);
        int32_t len = sockets::hostToNetwork32(static_cast<int32_t>(buf->readableBytes()));
        buf->prepend(&len, sizeof len);
    }
    
    // 从Buffer解码消息
    static int decode(common::net::Buffer* buf, std::shared_ptr<google::protobuf::Message>& message) {
        while (buf->readableBytes() >= kMinMessageLen + kHeaderLen) {
            const int32_t len = buf->peekInt32();
            if (len > kMaxMessageLen || len < kMinMessageLen)
            {
                LOG_ERROR<<"len: "<< len << ", kMaxMessageLen:"<< kMaxMessageLen << ", kMinMessageLen:"<< kMinMessageLen;
                return -1;
            }
            else if (buf->readableBytes() >= implicit_cast<size_t>(len + kHeaderLen))
            {
                ErrorCode errorCode = kNoError;
                message = MessageCodec::parse(buf->peek()+kHeaderLen, len, &errorCode);
                if (errorCode == kNoError && message)
                {
                    buf->retrieve(kHeaderLen+len);
                }
                else
                {
                    LOG_ERROR<<"parse message fail, errorCode:"<< errorCode;
                    return -2;
                }
            }
        }
        return 0;
    }

    static google::protobuf::Message* createMessage(const std::string& typeName)
    {
        google::protobuf::Message* message = NULL;
        const google::protobuf::Descriptor* descriptor =
            google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(typeName);
        if (descriptor)
        {
            const google::protobuf::Message* prototype =
            google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
            if (prototype)
            {
            message = prototype->New();
            }
        }
        return message;
    }

    static std::shared_ptr<google::protobuf::Message> parse(const char* buf, int len, ErrorCode* error)
    {
        std::shared_ptr<google::protobuf::Message> message;

        // check sum
        int32_t expectedCheckSum = asInt32(buf + len - kHeaderLen);
        int32_t checkSum = static_cast<int32_t>(
            ::adler32(1,
                        reinterpret_cast<const Bytef*>(buf),
                        static_cast<int>(len - kHeaderLen)));
        if (checkSum == expectedCheckSum)
        {
            // get message type name
            int32_t nameLen = asInt32(buf);
            if (nameLen >= 2 && nameLen <= len - 2*kHeaderLen)
            {
            std::string typeName(buf + kHeaderLen, buf + kHeaderLen + nameLen - 1);
            // create message object
            message.reset(MessageCodec::createMessage(typeName));
            if (message)
            {
                // parse from buffer
                const char* data = buf + kHeaderLen + nameLen;
                int32_t dataLen = len - nameLen - 2*kHeaderLen;
                if (message->ParseFromArray(data, dataLen))
                {
                *error = kNoError;
                }
                else
                {
                *error = kParseError;
                }
            }
            else
            {
                *error = kUnknownMessageType;
            }
            }
            else
            {
            *error = kInvalidNameLen;
            }
        }
        else
        {
            *error = kCheckSumError;
        }
        return message;
    }
};