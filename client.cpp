#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <unistd.h>

#include <amqpcpp.h>
#include <amqpcpp/libboostasio.h>
#include <boost/asio/io_service.hpp>

#include <nlohmann/json.hpp>

#include "Trace.h"

using json = nlohmann::json;

int help(
    int argc, char *argv[],
    const char *message = nullptr,
    int r = EXIT_FAILURE)
{
    (void)argc;

    if(message) std::cout << "WARNING: " << message << '\n';

    std::cerr
        << argv[0]
        <<
            " -a broker_address"
            " -i [input.json|-]"
            " -x exchange"
            " -k key"
        << std::endl;
    return r;
}

void send(
    boost::asio::io_service &service,
    AMQP::Channel &channel,
    const std::string &exchange,
    const std::string &key,
    const std::string &input)
{
    assert(input.empty());

    std::ifstream file(input, std::ios_base::binary | std::ios_base::in);

    file >> std::noskipws;

    std::vector<char> data;

    std::copy(
        std::istream_iterator<char>{file},
        std::istream_iterator<char>(),
        std::back_inserter(data));

    assert(!exchange.empty());
    assert(!key.empty());

    TRACE(
        TraceLevel::Info,
        "publishing to exchange: ", exchange,
        ", with key: ", key,
        ", data size: ", data.size());

    channel.startTransaction();

    channel.publish(exchange, key, AMQP::Envelope{data.data(), data.size()});

    auto &r = channel.commitTransaction();

    r.onSuccess(
        [&]()
        {
            TRACE(TraceLevel::Info, "published");
            service.stop();
        });

    r.onError(
        [&](const char *msg)
        {
            TRACE(TraceLevel::Error, msg);
            service.stop();
        });
}

int start(
    const std::string &address,
    const std::string &exchange,
    const std::string &key,
    const std::string &input)
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
            send(service, channel, exchange, key, input);
        });

    return service.run();
}

int main(int argc, char *argv[])
{
    std::string address;
    std::string exchange;
    std::string key;
    std::string input;

    for(int c; -1 != (c = ::getopt(argc, argv, "a:i:k:x:"));)
    {
        switch(c)
        {
            case 'a':
                address = optarg ? optarg : "";
                break;
            case 'i':
                input =  optarg ? optarg : "";
                break;
            case 'x':
                exchange =  optarg ? optarg : "";
                break;
            case 'k':
                key =  optarg ? optarg : "";
                break;
            case ':':
            case '?':
            default:
                return help(argc, argv);
        }
    }

    if(
        address.empty()
        || exchange.empty()
        || key.empty()
        || input.empty()) return help(argc, argv, "missing required arguments");

    try
    {
        return start(address, exchange, key, input);
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
