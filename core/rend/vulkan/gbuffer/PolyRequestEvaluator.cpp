#include "PolyRequestEvaluator.h"
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstdio>

PolyRequestEvaluator::PolyRequestEvaluator()
{
    // 1. Enregistrement de la fonction personnalisée
    // L'enregistrement lie le nom "isClose" à l'instance is_close_obj
    if (!symbol_table.add_function("isClose", is_close_obj)) {
        fprintf(stderr, "CRITICAL: Registration of 'isClose' FAILED!\n");
    }

    // 2. Configuration du parseur
    parser.settings().enable_all_logic_ops();
    parser.settings().enable_all_arithmetic_ops();
    parser.settings().enable_all_inequality_ops();

    // 3. Constantes standard
    symbol_table.add_constants();

    // 4. Liaison des variables (Passage par référence interne)
    symbol_table.add_variable("WP_X", wp_x);
    symbol_table.add_variable("WP_Y", wp_y);
    symbol_table.add_variable("WP_Z", wp_z);
    symbol_table.add_variable("Z",    z);
    symbol_table.add_variable("TH",   th);
    symbol_table.add_variable("PC",   pc);
    symbol_table.add_variable("MID",  mid);

    symbol_table.add_variable("MID_OPAQUE",          mid_opaque);
    symbol_table.add_variable("MID_OPAQUE_MOD",      mid_opaque_mod);
    symbol_table.add_variable("MID_TRANSLUCENT",     mid_translucent);
    symbol_table.add_variable("MID_TRANSLUCENT_MOD", mid_translucent_mod);
    symbol_table.add_variable("MID_PUNCH_THROUGH",   mid_punch_through);

    symbol_table.add_variable("MID_HAS_TEX",  mid_has_tex);
    symbol_table.add_variable("MID_GOURAUD",  mid_gouraud);
    symbol_table.add_variable("MID_HAS_BUMP", mid_has_bump);
    symbol_table.add_variable("MID_FOG",      mid_fog);

    // 5. Enregistrement FINAL de la table dans l'expression
    expression.register_symbol_table(symbol_table);
}

bool PolyRequestEvaluator::parse(const std::string& expression_string)
{
    // Sécurité : on ré-enregistre la table avant le parsing
    expression.register_symbol_table(symbol_table);

    if (!parser.compile(expression_string, expression))
    {
        fprintf(stderr, "ExprTK Error: %s | Expression: %s\n",
                parser.error().c_str(), expression_string.c_str());
        return false;
    }
    return true;
}

double PolyRequestEvaluator::evaluate(const PolyData& data)
{
    // Mise à jour des valeurs physiques
    wp_x = data.wp_x;
    wp_y = data.wp_y;
    wp_z = data.wp_z;
    z    = data.z;
    th   = static_cast<double>(data.texture_hash);
    pc   = static_cast<double>(data.poly_count);
    mid  = static_cast<double>(data.material_id);

    // Décodage des flags du Material ID (G-Buffer)
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

    // Évaluation de l'expression compilée
    return expression.value();
}