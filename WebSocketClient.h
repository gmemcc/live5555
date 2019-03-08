//
// Created by alex on 10/9/17.
//

#ifndef LIVE5555_WEBSOCKETCLIENT_H
#define LIVE5555_WEBSOCKETCLIENT_H

#include <pthread.h>

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

typedef websocketpp::client<websocketpp::config::asio_client> WebSocketppClient;
typedef websocketpp::connection_hdl WebSocketppConnHdl;

class WebSocketClient {
private:
    char *url;
    pthread_t wsThread;
    WebSocketppClient *wsppClient;
    WebSocketppConnHdl wsppConnHdl;
public:
    WebSocketClient( char *url );

    char *getUrl() const;

    virtual void connect();

    virtual void sendBytes( unsigned char *buf, unsigned size );

    virtual void sendText( char *text );

    virtual ~WebSocketClient();

    pthread_t getWsThread() const;

    WebSocketppClient *getWsppClient();

    void setWsppConnHdl( WebSocketppConnHdl wsppConnHdl );
};


#endif //LIVE5555_WEBSOCKETCLIENT_H
