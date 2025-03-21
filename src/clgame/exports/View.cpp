#include "../clg_local.h"

#include "../clg_effects.h"
#include "../clg_entities.h"
#include "../clg_input.h"
#include "../clg_main.h"
#include "../clg_media.h"
#include "../clg_parse.h"
#include "../clg_predict.h"
#include "../clg_screen.h"
#include "../clg_tents.h"
#include "../clg_view.h"

#include "shared/interfaces/IClientGameExports.h"
#include "../ClientGameExports.h"
#include "View.h"

//---------------
// ClientGameView::PreRenderView
//
//---------------
void ClientGameView::PreRenderView() {

}

//---------------
// ClientGameView::ClearScene
//
//---------------
void ClientGameView::ClearScene() {
// WID: TODO: Ifdef... remove... ? hehe
#if USE_DLIGHTS
    view.num_dlights = 0;
#endif
    view.num_entities = 0;
    view.num_particles = 0;
}

//---------------
// ClientGameView::RenderView
//
//---------------
void ClientGameView::RenderView() {
    // Calculate client view values.
    CLG_UpdateOrigin();

    // Finish calculating view values.
    CLG_FinishViewValues();

    // Add entities here.
    CLG_AddPacketEntities();
    CLG_AddTempEntities();
    CLG_AddParticles();

#if USE_DLIGHTS
    CLG_AddDLights();
#endif
#if USE_LIGHTSTYLES
    CLG_AddLightStyles();
#endif

    // Last but not least, pass our array over to the client.
    cl->refdef.num_entities = view.num_entities;
    cl->refdef.entities = view.entities;
    cl->refdef.num_particles = view.num_particles;
    cl->refdef.particles = view.particles;
#if USE_DLIGHTS
    cl->refdef.num_dlights = view.num_dlights;
    cl->refdef.dlights = view.dlights;
#endif
#if USE_LIGHTSTYLES
    cl->refdef.lightstyles = view.lightstyles;
#endif
}

//---------------
// ClientGameView::PostRenderView
//
//---------------
void ClientGameView::PostRenderView() {
    V_SetLightLevel();
}