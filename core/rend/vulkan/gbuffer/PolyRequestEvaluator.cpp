#include "PolyRequestEvaluator.h"
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <regex>
#include <sstream>

namespace rend {

PolyRequestEvaluator::PolyRequestEvaluator()
{
    symbol_table.add_function("isCloseInternal", is_close_obj);
    symbol_table.add_constants();

    symbol_table.add_variable("WP_X", wp_x);
    symbol_table.add_variable("WP_Y", wp_y);
    symbol_table.add_variable("WP_Z", wp_z);
    symbol_table.add_variable("TH", th);
    symbol_table.add_variable("PC", pc);
    symbol_table.add_variable("MID", mid);

    symbol_table.add_variable("wp_x", wp_x);
    symbol_table.add_variable("wp_y", wp_y);
    symbol_table.add_variable("wp_z", wp_z);
    symbol_table.add_variable("texture_hash", th);
    symbol_table.add_variable("poly_count", pc);
    symbol_table.add_variable("material_id", mid);

    symbol_table.add_variable("MID_OPAQUE", mid_opaque);
    symbol_table.add_variable("MID_OPAQUE_MOD", mid_opaque_mod);
    symbol_table.add_variable("MID_TRANSLUCENT", mid_translucent);
    symbol_table.add_variable("MID_TRANSLUCENT_MOD", mid_translucent_mod);
    symbol_table.add_variable("MID_PUNCH_THROUGH", mid_punch_through);

    symbol_table.add_variable("MID_HAS_TEX", mid_has_tex);
    symbol_table.add_variable("MID_GOURAUD", mid_gouraud);
    symbol_table.add_variable("MID_HAS_BUMP", mid_has_bump);
    symbol_table.add_variable("MID_FOG", mid_fog);

    expression.register_symbol_table(symbol_table);
}

std::string PolyRequestEvaluator::transpileIsClose(std::string expr) {
    std::regex re("is[Cc]lose\\s*\\(([^\\)]+)\\)");
    std::smatch match;
    std::string result_expr = expr;
    std::string processed_expr;
    while (std::regex_search(result_expr, match, re)) {
        processed_expr += match.prefix().str();
        std::string content = match[1];
        std::vector<std::string> args;
        std::stringstream ss(content);
        std::string segment;
        while (std::getline(ss, segment, ',')) {
            segment.erase(0, segment.find_first_not_of(" \t\n\r"));
            segment.erase(segment.find_last_not_of(" \t\n\r") + 1);
            args.push_back(segment);
        }
        if (args.size() < 2) {
            processed_expr += match[0].str();
        } else {
            std::string a = args[0], b = args[1], rtol = "1e-9", atol = "0.0";
            for (size_t i = 2; i < args.size(); ++i) {
                if (args[i].find("rel_tol=") != std::string::npos) rtol = args[i].substr(args[i].find('=') + 1);
                else if (args[i].find("abs_tol=") != std::string::npos) atol = args[i].substr(args[i].find('=') + 1);
                else if (i == 2) rtol = args[i];
                else if (i == 3) atol = args[i];
            }
            processed_expr += "isCloseInternal(" + a + "," + b + "," + rtol + "," + atol + ")";
        }
        result_expr = match.suffix().str();
    }
    processed_expr += result_expr;
    return processed_expr;
}

bool PolyRequestEvaluator::parse(const std::string& expression_string)
{
    std::string transpiled = transpileIsClose(expression_string);
    if (!parser.compile(transpiled, expression))
    {
        return false;
    }
    return true;
}

double PolyRequestEvaluator::evaluate(const PolyData& data)
{
    wp_x = data.wp_x;
    wp_y = data.wp_y;
    wp_z = data.wp_z;
    th = (double)data.texture_hash;
    pc = (double)data.poly_count;
    mid = (double)data.material_id;

    uint32_t m = data.material_id;
    uint32_t list_type = (m >> 4) & 0x7;

    mid_opaque          = (list_type == 0) ? 1.0 : 0.0;
    mid_opaque_mod      = (list_type == 1) ? 1.0 : 0.0;
    mid_translucent     = (list_type == 2) ? 1.0 : 0.0;
    mid_translucent_mod = (list_type == 3) ? 1.0 : 0.0;
    mid_punch_through   = (list_type == 4) ? 1.0 : 0.0;

    mid_has_tex  = (m >> 3) & 1 ? 1.0 : 0.0;
    mid_gouraud  = (m >> 2) & 1 ? 1.0 : 0.0;
    mid_has_bump = (m >> 1) & 1 ? 1.0 : 0.0;
    mid_fog      = m & 1 ? 1.0 : 0.0;

    return expression.value();
}

} // namespace rend