struct streamlike_s;

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

    protected:
        Streamlike() = default;
        ~Streamlike() = default;
        struct streamlike_s *self;
};

} // namspace streamlike
