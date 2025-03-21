// LICENSE HERE.

//
// svgame/entities/info_player_start.cpp
//
//
// info_player_start entity implementation.
//

// Include local game header.
#include "../../g_local.h"

//=====================================================
void SP_misc_teleporter_dest(Entity* ent);

/*QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 32)
potential spawning position for deathmatch games
*/
void SP_info_player_deathmatch(Entity* self)
{
    if (!deathmatch->value) {
        SVG_FreeEntity(self);
        return;
    }
    SP_misc_teleporter_dest(self);
}

