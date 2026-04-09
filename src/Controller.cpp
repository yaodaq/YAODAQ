#include "yaodaq/Controller.hpp"
void yaodaq::Controller::Send( const std::string_view request ) { sendUtf8Text( std::string( request ) ); }
void yaodaq::Controller::onResponse( const std::string& response ) { this->Receive( response ); }
