/*
Copyright (C) 2003-2006 Andrey Nazarov

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "gl.h"

typedef void (*tessfunc_t)(const maliasmesh_t *);

static int      oldframenum;
static int      newframenum;
static float    frontlerp;
static float    backlerp;
static vec3_t   origin;
static vec3_t   oldscale;
static vec3_t   newscale;
static vec3_t   translate;
static vec_t    shellscale;
static tessfunc_t tessfunc;
static vec4_t   color;

static const vec_t  *shadelight;
static vec3_t       shadedir;

static float    celscale;

static GLfloat  shadowmatrix[16];

static void setup_dotshading(void)
{
    float cp, cy, sp, sy;
    vec_t yaw;

    shadelight = NULL;

    if (!gl_dotshading->integer)
        return;

    if (glr.ent->flags & RF_SHELL_MASK)
        return;

    shadelight = color;

    // matches the anormtab.h precalculations
    yaw = -Radians(glr.ent->angles[vec3_t::Yaw]);
    cy = std::cosf(yaw);
    sy = std::sinf(yaw);
    cp = std::cosf(-M_PI / 4);
    sp = std::sinf(-M_PI / 4);
    shadedir[0] = cp * cy;
    shadedir[1] = cp * sy;
    shadedir[2] = -sp;
}

static inline vec_t shadedot(const vec_t *normal)
{
    vec_t d = DotProduct(normal, shadedir);

    // matches the anormtab.h precalculations
    if (d < 0) {
        d *= 0.3f;
    }

    return d + 1;
}

static inline vec_t *get_static_normal(vec_t *normal, const maliasvert_t *vert)
{
    unsigned int lat = vert->norm[0];
    unsigned int lng = vert->norm[1];

    normal[0] = TAB_SIN(lat) * TAB_COS(lng);
    normal[1] = TAB_SIN(lat) * TAB_SIN(lng);
    normal[2] = TAB_COS(lat);

    return normal;
}

static void tess_static_shell(const maliasmesh_t *mesh)
{
    maliasvert_t *src_vert = &mesh->verts[newframenum * mesh->numverts];
    vec_t *dst_vert = tess.vertices;
    int count = mesh->numverts;
    vec3_t normal;

    while (count--) {
        get_static_normal(normal, src_vert);

        dst_vert[0] = normal[0] * shellscale +
                      src_vert->pos[0] * newscale[0] + translate[0];
        dst_vert[1] = normal[1] * shellscale +
                      src_vert->pos[1] * newscale[1] + translate[1];
        dst_vert[2] = normal[2] * shellscale +
                      src_vert->pos[2] * newscale[2] + translate[2];
        dst_vert += 4;

        src_vert++;
    }
}

static void tess_static_shade(const maliasmesh_t *mesh)
{
    maliasvert_t *src_vert = &mesh->verts[newframenum * mesh->numverts];
    vec_t *dst_vert = tess.vertices;
    int count = mesh->numverts;
    vec3_t normal;
    vec_t d;

    while (count--) {
        d = shadedot(get_static_normal(normal, src_vert));

        dst_vert[0] = src_vert->pos[0] * newscale[0] + translate[0];
        dst_vert[1] = src_vert->pos[1] * newscale[1] + translate[1];
        dst_vert[2] = src_vert->pos[2] * newscale[2] + translate[2];
        dst_vert[4] = shadelight[0] * d;
        dst_vert[5] = shadelight[1] * d;
        dst_vert[6] = shadelight[2] * d;
        dst_vert[7] = shadelight[3];
        dst_vert += VERTEX_SIZE;

        src_vert++;
    }
}

static void tess_static_plain(const maliasmesh_t *mesh)
{
    maliasvert_t *src_vert = &mesh->verts[newframenum * mesh->numverts];
    vec_t *dst_vert = tess.vertices;
    int count = mesh->numverts;

    while (count--) {
        dst_vert[0] = src_vert->pos[0] * newscale[0] + translate[0];
        dst_vert[1] = src_vert->pos[1] * newscale[1] + translate[1];
        dst_vert[2] = src_vert->pos[2] * newscale[2] + translate[2];
        dst_vert += 4;

        src_vert++;
    }
}

static inline vec_t *get_lerped_normal(vec_t *normal,
                                       const maliasvert_t *oldvert,
                                       const maliasvert_t *newvert)
{
    vec3_t oldnorm, newnorm, tmp;
    vec_t len;

    get_static_normal(oldnorm, oldvert);
    get_static_normal(newnorm, newvert);

    LerpVector2(oldnorm, newnorm, backlerp, frontlerp, tmp);

    // normalize result
    len = 1 / VectorLength(tmp);
    VectorScale(tmp, len, normal);

    return normal;
}

static void tess_lerped_shell(const maliasmesh_t *mesh)
{
    maliasvert_t *src_oldvert = &mesh->verts[oldframenum * mesh->numverts];
    maliasvert_t *src_newvert = &mesh->verts[newframenum * mesh->numverts];
    vec_t *dst_vert = tess.vertices;
    int count = mesh->numverts;
    vec3_t normal;

    while (count--) {
        get_lerped_normal(normal, src_oldvert, src_newvert);

        dst_vert[0] = normal[0] * shellscale +
                      src_oldvert->pos[0] * oldscale[0] +
                      src_newvert->pos[0] * newscale[0] + translate[0];
        dst_vert[1] = normal[1] * shellscale +
                      src_oldvert->pos[1] * oldscale[1] +
                      src_newvert->pos[1] * newscale[1] + translate[1];
        dst_vert[2] = normal[2] * shellscale +
                      src_oldvert->pos[2] * oldscale[2] +
                      src_newvert->pos[2] * newscale[2] + translate[2];
        dst_vert += 4;

        src_oldvert++;
        src_newvert++;
    }
}

static void tess_lerped_shade(const maliasmesh_t *mesh)
{
    maliasvert_t *src_oldvert = &mesh->verts[oldframenum * mesh->numverts];
    maliasvert_t *src_newvert = &mesh->verts[newframenum * mesh->numverts];
    vec_t *dst_vert = tess.vertices;
    int count = mesh->numverts;
    vec3_t normal;
    vec_t d;

    while (count--) {
        d = shadedot(get_lerped_normal(normal, src_oldvert, src_newvert));

        dst_vert[0] =
            src_oldvert->pos[0] * oldscale[0] +
            src_newvert->pos[0] * newscale[0] + translate[0];
        dst_vert[1] =
            src_oldvert->pos[1] * oldscale[1] +
            src_newvert->pos[1] * newscale[1] + translate[1];
        dst_vert[2] =
            src_oldvert->pos[2] * oldscale[2] +
            src_newvert->pos[2] * newscale[2] + translate[2];
        dst_vert[4] = shadelight[0] * d;
        dst_vert[5] = shadelight[1] * d;
        dst_vert[6] = shadelight[2] * d;
        dst_vert[7] = shadelight[3];
        dst_vert += VERTEX_SIZE;

        src_oldvert++;
        src_newvert++;
    }
}

static void tess_lerped_plain(const maliasmesh_t *mesh)
{
    maliasvert_t *src_oldvert = &mesh->verts[oldframenum * mesh->numverts];
    maliasvert_t *src_newvert = &mesh->verts[newframenum * mesh->numverts];
    vec_t *dst_vert = tess.vertices;
    int count = mesh->numverts;

    while (count--) {
        dst_vert[0] =
            src_oldvert->pos[0] * oldscale[0] +
            src_newvert->pos[0] * newscale[0] + translate[0];
        dst_vert[1] =
            src_oldvert->pos[1] * oldscale[1] +
            src_newvert->pos[1] * newscale[1] + translate[1];
        dst_vert[2] =
            src_oldvert->pos[2] * oldscale[2] +
            src_newvert->pos[2] * newscale[2] + translate[2];
        dst_vert += 4;

        src_oldvert++;
        src_newvert++;
    }
}

static glCullResult_t cull_static_model(model_t *model)
{
    maliasframe_t *newframe = &model->frames[newframenum];
    vec3_t bounds[2];
    glCullResult_t cull;

    if (glr.entrotated) {
        cull = GL_CullSphere(origin, newframe->radius);
        if (cull == CULL_OUT) {
            c.spheresCulled++;
            return cull;
        }
        if (cull == CULL_CLIP) {
            cull = GL_CullLocalBox(origin, newframe->bounds);
            if (cull == CULL_OUT) {
                c.rotatedBoxesCulled++;
                return cull;
            }
        }
    } else {
        VectorAdd(newframe->bounds[0], origin, bounds[0]);
        VectorAdd(newframe->bounds[1], origin, bounds[1]);
        cull = GL_CullBox(bounds);
        if (cull == CULL_OUT) {
            c.boxesCulled++;
            return cull;
        }
    }

    VectorCopy(newframe->scale, newscale);
    VectorCopy(newframe->translate, translate);

    return cull;
}

static glCullResult_t cull_lerped_model(model_t *model)
{
    maliasframe_t *newframe = &model->frames[newframenum];
    maliasframe_t *oldframe = &model->frames[oldframenum];
    vec3_t bounds[2];
    vec_t radius;
    glCullResult_t cull;

    if (glr.entrotated) {
        radius = newframe->radius > oldframe->radius ?
                 newframe->radius : oldframe->radius;
        cull = GL_CullSphere(origin, radius);
        if (cull == CULL_OUT) {
            c.spheresCulled++;
            return cull;
        }
        UnionBounds(newframe->bounds, oldframe->bounds, bounds);
        if (cull == CULL_CLIP) {
            cull = GL_CullLocalBox(origin, bounds);
            if (cull == CULL_OUT) {
                c.rotatedBoxesCulled++;
                return cull;
            }
        }
    } else {
        UnionBounds(newframe->bounds, oldframe->bounds, bounds);
        VectorAdd(bounds[0], origin, bounds[0]);
        VectorAdd(bounds[1], origin, bounds[1]);
        cull = GL_CullBox(bounds);
        if (cull == CULL_OUT) {
            c.boxesCulled++;
            return cull;
        }
    }

    VectorScale(oldframe->scale, backlerp, oldscale);
    VectorScale(newframe->scale, frontlerp, newscale);

    LerpVector2(oldframe->translate, newframe->translate,
                backlerp, frontlerp, translate);

    return cull;
}

static void setup_color(void)
{
    int flags = glr.ent->flags;
    float f, m;
    int i;

    memset(&glr.lightpoint, 0, sizeof(glr.lightpoint));

    if (flags & RF_SHELL_MASK) {
        VectorClear(color);
        if (flags & RenderEffects::HalfDamShell) {
            color[0] = 0.56f;
            color[1] = 0.59f;
            color[2] = 0.45f;
        }
        if (flags & RenderEffects::DoubleShell) {
            color[0] = 0.9f;
            color[1] = 0.7f;
        }
        if (flags & RenderEffects::RedShell) {
            color[0] = 1;
        }
        if (flags & RenderEffects::GreenShell) {
            color[1] = 1;
        }
        if (flags & RenderEffects::BlueShell) {
            color[2] = 1;
        }
    } else if (flags & RenderEffects::FullBright) {
        VectorSet(color, 1, 1, 1);
    } else if ((flags & RenderEffects::InfraRedVisible) && (glr.fd.rdflags & RDF_IRGOGGLES)) {
        VectorSet(color, 1, 0, 0);
    } else {
        // MATHLIB: Quick workaround.
        vec3_t tempColor = { color[0], color[1], color[2] };
        GL_LightPoint(origin, tempColor);
        color[0] = tempColor[0];
        color[1] = tempColor[1];
        color[2] = tempColor[2];

        if (flags & RenderEffects::MinimalLight) {
            for (i = 0; i < 3; i++) {
                if (color[i] > 0.1f) {
                    break;
                }
            }
            if (i == 3) {
                VectorSet(color, 0.1f, 0.1f, 0.1f);
            }
        }

        if (flags & RenderEffects::Glow) {
            f = 0.1f * std::sinf(glr.fd.time * 7);
            for (i = 0; i < 3; i++) {
                m = color[i] * 0.8f;
                color[i] += f;
                if (color[i] < m)
                    color[i] = m;
            }
        }

        for (i = 0; i < 3; i++) {
            clamp(color[i], 0, 1);
        }
    }

    if (flags & RenderEffects::Translucent) {
        color[3] = glr.ent->alpha;
    } else {
        color[3] = 1;
    }
}

static void setup_celshading(void)
{
    float value = Cvar_ClampValue(gl_celshading, 0, 10);
    vec3_t dir;

    celscale = 0;

    if (value == 0)
        return;

    if (glr.ent->flags & (RenderEffects::Translucent | RF_SHELL_MASK))
        return;

    VectorSubtract(origin, glr.fd.vieworg, dir);
    celscale = 1.0f - VectorLength(dir) / 700.0f;
}

static void draw_celshading(maliasmesh_t *mesh)
{
    if (celscale < 0.01f || celscale > 1)
        return;

    GL_BindTexture(0, TEXNUM_BLACK);
    GL_StateBits(GLS_BLEND_BLEND);
    GL_ArrayBits(GLA_VERTEX);

    qglLineWidth(gl_celshading->value * celscale);
    qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    qglCullFace(GL_FRONT);
    qglColor4f(0, 0, 0, color[3] * celscale);
    qglDrawElements(GL_TRIANGLES, mesh->numindices, QGL_INDEX_ENUM,
                    mesh->indices);
    qglCullFace(GL_BACK);
    qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    qglLineWidth(1);
}

static void setup_shadow(void)
{
    GLfloat matrix[16], tmp[16];
    cplane_t *plane;
    vec3_t dir;

    shadowmatrix[15] = 0;

    if (!gl_shadows->integer)
        return;

    if (glr.ent->flags & (RenderEffects::WeaponModel | RF_NOSHADOW))
        return;

    if (!glr.lightpoint.surf)
        return;

    // position fake light source straight over the model
    if (glr.lightpoint.surf->drawflags & DSURF_PLANEBACK)
        VectorSet(dir, 0, 0, -1);
    else
        VectorSet(dir, 0, 0, 1);

    // project shadow on ground plane
    plane = &glr.lightpoint.plane;

    matrix[0] = plane->normal[1] * dir[1] + plane->normal[2] * dir[2];
    matrix[4] = -plane->normal[1] * dir[0];
    matrix[8] = -plane->normal[2] * dir[0];
    matrix[12] = plane->dist * dir[0];

    matrix[1] = -plane->normal[0] * dir[1];
    matrix[5] = plane->normal[0] * dir[0] + plane->normal[2] * dir[2];
    matrix[9] = -plane->normal[2] * dir[1];
    matrix[13] = plane->dist * dir[1];

    matrix[2] = -plane->normal[0] * dir[2];
    matrix[6] = -plane->normal[1] * dir[2];
    matrix[10] = plane->normal[0] * dir[0] + plane->normal[1] * dir[1];
    matrix[14] = plane->dist * dir[2];

    matrix[3] = 0;
    matrix[7] = 0;
    matrix[11] = 0;
    matrix[15] = DotProduct(plane->normal, dir);

    GL_MultMatrix(tmp, glr.viewmatrix, matrix);

    // rotate for entity
    matrix[0] = glr.entaxis[0][0];
    matrix[4] = glr.entaxis[1][0];
    matrix[8] = glr.entaxis[2][0];
    matrix[12] = origin[0];

    matrix[1] = glr.entaxis[0][1];
    matrix[5] = glr.entaxis[1][1];
    matrix[9] = glr.entaxis[2][1];
    matrix[13] = origin[1];

    matrix[2] = glr.entaxis[0][2];
    matrix[6] = glr.entaxis[1][2];
    matrix[10] = glr.entaxis[2][2];
    matrix[14] = origin[2];

    matrix[3] = 0;
    matrix[7] = 0;
    matrix[11] = 0;
    matrix[15] = 1;

    GL_MultMatrix(shadowmatrix, tmp, matrix);
}

static void draw_shadow(maliasmesh_t *mesh)
{
    if (shadowmatrix[15] < 0.5f)
        return;

    // load shadow projection matrix
    GL_LoadMatrix(shadowmatrix);

    // eliminate z-fighting by utilizing stencil buffer, if available
    if (gl_config.stencilbits) {
        qglEnable(GL_STENCIL_TEST);
        qglStencilFunc(GL_EQUAL, 0, 0xff);
        qglStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
    }

    GL_StateBits(GLS_BLEND_BLEND);
    GL_BindTexture(0, TEXNUM_WHITE);
    GL_ArrayBits(GLA_VERTEX);

    qglEnable(GL_POLYGON_OFFSET_FILL);
    qglPolygonOffset(-1.0f, -2.0f);
    qglColor4f(0, 0, 0, color[3] * 0.5f);
    qglDrawElements(GL_TRIANGLES, mesh->numindices, QGL_INDEX_ENUM,
                    mesh->indices);
    qglDisable(GL_POLYGON_OFFSET_FILL);

    // once we have drawn something to stencil buffer, continue to clear it for
    // the lifetime of OpenGL context. leaving stencil buffer "dirty" and
    // clearing just depth is slower (verified for Nvidia and ATI drivers).
    if (gl_config.stencilbits) {
        qglDisable(GL_STENCIL_TEST);
        gl_static.stencil_buffer_bit |= GL_STENCIL_BUFFER_BIT;
    }
}

static int texnum_for_mesh(maliasmesh_t *mesh)
{
    r_entity_t *ent = glr.ent;

    if (ent->flags & RF_SHELL_MASK)
        return TEXNUM_WHITE;

    if (ent->skin)
        return IMG_ForHandle(ent->skin)->texnum;

    if (!mesh->numskins)
        return TEXNUM_DEFAULT;

    if (ent->skinNumber < 0 || ent->skinNumber >= mesh->numskins) {
        Com_DPrintf("%s: no such skin: %d\n", "GL_DrawAliasModel", ent->skinNumber);
        return mesh->skins[0]->texnum;
    }

    if (mesh->skins[ent->skinNumber]->texnum == TEXNUM_DEFAULT)
        return mesh->skins[0]->texnum;

    return mesh->skins[ent->skinNumber]->texnum;
}

static void draw_alias_mesh(maliasmesh_t *mesh)
{
    glStateBits_t state = GLS_DEFAULT;

    // fall back to entity matrix
    GL_LoadMatrix(glr.entmatrix);

    if (shadelight)
        state = (glStateBits_t)(state | GLS_SHADE_SMOOTH); // CPP: Bitflag cast

    if (glr.ent->flags & RenderEffects::Translucent)
        state = (glStateBits_t)(state | GLS_BLEND_BLEND | GLS_DEPTHMASK_FALSE); // CPP: Bitflag cast

    GL_StateBits(state);

    GL_BindTexture(0, texnum_for_mesh(mesh));

    (*tessfunc)(mesh);
    c.trisDrawn += mesh->numtris;

    if (shadelight) {
        GL_ArrayBits((glArrayBits_t)(GLA_VERTEX | GLA_TC | GLA_COLOR)); // CPP: Cast
        GL_VertexPointer(3, VERTEX_SIZE, tess.vertices);
        GL_ColorFloatPointer(4, VERTEX_SIZE, tess.vertices + 4);
    } else {
        GL_ArrayBits((glArrayBits_t)(GLA_VERTEX | GLA_TC)); // CPP: Cast
        GL_VertexPointer(3, 4, tess.vertices);
        qglColor4fv(color);
    }

    GL_TexCoordPointer(2, 0, (GLfloat *)mesh->tcoords);

    GL_LockArrays(mesh->numverts);

    qglDrawElements(GL_TRIANGLES, mesh->numindices, QGL_INDEX_ENUM,
                    mesh->indices);

    draw_celshading(mesh);

    if (gl_showtris->integer) {
        GL_EnableOutlines();
        qglDrawElements(GL_TRIANGLES, mesh->numindices, QGL_INDEX_ENUM,
                        mesh->indices);
        GL_DisableOutlines();
    }

    // FIXME: unlock arrays before changing matrix?
    draw_shadow(mesh);

    GL_UnlockArrays();
}

void GL_DrawAliasModel(model_t *model)
{
    r_entity_t *ent = glr.ent;
    glCullResult_t cull;
    int i;

    newframenum = ent->frame;
    if (newframenum < 0 || newframenum >= model->numframes) {
        Com_DPrintf("%s: no such frame %d\n", __func__, newframenum);
        newframenum = 0;
    }

    oldframenum = ent->oldframe;
    if (oldframenum < 0 || oldframenum >= model->numframes) {
        Com_DPrintf("%s: no such oldframe %d\n", __func__, oldframenum);
        oldframenum = 0;
    }

    backlerp = ent->backlerp;
    frontlerp = 1.0f - backlerp;

    // optimized case
    if (backlerp == 0)
        oldframenum = newframenum;

    // interpolate origin, if necessarry
    if (ent->flags & RenderEffects::FrameLerp)
        LerpVector2(ent->oldorigin, ent->origin,
                    backlerp, frontlerp, origin);
    else
        VectorCopy(ent->origin, origin);

    // cull the model, setup scale and translate vectors
    if (newframenum == oldframenum)
        cull = cull_static_model(model);
    else
        cull = cull_lerped_model(model);
    if (cull == CULL_OUT)
        return;

    // setup parameters common for all meshes
    setup_color();
    setup_celshading();
    setup_dotshading();
    setup_shadow();

    // select proper tessfunc
    if (ent->flags & RF_SHELL_MASK) {
        shellscale = (ent->flags & RenderEffects::WeaponModel) ?
            WEAPONSHELL_SCALE : POWERSUIT_SCALE;
        tessfunc = newframenum == oldframenum ?
            tess_static_shell : tess_lerped_shell;
    } else if (shadelight) {
        tessfunc = newframenum == oldframenum ?
            tess_static_shade : tess_lerped_shade;
    } else {
        tessfunc = newframenum == oldframenum ?
            tess_static_plain : tess_lerped_plain;
    }

	float scale = 1.f;
	if (ent->scale > 0.f)
		scale = ent->scale;

    GL_RotateForEntity(origin, scale);

    if ((ent->flags & (RenderEffects::WeaponModel | RF_LEFTHAND)) ==
        (RenderEffects::WeaponModel | RF_LEFTHAND)) {
        qglMatrixMode(GL_PROJECTION);
        qglScalef(-1, 1, 1);
        qglMatrixMode(GL_MODELVIEW);
        qglFrontFace(GL_CCW);
    }

    if (ent->flags & RenderEffects::DepthHack)
        qglDepthRange(0, 0.25f);

    // draw all the meshes
    for (i = 0; i < model->nummeshes; i++)
        draw_alias_mesh(&model->meshes[i]);

    if (ent->flags & RenderEffects::DepthHack)
        qglDepthRange(0, 1);

    if ((ent->flags & (RenderEffects::WeaponModel | RF_LEFTHAND)) ==
        (RenderEffects::WeaponModel | RF_LEFTHAND)) {
        qglMatrixMode(GL_PROJECTION);
        qglScalef(-1, 1, 1);
        qglMatrixMode(GL_MODELVIEW);
        qglFrontFace(GL_CW);
    }
}

