#include <migraphx/inline_subgraph.hpp>
#include <migraphx/program.hpp>
#include <migraphx/instruction.hpp>
#include <migraphx/make_op.hpp>
#include <migraphx/ranges.hpp>
#include <migraphx/iterator_for.hpp>

namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {

void inline_subgraph::apply(module& p) const
{
    for(auto ins : iterator_for(p))
    {
        if(ins->name() != "if")
            continue;

        auto arg_cond          = ins->inputs().front()->eval();
        const auto& mod_inputs = ins->module_inputs();
        // condition is not constant, but both subgraph outputs
        // are constant, so we can replace each subgraph with
        // a literal
        if(arg_cond.empty())
        {
            std::vector<argument> arg_outs;
            for(const auto& mod : mod_inputs)
            {
                auto last = std::prev(mod->end());
                std::vector<instruction_ref> mod_outputs;
                if(last->name() == "@return")
                {
                    mod_outputs = last->inputs();
                }
                else
                {
                    // no return instruction, last instruction is output
                    mod_outputs.push_back(last);
                }

                // only one output is considered for now
                for (const auto& out : mod_outputs)
                {
                    auto mod_out = out->eval();
                    if (mod_out.empty())
                    {
                        return;
                    }
                    arg_outs.push_back(mod_out);
                }
            }

            auto l0    = p.add_literal(literal(arg_outs.at(0).get_shape(), arg_outs.at(0).data()));
            auto l1    = p.add_literal(literal(arg_outs.at(1).get_shape(), arg_outs.at(1).data()));
            auto lens  = l0->get_shape().lens();
            auto cond  = ins->inputs().front();
            auto icond = p.insert_instruction(
                ins, make_op("convert", {{"target_type", shape::int32_type}}), cond);
            auto mcond = p.insert_instruction(
                ins, make_op("multibroadcast", {{"output_lens", lens}}), icond);
            auto ccond = p.insert_instruction(ins, make_op("contiguous"), mcond);
            auto l01   = p.insert_instruction(ins, make_op("concat", {{"axis", 0}}), l0, l1);
            auto rl    = p.insert_instruction(
                ins, make_op("reshape", {{"dims", {l0->get_shape().elements() * 2}}}), l01);
            auto r = p.insert_instruction(ins, make_op("gather", {{"axis", 0}}), rl, ccond);
            p.replace_instruction(ins, r);
        }
        // cond is constant, inline the corresponding subgraph and discard the other one
        else
        {
            inline_submodule(p, ins);
        }
    }
}

void inline_subgraph::inline_submodule(module& p, instruction_ref ins) const
{

    auto arg_cond = ins->inputs().front()->eval();
    assert(not arg_cond.empty());
    const auto& mod_inputs = ins->module_inputs();
    const auto* smod       = (arg_cond.at<bool>()) ? mod_inputs.at(0) : mod_inputs.at(1);

    std::unordered_map<instruction_ref, instruction_ref> map_ins;
    std::vector<instruction_ref> mod_outputs;
    for(auto sins : iterator_for(*smod))
    {
        instruction_ref copy_ins{};
        if(sins->name() == "@literal")
        {
            auto l   = sins->get_literal();
            copy_ins = p.add_literal(l);
        }
        else if(sins->name() == "@param")
        {
            auto&& name = any_cast<builtin::param>(sins->get_operator()).parameter;
            auto s      = sins->get_shape();
            copy_ins    = p.add_parameter(name, s);
        }
        else if(sins->name() == "@outline")
        {
            auto s   = sins->get_shape();
            copy_ins = p.add_outline(s);
        }
        else
        {
            auto mod_args = sins->module_inputs();
            auto inputs   = sins->inputs();
            std::vector<instruction_ref> copy_inputs(inputs.size());
            std::transform(inputs.begin(), inputs.end(), copy_inputs.begin(), [&](auto i) {

                assert(contains(map_ins, i) or p.has_instruction(i));
                return p.has_instruction(i) ? i : map_ins[i];
            });

            if(sins->name() == "@return")
            {
                mod_outputs = copy_inputs;
                break;
            }

            if(mod_args.empty())
            {
                copy_ins = p.insert_instruction(ins, sins->get_operator(), copy_inputs);
            }
            else
            {
                copy_ins = p.insert_instruction(ins, sins->get_operator(), copy_inputs, mod_args);
            }
        }
        map_ins[sins] = copy_ins;
        mod_outputs   = {copy_ins};
    }

    if(not mod_outputs.empty())
    {
        p.replace_instruction(ins, mod_outputs.front());
    }
}

} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx
