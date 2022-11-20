#include <boost/asio/io_service.hpp>
#include <amqpcpp.h>
#include <amqpcpp/libboostasio.h>

#include <unistd.h>

#include "json.h"

using json = nlohmann::json;

int help(
    int argc, char *const argv[],
    const char *message = nullptr,
    int r = EXIT_FAILURE)
{
    (void)argc;
    if(message) std::cout << "WARNING: " << message << '\n';

    std::cout
        << argv[0]
        <<
            " -a broker_address"
            " -q queue"
            " [-t consumerTag]"
            " -d device"
            " [-r rate (1200-115200)]"
            " [-p parity(O/E/N)]"
        << std::endl;
    return r;
}

void startConsuming(
    AMQP::Channel &channel,
    const std::string &queue,
    const std::string &consumerTag,
    Modbus::RTU::Master &rtuMaster)
{
    auto &r = channel.consume(queue, consumerTag);

    r.onReceived(
        [&](const AMQP::Message &msg, uint64_t tag, bool redelivered)
        {
            TRACE(
                TraceLevel::Debug,
                "key ", msg.routingkey(),
                ", tag ", tag,
                ", redelivered ", redelivered,
                ", hasReplyTo ", msg.hasReplyTo(),
                ", size ", msg.size(),
                ", bodySize ", msg.bodySize());

            auto input = json::parse(msg.body(), msg.body() + msg.bodySize());

            TRACE(TraceLevel::Debug, input.dump());

            ENSURE(input.is_array(), RuntimeError);

            json output;

            for(const auto &object : input)
            {
                ENSURE(object.is_object(), RuntimeError);
                Modbus::RTU::JSON::dispatch(rtuMaster, object, output);
            }

            auto result = output.dump();
            TRACE(TraceLevel::Debug, "result ", result);

            if(msg.hasReplyTo())
            {
                // exchange is set to "" (default), this is required by reply-to
                channel.publish(
                    "", msg.replyTo(),
                    AMQP::Envelope{result.c_str(), result.size()});
            }

            channel.ack(tag);
        });
}

void addQueue(
    boost::asio::io_service &service,
    AMQP::Channel &channel,
    const std::string &queue,
    const std::string &consumerTag,
    Modbus::RTU::Master &rtuMaster)
{
    auto &r = channel.declareQueue(queue, AMQP::exclusive);

    r.onError(
        [&](const char *msg)
        {
            TRACE(TraceLevel::Error, msg);
            service.stop();
        });

    r.onSuccess(
        [&](const std::string &name, uint32_t msgCnt, uint32_t consumerCntr)
        {
            TRACE(
                TraceLevel::Info,
                "added ", name,
                ", msgCnt ", msgCnt,
                ", consumerCntr ", consumerCntr);

            startConsuming(channel, queue, consumerTag, rtuMaster);
        });
}

int start(
    const std::string &address,
    const std::string &queue,
    const std::string &consumerTag,
    Modbus::RTU::Master &rtuMaster)
{
    boost::asio::io_service service(1);
    AMQP::LibBoostAsioHandler handler(service);
    AMQP::TcpConnection connection(&handler, AMQP::Address(address));
    AMQP::TcpChannel channel(&connection);

    channel.onError(
        [&](const char *msg)
        {
            TRACE(TraceLevel::Error, msg);
            service.stop();
        });

    channel.onReady(
        [&]()
        {
            TRACE(TraceLevel::Info, "ready");
            addQueue(service, channel, queue, consumerTag, rtuMaster);
        });

    return service.run();
}

int main(int argc, char *const argv[])
{
    std::string address;
    std::string queue;
    std::string consumerTag;
    std::string device;
    std::string rate = "19200";
    std::string parity = "E";

    for(int c; -1 != (c = ::getopt(argc, argv, "ha:d:e:q:p:r:t:"));)
    {
        switch(c)
        {
            case 'h':
                return help(argc, argv, EXIT_SUCCESS);
            case 'a':
                address = optarg ? optarg : "";
                break;
            case 'd':
                device = optarg ? optarg : "";
                break;
            case 'q':
                queue = optarg ? optarg : "";
                break;
            case 'p':
                parity = optarg ? optarg : "";
                break;
            case 'r':
                rate = optarg ? optarg : "";
                break;
            case 't':
                consumerTag = optarg ? optarg : "";
                break;
            case ':':
            case '?':
            default:
                return help(argc, argv, "geopt() failure");
        }
    }

    if(
        address.empty()
        || queue.empty()
        || device.empty()) return help(argc, argv, "missing required arguments");

    try
    {
        Modbus::RTU::Master rtuMaster
        {
            device.c_str(),
            Modbus::toBaudRate(rate),
            Modbus::toParity(parity),
            Modbus::SerialPort::DataBits::Eight,
            Modbus::SerialPort::StopBits::One
        };

        return start(address, queue, consumerTag, rtuMaster);
    }
    catch(const json::parse_error &e)
    {
        TRACE(
            TraceLevel::Error,
            "JSON parse error ", e.what(),
            ", id ", e.id,
            ", byte position ", e.byte);
        return EXIT_FAILURE;
    }
    catch(const std::exception &except)
    {
        TRACE(TraceLevel::Error, "std exception ", except.what());
        return EXIT_FAILURE;
    }
    catch(...)
    {
        TRACE(TraceLevel::Error, "unsupported exception");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
