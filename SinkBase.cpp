#include "live5555/SinkBase.h"
#include "spdlog/spdlog.h"

static auto LOGGER = spdlog::stdout_color_st( "SinkBase" );

SinkBase::SinkBase( UsageEnvironment &env, unsigned recvBufSize ) : MediaSink( env ) {
    this->recvBufSize = recvBufSize;
    this->recvBuf = new unsigned char[recvBufSize];
}

SinkBase::~SinkBase() {
    delete[] this->recvBuf;
}

// 缺省实现：保存已分帧源的下一帧到缓冲区中，然后执行回调
Boolean SinkBase::continuePlaying() {
    if ( fSource == NULL ) return False;
    fSource->getNextFrame( recvBuf, recvBufSize, afterGettingFrame, this, onSourceClosure, this );
    return True;
};

// 由于getNextFrame需要的是一个函数指针，因此这里用静态函数。此函数简单的转调对应的成员函数
void SinkBase::afterGettingFrame( void *clientData, unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime,
    unsigned /*durationInMicroseconds*/ ) {
    SinkBase *sink = (SinkBase *) clientData;
    sink->afterGettingFrame( frameSize, numTruncatedBytes, presentationTime );
}

// 缺省实现：递归获取下一帧
void SinkBase::afterGettingFrame( unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime ) {
    LOGGER->trace( "Frame of {} bytes received",frameSize );
    fSource->getNextFrame( recvBuf, recvBufSize, afterGettingFrame, this, onSourceClosure, this );
}