#pragma once

#include "prajna/reference_count.hpp"

namespace prajna {
inline namespace interoperation {

using i64 = int64_t;

template <typename Type_, i64 Rank_>
class Array {
   public:
    Type_ data[Rank_];

   public:
    template <typename... Values_>
    Array(Values_... values) : data{values...} {}

    i64 length() const { return Rank_; }

    const Type_& operator[](i64 offset) const { return data[offset]; }
    Type_& operator[](i64 offset) { return data[offset]; }
};

template <i64 Rank_>
class Layout {
   public:
    Array<i64, Rank_> shape;
    Array<i64, Rank_> stride;

   public:
    static Layout create(Array<i64, Rank_> shape) {
        Layout<Rank_> self;
        self.shape = shape;
        auto i = Rank_ - 1;
        self.stride[i] = 1;
        while (i > 0) {
            i = i - 1;
            self.stride[i] = self.shape[i + 1] * self.stride[i + 1];
        }
        return self;
    }

    Array<i64, Rank_> linearIndexToArrayIndex(i64 offset) {
        Array<i64, Rank_> array_idx;
        auto remain = offset;
        i64 i = 0;
        while (i < Rank_) {
            array_idx[i] = remain / this->stride[i];
            remain = remain % this->stride[i];
            i = i + 1;
        }

        return array_idx;
    }

    i64 arrayIndexToLinearIndex(Array<i64, Rank_> idx) {
        i64 offset = 0;
        i64 i = Rank_ - 1;
        while (i >= 0) {
            offset = offset + idx[i] * this->stride[i];
            i = i - 1;
        }
        return offset;
    }

    i64 length() { return this->shape[0] * this->stride[0]; }
};

template <typename Type_, i64 Rank_>
class Tensor {
   public:
    Type_* data = nullptr;
    Layout<Rank_> layout;

   protected:
    Tensor() = default;

   public:
    static Tensor<Type_, Rank_> create(Array<i64, Rank_> shape) {
        Tensor<Type_, Rank_> self;
        self.layout = Layout<Rank_>::create(shape);

        auto bytes = self.layout.length() * sizeof(Type_);
        self.data = reinterpret_cast<Type_*>(malloc(bytes));

        registerReferenceCount(self.data);
        incReferenceCount(self.data);

        return self;
    }

    Tensor(const Tensor& ts) : data(ts.data), layout(ts.layout) { incReferenceCount(this->data); }

    ~Tensor() {
        decReferenceCount(this->data);
        if (this->data) {
            if (getReferenceCount(this->data) == 0) {
                free(this->data);
            }
        }
    }

    const Type_& at(Array<i64, Rank_> idx) const {
        i64 offset = this->layout.arrayIndexToLinearIndex(idx);
        return this->data[offset];
    }

    Type_& at(Array<i64, Rank_> idx) {
        i64 offset = this->layout.arrayIndexToLinearIndex(idx);
        return this->data[offset];
    }

    template <typename... Idx_>
    const Type_& operator()(Idx_... indices) const {
        Array<i64, Rank_> idx(indices...);
        return this->at(idx);
    }

    template <typename... Idx_>
    Type_& operator()(Idx_... indices) {
        Array<i64, Rank_> idx(indices...);
        return this->at(idx);
    }

    Array<i64, Rank_> shape() const { return layout.shape; }
};

}  // namespace interoperation
}  // namespace prajna
