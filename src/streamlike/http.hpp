#ifndef STREAMLIKE_HTTP_HPP
#define STREAMLIKE_HTTP_HPP

#include <string>

#include "../streamlike.hpp"

namespace streamlike {

class StreamlikeHttp : public Streamlike {
    public:
        StreamlikeHttp(const char *url);
        StreamlikeHttp(const std::string& url);
        StreamlikeHttp(StreamlikeHttp&&) = default;
        StreamlikeHttp& operator=(StreamlikeHttp&&) = default;
        ~StreamlikeHttp();
};

} // namespace streamlike

#endif /* STREAMLIKE_HTTP_HPP */
