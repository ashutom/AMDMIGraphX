
#include "verify_program.hpp"
#include <migraphx/program.hpp>
#include <migraphx/generate.hpp>
#include <migraphx/serialize.hpp>

#include <migraphx/make_op.hpp>

#include <migraphx/op/common.hpp>

struct test_lstm_three_outputs : verify_program<test_lstm_three_outputs>
{
    migraphx::program create_program() const
    {
        std::size_t batch_size  = 2;
        std::size_t seq_len     = 3;
        std::size_t hidden_size = 5;
        std::size_t input_size  = 8;
        std::size_t num_dirct   = 1;
        float clip              = 0.0f;

        migraphx::program p;
        auto* mm = p.get_main_module();
        migraphx::shape in_shape{migraphx::shape::float_type, {seq_len, batch_size, input_size}};
        migraphx::shape w_shape{migraphx::shape::float_type,
                                {num_dirct, 4 * hidden_size, input_size}};
        migraphx::shape r_shape{migraphx::shape::float_type,
                                {num_dirct, 4 * hidden_size, hidden_size}};
        auto seq = mm->add_parameter("seq", in_shape);
        auto w   = mm->add_parameter("w", w_shape);
        auto r   = mm->add_parameter("r", r_shape);
        auto hs  = mm->add_instruction(
            migraphx::make_op(
                "lstm",
                {{"hidden_size", hidden_size},
                 {"actv_func",
                  migraphx::to_value(std::vector<migraphx::operation>{migraphx::make_op("sigmoid"),
                                                                      migraphx::make_op("tanh"),
                                                                      migraphx::make_op("tanh")})},
                 {"direction", migraphx::to_value(migraphx::op::rnn_direction::forward)},
                 {"clip", clip}}),
            seq,
            w,
            r);
        auto last_hs   = mm->add_instruction(migraphx::make_op("rnn_last_hs_output"), hs);
        auto last_cell = mm->add_instruction(migraphx::make_op("rnn_last_cell_output"), hs);
        mm->add_return({hs, last_hs, last_cell});

        return p;
    }
    std::string section() const { return "rnn"; }
};
