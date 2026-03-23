#include <filesystem>
#include <fstream>
#include <iostream>
#include <ixwebsocket/IXHttpServer.h>
#include <sstream>

using namespace ix;

class SimpleHttpServer
{
public:
  SimpleHttpServer( const std::string& root, const std::string& host, int port ) : _httpServer( port, host )
  {
    //if(!std::filesystem::is_directory(root)) std::cout<<"root must be a directory"<<std::endl;
    //m_root = std::filesystem::absolute(root);
    //std::cout<<m_root.string()<<std::endl;

    _httpServer.setOnConnectionCallback(
      [this]( HttpRequestPtr request, std::shared_ptr<ConnectionState> connectionState ) -> HttpResponsePtr
      {
        std::cout << request->uri << " " << request->method << "  " << request->version << "  " << request->body << std::endl;

        // Handle GET requests based on URI
        if( request->method == "GET" )
        {
          std::string url = request->uri;
          if( url.size() >= 1 ) url.erase( 0, 1 );
          if( request->uri == "" || request->uri == "index.html" )
          {
            std::string url = m_root + "yaodaq.html";
            return serveFile( url );
          }
          else
          {
            std::string url = m_root + url;
            return serveFile( url );
          }
        }
        //return serveFile("index.html");
      } );
  }
  // Serve a 404 Not Found page if the requested file is not found
  HttpResponsePtr serveNotFound()
  {
    WebSocketHttpHeaders headers;
    headers["Content-Type"] = "text/html";

    std::string notFoundHtml = "<h1>404 Not Found</h1>";
    return std::make_shared<HttpResponse>( 404, "Not Found", HttpErrorCode::Ok, headers, notFoundHtml );
  }
  void start()
  {
    _httpServer.listen();
    _httpServer.start();
  }

  void stop() { _httpServer.stop(); }

private:
  std::string     m_root;
  /*HttpResponsePtr handleRequest(HttpRequestPtr request,std::shared_ptr<ConnectionState> connectionState)
    {
        // Handle GET requests
        if (request->getMethod() == "GET")
        {
            if (request->getUri() == "/") {
                serveIndexHtml(res);
            } else {
                serveNotFound(res);
            }
        } else {
            return std::make_shared<HttpResponse>(200, "OK",
                HttpErrorCode::Ok,
                WebSocketHttpHeaders(), "");
        }
    }*/
  // Generic function to serve files based on filename
  HttpResponsePtr serveFile( const std::string& filename )
  {
    //WebSocketHttpHeaders headers;

    // Determine content type based on file extension
    //std::string contentType = "text/html";
    //if (filename.find(".css")) {
    //    contentType = "text/css";
    // }

    // headers["Content-Type"] = contentType;

    // Read the file content
    std::ifstream file( filename );
    if( !file.is_open() ) { return std::make_shared<HttpResponse>( 200, "OK", HttpErrorCode::Ok, WebSocketHttpHeaders{}, "" ); }

    std::stringstream fileContent;
    fileContent << file.rdbuf();  // Read the whole file into the string stream
    file.close();

    return std::make_shared<HttpResponse>( 200, "OK", HttpErrorCode::Ok, WebSocketHttpHeaders{}, fileContent.str() );
  }
  ix::HttpServer _httpServer;
};

int main()
{
  // Create the HTTP server and listen on port 8080
  SimpleHttpServer httpServer( "/home/work/YAODAQ-1/build/web/", "127.0.0.1", 9999 );
  httpServer.start();

  // Keep the server running until the user presses Enter
  std::string input;
  std::getline( std::cin, input );

  httpServer.stop();
  return 0;
}
