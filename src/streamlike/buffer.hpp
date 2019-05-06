#ifndef STREAMLIKE_BUFFER_HPP
#define STREAMLIKE_BUFFER_HPP
#include "../streamlike.hpp"

#include <utility>

namespace streamlike {

template<class T>
class StreamlikeBuffer : public Streamlike {
    public:
        StreamlikeBuffer(T&& innerStream);
        StreamlikeBuffer(T&& innerStream, size_t bufferSize, size_t stepSize);
        StreamlikeBuffer(StreamlikeBuffer&&) = default;
        StreamlikeBuffer& operator=(StreamlikeBuffer&&) = default;
        void startReadingThread();
        ~StreamlikeBuffer();
        size_t read(void *buffer, size_t size);

    private:
        T mInnerStream;
        bool mThreadStarted;
};

class StreamlikeBufferImpl {
    private:
        using self_type = Streamlike::self_type;

        StreamlikeBufferImpl() = delete;
        static self_type createSelf(self_type innerSelf);
        static self_type createSelf(self_type innerSelf, size_t bufferSize,
                                    size_t stepSize);
        static void destroySelf(self_type self);
        static void startReadingThread(self_type self);

        template<class T>
        friend class StreamlikeBuffer;
};

template<class T>
StreamlikeBuffer<T>::StreamlikeBuffer(T&& innerStream)
        : Streamlike(StreamlikeBufferImpl::createSelf(getSelf(innerStream))),
          mInnerStream(std::forward<T>(innerStream)), mThreadStarted() {}

template<class T>
StreamlikeBuffer<T>::StreamlikeBuffer(T&& innerStream, size_t bufferSize,
                                      size_t stepSize)
        : Streamlike(StreamlikeBufferImpl::createSelf(
                            getSelf(innerStream), bufferSize, stepSize)),
          mInnerStream(std::forward<T>(innerStream)), mThreadStarted() {}

template<class T>
StreamlikeBuffer<T>::~StreamlikeBuffer() {
    StreamlikeBufferImpl::destroySelf(self);
}

template<class T>
StreamlikeBuffer<T> createStreamlikeBuffer(T&& innerStream) {
    return { std::forward<T>(innerStream) };
}

template<class T>
StreamlikeBuffer<T> createStreamlikeBuffer(T&& innerStream, size_t bufferSize,
                                           size_t stepSize) {
    return { std::forward<T>(innerStream, bufferSize, stepSize) };
}

template<class T>
void StreamlikeBuffer<T>::startReadingThread() {
    StreamlikeBufferImpl::startReadingThread(self);
    mThreadStarted = true;
}

template<class T>
size_t StreamlikeBuffer<T>:: read(void *buffer, size_t size) {
    if (!mThreadStarted) {
        startReadingThread();
    }
    return Streamlike::read(buffer, size);
}

} // namespace streamlike

#endif /* STREAMLIKE_BUFFER_HPP */
