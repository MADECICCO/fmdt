#pragma once

#include <stdint.h>
#include <aff3ct.hpp>

namespace ftr_mrg {
    enum class tsk : size_t { merge, SIZE };
    namespace sck {
        enum class merge : size_t { in_img1, in_img2, in_ROI_id, in_ROI_xmin, in_ROI_xmax, in_ROI_ymin,
                                    in_ROI_ymax, in_ROI_S, in_ROI_Sx, in_ROI_Sy, in_ROI_x, in_ROI_y, in_n_ROI,
                                    out_ROI_id, out_ROI_xmin, out_ROI_xmax, out_ROI_ymin, out_ROI_ymax, out_ROI_S,
                                    out_ROI_Sx, out_ROI_Sy, out_ROI_x, out_ROI_y, out_n_ROI, out_img, status };
    }
}

class Features_merger : public aff3ct::module::Module {
protected:
    const int i0, i1, j0, j1;
    const int b;
    const int S_min, S_max;
    const size_t max_ROI_size;
    const uint32_t** in_img1;
    const uint8_t** in_img2;
    uint8_t** out_img;
public:
    Features_merger(const int i0, const int i1, const int j0, const int j1, const int b, const uint32_t S_min,
                    const uint32_t S_max, const size_t max_ROI_size);
    virtual ~Features_merger();
    virtual Features_merger* clone() const;
    inline uint8_t** get_out_img();
    inline aff3ct::module::Task& operator[](const ftr_mrg::tsk t);
    inline aff3ct::module::Socket& operator[](const ftr_mrg::sck::merge s);
protected:
    void init_data();
    void deep_copy(const Features_merger &m);
};

#include "fmdt/Features/Features_merger.hxx"
