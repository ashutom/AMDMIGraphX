#ifndef MIGRAPHX_GUARD_OPERATORS_DOT_HPP
#define MIGRAPHX_GUARD_OPERATORS_DOT_HPP

#include <array>
#include <migraphx/check_shapes.hpp>
#include <migraphx/stringutils.hpp>
#include <migraphx/streamutils.hpp>
#include <migraphx/literal.hpp>
#include <migraphx/shape_for_each.hpp>
#include <migraphx/config.hpp>
#include <migraphx/gemm.hpp>
// #include <migraphx/ref/gemm.hpp>
#include <cmath>
#include <utility>

namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {
namespace op {

struct dot
{
    float alpha = 1.0;
    float beta  = 1.0;

    template <class Self, class F>
    static auto reflect(Self& self, F f)
    {
        return pack(f(self.alpha, "alpha"), f(self.beta, "beta"));
    }

    std::string name() const { return "dot"; }
    shape compute_shape(std::vector<shape> inputs) const
    {
        check_shapes{inputs, *this}.same_type();
        const shape& a = inputs.at(0);
        const shape& b = inputs.at(1);
        auto t         = a.type();

        if(!std::all_of(inputs.begin(), inputs.end(), [](auto s) { return s.lens().size() >= 2; }))
        {
            MIGRAPHX_THROW("DOT: dot only accept 2 or more dims operands");
        }

        // only handle the case that the batch size of a and b are the same
        if(!std::equal(
               a.lens().rbegin() + 2, a.lens().rend(), b.lens().rbegin() + 2, b.lens().rend()))
        {
            MIGRAPHX_THROW("DOT: batch size of A and B mismatch: {" + to_string_range(a.lens()) +
                           "} x {" + to_string_range(b.lens()) + "}");
        }

        std::size_t dim_0 = a.lens().size() - 2;
        std::size_t dim_1 = a.lens().size() - 1;
        if(a.lens()[dim_1] != b.lens()[dim_0])
        {
            MIGRAPHX_THROW("DOT: inner dimensions do not match: {" + to_string_range(a.lens()) +
                           "} x {" + to_string_range(b.lens()) + "}");
        }

        auto out_lens   = a.lens();
        out_lens[dim_1] = b.lens()[dim_1];
        if(inputs.size() == 3 && out_lens != inputs.at(2).lens())
        {
            MIGRAPHX_THROW("DOT: dimension mismatch, operand C: {" +
                           to_string_range(inputs.at(2).lens()) +
                           "}, cannot add to operand A * B: {" + to_string_range(out_lens) + "}");
        }

        return {t, out_lens};
    }

    argument compute(shape output_shape, std::vector<argument> args) const
    {
        argument result;
        if(args.size() == 3)
            result = args[2];
        else
            result = argument{output_shape};
        visit_all(result, args[0], args[1])(
            [&](auto cmat, auto amat, auto bmat) { gemm(cmat, amat, bmat, alpha, beta); });
        return result;
    }

    // argument compute(shape output_shape, std::vector<argument> args) const
    // {
    //     argument result{output_shape};
    //     // 3 inputs, it is alpha * A * B + beta * C, then
    //     // A and B are matrices, and C is of the same shape to A * B

    //     // first, convert the args[0] and args[1] from int8_t to int32_t
    //     argument arg_0{{shape::int32_type, {args.at(0).get_shape().lens()}}};
    //     argument arg_1{{shape::int32_type, {args.at(1).get_shape().lens()}}};
    //     arg_0.visit([&](auto output) {
    //         args.at(0).visit(
    //             [&](auto input) { std::copy(input.begin(), input.end(), output.begin()); });
    //     });

    //     arg_1.visit([&](auto output) {
    //         args.at(1).visit(
    //             [&](auto input) { std::copy(input.begin(), input.end(), output.begin()); });
    //     });

    //     if(args.size() == 3)
    //     {
    //         // no need to consider the value of args[2]
    //         if(beta == 0)
    //         {
    //             result.visit([&](auto output) { std::fill(output.begin(), output.end(), 0); });
    //         }
    //         else
    //         {
    //             visit_all(result, args[2])([&](auto output, auto input) {
    //                 std::copy(input.begin(), input.end(), output.begin());
    //             });
    //         }

    //         migemm(result, arg_0, arg_1, alpha, beta);

    //         return result;
    //     }

    //     // 2 input arguments
    //     migemm(result, arg_0, arg_1, alpha, int32_t{0});

    //     return result;
    // }
};

} // namespace op
} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx

#endif
