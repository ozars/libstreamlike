extern "C" {
#include "buffer.h"
}
#include "buffer.hpp"

#include <stdexcept>

namespace streamlike {

StreamlikeBufferImpl::self_type StreamlikeBufferImpl::createSelf(
        self_type innerSelf) {
    auto self = sl_buffer_create(innerSelf);
    if (!self) {
        throw std::runtime_error("Couldn't create buffer stream");
    }
    return self;
}

StreamlikeBufferImpl::self_type StreamlikeBufferImpl::createSelf(
        self_type innerSelf, size_t bufferSize, size_t stepSize) {
    auto self = sl_buffer_create2(innerSelf, bufferSize, stepSize);
    if (!self) {
        throw std::runtime_error("Couldn't create buffer stream");
    }
    return self;
}

void StreamlikeBufferImpl::destroySelf(self_type self) {
    if (self) {
        sl_buffer_destroy(self);
    }
}

void StreamlikeBufferImpl::startReadingThread(self_type self) {
    if (sl_buffer_threaded_fill_buffer(self) != 0) {
        throw std::runtime_error("Error while starting buffer thread");
    }
}

} // namespace streamlike
