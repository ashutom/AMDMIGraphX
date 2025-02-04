#ifndef MIGRAPHX_GUARD_GPU_PREFIX_SCAN_SUM_HPP
#define MIGRAPHX_GUARD_GPU_PREFIX_SCAN_SUM_HPP

#include <migraphx/gpu/name.hpp>
#include <migraphx/gpu/hip.hpp>
#include <migraphx/gpu/context.hpp>
#include <migraphx/gpu/device/prefix_scan_sum.hpp>
#include <migraphx/op/prefix_scan_sum.hpp>
#include <migraphx/reflect.hpp>
#include <migraphx/shape.hpp>
#include <migraphx/argument.hpp>
#include <migraphx/config.hpp>
#include <migraphx/type_name.hpp>
#include <utility>
#include <iostream>

namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {
namespace gpu {

struct context;

struct hip_prefix_scan_sum : oper<hip_prefix_scan_sum>
{
    op::prefix_scan_sum op;

    template <class Self, class T>
    static auto reflect(Self& self, T f)
    {
        return migraphx::reflect(self.op, f);
    }

    shape compute_shape(const std::vector<shape>& inputs) const
    {
        std::vector<shape> in_shapes{inputs};
        in_shapes.pop_back();
        check_shapes{in_shapes, *this}.standard();
        return op.normalize_compute_shape(in_shapes);
    }

    argument compute(context& ctx, const shape&, const std::vector<argument>& args) const
    {
        if(op.exclusive or op.reverse)
            MIGRAPHX_THROW("Exclusive and reverse scan not supported");
        device::prefix_scan_sum(ctx.get_stream().get(), args[1], args[0], op.axis);
        return args[1];
    }

    std::ptrdiff_t output_alias(const std::vector<shape>& shapes) const
    {
        return shapes.size() - 1;
    }
};

} // namespace gpu
} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx
#endif // MIGRAPHX_GUARD_GPU_PREFIX_SCAN_SUM_HPP
