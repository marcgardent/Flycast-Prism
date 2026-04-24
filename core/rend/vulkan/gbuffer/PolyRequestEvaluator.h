#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#ifdef verify
#pragma push_macro("verify")
#undef verify
#define RESTORE_VERIFY
#endif

#include "exprtk.hpp"

#ifdef RESTORE_VERIFY
#pragma pop_macro("verify")
#undef RESTORE_VERIFY
#endif

// Structure de données en entrée
struct PolyData {
    double wp_x, wp_y, wp_z;
    double z;
    unsigned int texture_hash;
    unsigned int poly_count;
    unsigned int material_id;
};

// Implémentation de isClose compatible Python (PEP 485)
template <typename T>
struct IsClose : public exprtk::igeneric_function<T>
{
    typedef typename exprtk::igeneric_function<T>::parameter_list_t parameter_list_t;
    typedef typename exprtk::type_store<T>::scalar_view scalar_view_t;

    // Utiliser une chaîne vide "" permet d'accepter un nombre variable d'arguments (variadic)
    IsClose() : exprtk::igeneric_function<T>("") {}

    inline T operator()(parameter_list_t parameters)
    {
        // Validation minimale : il faut au moins a et b
        if (parameters.size() < 2) return T(0);

        const T a = scalar_view_t(parameters[0])();
        const T b = scalar_view_t(parameters[1])();

        // Valeurs par défaut de Python (rel_tol=1e-09, abs_tol=0.0)
        const T rel_tol = (parameters.size() > 2) ? scalar_view_t(parameters[2])() : T(1e-09);
        const T abs_tol = (parameters.size() > 3) ? scalar_view_t(parameters[3])() : T(0.0);

        if (std::isinf(a) || std::isinf(b)) {
            return (a == b) ? T(1) : T(0);
        }

        const T diff = std::abs(a - b);
        const T threshold = std::max(rel_tol * std::max(std::abs(a), std::abs(b)), abs_tol);

        return (diff <= threshold) ? T(1) : T(0);
    }
};

class PolyRequestEvaluator
{
public:
    PolyRequestEvaluator();

    PolyRequestEvaluator(const PolyRequestEvaluator&) = delete;
    PolyRequestEvaluator& operator=(const PolyRequestEvaluator&) = delete;

    bool parse(const std::string& expression_string);
    double evaluate(const PolyData& data);

private:
    // Méthode de transpilation pour gérer les arguments nommés
    std::string transpileIsClose(std::string expr);

    // Variables liées à la symbol_table
    double wp_x = 0, wp_y = 0, wp_z = 0;
    double z = 0, th = 0, pc = 0, mid = 0;
    double mid_opaque = 0, mid_opaque_mod = 0, mid_translucent = 0;
    double mid_translucent_mod = 0, mid_punch_through = 0;
    double mid_has_tex = 0, mid_gouraud = 0, mid_has_bump = 0, mid_fog = 0;

    // Composants ExprTk
    IsClose<double> is_close_obj;
    exprtk::symbol_table<double> symbol_table;
    exprtk::expression<double> expression;
    exprtk::parser<double> parser;
};