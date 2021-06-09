#ifndef MIGRAPHX_GUARD_OPERATORS_LOOP_HPP
#define MIGRAPHX_GUARD_OPERATORS_LOOP_HPP

#include "migraphx/errors.hpp"
#include "migraphx/raw_data.hpp"
#include <array>
#include <iterator>
#include <migraphx/check_shapes.hpp>
#include <migraphx/argument.hpp>
#include <migraphx/functional.hpp>
#include <migraphx/config.hpp>
#include <migraphx/module.hpp>
#include <cmath>
#include <utility>
#include <set>

namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {
namespace op {

struct loop
{
    int64_t max_iter_num = 0;

    template <class Self, class F>
    static auto reflect(Self& self, F f)
    {
        return pack(f(self.max_iter_num, "max_iter_num"));
    }

    std::string name() const { return "loop"; }

    shape compute_shape(const std::vector<shape>& inputs, std::vector<module_ref> mods) const
    {
        check_shapes{inputs, *this}.standard();
        if(mods.size() != 1)
        {
            MIGRAPHX_THROW("LOOP: operator should have one submodule.");
        }

        const auto& mod     = mods.front();
        auto mod_out_shapes = mod->get_output_shapes();
        auto dep_param_num  = inputs.size() - 2;
        // first two names -- iter_num and cond_var -- are not counted
        mod_out_shapes.erase(mod_out_shapes.begin());
        std::vector<shape> ins_out_shapes(mod_out_shapes.begin(),
                                          mod_out_shapes.begin() + dep_param_num);
        mod_out_shapes.erase(mod_out_shapes.begin(), mod_out_shapes.begin() + dep_param_num);
        for(const auto& out_s : mod_out_shapes)
        {
            auto lens = out_s.lens();
            lens.insert(lens.begin(), max_iter_num);
            ins_out_shapes.push_back({out_s.type(), lens});
        }

        return shape(ins_out_shapes);
    }

    std::pair<int, bool> get_name_index(const std::string& name,
                                        const std::string& param_prefix) const
    {
        auto loc = name.find(param_prefix);
        if(loc != std::string::npos)
        {
            int index = std::stoi(name.substr(loc + param_prefix.size()));
            return {index, true};
        }

        std::string out_prefix = "#output_";
        loc                    = name.find(out_prefix);
        if(loc != std::string::npos)
        {
            int index = std::stoi(name.substr(loc + out_prefix.size()));
            return {index, false};
        }

        return {-1, false};
    }

    argument compute(const shape& out_shape,
                     const std::vector<argument>& args,
                     const std::vector<module_ref>& mods,
                     const std::function<std::vector<argument>(
                         module_ref&, const std::unordered_map<std::string, argument>&)>& run) const
    {
        auto iter_num            = args.at(0).at<int64_t>();
        auto cond                = args.at(1).at<bool>();
        std::size_t dep_num      = args.size() - 2;
        module_ref mod           = mods.at(0);
        auto mod_name            = mod->name();
        std::string param_prefix = "#" + mod->name() + "_in_";

        std::vector<shape> vec_out_shapes = out_shape.sub_shapes();
        std::vector<argument> scan_outputs;
        std::transform(vec_out_shapes.begin() + dep_num,
                       vec_out_shapes.end(),
                       std::back_inserter(scan_outputs),
                       [&](auto& s) { return argument{s}; });

        std::vector<argument> in_args(args.begin() + 2, args.end());
        shape s_iter{shape::int64_type};
        shape s_cond{shape::bool_type};
        uint64_t iter = 0;
        in_args.insert(in_args.begin(), {s_cond, &cond});
        in_args.insert(in_args.begin(), {s_iter, &iter});
        const auto& param_name_shapes = mod->get_parameter_shapes();
        for(iter = 0; (iter < iter_num) and cond; ++iter)
        {
            std::unordered_map<std::string, argument> params;
            for(auto pn : param_name_shapes)
            {
                auto name = pn.first;
                std::cout << "loop_compute1, name = " << name << std::endl;
                auto io_index = get_name_index(name, param_prefix);
                assert((io_index.first != -1) and (io_index.second));

                // name is for input
                std::cout << "loop_compute2, index = " << io_index.first << std::endl;
                if(io_index.second)
                {
                    params[name] = in_args.at(io_index.first);
                }
            }

            auto mod_args = run(mod, params);
            std::cout << "mod_output:" << std::endl;
            for(const auto& arg : mod_args)
            {
                std::cout << "\targ = " << arg << std::endl;
            }

            std::cout << "loop_compute3, mod_arg_size = " << mod_args.size() << std::endl;
            // cond = mod_args.at(0).at<bool>();
            // copy dependency carray variables from mod outputs to inputs
            std::copy(mod_args.begin(), mod_args.begin() + dep_num + 1, in_args.begin() + 1);

            std::cout << "loop_compute4" << std::endl;
            // concat scan outputs
            std::vector<argument> mod_scan_outs(mod_args.begin() + 1 + dep_num, mod_args.end());
            std::cout << "loop_compute5, mod_scan_out_size = " << mod_scan_outs.size() << std::endl;
            std::cout << "loop_compute5, scan_out_size = " << scan_outputs.size() << std::endl;
            for(std::size_t i = 0; i < mod_scan_outs.size(); ++i)
            {
                auto& mod_out  = mod_scan_outs.at(i);
                auto& scan_out = scan_outputs.at(i);
                std::cout << "loop_compute6" << std::endl;

                auto in_data         = mod_out.data();
                auto out_data        = scan_out.data();
                std::size_t out_size = mod_out.get_shape().bytes();
                std::cout << "loop_compute7" << std::endl;
                memcpy(out_data + iter * out_size, in_data, out_size);
                std::cout << "loop_compute8" << std::endl;
            }
            std::cout << "loop_compute9" << std::endl;
        }

        // copy dependency carry output to final output
        std::vector<argument> outputs(in_args.begin() + 2, in_args.end());
        outputs.insert(outputs.end(), scan_outputs.begin(), scan_outputs.end());

        std::cout << "loop output = " << std::endl;
        for(auto& out : outputs)
        {
            std::cout << "\tins_out = " << out << std::endl;
        }

        return argument{outputs};
    }
};

} // namespace op
} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx

#endif
