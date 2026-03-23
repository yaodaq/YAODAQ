#include "yaodaq/WebsocketCloseConstants.hpp"

const std::string yaodaq::WebSocketCloseConstant::NoYaodaqIdMessage{ "The 'Yaodaq-Id' header is required but was not found." };
const std::string yaodaq::WebSocketCloseConstant::ClientWithThisNameAlreadyConnectedMessage{ "A client with this name is already connected." };
