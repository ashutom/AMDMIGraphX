
#include "verify_program.hpp"
#include <migraphx/program.hpp>
#include <migraphx/generate.hpp>
#include <migraphx/operators.hpp>

struct test_group_conv : verify_program<test_group_conv>
{
    migraphx::program create_program() const
    {
        migraphx::program p;
        auto input =
            p.add_parameter("x", migraphx::shape{migraphx::shape::float_type, {1, 4, 16, 16}});
        auto weights =
            p.add_parameter("w", migraphx::shape{migraphx::shape::float_type, {4, 1, 3, 3}});
        migraphx::op::convolution op;
        op.group = 4;
        p.add_instruction(op, input, weights);
        return p;
    }
};
