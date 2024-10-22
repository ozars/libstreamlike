extern "C" {
#include "file.h"
}
#include "file.hpp"
#include <stdexcept>

namespace streamlike {

StreamlikeFile::StreamlikeFile(const char *path, const char *mode)
        : Streamlike(sl_fopen(path, mode)) {
    if (!self) {
        throw std::runtime_error("Couldn't create file stream");
    }
}

StreamlikeFile::StreamlikeFile(const std::string& path,
                               const std::string& mode)
    : StreamlikeFile(path.c_str(), mode.c_str()) {}

StreamlikeFile::StreamlikeFile(FILE* fp)
        : Streamlike(sl_fopen2(fp)) {
    if (!self) {
        throw std::runtime_error("Couldn't create file stream");
    }
}

StreamlikeFile::~StreamlikeFile() {
    if (self) {
        sl_fclose(self);
    }
}

} // namespace streamlike
