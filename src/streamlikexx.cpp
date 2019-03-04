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

off_t Streamlike::tell() {
    return sl_tell(self);
}
int Streamlike::eof() {
    return sl_eof(self);
}

int Streamlike::error() {
    return sl_error(self);
}

off_t Streamlike::length() {
    return sl_length(self);
}

sl_seekable_t Streamlike::seekable() {
    return sl_seekable(self);
}

int Streamlike::ckp_count() {
    return sl_ckp_count(self);
}

const sl_ckp_t* Streamlike::ckp(int idx) {
    return sl_ckp(self, idx);
}

off_t Streamlike::ckp_offset(const sl_ckp_t* ckp) {
    return sl_ckp_offset(self, ckp);
}

size_t Streamlike::ckp_metadata(const sl_ckp_t* ckp, const void** result) {
    return sl_ckp_metadata(self, ckp, result);
}

bool Streamlike::hasRead() {
    return self->read;
}

bool Streamlike::hasInput() {
    return self->input;
}

bool Streamlike::hasWrite() {
    return self->write;
}

bool Streamlike::hasFlush() {
    return self->flush;
}

bool Streamlike::hasSeek() {
    return self->seek;
}

bool Streamlike::hasTell() {
    return self->tell;
}

bool Streamlike::hasEof() {
    return self->eof;
}

bool Streamlike::hasError() {
    return self->error;
}

bool Streamlike::hasLength() {
    return self->length;
}

Streamlike::Streamlike(struct streamlike_s *self, c_dtor_callback_type cdtor)
        : self(self), cdtor(cdtor) {}

Streamlike::Streamlike(Streamlike&& old)
        : self(old.self), cdtor(old.cdtor) {
    old.self = nullptr;
}

Streamlike::~Streamlike() {
    if (self && cdtor && cdtor(self) != 0) {
        throw std::runtime_error("Failed destroying streamlike object");
    }
}

Streamlike& Streamlike::operator=(Streamlike&& old) {
    self = old.self;
    old.self = nullptr;
    return *this;
}

}
