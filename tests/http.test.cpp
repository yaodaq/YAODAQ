#include <iostream>
#include <ixwebsocket/IXHttpClient.h>

int main()
{
  ix::HttpClient http;

  auto args = http.createRequest();

  args->connectTimeout  = 10;
  args->transferTimeout = 10;
  args->followRedirects = true;
  args->verbose         = true;

  std::string url = "http://192.168.50.119:8081/api/login/admin/password";

  auto response = http.get( url, args );

  if( !response )
  {
    std::cerr << "null response\n";
    return 1;
  }

  std::cout << "status      : " << response->statusCode << '\n';
  std::cout << "description : " << response->description << '\n';
  std::cout << "errorCode   : " << (int)response->errorCode << '\n';
  std::cout << "headers:\n";

  for( auto& it: response->headers ) { std::cout << it.first << " = " << it.second << '\n'; }

  std::cout << "body size   : " << response->body.size() << '\n';
  std::cout << "body        : [" << response->body << "]\n";
  std::cout << response->errorMsg << std::endl;
}
