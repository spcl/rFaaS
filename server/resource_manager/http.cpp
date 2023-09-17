
//#include <rapidjson/document.h>

#include "http.hpp"
#include "db.hpp"

#include <spdlog/spdlog.h>

namespace rfaas::resource_manager {

  HTTPHandler::HTTPHandler(ExecutorDB & db):
    _database(db)
  {}

  void HTTPHandler::onRequest(
    const Pistache::Http::Request& req, Pistache::Http::ResponseWriter response
  ) {


    rapidjson::Document document;
    document.Parse(req.body().c_str());

    auto node_name = req.query().get("node");
    if (!node_name.has_value()) {
      response.send(Pistache::Http::Code::Bad_Request, "Malformed Parameters");
      return;
    }

    if(req.resource() == "/add") {

      if(
          !(document.HasMember("ip_address")  && document["ip_address"].IsString()) ||
          !(document.HasMember("port")        && document["port"].IsInt()) ||
          !(document.HasMember("cores")       && document["cores"].IsInt()) ||
          !(document.HasMember("memory")      && document["cores"].IsInt())
      ) {
        response.send(Pistache::Http::Code::Bad_Request, "Malformed Input");
        return;
      }

      std::string ip_address{document["ip_address"].GetString()};
      int port{document["port"].GetInt()};
      int cores{document["cores"].GetInt()};
      int memory{document["memory"].GetInt()};

      // Return 400 if the request is malformed or incorret
      // If good, then return 200
      if(_database.add(node_name.value(), ip_address, port, cores, memory) == ExecutorDB::ResultCode::OK) {
        response.send(Pistache::Http::Code::Ok, "Sucess");
      } else {
        response.send(Pistache::Http::Code::Internal_Server_Error, "Failure");
      }

    } else if(req.resource() == "/remove") {

      if(
          !(document.HasMember("ip_address")  && document["ip_address"].IsString()) ||
          !(document.HasMember("port")        && document["port"].IsInt()) ||
          !(document.HasMember("cores")       && document["cores"].IsInt())
      ) {
        response.send(Pistache::Http::Code::Bad_Request, "Malformed Input");
        return;
      }

      // Return 400 if the request is malformed or incorret
      // If good, then return 200
      if(_database.remove(node_name.value()) == ExecutorDB::ResultCode::OK) {
        response.send(Pistache::Http::Code::Ok);
      } else {
        response.send(Pistache::Http::Code::Bad_Request);
      }

    } else {
      response.send(Pistache::Http::Code::Not_Found, "Operation not supported");
    }
  }

  HTTPServer::HTTPServer(ExecutorDB & db, Settings & settings):
    _server(Pistache::Address{settings.http_network_address, settings.http_network_port})
  {
    spdlog::info(
      "[HTTPServer] Initialize on adddress {} and port {}",
      settings.http_network_address, settings.http_network_port
    );
    auto opts = Pistache::Http::Endpoint::options().threads(1);
    _server.init(opts);
    _server.setHandler(Pistache::Http::make_handler<HTTPHandler>(db));
  }

  void HTTPServer::start()
  {
    spdlog::info("[HTTPServer] Begin listening");
    _server.serveThreaded();
  }

  void HTTPServer::stop()
  {
    spdlog::info("Background thread stops waiting for HTTP requests");
    _server.shutdown();
  }

}
