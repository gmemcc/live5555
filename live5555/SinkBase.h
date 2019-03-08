#ifndef LIVE5555_SINKBASE_H
#define LIVE5555_SINKBASE_H

#include "common.h"

class SinkBase : public MediaSink {

private:
    static void afterGettingFrame( void *clientData, unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime,
        unsigned /*durationInMicroseconds*/ );

protected:
    unsigned recvBufSize;

    unsigned char *recvBuf;

    SinkBase( UsageEnvironment &env, unsigned recvBufSize );

    // sink->startPlaying会调用continuePlaying，实现播放逻辑
    virtual Boolean continuePlaying();

    virtual ~SinkBase();

public:
    virtual void afterGettingFrame( unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime );
};

#endif //LIVE5555_SINKBASE_H
