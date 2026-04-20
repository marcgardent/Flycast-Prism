#include "ui/settings.h"
namespace library {
    std::string getGameId() {

        std::string game_id(settings.content.gameId);
        if (game_id.empty())
            return "";


        const size_t str_end = game_id.find_last_not_of(' ');
        if (str_end == std::string::npos)
            return "";
        game_id = game_id.substr(0, str_end + 1);
        std::replace(game_id.begin(), game_id.end(), ' ', '_');

        return game_id;
    }
}
