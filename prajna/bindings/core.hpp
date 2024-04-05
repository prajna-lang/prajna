#pragma once

namespace prajna {
inline namespace interoperation {

using i64 = int64_t;

template <typename Type_, i64 Dim_>
class Array {
   public:
    Type_ data[Dim_];

   public:
    template <typename... Values_>
    Array(Values_... values) : data{values...} {}

    i64 Length() const { return Dim_; }

    const Type_& operator[](i64 offset) const { return data[offset]; }
    Type_& operator[](i64 offset) { return data[offset]; }
};

template <typename Type>
struct Ptr {
    Type* raw_ptr;
    size_t size;
    size_t* _reference_counter;

    static Ptr<Type> Allocate(size_t size) {
        Ptr<Type> self;
        self.raw_ptr = new Type[size];
        self.size = size;

        self._reference_counter = new size_t;
        *self._reference_counter = 1;
        return self;
    }

    Type& operator[](size_t offset) { return this->raw_ptr[offset]; }

    // ~Ptr() { delete[] this->raw_ptr; }
};

template <i64 Dim_>
class Layout {
   public:
    Array<i64, Dim_> shape;
    Array<i64, Dim_> stride;

   public:
    static Layout Create(Array<i64, Dim_> shape) {
        Layout<Dim_> self;
        self.shape = shape;
        auto i = Dim_ - 1;
        self.stride[i] = 1;
        while (i > 0) {
            i = i - 1;
            self.stride[i] = self.shape[i + 1] * self.stride[i + 1];
        }
        return self;
    }

    Array<i64, Dim_> LinearIndexToArrayIndex(i64 offset) {
        Array<i64, Dim_> array_idx;
        auto remain = offset;
        i64 i = 0;
        while (i < Dim_) {
            array_idx[i] = remain / this->stride[i];
            remain = remain % this->stride[i];
            i = i + 1;
        }

        return array_idx;
    }

    i64 ArrayIndexToLinearIndex(Array<i64, Dim_> idx) {
        i64 offset = 0;
        i64 i = Dim_ - 1;
        while (i >= 0) {
            offset = offset + idx[i] * this->stride[i];
            i = i - 1;
        }
        return offset;
    }

    i64 Length() { return this->shape[0] * this->stride[0]; }
};

template <typename Type_, i64 Dim_>
class Tensor {
   public:
    Ptr<Type_> data;
    Layout<Dim_> layout;

   protected:
    Tensor() = default;

   public:
    static Tensor<Type_, Dim_> Create(Array<i64, Dim_> shape) {
        Tensor<Type_, Dim_> self;
        self.layout = Layout<Dim_>::Create(shape);

        self.data = Ptr<Type_>::Allocate(self.layout.Length());

        // TODO(zhangzhimin): 初始化和释放还没有处理
        // RegisterReferenceCount(self.data);
        // __copy__(self.data);

        return self;
    }

    Tensor(const Tensor& ts) : data(ts.data), layout(ts.layout) {
        // __copy__(this->data);
    }

    ~Tensor() {
        // __finalize__(this->data);
        // if (this->data) {
        //     if (GetReferenceCount(this->data) == 0) {
        //         free(this->data);
        //     }
        // }
    }

    Type_& at(Array<i64, Dim_> idx) const {
        i64 offset = this->layout.ArrayIndexToLinearIndex(idx);
        return this->data[offset];
    }

    Type_& at(Array<i64, Dim_> idx) {
        i64 offset = this->layout.ArrayIndexToLinearIndex(idx);
        return this->data[offset];
    }

    template <typename... Idx_>
    Type_& operator()(Idx_... indices) const {
        Array<i64, Dim_> idx(indices...);
        return this->at(idx);
    }

    template <typename... Idx_>
    Type_& operator()(Idx_... indices) {
        Array<i64, Dim_> idx(indices...);
        return this->at(idx);
    }

    Array<i64, Dim_> Shape() const { return layout.shape; }
};

}  // namespace interoperation
}  // namespace prajna
