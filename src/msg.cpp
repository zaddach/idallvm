#include <pro.h>
#include "idallvm/msg.h"

class ida_raw_ostream : public llvm::raw_ostream
{
public:
    void write_impl(const char *ptr, size_t size) override {
        char* buf = static_cast<char*>(alloca(size + 1));
        memcpy(buf, ptr, size);
        buf[size] = '\0';
        msg("%s", buf);
    }

    uint64_t current_pos() const override {
        return 0;
    }

    virtual ~ida_raw_ostream() {
        flush();
    }
};

static ida_raw_ostream _outs;

llvm::raw_ostream& outs(void) {
    return _outs;
}
