extern "C" {
#include "buffer.h"
}
#include "buffer.hpp"
#include <stdexcept>

namespace streamlike {

StreamlikeBuffer::StreamlikeBuffer(Streamlike&& innerStream)
        : Streamlike(sl_buffer_create(getSelf(innerStream)), sl_buffer_destroy),
          mInnerStream(std::move(innerStream)) {
    if (!self) {
        throw std::runtime_error("Couldn't create buffer stream");
    }
}

StreamlikeBuffer::StreamlikeBuffer(Streamlike&& innerStream, size_t bufferSize,
                                   size_t stepSize)
        : Streamlike(sl_buffer_create2(getSelf(innerStream), bufferSize, stepSize),
                     sl_buffer_destroy),
          mInnerStream(std::move(innerStream)) {
    if (!self) {
        throw std::runtime_error("Couldn't create buffer stream");
    }
}

} // namespace streamlike
