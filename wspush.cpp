#include <iostream>
#include "live5555/client.h"

#include "spdlog/spdlog.h"

#include "WebSocketClient.h"

static auto LOGGER = spdlog::stdout_color_st( "wspush" );

class VideoSink : public SinkBase {
private:
#ifdef _SAVE_H264_SEQ
    FILE *os = fopen( "./rtsp.h264", "w" );
#endif
    WebSocketClient *wsClient;
    bool firstFrameWritten;
    const char *sPropParameterSetsStr;
    unsigned char const start_code[4] = { 0x00, 0x00, 0x00, 0x01 };
public:
    VideoSink( UsageEnvironment &env, unsigned int recvBufSize, WebSocketClient *wsClient ) : SinkBase( env, recvBufSize ), wsClient( wsClient ) {
        // 缓冲区前面留出起始码4字节
        recvBuf += sizeof( start_code );
    }

    virtual ~VideoSink() {
    }

    virtual void onMediaSubsessionOpened( MediaSubsession *subSession ) {
        sPropParameterSetsStr = subSession->fmtp_spropparametersets();
    }

    void afterGettingFrame( unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime ) override {
        size_t scLen = sizeof( start_code );
        if ( !firstFrameWritten ) {
            // 填写起始码
            memcpy( recvBuf - scLen, start_code, scLen );
            // 防止RTSP源不送SPS/PPS
            unsigned numSPropRecords;
            SPropRecord *sPropRecords = parseSPropParameterSets( sPropParameterSetsStr, numSPropRecords );
            for ( unsigned i = 0; i < numSPropRecords; ++i ) {
                unsigned int propLen = sPropRecords[ i ].sPropLength;
                size_t bufLen = propLen + scLen;
                unsigned char buf[bufLen];
                memcpy( buf, start_code, scLen );
                memcpy( buf + scLen, sPropRecords[ i ].sPropBytes, propLen );
                wsClient->sendBytes( buf, bufLen );
#ifdef _SAVE_H264_SEQ
                fwrite( buf, sizeof( unsigned char ), bufLen, os );
#endif
            }
            firstFrameWritten = true;
        }
#ifdef _SAVE_H264_SEQ
        fwrite( recvBuf - scLen, sizeof( unsigned char ), frameSize + scLen, os );
#endif
        unsigned naluHead = recvBuf[ 0 ];
        unsigned nri = naluHead >> 5;
        unsigned f = nri >> 2;
        unsigned type = naluHead & 0b00011111;
        wsClient->sendBytes( recvBuf - scLen, frameSize + scLen );
        LOGGER->trace( "NALU info: nri {} type {}", nri, type );
        SinkBase::afterGettingFrame( frameSize, numTruncatedBytes, presentationTime );
    }
};

class H264RTSPClient : public RTSPClientBase {
private:
    VideoSink *videoSink;
public:
    H264RTSPClient( UsageEnvironment &env, const char *rtspURL, VideoSink *videoSink ) :
        RTSPClientBase( env, rtspURL ), videoSink( videoSink ) {}

protected:
    // 测试用的摄像头（RTSP源）仅仅有一个子会话，因此这里简化了实现：
    bool acceptSubSession( const char *mediumName, const char *codec ) override {
        return true;
    }

    MediaSink *createSink( const char *mediumName, const char *codec, MediaSubsession *subSession ) override {
        videoSink->onMediaSubsessionOpened( subSession );
        return videoSink;
    }
};

int main() {
    spdlog::set_pattern( "%Y-%m-%d %H:%M:%S.%e [%l] [%n] %v" );
    spdlog::set_level( spdlog::level::trace );

    WebSocketClient *wsClient;
    wsClient = new WebSocketClient( "ws://192.168.0.89:9090/h264src" );
    wsClient->connect();
    sleep( 3 ); // 等待WebSocket连接建立
    wsClient->sendText( "ch1" );
    TaskScheduler *scheduler = BasicTaskScheduler::createNew();
    BasicUsageEnvironment *env = BasicUsageEnvironment::createNew( *scheduler );
    VideoSink *sink = new VideoSink( *env, 1024 * 1024, wsClient );
    H264RTSPClient *client = new H264RTSPClient( *env, "rtsp://admin:kingsmart123@192.168.0.196:554/ch1/sub/av_stream", sink );
    client->start();
    return 0;
}