
#include "http.hpp"
#include "db.hpp"

namespace rfaas::resource_manager {

  HTTPHandler::HTTPHandler(ExecutorDB & db):
    _database(db)
  {}

  void HTTPHandler::onRequest(
    const Pistache::Http::Request& req, Pistache::Http::ResponseWriter response
  ) {

    if(req.resource() == "/add") {

      // Return 400 if the request is malformed or incorret
      // If good, then return 200
      if(_database.add(req.body()) == ExecutorDB::ResultCode::OK) {
        response.send(Pistache::Http::Code::Ok);
      } else {
        response.send(Pistache::Http::Code::Bad_Request);
      }

    } else if(req.resource() == "/remove") {

      // Return 400 if the request is malformed or incorret
      // If good, then return 200
      if(_database.remove(req.body()) == ExecutorDB::ResultCode::OK) {
        response.send(Pistache::Http::Code::Ok);
      } else {
        response.send(Pistache::Http::Code::Bad_Request);
      }

    } else {
      response.send(Pistache::Http::Code::Not_Found);
    }
  }

  HTTPServer::HTTPServer(ExecutorDB & db, Settings & settings):
    _server(Pistache::Address{settings.http_network_address, settings.http_network_port})
  {
    auto opts = Pistache::Http::Endpoint::options().threads(1);
    _server.init(opts);
    _server.setHandler(Pistache::Http::make_handler<HTTPHandler>(db));
  }

  void HTTPServer::start()
  {
    _server.serveThreaded();
  }

  void HTTPServer::stop()
  {
    _server.shutdown();
  }

}
