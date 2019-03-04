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
        size_t read(void *buffer, size_t size);
        size_t input(const void **buffer, size_t size);
        size_t write(const void *buffer, size_t size);
        int flush();
        int seek(off_t offset, int whence);
        off_t tell();
        int eof();
        int error();
        off_t length();

        sl_seekable_t seekable();
        int ckp_count();
        const sl_ckp_t* ckp(int idx);
        off_t ckp_offset(const sl_ckp_t* ckp);
        size_t ckp_metadata(const sl_ckp_t* ckp, const void** result);

        bool hasRead();
        bool hasInput();
        bool hasWrite();
        bool hasFlush();
        bool hasSeek();
        bool hasTell();
        bool hasEof();
        bool hasError();
        bool hasLength();

        Streamlike(Streamlike&& old);
        Streamlike& operator=(Streamlike&& old);
        ~Streamlike();

    protected:
        using self_type = streamlike_t*;
        /* This is a terrible hack that I use to pass related destroying
         * callback, since virtual destructor causes issues with object
         * slicing. This callback is initialized by derived classes. */
        using c_dtor_callback_type = int (*)(self_type);

        Streamlike() = default;
        Streamlike(self_type self, c_dtor_callback_type cdtor);
        Streamlike(Streamlike& copy) = delete;
        Streamlike& operator=(Streamlike& copy) = delete;

        inline static self_type getSelf(Streamlike& obj) {
            return obj.self;
        };

        self_type self;
        c_dtor_callback_type cdtor;
};

} // namspace streamlike

#endif /* STREAMLIKE_HPP */
