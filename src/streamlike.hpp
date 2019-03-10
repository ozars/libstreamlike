#ifndef STREAMLIKE_HPP
#define STREAMLIKE_HPP

#ifndef STREAMLIKE_H
/* TODO: This is redundant, find a better way to eliminate this redundancy. */
extern "C" {
    typedef enum sl_seekable_e
    {
        SL_SEEKING_NOT_SUPPORTED = 0, /**< Seeking isn't supported at all (Value:
                                        `0`). */
        SL_SEEKING_SUPPORTED     = 1, /**< Seeking is supported completely (Value:
                                        `1`). */
        SL_SEEKING_EMULATED      = 2, /**< Seeking is emulated through reading and
                                        discarding data read (Value: `2`). */
        SL_SEEKING_CHECKPOINTS   = 3  /**< Seeking to checkpoints is supported,
                                        while seeking to other parts is emulated
                                        (Value: `3`). */
    } sl_seekable_t;
    typedef struct streamlike_s streamlike_t;
    typedef struct sl_ckp_s sl_ckp_t;
}
#endif

namespace streamlike {

class Streamlike {
    public:
        using self_type = streamlike_t*;

        size_t read(void *buffer, size_t size);
        size_t input(const void **buffer, size_t size);
        size_t write(const void *buffer, size_t size);
        int flush();
        int seek(off_t offset, int whence);
        off_t tell() const;
        int eof() const;
        int error() const;
        off_t length() const;

        sl_seekable_t seekable() const;
        int ckp_count() const;
        const sl_ckp_t* ckp(int idx) const;
        off_t ckp_offset(const sl_ckp_t* ckp) const;
        size_t ckp_metadata(const sl_ckp_t* ckp, const void** result) const;

        bool hasRead() const;
        bool hasInput() const;
        bool hasWrite() const;
        bool hasFlush() const;
        bool hasSeek() const;
        bool hasTell() const;
        bool hasEof() const;
        bool hasError() const;
        bool hasLength() const;

        Streamlike(Streamlike&& old);
        Streamlike& operator=(Streamlike&& old);
        ~Streamlike() = default;

    protected:

        Streamlike() = default;
        Streamlike(self_type self);
        Streamlike(Streamlike& copy) = delete;
        Streamlike& operator=(Streamlike& copy) = delete;

        inline static self_type getSelf(Streamlike& obj) {
            return obj.self;
        };

        self_type self;
};

} // namspace streamlike

#endif /* STREAMLIKE_HPP */
