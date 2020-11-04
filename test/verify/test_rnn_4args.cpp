
#include "verify_program.hpp"
#include <migraphx/program.hpp>
#include <migraphx/generate.hpp>
#include <migraphx/operators.hpp>

struct test_rnn_4args : verify_program<test_rnn_4args>
{
    migraphx::program create_program() const
    {
        std::size_t batch_size  = 2;
        std::size_t seq_len     = 5;
        std::size_t hidden_size = 4;
        std::size_t input_size  = 3;
        std::size_t num_dirct   = 1;
        float clip              = 0.0f;

        migraphx::program p;
        migraphx::shape in_shape{migraphx::shape::float_type, {seq_len, batch_size, input_size}};
        migraphx::shape w_shape{migraphx::shape::float_type, {num_dirct, hidden_size, input_size}};
        migraphx::shape r_shape{migraphx::shape::float_type, {num_dirct, hidden_size, hidden_size}};
        migraphx::shape b_shape{migraphx::shape::float_type, {num_dirct, 2 * hidden_size}};

        auto seq  = p.add_parameter("seq", in_shape);
        auto w    = p.add_parameter("w", w_shape);
        auto r    = p.add_parameter("r", r_shape);
        auto bias = p.add_parameter("bias", b_shape);

        p.add_instruction(migraphx::op::rnn{hidden_size,
                                            {migraphx::op::tanh{}, migraphx::op::tanh{}},
                                            migraphx::op::rnn_direction::reverse,
                                            clip},
                          seq,
                          w,
                          r,
                          bias);

        return p;
    }
    std::string section() const { return "rnn"; }
};
