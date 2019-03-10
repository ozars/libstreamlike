extern "C" {
#include "streamlike.h"
}
#include "streamlike.hpp"
#include <stdexcept>

namespace streamlike {

size_t Streamlike::read(void *buffer, size_t size) {
    return sl_read(self, buffer, size);
}

size_t Streamlike::input(const void **buffer, size_t size) {
    return sl_input(self, buffer, size);
}

size_t Streamlike::write(const void *buffer, size_t size) {
    return sl_write(self, buffer, size);
}

int Streamlike::flush() {
    return sl_flush(self);
}

int Streamlike::seek(off_t offset, int whence) {
    return sl_seek(self, offset, whence);
}

off_t Streamlike::tell() const {
    return sl_tell(self);
}
int Streamlike::eof() const {
    return sl_eof(self);
}

int Streamlike::error() const {
    return sl_error(self);
}

off_t Streamlike::length() const {
    return sl_length(self);
}

sl_seekable_t Streamlike::seekable() const {
    return sl_seekable(self);
}

int Streamlike::ckp_count() const {
    return sl_ckp_count(self);
}

const sl_ckp_t* Streamlike::ckp(int idx) const {
    return sl_ckp(self, idx);
}

off_t Streamlike::ckp_offset(const sl_ckp_t* ckp) const {
    return sl_ckp_offset(self, ckp);
}

size_t Streamlike::ckp_metadata(const sl_ckp_t* ckp, const void** result) const {
    return sl_ckp_metadata(self, ckp, result);
}

bool Streamlike::hasRead() const {
    return self->read;
}

bool Streamlike::hasInput() const {
    return self->input;
}

bool Streamlike::hasWrite() const {
    return self->write;
}

bool Streamlike::hasFlush() const {
    return self->flush;
}

bool Streamlike::hasSeek() const {
    return self->seek;
}

bool Streamlike::hasTell() const {
    return self->tell;
}

bool Streamlike::hasEof() const {
    return self->eof;
}

bool Streamlike::hasError() const {
    return self->error;
}

bool Streamlike::hasLength() const {
    return self->length;
}

Streamlike::Streamlike(Streamlike&& old) {
    self = old.self;
    old.self = nullptr;
}

Streamlike& Streamlike::operator=(Streamlike&& old) {
    self = old.self;
    old.self = nullptr;
    return *this;
}

Streamlike::Streamlike(self_type self)
    : self(self) {}

}
