/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * SoundFont file loading code borrowed from Smurf SoundFont Editor
 * Copyright (C) 1999-2001 Josh Green
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */


#include "fluid_sffile.h"
#include "fluid_sfont.h"
#include "fluid_sys.h"

/*=================================sfload.c========================
  Borrowed from Smurf SoundFont Editor by Josh Green
  =================================================================*/

/*
   functions for loading data from sfont files, with appropriate byte swapping
   on big endian machines. Sfont IDs are not swapped because the ID read is
   equivalent to the matching ID list in memory regardless of LE/BE machine
*/

/* sf file chunk IDs */
enum
{
    UNKN_ID,
    RIFF_ID,
    LIST_ID,
    SFBK_ID,
    INFO_ID,
    SDTA_ID,
    PDTA_ID, /* info/sample/preset */

    IFIL_ID,
    ISNG_ID,
    INAM_ID,
    IROM_ID, /* info ids (1st byte of info strings) */
    IVER_ID,
    ICRD_ID,
    IENG_ID,
    IPRD_ID, /* more info ids */
    ICOP_ID,
    ICMT_ID,
    ISFT_ID, /* and yet more info ids */

    SNAM_ID,
    SMPL_ID, /* sample ids */
    PHDR_ID,
    PBAG_ID,
    PMOD_ID,
    PGEN_ID, /* preset ids */
    IHDR_ID,
    IBAG_ID,
    IMOD_ID,
    IGEN_ID, /* instrument ids */
    SHDR_ID, /* sample info */
    SM24_ID
};

static const char idlist[] = {"RIFFLISTsfbkINFOsdtapdtaifilisngINAMiromiverICRDIENGIPRD"
                              "ICOPICMTISFTsnamsmplphdrpbagpmodpgeninstibagimodigenshdrsm24"};


/* generator types */
typedef enum {
    Gen_StartAddrOfs,
    Gen_EndAddrOfs,
    Gen_StartLoopAddrOfs,
    Gen_EndLoopAddrOfs,
    Gen_StartAddrCoarseOfs,
    Gen_ModLFO2Pitch,
    Gen_VibLFO2Pitch,
    Gen_ModEnv2Pitch,
    Gen_FilterFc,
    Gen_FilterQ,
    Gen_ModLFO2FilterFc,
    Gen_ModEnv2FilterFc,
    Gen_EndAddrCoarseOfs,
    Gen_ModLFO2Vol,
    Gen_Unused1,
    Gen_ChorusSend,
    Gen_ReverbSend,
    Gen_Pan,
    Gen_Unused2,
    Gen_Unused3,
    Gen_Unused4,
    Gen_ModLFODelay,
    Gen_ModLFOFreq,
    Gen_VibLFODelay,
    Gen_VibLFOFreq,
    Gen_ModEnvDelay,
    Gen_ModEnvAttack,
    Gen_ModEnvHold,
    Gen_ModEnvDecay,
    Gen_ModEnvSustain,
    Gen_ModEnvRelease,
    Gen_Key2ModEnvHold,
    Gen_Key2ModEnvDecay,
    Gen_VolEnvDelay,
    Gen_VolEnvAttack,
    Gen_VolEnvHold,
    Gen_VolEnvDecay,
    Gen_VolEnvSustain,
    Gen_VolEnvRelease,
    Gen_Key2VolEnvHold,
    Gen_Key2VolEnvDecay,
    Gen_Instrument,
    Gen_Reserved1,
    Gen_KeyRange,
    Gen_VelRange,
    Gen_StartLoopAddrCoarseOfs,
    Gen_Keynum,
    Gen_Velocity,
    Gen_Attenuation,
    Gen_Reserved2,
    Gen_EndLoopAddrCoarseOfs,
    Gen_CoarseTune,
    Gen_FineTune,
    Gen_SampleId,
    Gen_SampleModes,
    Gen_Reserved3,
    Gen_ScaleTune,
    Gen_ExclusiveClass,
    Gen_OverrideRootKey,
    Gen_Dummy
} Gen_Type;

#define Gen_MaxValid Gen_Dummy - 1 /* maximum valid generator */
#define Gen_Count Gen_Dummy /* count of generators */
#define GenArrSize sizeof(SFGenAmount) * Gen_Count /* gen array size */


static const unsigned short invalid_inst_gen[] = {
    Gen_Unused1,
    Gen_Unused2,
    Gen_Unused3,
    Gen_Unused4,
    Gen_Reserved1,
    Gen_Reserved2,
    Gen_Reserved3,
    0
};

static const unsigned short invalid_preset_gen[] = {
    Gen_StartAddrOfs,
    Gen_EndAddrOfs,
    Gen_StartLoopAddrOfs,
    Gen_EndLoopAddrOfs,
    Gen_StartAddrCoarseOfs,
    Gen_EndAddrCoarseOfs,
    Gen_StartLoopAddrCoarseOfs,
    Gen_Keynum,
    Gen_Velocity,
    Gen_EndLoopAddrCoarseOfs,
    Gen_SampleModes,
    Gen_ExclusiveClass,
    Gen_OverrideRootKey,
    0
};


#define CHNKIDSTR(id) &idlist[(id - 1) * 4]

/* sfont file chunk sizes */
#define SF_PHDR_SIZE (38)
#define SF_BAG_SIZE  (4)
#define SF_MOD_SIZE  (10)
#define SF_GEN_SIZE  (4)
#define SF_IHDR_SIZE (22)
#define SF_SHDR_SIZE (46)


#define READCHUNK(sf, var)                                                  \
    do                                                                      \
    {                                                                       \
        if (sf->fcbs->fread(var, 8, sf->sffd) == FLUID_FAILED)              \
            return FALSE;                                                   \
        ((SFChunk *)(var))->size = FLUID_LE32TOH(((SFChunk *)(var))->size); \
    } while (0)

#define READD(sf, var)                                            \
    do                                                            \
    {                                                             \
        uint32_t _temp;                                           \
        if (sf->fcbs->fread(&_temp, 4, sf->sffd) == FLUID_FAILED) \
            return FALSE;                                         \
        var = FLUID_LE32TOH(_temp);                               \
    } while (0)

#define READW(sf, var)                                            \
    do                                                            \
    {                                                             \
        uint16_t _temp;                                           \
        if (sf->fcbs->fread(&_temp, 2, sf->sffd) == FLUID_FAILED) \
            return FALSE;                                         \
        var = FLUID_LE16TOH(_temp);                               \
    } while (0)

#define READID(sf, var)                                        \
    do                                                         \
    {                                                          \
        if (sf->fcbs->fread(var, 4, sf->sffd) == FLUID_FAILED) \
            return FALSE;                                      \
    } while (0)

#define READSTR(sf, var)                                        \
    do                                                          \
    {                                                           \
        if (sf->fcbs->fread(var, 20, sf->sffd) == FLUID_FAILED) \
            return FALSE;                                       \
        (*var)[20] = '\0';                                      \
    } while (0)

#define READB(sf, var)                                          \
    do                                                          \
    {                                                           \
        if (sf->fcbs->fread(&var, 1, sf->sffd) == FLUID_FAILED) \
            return FALSE;                                       \
    } while (0)

#define FSKIP(sf, size)                                                \
    do                                                                 \
    {                                                                  \
        if (sf->fcbs->fseek(sf->sffd, size, SEEK_CUR) == FLUID_FAILED) \
            return FALSE;                                              \
    } while (0)

#define FSEEK(sf, pos)                                                 \
    do                                                                 \
    {                                                                  \
        if (sf->fcbs->fseek(sf->sffd, pos, SEEK_SET) == FLUID_FAILED)  \
            return FALSE;                                              \
    } while (0)


static int load_body(SFData *sf, unsigned int size);
static int read_info_subchunks(SFData *sf, int size);
static int process_sdta(SFData *sf, unsigned int size);
static int process_pdta(SFData *sf, int size);

static int load_preset_headers(SFData *sf, int start_idx, int num_records);
static int load_preset_zones(SFData *sf, fluid_list_t *preset_list, int num_presets);
static int load_preset_modulators(SFData *sf, fluid_list_t *preset_list, int num_presets);
static int load_preset_generators(SFData *sf, fluid_list_t *preset_list, int num_presets);

static int load_inst_headers(SFData *sf, int start_idx, int num_records);
static int load_inst_zones(SFData *sf, fluid_list_t *inst_list, int num_insts);
static int load_inst_modulators(SFData *sf, fluid_list_t *inst_list, int num_insts);
static int load_inst_generators(SFData *sf, fluid_list_t *inst_list, int num_insts);

static int load_sample_headers(SFData *sf, int start_idx, int num_records);

static int load_all_presets(SFData *sf);

static int fixup_preset_zones(SFData *sf, fluid_list_t *preset_list, int num_presets);
static int fixup_inst_zones(SFData *sf, fluid_list_t *inst_list, int num_insts);
static int fixup_samples(SFData *sf, fluid_list_t *sample_list, int num_samples);

static unsigned int chunkid(unsigned int id);
static int read_listchunk(SFData *sf, SFChunk *chunk);
static int pdtahelper(SFData *sf, unsigned int chunk_id, unsigned int record_size, int min_record_count,
        int *chunk_pos, int *record_count, int *available_size);
static fluid_list_t *find_gen_by_id(int genid, fluid_list_t *genlist);
static int valid_inst_genid(unsigned short genid);
static int valid_preset_genid(unsigned short genid);

static void delete_preset(SFPreset *preset);
static void delete_inst(SFInst *inst);
static void delete_preset_zone(SFPresetZone *zone);
static void delete_inst_zone(SFInstZone *zone);

/*
 * Open a SoundFont file and parse it's contents into a SFData structure.
 *
 * @param fname filename
 * @param fcbs file callback structure
 * @return the parsed SoundFont as SFData structure or NULL on error
 */
SFData *fluid_sffile_load(const char *fname, const fluid_file_callbacks_t *fcbs)
{
    SFData *sf;
    int fsize = 0;

    if (!(sf = FLUID_NEW(SFData)))
    {
        FLUID_LOG(FLUID_ERR, "Out of memory");
        return NULL;
    }
    FLUID_MEMSET(sf, 0, sizeof(SFData));

    sf->fcbs = fcbs;

    if ((sf->sffd = fcbs->fopen(fname)) == NULL)
    {
        FLUID_LOG(FLUID_ERR, _("Unable to open file \"%s\""), fname);
        goto error_exit;
    }

    sf->fname = FLUID_STRDUP(fname);
    if (sf->fname == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Out of memory");
        goto error_exit;
    }

    /* get size of file by seeking to end */
    if (fcbs->fseek(sf->sffd, 0L, SEEK_END) == FLUID_FAILED)
    {
        FLUID_LOG(FLUID_ERR, _("Seek to end of file failed"));
        goto error_exit;
    }
    if ((fsize = fcbs->ftell(sf->sffd)) == FLUID_FAILED)
    {
        FLUID_LOG(FLUID_ERR, _("Get end of file position failed"));
        goto error_exit;
    }

    if (fcbs->fseek(sf->sffd, 0, SEEK_SET) == FLUID_FAILED)
    {
        FLUID_LOG(FLUID_ERR, _("Rewind to start of file failed"));
        goto error_exit;
    }

    if (!load_body(sf, fsize))
    {
        goto error_exit;
    }

    return sf;

error_exit:
    fluid_sffile_close(sf);
    return NULL;
}

/*
 * Close a SoundFont file and free the SFData structure.
 *
 * @param sf pointer to SFData structure
 * @param fcbs file callback structure
 */
void fluid_sffile_close(SFData *sf)
{
    fluid_list_t *entry;
    SFPreset *preset;
    SFInst *inst;

    if (sf->sffd)
    {
        sf->fcbs->fclose(sf->sffd);
    }

    FLUID_FREE(sf->fname);

    entry = sf->info;
    while(entry)
    {
        FLUID_FREE(fluid_list_get(entry));
        entry = fluid_list_next(entry);
    }
    delete_fluid_list(sf->info);

    entry = sf->preset;
    while(entry)
    {
        preset = (SFPreset *)fluid_list_get(entry);
        delete_preset(preset);
        entry = fluid_list_next(entry);
    }
    delete_fluid_list(sf->preset);

    entry = sf->inst;
    while(entry)
    {
        inst = (SFInst *)fluid_list_get(entry);
        delete_inst(inst);
        entry = fluid_list_next(entry);
    }
    delete_fluid_list(sf->inst);

    entry = sf->sample;
    while(entry)
    {
        FLUID_FREE(fluid_list_get(entry));
        entry = fluid_list_next(entry);
    }
    delete_fluid_list(sf->sample);

    FLUID_FREE(sf);
}

/*
 * Private functions
 */

/* sound font file load functions */
static unsigned int chunkid(unsigned int id)
{
    unsigned int i;
    unsigned int *p;

    p = (unsigned int *)&idlist;
    for (i = 0; i < sizeof(idlist) / sizeof(int); i++, p += 1)
        if (*p == id)
            return (i + 1);

    return UNKN_ID;
}

static int load_body(SFData *sf, unsigned int filesize)
{
    SFChunk chunk;

    /* load RIFF chunk */
    READCHUNK(sf, &chunk);
    if (chunkid(chunk.id) != RIFF_ID)
    {
        FLUID_LOG(FLUID_ERR, _("Not a RIFF file"));
        return FALSE;
    }

    /* load file ID */
    READID(sf, &chunk.id);
    if (chunkid(chunk.id) != SFBK_ID)
    {
        FLUID_LOG(FLUID_ERR, _("Not a SoundFont file"));
        return FALSE;
    }

    if (chunk.size != filesize - 8)
    {
        FLUID_LOG(FLUID_ERR, _("SoundFont file size mismatch"));
        return FALSE;
    }

    /* Read all info subchunks */
    if (!read_listchunk(sf, &chunk))
        return FALSE;
    if (chunkid(chunk.id) != INFO_ID)
    {
        FLUID_LOG(FLUID_ERR, _("Invalid ID found when expecting INFO chunk"));
        return FALSE;
    }
    if (!read_info_subchunks(sf, chunk.size))
        return FALSE;

    /* Record positions of sample data (16 and optionally 24 bit) */
    if (!read_listchunk(sf, &chunk))
        return FALSE;
    if (chunkid(chunk.id) != SDTA_ID)
    {
        FLUID_LOG(FLUID_ERR, _("Invalid ID found when expecting SAMPLE chunk"));
        return FALSE;
    }
    if (!process_sdta(sf, chunk.size))
        return FALSE;

    /* Record positions and length of all HYDRA subchunks */
    if (!read_listchunk(sf, &chunk))
        return FALSE;
    if (chunkid(chunk.id) != PDTA_ID)
    {
        FLUID_LOG(FLUID_ERR, _("Invalid ID found when expecting HYDRA chunk"));
        return FALSE;
    }
    if (!process_pdta(sf, chunk.size))
        return FALSE;

    /* Read all preset headers */
    if (!load_preset_headers(sf, 0, sf->phdr_count - 1))
        return FALSE;

    /* Load all presets */
    if (!load_all_presets(sf))
        return FALSE;

    return TRUE;
}

static int load_all_presets(SFData *sf)
{
    int num_presets = sf->phdr_count - 1;
    int num_insts = sf->ihdr_count - 1;
    int num_samples = sf->shdr_count - 1;

    if (!load_preset_zones(sf, sf->preset, num_presets))
        return FALSE;

    if (!load_preset_modulators(sf, sf->preset, num_presets))
        return FALSE;

    if (!load_preset_generators(sf, sf->preset, num_presets))
        return FALSE;

    if (!load_inst_headers(sf, 0, num_insts))
        return FALSE;

    if (!load_inst_zones(sf, sf->inst, num_insts))
        return FALSE;

    if (!load_inst_modulators(sf, sf->inst, num_insts))
        return FALSE;

    if (!load_inst_generators(sf, sf->inst, num_insts))
        return FALSE;

    if (!load_sample_headers(sf, 0, num_samples))
        return FALSE;

    if (!fixup_preset_zones(sf, sf->preset, num_presets))
        return FALSE;

    if (!fixup_inst_zones(sf, sf->inst, num_insts))
        return FALSE;

    if (!fixup_samples(sf, sf->sample, num_samples))
        return FALSE;

    return TRUE;
}

static int read_listchunk(SFData *sf, SFChunk *chunk)
{
    READCHUNK(sf, chunk); /* read list chunk */
    if (chunkid(chunk->id) != LIST_ID) /* error if ! list chunk */
    {
        FLUID_LOG(FLUID_ERR, _("Invalid chunk id in level 0 parse"));
        return FALSE;
    }
    READID(sf, &chunk->id); /* read id string */
    chunk->size -= 4;
    return TRUE;
}

static int read_info_subchunks(SFData *sf, int size)
{
    SFChunk chunk;
    unsigned char id;
    char *item;
    unsigned short ver;

    while (size > 0)
    {
        READCHUNK(sf, &chunk);
        size -= 8;

        id = chunkid(chunk.id);

        if (id == IFIL_ID)
        { /* sound font version chunk? */
            if (chunk.size != 4)
            {
                FLUID_LOG(FLUID_ERR, _("Sound font version info chunk has invalid size"));
                return FALSE;
            }

            READW(sf, ver);
            sf->version.major = ver;
            READW(sf, ver);
            sf->version.minor = ver;

            if (sf->version.major < 2)
            {
                FLUID_LOG(FLUID_ERR, _("Sound font version is %d.%d which is not"
                                       " supported, convert to version 2.0x"),
                          sf->version.major, sf->version.minor);
                return FALSE;
            }

            if (sf->version.major == 3)
            {
#if !LIBSNDFILE_SUPPORT
                FLUID_LOG(FLUID_WARN,
                          _("Sound font version is %d.%d but fluidsynth was compiled without"
                            " support for (v3.x)"),
                          sf->version.major, sf->version.minor);
                return FALSE;
#endif
            }
            else if (sf->version.major > 2)
            {
                FLUID_LOG(FLUID_WARN,
                          _("Sound font version is %d.%d which is newer than"
                            " what this version of fluidsynth was designed for (v2.0x)"),
                          sf->version.major, sf->version.minor);
                return FALSE;
            }
        }
        else if (id == IVER_ID)
        { /* ROM version chunk? */
            if (chunk.size != 4)
            {
                FLUID_LOG(FLUID_ERR, _("ROM version info chunk has invalid size"));
                return FALSE;
            }

            READW(sf, ver);
            sf->romver.major = ver;
            READW(sf, ver);
            sf->romver.minor = ver;
        }
        else if (id != UNKN_ID)
        {
            if ((id != ICMT_ID && chunk.size > 256) || (chunk.size > 65536) || (chunk.size % 2))
            {
                FLUID_LOG(FLUID_ERR, _("INFO sub chunk %.4s has invalid chunk size of %d bytes"),
                          &chunk.id, chunk.size);
                return FALSE;
            }

            /* alloc for chunk id and da chunk */
            if (!(item = FLUID_MALLOC(chunk.size + 1)))
            {
                FLUID_LOG(FLUID_ERR, "Out of memory");
                return FALSE;
            }

            /* attach to INFO list, fluid_sffile_close will cleanup if FAIL occurs */
            sf->info = fluid_list_append(sf->info, item);

            *(unsigned char *)item = id;
            if (sf->fcbs->fread(&item[1], chunk.size, sf->sffd) == FLUID_FAILED)
                return FALSE;

            /* force terminate info item (don't forget uint8 info ID) */
            *(item + chunk.size) = '\0';
        }
        else
        {
            FLUID_LOG(FLUID_ERR, _("Invalid chunk id in INFO chunk"));
            return FALSE;
        }
        size -= chunk.size;
    }

    if (size < 0)
    {
        FLUID_LOG(FLUID_ERR, _("INFO chunk size mismatch"));
        return FALSE;
    }

    return TRUE;
}

static int process_sdta(SFData *sf, unsigned int size)
{
    SFChunk chunk;

    if (size == 0)
        return TRUE; /* no sample data? */

    /* read sub chunk */
    READCHUNK(sf, &chunk);
    size -= 8;

    if (chunkid(chunk.id) != SMPL_ID)
    {
        FLUID_LOG(FLUID_ERR, _("Expected SMPL chunk, found invalid id instead"));
        return FALSE;
    }

    /* SDTA chunk may also contain sm24 chunk for 24 bit samples */
    if (chunk.size > size)
    {
        FLUID_LOG(FLUID_ERR, _("SDTA chunk size mismatch"));
        return FALSE;
    }

    /* sample data follows */
    sf->samplepos = sf->fcbs->ftell(sf->sffd);

    /* used in fixup_sample() to check validity of sample headers */
    sf->samplesize = chunk.size;

    FSKIP(sf, chunk.size);
    size -= chunk.size;

    if (sf->version.major >= 2 && sf->version.minor >= 4)
    {
        /* any chance to find another chunk here? */
        if (size > 8)
        {
            /* read sub chunk */
            READCHUNK(sf, &chunk);
            size -= 8;

            if (chunkid(chunk.id) == SM24_ID)
            {
                int sm24size, sdtahalfsize;

                FLUID_LOG(FLUID_DBG, "Found SM24 chunk");
                if (chunk.size > size)
                {
                    FLUID_LOG(FLUID_WARN, "SM24 exeeds SDTA chunk, ignoring SM24");
                    goto ret; // no error
                }

                sdtahalfsize = sf->samplesize / 2;
                /* + 1 byte in the case that half the size of smpl chunk is an odd value */
                sdtahalfsize += sdtahalfsize % 2;
                sm24size = chunk.size;

                if (sdtahalfsize != sm24size)
                {
                    FLUID_LOG(FLUID_WARN, "SM24 not equal to half the size of SMPL chunk (0x%X != "
                                          "0x%X), ignoring SM24",
                              sm24size, sdtahalfsize);
                    goto ret; // no error
                }

                /* sample data24 follows */
                sf->sample24pos = sf->fcbs->ftell(sf->sffd);
                sf->sample24size = sm24size;
            }
        }
    }

ret:
    FSKIP(sf, size);

    return TRUE;
}

static int pdtahelper(SFData *sf, unsigned int chunk_id, unsigned int record_size,
        int min_record_count, int *chunk_pos, int *record_count,
        int *available_size)
{
    SFChunk chunk;

    READCHUNK(sf, &chunk);
    *available_size -= 8;

    if (chunkid(chunk.id) != chunk_id)
    {
        FLUID_LOG(FLUID_ERR, _("Expected PDTA sub-chunk \"%.4s\", found invalid id instead"), CHNKIDSTR(chunk_id));
        return FALSE;
    }

    if (chunk.size % record_size)
    {
        FLUID_LOG(FLUID_ERR, _("\"%.4s\" chunk size is not a multiple of %d bytes"),
                CHNKIDSTR(chunk_id), record_size);
        return FALSE;
    }

    *record_count = chunk.size / record_size;
    if (*record_count < min_record_count)
    {
        FLUID_LOG(FLUID_ERR, _("\"%.4s\" chunk needs to have at least %d record(s)"),
                CHNKIDSTR(chunk_id), min_record_count);
        return FALSE;
    }

    *available_size -= chunk.size;
    if (*available_size < 0)
    {
        FLUID_LOG(FLUID_ERR, _("\"%.4s\" chunk size exceeds remaining PDTA chunk size"),
                CHNKIDSTR(chunk_id));
        return FALSE;
    }

    *chunk_pos = sf->fcbs->ftell(sf->sffd);
    if (*chunk_pos == FLUID_FAILED)
    {
        FLUID_LOG(FLUID_ERR, "Unable to read file position");
        return FALSE;                                              \
    }

    FSKIP(sf, chunk.size);

    return TRUE;
}

/* Record positions of all HYDRA subchunks and do basic sanity checks on the subchunk sizes */
static int process_pdta(SFData *sf, int size)
{

    /* Preset subchunks */
    if (!pdtahelper(sf, PHDR_ID, SF_PHDR_SIZE, 1, &sf->phdr_pos, &sf->phdr_count, &size))
        return FALSE;

    if (!pdtahelper(sf, PBAG_ID, SF_BAG_SIZE, 1, &sf->pbag_pos, &sf->pbag_count, &size))
        return FALSE;

    if (!pdtahelper(sf, PMOD_ID, SF_MOD_SIZE, 0, &sf->pmod_pos, &sf->pmod_count, &size))
        return FALSE;

    if (!pdtahelper(sf, PGEN_ID, SF_GEN_SIZE, 0, &sf->pgen_pos, &sf->pgen_count, &size))
        return FALSE;

    /* Instrument subchunks */
    if (!pdtahelper(sf, IHDR_ID, SF_IHDR_SIZE, 1, &sf->ihdr_pos, &sf->ihdr_count, &size))
        return FALSE;

    if (!pdtahelper(sf, IBAG_ID, SF_BAG_SIZE, 1, &sf->ibag_pos, &sf->ibag_count, &size))
        return FALSE;

    if (!pdtahelper(sf, IMOD_ID, SF_MOD_SIZE, 0, &sf->imod_pos, &sf->imod_count, &size))
        return FALSE;

    if (!pdtahelper(sf, IGEN_ID, SF_GEN_SIZE, 0, &sf->igen_pos, &sf->igen_count, &size))
        return FALSE;

    /* Sample subchunk */
    if (!pdtahelper(sf, SHDR_ID, SF_SHDR_SIZE, 1, &sf->shdr_pos, &sf->shdr_count, &size))
        return FALSE;

    return TRUE;
}

/* Load all preset headers */
static int load_preset_headers(SFData *sf, int start_idx, int num_records)
{
    int i;
    SFPreset *preset;
    SFPreset *prev_preset = NULL;
    unsigned short last_pbag_idx;

    FSEEK(sf, sf->phdr_pos + (start_idx * SF_PHDR_SIZE));

    for (i = 0; i < num_records; i++)
    {
        preset = FLUID_NEW(SFPreset);
        if (preset == NULL)
        {
            FLUID_LOG(FLUID_ERR, "Out of memory");
            return FALSE;
        }
        FLUID_MEMSET(preset, 0, sizeof(SFPreset));

        sf->preset = fluid_list_append(sf->preset, preset);

        READSTR(sf, &preset->name);
        READW(sf, preset->prenum);
        READW(sf, preset->bank);
        READW(sf, preset->pbag_idx);
        READD(sf, preset->libr);
        READD(sf, preset->genre);
        READD(sf, preset->morph);

        if (prev_preset)
        {
            prev_preset->pbag_count = preset->pbag_idx - prev_preset->pbag_idx;
            if (prev_preset->pbag_count < 0)
            {
                FLUID_LOG(FLUID_ERR, _("Preset header indices not monotonic"));
                return FALSE;
            }
        }

        prev_preset = preset;
    }

    if (prev_preset)
    {
        FSKIP(sf, 20 + 2 + 2); /* skip name + prenum + bank */
        READW(sf, last_pbag_idx);
        prev_preset->pbag_count = last_pbag_idx - prev_preset->pbag_idx;
        if (prev_preset->pbag_count < 0)
        {
            FLUID_LOG(FLUID_ERR, _("Preset header indices not monotonic"));
            return FALSE;
        }
    }

    return TRUE;
}


/* Read preset zone headers (pbag) for the given number of presets, starting at
 * the preset pointed to by preset_list */
static int load_preset_zones(SFData *sf, fluid_list_t *preset_list, int num_presets)
{
    int i;
    SFPreset *preset;
    SFPresetZone *zone;
    SFPresetZone *prev_zone = NULL;
    unsigned short last_gen_idx;
    unsigned short last_mod_idx;

    while (preset_list && num_presets > 0)
    {
        preset = (SFPreset *)fluid_list_get(preset_list);

        /* If this is the first record we read, move file position to first pbag record */
        if (prev_zone == NULL)
        {
            FSEEK(sf, sf->pbag_pos + (preset->pbag_idx * SF_BAG_SIZE));
        }

        for (i = 0; i < preset->pbag_count; i++)
        {
            zone = FLUID_NEW(SFPresetZone);
            if (zone == NULL)
            {
                FLUID_LOG(FLUID_ERR, "Out of memory");
                return FALSE;
            }
            FLUID_MEMSET(zone, 0, sizeof(SFPresetZone));

            preset->zone = fluid_list_append(preset->zone, zone);

            READW(sf, zone->gen_idx);
            READW(sf, zone->mod_idx);

            if (prev_zone)
            {
                prev_zone->gen_count = zone->gen_idx - prev_zone->gen_idx;
                prev_zone->mod_count = zone->mod_idx - prev_zone->mod_idx;
                if (prev_zone->gen_count < 0 || prev_zone->mod_count < 0)
                {
                    FLUID_LOG(FLUID_ERR, "Preset zone indices not monotonic");
                    return FALSE;
                }
            }

            prev_zone = zone;
        }

        preset_list = fluid_list_next(preset_list);
        num_presets--;
    }

    /* Read additional pbag record to determine the modulator and generator count of
     * the last zone we've read */
    if (prev_zone)
    {
        READW(sf, last_gen_idx);
        READW(sf, last_mod_idx);

        prev_zone->gen_count = last_gen_idx - prev_zone->gen_idx;
        prev_zone->mod_count = last_mod_idx - prev_zone->mod_idx;
        if (zone->gen_count < 0 || zone->mod_count < 0)
        {
            FLUID_LOG(FLUID_ERR, "Preset zone indices not monotonic");
            return FALSE;
        }
    }

    return TRUE;
}

/* preset modulator loader */
static int load_preset_modulators(SFData *sf, fluid_list_t *preset_list, int num_presets)
{
    int i;
    SFPreset *preset;
    fluid_list_t *zone_list;
    SFPresetZone *zone;

    SFMod *mod = NULL;

    while (preset_list && num_presets > 0)
    {
        preset = (SFPreset *)fluid_list_get(preset_list);
        zone_list = preset->zone;

        while (zone_list)
        {
            zone = (SFPresetZone *)fluid_list_get(zone_list);

            /* If this is the first modulator we read, move the file pointer to
             * point to this pmod record */
            if (mod == NULL)
            {
                FSEEK(sf, sf->pmod_pos + (zone->mod_idx * SF_MOD_SIZE));
            }

            /* load zone's modulators */
            for (i = 0; i < zone->mod_count; i++)
            {
                mod = FLUID_NEW(SFMod);
                if (mod == NULL)
                {
                    FLUID_LOG(FLUID_ERR, "Out of memory");
                    return FALSE;
                }
                zone->mod = fluid_list_append(zone->mod, mod);

                READW(sf, mod->src);
                READW(sf, mod->dest);
                READW(sf, mod->amount);
                READW(sf, mod->amtsrc);
                READW(sf, mod->trans);
            }

            zone_list = fluid_list_next(zone_list);
        }

        preset_list = fluid_list_next(preset_list);
        num_presets--;
    }

    return TRUE;
}


/* -------------------------------------------------------------------
 * preset generator loader
 * generator (per preset) loading rules:
 * Zones with no generators or modulators shall be annihilated
 * Global zone must be 1st zone, discard additional ones (instrumentless zones)
 *
 * generator (per zone) loading rules (in order of decreasing precedence):
 * KeyRange is 1st in list (if exists), else discard
 * if a VelRange exists only preceded by a KeyRange, else discard
 * if a generator follows an instrument discard it
 * if a duplicate generator exists replace previous one
 * ------------------------------------------------------------------- */
static int load_preset_generators(SFData *sf, fluid_list_t *preset_list, int num_presets)
{
    int i;
    SFPreset *preset;
    fluid_list_t *zone_list;
    SFPresetZone *zone;

    SFGen *gen = NULL;
    SFGen *duplicate_gen = NULL;
    SFGenAmount genval;
    unsigned short genid;
    int level;

    while (preset_list && num_presets > 0)
    {
        preset = (SFPreset *)fluid_list_get(preset_list);
        zone_list = preset->zone;

        while(zone_list)
        {
            zone = (SFPresetZone *)fluid_list_get(zone_list);
            level = 0;

            /* load all generators for this zone */
            for (i = 0; i < zone->gen_count; i++)
            {
                /* If this is the first generator we read, move the file pointer to
                * point to this pgen record */
                if (gen == NULL)
                {
                    FSEEK(sf, sf->pgen_pos + (zone->gen_idx * SF_GEN_SIZE));
                }

                READW(sf, genid);

                if ((level == 0 && genid == Gen_KeyRange) ||
                    (level <= 1 && genid == Gen_VelRange))
                {
                    level++;
                    READB(sf, genval.range.lo);
                    READB(sf, genval.range.hi);
                }
                else if (level < 3 && genid == Gen_Instrument)
                {
                    level = 3;
                    READW(sf, genval.uword);
                }
                else if (level <= 2 && valid_preset_genid(genid))
                {
                    level = 2;
                    READW(sf, genval.sword);
                    duplicate_gen = fluid_list_get(find_gen_by_id(genid, zone->gen));
                }
                else {
                    FSKIP(sf, 2);
                    FLUID_LOG(FLUID_WARN, "Ignoring generator");
                    continue;
                }

                if (duplicate_gen != NULL)
                {
                    gen = duplicate_gen;
                    duplicate_gen = NULL;
                }
                else
                {
                    gen = FLUID_NEW(SFGen);
                    if (gen == NULL)
                    {
                        FLUID_LOG(FLUID_ERR, "Out of memory");
                        return FALSE;
                    }
                    zone->gen = fluid_list_append(zone->gen, gen);
                    gen->id = genid;
                }

                gen->amount = genval;
            }

            zone_list = fluid_list_next(zone_list);
        }

        preset_list = fluid_list_next(preset_list);
        num_presets--;
    }

    return TRUE;
}

/* instrument header loader */
static int load_inst_headers(SFData *sf, int start_idx, int num_records)
{
    int i;
    SFInst *inst;
    SFInst *prev_inst = NULL;
    unsigned short last_ibag_idx;

    FSEEK(sf, sf->ihdr_pos + (start_idx * SF_IHDR_SIZE));

    for (i = 0; i < num_records; i++)
    {
        inst = FLUID_NEW(SFInst);
        if (inst == NULL)
        {
            FLUID_LOG(FLUID_ERR, "Out of memory");
            return FALSE;
        }
        FLUID_MEMSET(inst, 0, sizeof(SFInst));

        sf->inst = fluid_list_append(sf->inst, inst);

        READSTR(sf, &inst->name);
        READW(sf, inst->ibag_idx);
        inst->idx = start_idx + i;

        if (prev_inst)
        {
            prev_inst->ibag_count = inst->ibag_idx - prev_inst->ibag_idx;
            if (prev_inst->ibag_count < 0)
            {
                FLUID_LOG(FLUID_ERR, _("Instrument header indices not monotonic"));
                return FALSE;
            }
        }

        prev_inst = inst;
    }

    if (prev_inst)
    {
        FSKIP(sf, 20); /* skip name */
        READW(sf, last_ibag_idx);
        prev_inst->ibag_count = last_ibag_idx - prev_inst->ibag_idx;
        if (prev_inst->ibag_count < 0)
        {
            FLUID_LOG(FLUID_ERR, _("Instrument header indices not monotonic"));
            return FALSE;
        }
    }

    return TRUE;
}

/* instrument bag loader */
static int load_inst_zones(SFData *sf, fluid_list_t *inst_list, int num_insts)
{
    int i;
    SFInst *inst;
    SFInstZone *zone;
    SFInstZone *prev_zone = NULL;
    unsigned short last_gen_idx;
    unsigned short last_mod_idx;

    while (inst_list && num_insts > 0)
    {
        inst = (SFInst *)fluid_list_get(inst_list);

        /* If this is the first record we read, move file position to first ibag record */
        if (prev_zone == NULL)
        {
            FSEEK(sf, sf->ibag_pos + (inst->ibag_idx * SF_BAG_SIZE));
        }

        for (i = 0; i < inst->ibag_count; i++)
        {
            zone = FLUID_NEW(SFInstZone);
            if (zone == NULL)
            {
                FLUID_LOG(FLUID_ERR, "Out of memory");
                return FALSE;
            }
            FLUID_MEMSET(zone, 0, sizeof(SFInstZone));

            inst->zone = fluid_list_append(inst->zone, zone);

            READW(sf, zone->gen_idx);
            READW(sf, zone->mod_idx);

            if (prev_zone)
            {
                prev_zone->gen_count = zone->gen_idx - prev_zone->gen_idx;
                prev_zone->mod_count = zone->mod_idx - prev_zone->mod_idx;
                if (prev_zone->gen_count < 0 || prev_zone->mod_count < 0)
                {
                    FLUID_LOG(FLUID_ERR, _("Instrument generator indices not monotonic"));
                    return FALSE;
                }
            }

            prev_zone = zone;
        }

        inst_list = fluid_list_next(inst_list);
        num_insts--;
    }

    /* Read additional ibag record to determine the modulator and generator count of
     * the last zone we've read */
    if (prev_zone)
    {
        READW(sf, last_gen_idx);
        READW(sf, last_mod_idx);

        prev_zone->gen_count = last_gen_idx - prev_zone->gen_idx;
        prev_zone->mod_count = last_mod_idx - prev_zone->mod_idx;
        if (prev_zone->gen_count < 0 || prev_zone->mod_count < 0)
        {
            FLUID_LOG(FLUID_ERR, _("Instrument generator indices not monotonic"));
            return FALSE;
        }
    }

    return TRUE;
}

/* instrument modulator loader */
static int load_inst_modulators(SFData *sf, fluid_list_t *inst_list, int num_insts)
{
    int i;
    SFInst *inst;
    fluid_list_t *zone_list;
    SFInstZone *zone;

    SFMod *mod = NULL;

    while (inst_list && num_insts > 0)
    {
        inst = (SFInst *)fluid_list_get(inst_list);
        zone_list = inst->zone;

        zone_list = inst->zone;
        while (zone_list)
        {
            zone = (SFInstZone *)fluid_list_get(zone_list);

            /* If this is the first modulator we read, move the file pointer to
             * point to this imod record */
            if (mod == NULL)
            {
                FSEEK(sf, sf->imod_pos + (zone->mod_idx * SF_MOD_SIZE));
            }

            /* load zone's modulators */
            for (i = 0; i < zone->mod_count; i++)
            {
                mod = FLUID_NEW(SFMod);
                if (mod == NULL)
                {
                    FLUID_LOG(FLUID_ERR, "Out of memory");
                    return FALSE;
                }
                zone->mod = fluid_list_append(zone->mod, mod);

                READW(sf, mod->src);
                READW(sf, mod->dest);
                READW(sf, mod->amount);
                READW(sf, mod->amtsrc);
                READW(sf, mod->trans);
            }

            zone_list = fluid_list_next(zone_list);
        }

        inst_list = fluid_list_next(inst_list);
        num_insts--;
    }

    return TRUE;
}

/* load instrument generators (see load_pgen for loading rules) */
static int load_inst_generators(SFData *sf, fluid_list_t *inst_list, int num_insts)
{
    int i;
    SFInst *inst;
    fluid_list_t *zone_list;
    SFInstZone *zone;

    SFGen *gen = NULL;
    SFGen *duplicate_gen = NULL;
    SFGenAmount genval;
    unsigned short genid;
    int level;

    while (inst_list && num_insts > 0)
    {
        inst = (SFInst *)fluid_list_get(inst_list);
        zone_list = inst->zone;

        while(zone_list)
        {
            zone = (SFInstZone *)fluid_list_get(zone_list);
            level = 0;

            for (i = 0; i < zone->gen_count; i++)
            {
                /* If this is the first generator we read, move the file pointer to
                * point to this igen record */
                if (gen == NULL)
                {
                    FSEEK(sf, sf->igen_pos + (zone->gen_idx * SF_GEN_SIZE));
                }

                READW(sf, genid);

                if ((level == 0 && genid == Gen_KeyRange) ||
                    (level <= 1 && genid == Gen_VelRange))
                {
                    level++;
                    READB(sf, genval.range.lo);
                    READB(sf, genval.range.hi);
                }
                else if (level < 3 && genid == Gen_SampleId)
                {
                    level = 3;
                    READW(sf, genval.uword);
                }
                else if (level <= 2 && valid_inst_genid(genid))
                {
                    level = 2;
                    READW(sf, genval.sword);
                    duplicate_gen = fluid_list_get(find_gen_by_id(genid, zone->gen));
                }
                else {
                    FSKIP(sf, 2);
                    FLUID_LOG(FLUID_WARN, "Ignoring generator");
                    continue;
                }

                if (duplicate_gen != NULL)
                {
                    gen = duplicate_gen;
                    duplicate_gen = NULL;
                }
                else
                {
                    gen = FLUID_NEW(SFGen);
                    if (gen == NULL)
                    {
                        FLUID_LOG(FLUID_ERR, "Out of memory");
                        return FALSE;
                    }
                    zone->gen = fluid_list_append(zone->gen, gen);
                    gen->id = genid;
                }

                gen->amount = genval;
            }

            zone_list = fluid_list_next(zone_list);
        }

        inst_list = fluid_list_next(inst_list);
        num_insts--;
    }

    return TRUE;
}


static int fixup_preset_zones(SFData *sf, fluid_list_t *preset_list, int num_presets)
{
    SFPreset *preset;

    fluid_list_t *zone_list;
    SFPresetZone *zone;
    SFPresetZone *first_zone;

    SFGen *gen;

    while(preset_list && num_presets > 0)
    {
        preset = (SFPreset *)fluid_list_get(preset_list);

        zone_list = preset->zone;
        first_zone = fluid_list_get(zone_list);

        while(zone_list)
        {
            zone = (SFPresetZone *)fluid_list_get(zone_list);
            /* Advance zone_list pointer here already, as we might remove the
             * current zone from the list and wouldn't be able to get the next
             * element after removing the curent one. */
            zone_list = fluid_list_next(zone_list);

            gen = fluid_list_get(fluid_list_last(zone->gen));

            /* Zones without modulators or generators should be ignored */
            if (gen == NULL && zone->mod == NULL)
            {
                preset->zone = fluid_list_remove(preset->zone, zone);
                delete_preset_zone(zone);
            }
            /* Zone has instrument generator: resolve link and remove generator from list */
            else if (gen && gen->id == Gen_Instrument)
            {
                /* FIXME: find instrument by id, not by index! */
                zone->inst = fluid_list_get(fluid_list_nth(sf->inst, gen->amount.uword));
                if (zone->inst == NULL)
                {
                    FLUID_LOG(FLUID_ERR, "Invalid instrument pointer");
                    return FALSE;
                }
                zone->gen = fluid_list_remove(zone->gen, gen);
                FLUID_FREE(gen);
            }
            /* Zone has no instrument generator, so it must be a global zone.
             * If it's not the first zone, then move it to front. But if there is
             * already a global zone, then discard this additional one. */
            else if (zone != first_zone)
            {
                preset->zone = fluid_list_remove(preset->zone, zone);

                if (first_zone->inst != NULL) /* if current first zone is not global zone */
                {
                    preset->zone = fluid_list_prepend(preset->zone, zone);
                    first_zone = zone;
                }
                else
                {
                    FLUID_LOG(FLUID_WARN, "Discarding additional global preset zone");
                    delete_preset_zone(zone);
                }
            }
        }

        preset_list = fluid_list_next(preset_list);
    }

    return TRUE;
}

static int fixup_inst_zones(SFData *sf, fluid_list_t *inst_list, int num_insts)
{
    SFInst *inst;

    fluid_list_t *zone_list;
    SFInstZone *zone;
    SFInstZone *first_zone;

    SFGen *gen;

    while(inst_list && num_insts > 0)
    {
        inst = (SFInst *)fluid_list_get(inst_list);

        zone_list = inst->zone;
        first_zone = fluid_list_get(zone_list);

        while(zone_list)
        {
            zone = (SFInstZone *)fluid_list_get(zone_list);
            /* Advance zone_list pointer here already, as we might remove the
             * current zone from the list and wouldn't be able to get the next
             * element after that. */
            zone_list = fluid_list_next(zone_list);

            gen = fluid_list_get(fluid_list_last(zone->gen));

            /* Zones without modulators or generators should be ignored */
            if (gen == NULL && zone->mod == NULL)
            {
                inst->zone = fluid_list_remove(inst->zone, zone);
                delete_inst_zone(zone);
            }
            /* Zone has sample generator: resolve link and remove generator from list */
            else if (gen && gen->id == Gen_SampleId)
            {
                zone->sample = fluid_list_get(fluid_list_nth(sf->sample, gen->amount.uword));
                if (zone->sample == NULL)
                {
                    FLUID_LOG(FLUID_ERR, "Invalid sample pointer");
                    return FALSE;
                }
                zone->gen = fluid_list_remove(zone->gen, gen);
                FLUID_FREE(gen);
            }
            /* Zone has no sample generator, so it must be a global zone.
             * If it's not the first zone, then move it to front. But if there is
             * already a global zone, then discard this additional one. */
            else if (zone != first_zone)
            {
                inst->zone = fluid_list_remove(inst->zone, zone);

                if (first_zone->sample != NULL) /* current first zone is not global zone */
                {
                    inst->zone = fluid_list_prepend(inst->zone, zone);
                    first_zone = zone;
                }
                else
                {
                    FLUID_LOG(FLUID_WARN, "Discarding additional global inst zone");
                    delete_inst_zone(zone);
                }
            }
        }

        inst_list = fluid_list_next(inst_list);
        num_insts--;
    }

    return TRUE;
}

/* sample header loader */
static int load_sample_headers(SFData *sf, int start_idx, int num_records)
{
    int i;
    SFSample *sample;

    FSEEK(sf, sf->shdr_pos + (start_idx * SF_SHDR_SIZE));

    for (i = 0; i < num_records; i++)
    {
        sample = FLUID_NEW(SFSample);
        if (sample == NULL)
        {
            FLUID_LOG(FLUID_ERR, "Out of memory");
            return FALSE;
        }
        sample->idx = start_idx + i;

        READSTR(sf, &sample->name);
        READD(sf, sample->start);
        READD(sf, sample->end); /* - end, loopstart and loopend */
        READD(sf, sample->loopstart); /* - will be checked and turned into */
        READD(sf, sample->loopend); /* - offsets in fixup_sample() */
        READD(sf, sample->samplerate);
        READB(sf, sample->origpitch);
        READB(sf, sample->pitchadj);
        FSKIP(sf, 2); /* skip sample link */
        READW(sf, sample->sampletype);

        sf->sample = fluid_list_append(sf->sample, sample);
    }

    return TRUE;
}


/* convert sample end, loopstart and loopend to offsets and check if valid */
static int fixup_samples(SFData *sf, fluid_list_t *sample_list, int num_samples)
{
    SFSample *sam;
    int invalid_loops = FALSE;
    int invalid_loopstart;
    int invalid_loopend, loopend_end_mismatch;
    unsigned int total_bytes = sf->samplesize;
    unsigned int total_samples = total_bytes / sizeof(short);

    while (sample_list && num_samples > 0)
    {
        unsigned int max_end;

        sam = (SFSample *)fluid_list_get(sample_list);

        /* Standard SoundFont files (SF2) use sample word indices for sample start and end pointers,
         * but SF3 files with Ogg Vorbis compression use byte indices for start and end. */
        max_end = (sam->sampletype & FLUID_SAMPLETYPE_OGG_VORBIS) ? total_bytes : total_samples;

        /* ROM samples are unusable for us by definition, so simply ignore them. */
        if (sam->sampletype & FLUID_SAMPLETYPE_ROM)
        {
            sam->start = sam->end = sam->loopstart = sam->loopend = 0;
            goto next_sample;
        }

        /* If end is over the sample data chunk or sam start is greater than 4
         * less than the end (at least 4 samples).
         *
         * FIXME: where does this number 4 come from? And do we need a different number for SF3
         * files?
         * Maybe we should check for the minimum Ogg Vorbis headers size? */
        if ((sam->end > max_end) || (sam->start > (sam->end - 4)))
        {
            FLUID_LOG(FLUID_WARN, _("Sample '%s' start/end file positions are invalid,"
                                    " disabling and will not be saved"),
                      sam->name);
            sam->start = sam->end = sam->loopstart = sam->loopend = 0;
            goto next_sample;
        }

        /* The SoundFont 2.4 spec defines the loopstart index as the first sample point of the loop
         */
        invalid_loopstart = (sam->loopstart < sam->start) || (sam->loopstart >= sam->loopend);
        /* while loopend is the first point AFTER the last sample of the loop.
         * this is as it should be. however we cannot be sure whether any of sam.loopend or sam.end
         * is correct. hours of thinking through this have concluded, that it would be best practice
         * to mangle with loops as little as necessary by only making sure loopend is within
         * max_end. incorrect soundfont shall preferably fail loudly. */
        invalid_loopend = (sam->loopend > max_end) || (sam->loopstart >= sam->loopend);

        loopend_end_mismatch = (sam->loopend > sam->end);

        if (sam->sampletype & FLUID_SAMPLETYPE_OGG_VORBIS)
        {
            /*
             * compressed samples get fixed up after decompression
             *
             * however we cant use the logic below, because uncompressed samples are stored in
             * individual buffers
             */
        }
        else if (invalid_loopstart || invalid_loopend ||
                 loopend_end_mismatch) /* loop is fowled?? (cluck cluck :) */
        {
            /* though illegal, loopend may be set to loopstart to disable loop */
            /* is it worth informing the user? */
            invalid_loops |= (sam->loopend != sam->loopstart);

            /* force incorrect loop points into the sample range, ignore padding */
            if (invalid_loopstart)
            {
                FLUID_LOG(FLUID_DBG, _("Sample '%s' has unusable loop start '%d',"
                                       " setting to sample start at '%d'"),
                          sam->name, sam->loopstart, sam->start);
                sam->loopstart = sam->start;
            }

            if (invalid_loopend)
            {
                FLUID_LOG(FLUID_DBG, _("Sample '%s' has unusable loop stop '%d',"
                                       " setting to sample stop at '%d'"),
                          sam->name, sam->loopend, sam->end);
                /* since at this time sam->end points after valid sample data (will correct that few
                 * lines below),
                 * set loopend to that first invalid sample, since it should never be played, but
                 * instead the last
                 * valid sample will be played */
                sam->loopend = sam->end;
            }
            else if (loopend_end_mismatch)
            {
                FLUID_LOG(FLUID_DBG, _("Sample '%s' has invalid loop stop '%d',"
                                       " sample stop at '%d', using it anyway"),
                          sam->name, sam->loopend, sam->end);
            }
        }

        /* convert sample end, loopstart, loopend to offsets from sam->start */
        sam->end -= sam->start + 1; /* marks last sample, contrary to SF spec. */
        sam->loopstart -= sam->start;
        sam->loopend -= sam->start;

next_sample:
        sample_list = fluid_list_next(sample_list);
        num_samples--;
    }

    if (invalid_loops)
    {
        FLUID_LOG(FLUID_WARN, _("Found samples with invalid loops, audible glitches possible."));
    }

    return TRUE;
}

static void delete_preset(SFPreset *preset)
{
    fluid_list_t *zone_list;
    SFPresetZone *zone;

    if (!preset)
        return;

    zone_list = preset->zone;
    while(zone_list)
    {
        zone = (SFPresetZone *)fluid_list_get(zone_list);
        delete_preset_zone(zone);
        zone_list = fluid_list_next(zone_list);
    }
    delete_fluid_list(preset->zone);

    FLUID_FREE(preset);
}

static void delete_inst(SFInst *inst)
{
    fluid_list_t *inst_list;
    SFInstZone *zone;

    if (!inst)
        return;

    inst_list = inst->zone;
    while(inst_list)
    {
        zone = (SFInstZone *)fluid_list_get(inst_list);
        delete_inst_zone(zone);
        inst_list = fluid_list_next(inst_list);
    }
    delete_fluid_list(inst->zone);

    FLUID_FREE(inst);
}

/* Free all elements of a preset zone */
static void delete_preset_zone(SFPresetZone *zone)
{
    fluid_list_t *entry;

    if (!zone)
        return;

    entry = zone->gen;
    while(entry)
    {
        FLUID_FREE(fluid_list_get(entry));
        entry = fluid_list_next(entry);
    }
    delete_fluid_list(zone->gen);

    entry = zone->mod;
    while(entry)
    {
        FLUID_FREE(fluid_list_get(entry));
        entry = fluid_list_next(entry);
    }
    delete_fluid_list(zone->mod);

    FLUID_FREE(zone);
}

/* Free all elements of an inst zone */
static void delete_inst_zone(SFInstZone *zone)
{
    fluid_list_t *entry;

    if (!zone)
        return;

    entry = zone->gen;
    while(entry)
    {
        FLUID_FREE(fluid_list_get(entry));
        entry = fluid_list_next(entry);
    }
    delete_fluid_list(zone->gen);

    entry = zone->mod;
    while(entry)
    {
        FLUID_FREE(fluid_list_get(entry));
        entry = fluid_list_next(entry);
    }
    delete_fluid_list(zone->mod);

    FLUID_FREE(zone);
}

/* Find a generator by its id in the passed in list.
 *
 * @return pointer to SFGen if found, otherwise NULL
 */
static fluid_list_t *find_gen_by_id(int genid, fluid_list_t *genlist)
{ /* is generator in gen list? */
    fluid_list_t *p = genlist;

    while (p)
    {
        if (p->data && ((SFGen *)p->data)->id == genid)
        {
            return p;
        }
        p = fluid_list_next(p);
    }

    return NULL;
}

/* check validity of instrument generator */
static int valid_inst_genid(unsigned short genid)
{
    int i = 0;

    if (genid > Gen_MaxValid)
        return FALSE;
    while (invalid_inst_gen[i] && invalid_inst_gen[i] != genid)
        i++;
    return (invalid_inst_gen[i] == 0);
}

/* check validity of preset generator */
static int valid_preset_genid(unsigned short genid)
{
    int i = 0;

    if (!valid_inst_genid(genid))
        return FALSE;
    while (invalid_preset_gen[i] && invalid_preset_gen[i] != genid)
        i++;
    return (invalid_preset_gen[i] == 0);
}
