//
// Created by alex on 10/9/17.
//

#include "WebSocketClient.h"

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

#include "spdlog/spdlog.h"

static auto LOGGER = spdlog::stdout_color_st( "WebSocketClient" );

WebSocketClient::WebSocketClient( char *url ) : url( url ), wsppClient( new WebSocketppClient()) {
}

WebSocketClient::~WebSocketClient() {
    delete wsppClient;
}

static void *wsRoutine( void *arg ) {
    WebSocketClient *client = (WebSocketClient *) arg;

    WebSocketppClient *wsppClient = client->getWsppClient();
    wsppClient->clear_access_channels( websocketpp::log::alevel::frame_header );
    wsppClient->clear_access_channels( websocketpp::log::alevel::frame_payload );
    wsppClient->init_asio();

    websocketpp::lib::error_code ec;
    WebSocketppClient::connection_ptr con = wsppClient->get_connection( std::string( client->getUrl()), ec );
    wsppClient->connect( con );
    client->setWsppConnHdl( con->get_handle());
    wsppClient->run();
}

void WebSocketClient::connect() {
    pthread_create( &wsThread, NULL, wsRoutine, (void *) this );
}

void WebSocketClient::sendBytes( unsigned char *buf, unsigned size ) {
    wsppClient->send( wsppConnHdl, buf, size, websocketpp::frame::opcode::BINARY );
}

void WebSocketClient::sendText( char *text ) {
    wsppClient->send( wsppConnHdl, text, strlen( text ), websocketpp::frame::opcode::TEXT );
}

char *WebSocketClient::getUrl() const {
    return url;
}

pthread_t WebSocketClient::getWsThread() const {
    return wsThread;
}

WebSocketppClient *WebSocketClient::getWsppClient() {
    return wsppClient;
};

void WebSocketClient::setWsppConnHdl( WebSocketppConnHdl wsppConnHdl ) {
    this->wsppConnHdl = wsppConnHdl;
}