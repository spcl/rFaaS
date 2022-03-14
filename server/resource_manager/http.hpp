

#include <pistache/http.h>
#include <pistache/endpoint.h>

#include "settings.hpp"

namespace rfaas::resource_manager {

  struct ExecutorDB;

  struct HTTPHandler : public Pistache::Http::Handler
  {
    ExecutorDB & _database;

    HTTPHandler(ExecutorDB &);

    HTTP_PROTOTYPE(HTTPHandler)
    void onRequest(const Pistache::Http::Request& req, Pistache::Http::ResponseWriter response) override;
  };

  struct HTTPServer
  {
    Pistache::Http::Endpoint _server;

    HTTPServer(ExecutorDB &, Settings &);

    void start();
    void stop();
  };

}
