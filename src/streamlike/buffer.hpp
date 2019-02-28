#ifndef STREAMLIKE_BUFFER_HPP
#define STREAMLIKE_BUFFER_HPP
#include "../streamlike.hpp"

namespace streamlike {

class StreamlikeBuffer : public Streamlike {
    public:
        StreamlikeBuffer(Streamlike&& innerStream);
        StreamlikeBuffer(Streamlike&& innerStream, size_t bufferSize,
                         size_t stepSize);
    private:
        Streamlike mInnerStream;
};

} // namespace streamlike

#endif /* STREAMLIKE_BUFFER_HPP */
