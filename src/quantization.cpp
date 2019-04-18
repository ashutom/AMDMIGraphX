#include <migraphx/quantization.hpp>
#include <migraphx/program.hpp>
#include <migraphx/instruction.hpp>
#include <migraphx/iterator_for.hpp>
#include <migraphx/op/fp_conversion.hpp>
#include <migraphx/stringutils.hpp>
#include <migraphx/ranges.hpp>
#include <utility>

namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {

instruction_ref insert_fp16(program& prog,
                            instruction_ref& ins,
                            shape::type_t type,
                            std::unordered_map<instruction_ref, instruction_ref>& map_fp16)
{
    if(map_fp16.count(ins) > 0)
    {
        return map_fp16[ins];
    }

    assert(ins->get_shape().type() == shape::float_type ||
           ins->get_shape().type() == shape::double_type);
    instruction_ref ins_fp16{};
    if(ins->name() == "@literal" && ins->outputs().size() == 1)
    {
        std::vector<float> values;
        auto l_fp32 = ins->get_literal();
        shape s     = ins->get_shape();
        l_fp32.visit([&](auto val) { values.assign(val.begin(), val.end()); });
        ins_fp16 = prog.add_literal(literal({shape::half_type, s.lens()}, values));
    }
    else
    {
        if(ins == std::prev(prog.end()))
        {
            ins_fp16 = prog.add_instruction(op::fp_conversion{type}, ins);
        }
        else
        {
            ins_fp16 = prog.insert_instruction(std::next(ins), op::fp_conversion{}, ins);
        }
    }
    map_fp16[ins] = ins_fp16;

    return ins_fp16;
}

void quantize(program& prog, const std::vector<std::string>& ins_names)
{
    std::unordered_map<instruction_ref, instruction_ref> map_fp16;
    for(auto ins : iterator_for(prog))
    {
        // all indicates every instruction is converted
        if((not contains(ins_names, "all")) and (not contains(ins_names, ins->name())))
        {
            continue;
        }

        shape::type_t orig_type = ins->get_shape().type();
        // process all inputs, if input is a fp32 or fp64, convert it
        // to a fp16 by adding a fp_conversion operator.
        auto inputs = ins->inputs();
        std::vector<instruction_ref> converted_inputs;
        for(auto input : inputs)
        {
            auto s = input->get_shape();
            if(s.type() == shape::float_type || s.type() == shape::double_type)
            {
                // if the input is a fp_conversion operator, uses its input
                // as its current input
                instruction_ref input_fp16{};
                if(input->name() == "fp_conversion")
                {
                    input_fp16 = input->inputs().front();
                }
                else
                {
                    input_fp16 = insert_fp16(prog, input, shape::half_type, map_fp16);
                }
                // instruction::replace_argument(ins, input, input_fp16, false);
                converted_inputs.push_back(input_fp16);
            }
            else
            {
                converted_inputs.push_back(input);
            }
        }

        if(inputs != converted_inputs)
        {
            auto op        = ins->get_operator();
            auto ins_shape = compute_shape(op, converted_inputs);
            if(ins_shape.type() != orig_type)
            {
                // insert another fp_conversion instruction to convert it back
                if(ins == std::prev(prog.end()))
                {
                    prog.add_instruction(op::fp_conversion{orig_type}, ins);
                }
                else
                {
                    auto ins_orig_type =
                        prog.insert_instruction(std::next(ins), op::fp_conversion{orig_type}, ins);
                    prog.replace_instruction(ins, ins_orig_type);
                }
            }

            prog.replace_instruction(ins, op, converted_inputs);
            // instruction::replace(ins, op, compute_shape(op, converted_inputs), converted_inputs);
        }
    }
}

} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx
