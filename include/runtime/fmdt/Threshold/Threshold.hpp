#pragma once

#include <stdint.h>
#include <aff3ct.hpp>

namespace thr {
    enum class tsk : size_t { apply, SIZE };
    namespace sck {
        enum class apply : size_t { in_img, out_img, status };
    }
}

class Threshold : public aff3ct::module::Module {
protected:
    const int i0, i1, j0, j1;
    const int b;
    const uint8_t thr_val;
    const uint8_t** in_img;
    uint8_t** out_img;
public:
    Threshold(const int i0, const int i1, const int j0, const int j1, const int b, const uint8_t thr_val);
    virtual ~Threshold();
    virtual Threshold* clone() const;
    inline uint8_t** get_out_img();
    inline aff3ct::module::Task& operator[](const thr::tsk t);
    inline aff3ct::module::Socket& operator[](const thr::sck::apply s);
protected:
    void init_data();
    void deep_copy(const Threshold &m);
};

#include "fmdt/Threshold/Threshold.hxx"
