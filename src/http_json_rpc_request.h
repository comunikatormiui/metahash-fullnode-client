#ifndef __HTTP_JSON_RPC_REQUEST_H__
#define __HTTP_JSON_RPC_REQUEST_H__

#include <string.h>

#define BOOST_ERROR_CODE_HEADER_ONLY
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception_ptr.hpp>

#include <mutex>

#include "json_rpc.h"
#include "task_handlers/time_duration.h"
#include "task_handlers/utils.h"
#include "connection_pool.h"
#include "log.h"

namespace   asio    = boost::asio;
namespace   ssl     = boost::asio::ssl;
namespace   ip      = boost::asio::ip;
using       tcp     = boost::asio::ip::tcp;
namespace   beast   = boost::beast;
namespace   http    = boost::beast::http;

using http_json_rpc_execute_callback = std::function<void()>;
using json_response_type = http::response_parser<http::string_body>;
using json_request_type = http::request<http::string_body>;

#define JRPC_BGN try

#define JRPC_END(ret) \
    catch (boost::exception& ex) { \
        LOGERR << __PRETTY_FUNCTION__ << "Json-rpc boost exception: " << boost::diagnostic_information(ex) << " at line " << __LINE__; \
        std::lock_guard<std::mutex> lock(m_locker);\
        m_result.set_error(-32603, "Json-rpc boost exception. Check log for extra information.");\
        close();\
        m_canceled = true;\
        return ret;\
    } catch (std::exception& ex) { \
        LOGERR << __PRETTY_FUNCTION__ << "Json-rpc std exception: " << ex.what() << " at line " << __LINE__; \
        std::lock_guard<std::mutex> lock(m_locker);\
        m_result.set_error(-32603, "Json-rpc std exception. Check log for extra information.");\
        close();\
        m_canceled = true;\
        return ret;\
    } catch (...) { \
        LOGERR << __PRETTY_FUNCTION__ << "Json-rpc unhandled exception at line " << __LINE__; \
        std::lock_guard<std::mutex> lock(m_locker);\
        m_result.set_error(-32603, "Json-rpc unhandled exception. Check log for extra information.");\
        close();\
        m_canceled = true;\
        return ret;\
    }

class http_json_rpc_request: public std::enable_shared_from_this<http_json_rpc_request>
{
    enum class timeout {
        request,
        connection
    };

public:
    http_json_rpc_request(std::string&& host, asio::io_context* ctx = nullptr,
                          int timeout = -1, int conn_timeout = -1, int attempts_count = -1);

    virtual ~http_json_rpc_request();

    void set_path(const char* path);

    template <typename T>
    void set_body(T&& body) {
        JRPC_BGN
        {
            m_req.body().assign(std::forward<T>(body));
            m_req.set(http::field::content_length, m_req.body().size());

            json_rpc_reader reader;
            if (reader.parse(m_req.body().data(), m_req.body().size())) {
                m_result.set_id(reader.get_id());
            }
        }
        JRPC_END()
    }

    template <typename T>
    void set_host(T&& host)
    {
        JRPC_BGN
        {
            m_host = std::forward<T>(host);
            std::string addr, path, port;
            utils::parse_address(m_host, addr, port, path, m_use_ssl);
            m_req.set(http::field::host, addr);
        }
        JRPC_END()
    }

    void reset_attempts();

    void execute(http_json_rpc_execute_callback callback = nullptr);

    const std::string_view get_result();
    const json_response_type* get_response();

protected:
    void on_resolve(const boost::system::error_code& e, tcp::resolver::results_type eps);
    void on_connect(const boost::system::error_code& ec, const tcp::endpoint& ep);
    void on_handshake(const boost::system::error_code& e);
    void on_write(const boost::system::error_code& e);
    void on_read(const boost::system::error_code& e, size_t sz);
    void on_timeout(timeout type);
    bool error_handler(const boost::system::error_code& e, const char* from);
    void perform_callback();
    bool verify_certificate(bool preverified, ssl::verify_context& ctx);
    void close(bool force = false);

    inline bool is_ssl() const { return m_use_ssl; }

protected:
    std::unique_ptr<asio::io_context>   m_io_ctx;
    tcp::socket                         m_socket;
    tcp::resolver                       m_resolver;
    utils::Timer                        m_timer;
    utils::time_duration                m_duration;
    json_request_type                   m_req;
    std::unique_ptr<json_response_type> m_response;
    boost::beast::flat_buffer           m_buf { 8192 };
    json_rpc_writer                     m_result;
    http_json_rpc_execute_callback      m_callback;
    std::string                         m_host;
    ssl::context                        m_ssl_ctx;      //
    ssl::stream<tcp::socket>            m_ssl_socket;   //
    std::mutex                          m_locker;
    pool_object                         m_pool_obj;
    unsigned int                        m_attempt;
    unsigned int                        m_timeout;
    unsigned int                        m_conn_timeout;
    unsigned int                        m_attempts_count;
    bool                                m_use_ssl;      //
    bool                                m_canceled;
    bool                                m_rerun;
};

#endif // __HTTP_JSON_RPC_REQUEST_H__
