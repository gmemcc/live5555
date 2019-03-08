#include "live5555/RTSPClientBase.h"
#include "spdlog/spdlog.h"

static auto LOGGER = spdlog::stdout_color_st( "RTSPClientBase" );

static const char *getResultString( char *resultString ) {
    return resultString ? resultString : "N/A";
}

// 父构造函数的第三个参数是调试信息冗余级别
RTSPClientBase::RTSPClientBase( UsageEnvironment &env, const char *rtspURL ) :
    RTSPClient( env, rtspURL, 0, NULL, 0, -1 ) {
}

void RTSPClientBase::start() {
    LOGGER->trace( "Starting RTSP client..." );
    this->rtspURL = rtspURL;
    LOGGER->trace( "Send RTSP command: DESCRIBE" );
    sendDescribeCommand( onDescribeResponse );
    LOGGER->trace( "Startup live555 eventloop" );
    envir().taskScheduler().doEventLoop( &eventLoopWatchVariable );
}


void RTSPClientBase::onDescribeResponse( RTSPClient *client, int resultCode, char *resultString ) {
    LOGGER->trace( "DESCRIBE response received, resultCode: {}", resultCode );
    RTSPClientBase *clientBase = (RTSPClientBase *) client;
    bool ok = false;
    if ( resultCode == 0 ) {
        clientBase->onDescribeResponse( resultCode, resultString );
    } else {
        LOGGER->trace( "Stopping due to DESCRIBE failure" );
        clientBase->stop();
    };
    delete[] resultString;
}

void RTSPClientBase::onDescribeResponse( int resultCode, const char *sdp ) {
    LOGGER->debug( "SDP received: \n{}", sdp );
    UsageEnvironment &env = envir();
    LOGGER->trace( "Create new media session according to SDP" );
    session = MediaSession::createNew( env, sdp );
    if ( session && session->hasSubsessions()) {
        MediaSubsessionIterator *it = new MediaSubsessionIterator( *session );
        // 遍历子会话，SDP中的每一个媒体行（m=***）对应一个子会话
        while ( MediaSubsession *subsess = it->next()) {
            const char *mediumName = subsess->mediumName();
            // 初始化子会话，导致相应的RTPSource被创建
            LOGGER->trace( "Initialize sub session {}", mediumName );
            if ( !acceptSubSession( mediumName, subsess->codecName())) {
                continue;
            }
            acceptedSubSessionCount++;
            bool ok = subsess->initiate();
            if ( !ok ) {
                LOGGER->error( "Failed to initialize sub session: {}", mediumName );
                stop();
                break;
            }
            const Boolean muxed = subsess->rtcpIsMuxed();
            const char *codec = subsess->codecName();
            const int port = subsess->clientPortNum();
            LOGGER->debug( "Initialized sub session... \nRTCP Muxed: {}\nPort: {}\nMedium : {}\nCodec: {}", muxed, port, mediumName, codec );

            LOGGER->trace( "Send RTSP command: SETUP for subsession {}", mediumName );
            sendSetupCommand( *subsess, onSetupResponse, False, False );
        }
    } else {
        stop();
    }
}

void RTSPClientBase::onSetupResponse( RTSPClient *client, int resultCode, char *resultString ) {
    LOGGER->trace( "SETUP response received, resultCode: {}, resultString: {}", resultCode, getResultString( resultString ));
    RTSPClientBase *clientBase = (RTSPClientBase *) client;
    if ( resultCode == 0 ) {
        clientBase->preparedSubSessionCount++;
        clientBase->onSetupResponse( resultCode, resultString );
    } else {
        LOGGER->trace( "Stopping due to SETUP failure" );
        clientBase->stop();
    }
    delete[] resultString;
}

void RTSPClientBase::onSetupResponse( int resultCode, const char *resultString ) {
    if ( preparedSubSessionCount == acceptedSubSessionCount ) {
        MediaSubsessionIterator *it = new MediaSubsessionIterator( *session );
        while ( MediaSubsession *subsess = it->next()) {
            const char *mediumName = subsess->mediumName();
            const char *codec = subsess->codecName();
            if ( acceptSubSession( mediumName, codec )) {
                MediaSink *sink = createSink( mediumName, codec, subsess );
                // 让Sink回调能够感知Client对象
                subsess->miscPtr = this;
                // 导致Sink的continuePlaying被调用，准备接受数据推送
                sink->startPlaying( *subsess->readSource(), NULL, subsess );
                // 此时数据推送不会立即开始，直到调用STSP命令PLAY
                RTCPInstance *rtcp = subsess->rtcpInstance();
                if ( rtcp ) {
                    // 正确处理针对此子会话的RTCP命令
                    rtcp->setByeHandler( onSubSessionClose, subsess );
                }
                LOGGER->trace( "Send RTSP command: PLAY" );
                // PLAY命令可以针对整个会话，也可以针对每个子会话
                sendPlayCommand( *session, onPlayResponse );
            }
        }
    }
}

void RTSPClientBase::onPlayResponse( RTSPClient *client, int resultCode, char *resultString ) {
    LOGGER->trace( "PLAY response received, resultCode: {}, resultString: {}", resultCode, getResultString( resultString ));
    RTSPClientBase *clientBase = (RTSPClientBase *) client;
    if ( resultCode == 0 ) {
        clientBase->onPlayResponse( resultCode, resultString );
    } else {
        LOGGER->trace( "Stopping due to PLAY failure" );
        clientBase->stop();
    }
    delete[] resultString;
}

void RTSPClientBase::onPlayResponse( int resultCode, char *resultString ) {
    // 此时服务器应该开始推送流过来
    // 如果播放的是定长的录像，这里应该注册回调，在时间到达后关闭客户端
    double &startTime = session->playStartTime();
    double &endTime = session->playEndTime();
    LOGGER->debug_if( startTime == endTime, "Session is infinite" );
}

void RTSPClientBase::onSubSessionClose( void *clientData ) {
    MediaSubsession *subsess = (MediaSubsession *) clientData;
    RTSPClientBase *clientBase = (RTSPClientBase *) subsess->miscPtr;
    clientBase->onSubSessionClose( subsess );
}

void RTSPClientBase::onSubSessionClose( MediaSubsession *subsess ) {
    LOGGER->debug( "Stopping subsession..." );
    // 首先关闭子会话的SINK
    Medium::close( subsess->sink );
    subsess->sink = NULL;

    // 检查是否所有兄弟子会话均已经结束
    MediaSession &session = subsess->parentSession();
    MediaSubsessionIterator iter( session );
    while (( subsess = iter.next()) != NULL ) {
        // 存在未结束的子会话，不能关闭当前客户端
        if ( subsess->sink != NULL ) return;
    }
    // 关闭客户端
    LOGGER->debug( "All subsession closed" );
    stop();
}

void RTSPClientBase::stop() {
    LOGGER->debug( "Stopping RTSP client..." );
    // 修改事件循环监控变量
    eventLoopWatchVariable = 0;
    UsageEnvironment &env = envir();
    if ( session != NULL ) {
        Boolean someSubsessionsWereActive = False;
        MediaSubsessionIterator iter( *session );
        MediaSubsession *subsession;
        // 检查是否存在需要处理的子会话
        while (( subsession = iter.next()) != NULL ) {
            if ( subsession->sink != NULL ) {
                // 强制关闭子会话的SINK
                Medium::close( subsession->sink );
                subsession->sink = NULL;
                if ( subsession->rtcpInstance() != NULL ) {
                    // 服务器可能在处理TEARDOWN时发来RTCP包BYE
                    subsession->rtcpInstance()->setByeHandler( NULL, NULL );
                }
                someSubsessionsWereActive = True;
            }
        }

        if ( someSubsessionsWereActive ) {
            // 向服务器发送TEARDOWN命令，让服务器关闭输入流
            sendTeardownCommand( *session, NULL );
        }
    }
    // 关闭客户端
    Medium::close( this );
}
