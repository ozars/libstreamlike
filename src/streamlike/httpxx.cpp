extern "C" {
#include "http.h"
}
#include "http.hpp"
#include <stdexcept>

namespace streamlike {

StreamlikeHttp::StreamlikeHttp(const char *url)
        : Streamlike(sl_http_create(url)) {
    if (!self) {
        throw std::runtime_error("Couldn't create http stream");
    }
}

StreamlikeHttp::StreamlikeHttp(const std::string& url)
    : StreamlikeHttp(url.c_str()) {}

StreamlikeHttp::~StreamlikeHttp() {
    if (self) {
        sl_http_destroy(self);
    }
}

} // namespace streamlike
