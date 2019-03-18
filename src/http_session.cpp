#include "http_session.h"
#include "task_handlers/task_handlers.h"
#include "task_handlers/base_handler.h"
#include "json_rpc.h"
#include "settings/settings.h"
#include "common/string_utils.h"
#include "common/log.h"

http_session::http_session(tcp::socket&& socket) :
    m_socket(std::move(socket))
{
}

http_session::~http_session()
{
    boost::system::error_code ec;
    m_socket.shutdown(m_socket.shutdown_both, ec);
    m_socket.close(ec);
}

void http_session::run()
{
    m_req.clear();
    m_req.body().clear();
    m_buf.consume(m_buf.size());

    auto self = shared_from_this();
    http::async_read(m_socket, m_buf, m_req, [self](beast::error_code ec, std::size_t bytes_transferred)
    {
        if (!ec && bytes_transferred > 0)
        {
            self->process_request();
            self->run();
        }
    });
}

asio::io_context& http_session::get_io_context()
{
    return m_socket.get_io_context();
}

void http_session::process_request()
{
    LOGDEBUG << "HTTP Session " << m_socket.remote_endpoint().address().to_string() << " >>> " << m_req.body();

    switch(m_req.method()) {
    case http::verb::post:
        process_post_request();
        break;
    case http::verb::get:
        process_get_request();
        break;
    default:
        send_bad_request("Incorrect http method");
        break;
    }
}

void http_session::send_bad_request(const char* error)
{
    http::response<http::string_body> response;
    response.result(http::status::bad_request);
    response.set(http::field::content_type, "text/plain");
    response.body().assign(error);
    send_response(response);
}

void http_session::send_json(const std::string& data)
{
    http::response<http::string_body> response;
    response.result(http::status::ok);
    response.set(http::field::content_type, "application/json");
    response.body().assign(data);
    send_response(response);
}

void http_session::send_response(http::response<http::string_body>& response)
{
    LOGDEBUG << "HTTP Session " << m_socket.remote_endpoint().address().to_string() << " <<< " << response.body().c_str();

    response.version(10);
    response.set(http::field::server, "metahash.service");
    response.set(http::field::content_length, response.body().size());
    response.set(http::field::keep_alive, false);
    response.keep_alive(false);
    http::write(m_socket, response);
}

void http_session::process_post_request()
{
    if (m_req.target().size() != 1 || m_req.target()[0] != '/')
    {
        send_bad_request("Incorrect path");
        return;
    }

    std::string json;
    json_rpc_reader reader;
    json_rpc_writer writer;
    if (reader.parse(m_req.body().c_str())) {
        auto it = post_handlers.find(std::make_pair(reader.get_method(), settings::system::useLocalDatabase));
        if (it == post_handlers.end()) {
            LOGERR << "Incorrect service method: " << reader.get_method();

            writer.set_id(reader.get_id());
            writer.set_error(-32601, string_utils::str_concat("Method '", reader.get_method(), "' not found"));
            json = writer.stringify();
        } else {
            auto res = it->second(shared_from_this(), m_req.body());
            // async operation
            if (!res)
                return;
            json.append(res.message);
        }
    } else {
        LOGERR << "Incorrect json " << reader.get_parse_error().Code() << ": " << m_req.body();
        writer.set_error(reader.get_parse_error().Code(), "Parse error");
        json = writer.stringify();
    }

    send_json(json);
}

void http_session::process_get_request()
{
    if (m_req.target().size() == 1) {
        send_bad_request("Incorrect path");
        return;
    }

    std::string_view params;
    std::string_view method(m_req.target().data(), m_req.target().size());
    size_t tmp = method.find_first_of('?');
    if (tmp != std::string_view::npos) {
        params = method.substr(tmp + 1, method.size() - tmp);
        method.remove_suffix(method.size() - tmp);
    }

    method.remove_prefix(1);

    std::string json;
    json_rpc_writer writer;
    auto it = get_handlers.find(method);
    if (it == get_handlers.end()) {
        LOGWARN << "Incorrect service method " << method;
        writer.set_id(1);
        writer.set_error(-32602, string_utils::str_concat("Method '", method, "' not found"));
        json = writer.stringify();
    } else {
        writer.set_id(1);
        if (!params.empty()) {
            json_utils::to_json(params, *writer.get_params(), writer.get_allocator());
        }
        auto res = it->second(shared_from_this(), writer.stringify());
        // async operation
        if (!res)
            return;
        json.append(res.message);
    }
    send_json(json);
}
