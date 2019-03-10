#ifndef STREAMLIKE_FILE_HPP
#define STREAMLIKE_FILE_HPP

#include <string>

#include "../streamlike.hpp"

namespace streamlike {

class StreamlikeFile : public Streamlike {
    public:
        StreamlikeFile(const char *path, const char *mode);
        StreamlikeFile(const std::string& path, const std::string& mode);
        StreamlikeFile(FILE* fp);
        StreamlikeFile(StreamlikeFile&& old) = default;
        StreamlikeFile& operator=(StreamlikeFile&&) = default;
        ~StreamlikeFile();
};

} // namespace streamlike

#endif /* STREAMLIKE_FILE_HPP */
