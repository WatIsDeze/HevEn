// ===========================================================================
// 
// HevEn (C) 2017 by Hevedy <https://github.com/Hevedy>
// 
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, 
// You can obtain one at https://mozilla.org/MPL/2.0/.
// 
// ---------------------------------------------------------------------------
// 
// Sauerbraten game engine source code, any release.
// Tesseract game engine source code, any release.
// 
// Copyright (C) 2001-2017 Wouter van Oortmerssen, Lee Salzman, Mike Dysart, Robert Pointon, and Quinton Reeves
// Copyright (C) 2001-2017 Wouter van Oortmerssen, Lee Salzman, Mike Dysart, Robert Pointon, Quinton Reeves, and Benjamin Segovia
// 
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// 
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
// 
// ---------------------------------------------------------------------------
//  File:			MD3.h
//  Description: 	MD3 models header
// ---------------------------------------------------------------------------
//  Log:			Source.
//
//
// ===========================================================================


struct md3;

struct md3frame
{
    vec bbmin, bbmax, origin;
    float radius;
    uchar name[16];
};

struct md3tag
{
    char name[64];
    float translation[3];
    float rotation[3][3];
};

struct md3vertex
{
    short vertex[3];
    short normal;
};

struct md3triangle
{
    int vertexindices[3];
};

struct md3header
{
    char id[4];
    int version;
    char name[64];
    int flags;
    int numframes, numtags, nummeshes, numskins;
    int ofs_frames, ofs_tags, ofs_meshes, ofs_eof; // offsets
};

struct md3meshheader
{
    char id[4];
    char name[64];
    int flags;
    int numframes, numshaders, numvertices, numtriangles;
    int ofs_triangles, ofs_shaders, ofs_uv, ofs_vertices, meshsize; // offsets
};

struct md3 : vertmodel, vertloader<md3>
{
    md3(const char *name) : vertmodel(name) {}

    static const char *formatname() { return "md3"; }
    int type() const { return MDL_MD3; }

    struct md3meshgroup : vertmeshgroup
    {
        bool load(const char *path, float smooth)
        {
            stream *f = openfile(path, "rb");
            if(!f) return false;
            md3header header;
            f->read(&header, sizeof(md3header));
            lilswap(&header.version, 1);
            lilswap(&header.flags, 9);
            if(strncmp(header.id, "IDP3", 4) != 0 || header.version != 15) // header check
            {
                delete f;
                conoutf("md3: corrupted header");
                return false;
            }

            name = newstring(path);

            numframes = header.numframes;

            int mesh_offset = header.ofs_meshes;
            loopi(header.nummeshes)
            {
                vertmesh &m = *new vertmesh;
                m.group = this;
                meshes.add(&m);

                md3meshheader mheader;
                f->seek(mesh_offset, SEEK_SET);
                f->read(&mheader, sizeof(md3meshheader));
                lilswap(&mheader.flags, 10);

                m.name = newstring(mheader.name);

                m.numtris = mheader.numtriangles;
                m.tris = new tri[m.numtris];
                f->seek(mesh_offset + mheader.ofs_triangles, SEEK_SET);
                loopj(m.numtris)
                {
                    md3triangle tri;
                    f->read(&tri, sizeof(md3triangle)); // read the triangles
                    lilswap(tri.vertexindices, 3);
                    loopk(3) m.tris[j].vert[k] = (ushort)tri.vertexindices[k];
                }

                m.numverts = mheader.numvertices;
                m.tcverts = new tcvert[m.numverts];
                f->seek(mesh_offset + mheader.ofs_uv , SEEK_SET);
                f->read(m.tcverts, m.numverts*2*sizeof(float)); // read the UV data
                lilswap(&m.tcverts[0].tc.x, 2*m.numverts);

                m.verts = new vert[numframes*m.numverts];
                f->seek(mesh_offset + mheader.ofs_vertices, SEEK_SET);
                loopj(numframes*m.numverts)
                {
                    md3vertex v;
                    f->read(&v, sizeof(md3vertex)); // read the vertices
                    lilswap(v.vertex, 4);

                    m.verts[j].pos = vec(v.vertex[0]/64.0f, -v.vertex[1]/64.0f, v.vertex[2]/64.0f);

                    float lng = (v.normal&0xFF)*2*M_PI/255.0f; // decode vertex normals
                    float lat = ((v.normal>>8)&0xFF)*2*M_PI/255.0f;
                    m.verts[j].norm = vec(cosf(lat)*sinf(lng), -sinf(lat)*sinf(lng), cosf(lng));
                }

                m.calctangents();

                mesh_offset += mheader.meshsize;
            }

            numtags = header.numtags;
            if(numtags)
            {
                tags = new tag[numframes*numtags];
                f->seek(header.ofs_tags, SEEK_SET);
                md3tag tag;

                loopi(header.numframes*header.numtags)
                {
                    f->read(&tag, sizeof(md3tag));
                    lilswap(tag.translation, 12);
                    if(tag.name[0] && i<header.numtags) tags[i].name = newstring(tag.name);
                    matrix4x3 &m = tags[i].matrix;
                    tag.translation[1] *= -1;
                    // undo the -y
                    loopj(3) tag.rotation[1][j] *= -1;
                    // then restore it
                    loopj(3) tag.rotation[j][1] *= -1;
                    m.a = vec(tag.rotation[0]);
                    m.b = vec(tag.rotation[1]);
                    m.c = vec(tag.rotation[2]);
                    m.d = vec(tag.translation);
                }
            }

            delete f;
            return true;
        }
    };

    vertmeshgroup *newmeshes() { return new md3meshgroup; }

    bool loaddefaultparts()
    {
        const char *pname = parentdir(name);
        part &mdl = addpart();
        defformatstring(name1, "Game/Data/Meshes/%s/tris.md3", name);
        mdl.meshes = sharemeshes(path(name1));
        if(!mdl.meshes)
        {
            defformatstring(name2, "Game/Data/Meshes/%s/tris.md3", pname);    // try md3 in parent folder (vert sharing)
            mdl.meshes = sharemeshes(path(name2));
            if(!mdl.meshes) return false;
        }
        Texture *tex, *masks;
        loadskin(name, pname, tex, masks);
        mdl.initskins(tex, masks);
        if(tex==notexture) conoutf("could not load model skin for %s", name1);
        return true;
    }

    bool load()
    {
        formatstring(dir, "Game/Data/Meshes/%s", name);
        defformatstring(cfgname, "Game/Data/Meshes/%s/DefMD3.hed", name);

        loading = this;
        identflags &= ~IDF_PERSIST;
        if(execfile(cfgname, false) && parts.length()) // configured md3, will call the md3* commands below
        {
            identflags |= IDF_PERSIST;
            loading = NULL;
            loopv(parts) if(!parts[i]->meshes) return false;
        }
        else // md3 without configuration, try default tris and skin
        {
            identflags |= IDF_PERSIST;
            loading = NULL;
            if(!loaddefaultparts()) return false;
        }
        translate.y = -translate.y;
        loaded();
        return true;
    }
};

vertcommands<md3> md3commands;

