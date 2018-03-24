/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * SoundFont loading code borrowed from Smurf SoundFont Editor by Josh Green
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


#ifndef _FLUID_DEFSFONT_H
#define _FLUID_DEFSFONT_H


#include "fluidsynth.h"
#include "fluidsynth_priv.h"
#include "fluid_sf2.h"
#include "fluid_list.h"
#include "fluid_mod.h"
#include "fluid_gen.h"



/*-----------------------------------sfont.h----------------------------*/

#define SF_SAMPMODES_LOOP	1
#define SF_SAMPMODES_UNROLL	2

#define SF_MIN_SAMPLERATE	400
#define SF_MAX_SAMPLERATE	50000

#define SF_MIN_SAMPLE_LENGTH	32

/***************************************************************
 *
 *       FORWARD DECLARATIONS
 */
typedef struct _fluid_defsfont_t fluid_defsfont_t;
typedef struct _fluid_defpreset_t fluid_defpreset_t;
typedef struct _fluid_preset_zone_t fluid_preset_zone_t;
typedef struct _fluid_inst_t fluid_inst_t;
typedef struct _fluid_inst_zone_t fluid_inst_zone_t;            /**< Soundfont Instrument Zone */

/* defines the velocity and key range for a zone */
struct _fluid_zone_range_t
{
  int keylo;
  int keyhi;
  int vello;
  int velhi;
  unsigned char ignore;	/* set to TRUE for legato playing to ignore this range zone */
};


/*

  Public interface

 */

fluid_sfont_t* fluid_defsfloader_load(fluid_sfloader_t* loader, const char* filename);


int fluid_defsfont_sfont_delete(fluid_sfont_t* sfont);
const char* fluid_defsfont_sfont_get_name(fluid_sfont_t* sfont);
fluid_preset_t* fluid_defsfont_sfont_get_preset(fluid_sfont_t* sfont, unsigned int bank, unsigned int prenum);
void fluid_defsfont_sfont_iteration_start(fluid_sfont_t* sfont);
int fluid_defsfont_sfont_iteration_next(fluid_sfont_t* sfont, fluid_preset_t* preset);


void fluid_defpreset_preset_delete(fluid_preset_t* preset);
const char* fluid_defpreset_preset_get_name(fluid_preset_t* preset);
int fluid_defpreset_preset_get_banknum(fluid_preset_t* preset);
int fluid_defpreset_preset_get_num(fluid_preset_t* preset);
int fluid_defpreset_preset_noteon(fluid_preset_t* preset, fluid_synth_t* synth, int chan, int key, int vel);

int fluid_zone_inside_range(fluid_zone_range_t* zone_range, int key, int vel);

/*
 * fluid_defsfont_t
 */
struct _fluid_defsfont_t
{
  char* filename;           /* the filename of this soundfont */
  unsigned int samplepos;   /* the position in the file at which the sample data starts */
  unsigned int samplesize;  /* the size of the sample data in bytes */
  short* sampledata;        /* the sample data, loaded in ram */
  
  unsigned int sample24pos;		/* position within sffd of the sm24 chunk, set to zero if no 24 bit sample support */
  unsigned int sample24size;		/* length within sffd of the sm24 chunk */
  char* sample24data;        /* if not NULL, the least significant byte of the 24bit sample data, loaded in ram */
  
  fluid_list_t* sample;      /* the samples in this soundfont */
  fluid_defpreset_t* preset; /* the presets of this soundfont */
  int mlock;                 /* Should we try memlock (avoid swapping)? */

  fluid_defpreset_t* iter_cur;       /* the current preset in the iteration */

  fluid_preset_t** preset_stack; /* List of presets that are available to use */
  int preset_stack_capacity;     /* Length of preset_stack array */
  int preset_stack_size;         /* Current number of items in the stack */
};


fluid_defsfont_t* new_fluid_defsfont(fluid_settings_t* settings);
int delete_fluid_defsfont(fluid_defsfont_t* sfont);
int fluid_defsfont_load(fluid_defsfont_t* sfont, const fluid_file_callbacks_t* file_callbacks, const char* file);
const char* fluid_defsfont_get_name(fluid_defsfont_t* sfont);
fluid_defpreset_t* fluid_defsfont_get_preset(fluid_defsfont_t* sfont, unsigned int bank, unsigned int prenum);
void fluid_defsfont_iteration_start(fluid_defsfont_t* sfont);
int fluid_defsfont_iteration_next(fluid_defsfont_t* sfont, fluid_preset_t* preset);
int fluid_defsfont_load_sampledata(fluid_defsfont_t* sfont, const fluid_file_callbacks_t* file_callbacks);
int fluid_defsfont_add_sample(fluid_defsfont_t* sfont, fluid_sample_t* sample);
int fluid_defsfont_add_preset(fluid_defsfont_t* sfont, fluid_defpreset_t* preset);


/*
 * fluid_preset_t
 */
struct _fluid_defpreset_t
{
  fluid_defpreset_t* next;
  fluid_defsfont_t* sfont;                  /* the soundfont this preset belongs to */
  char name[21];                        /* the name of the preset */
  unsigned int bank;                    /* the bank number */
  unsigned int num;                     /* the preset number */
  fluid_preset_zone_t* global_zone;        /* the global zone of the preset */
  fluid_preset_zone_t* zone;               /* the chained list of preset zones */
};

fluid_defpreset_t* new_fluid_defpreset(fluid_defsfont_t* sfont);
void delete_fluid_defpreset(fluid_defpreset_t* preset);
fluid_defpreset_t* fluid_defpreset_next(fluid_defpreset_t* preset);
int fluid_defpreset_import_sfont(fluid_defpreset_t* preset, SFPreset* sfpreset, fluid_defsfont_t* sfont);
int fluid_defpreset_set_global_zone(fluid_defpreset_t* preset, fluid_preset_zone_t* zone);
int fluid_defpreset_add_zone(fluid_defpreset_t* preset, fluid_preset_zone_t* zone);
fluid_preset_zone_t* fluid_defpreset_get_zone(fluid_defpreset_t* preset);
fluid_preset_zone_t* fluid_defpreset_get_global_zone(fluid_defpreset_t* preset);
int fluid_defpreset_get_banknum(fluid_defpreset_t* preset);
int fluid_defpreset_get_num(fluid_defpreset_t* preset);
const char* fluid_defpreset_get_name(fluid_defpreset_t* preset);
int fluid_defpreset_noteon(fluid_defpreset_t* preset, fluid_synth_t* synth, int chan, int key, int vel);

/*
 * fluid_preset_zone
 */
struct _fluid_preset_zone_t
{
  fluid_preset_zone_t* next;
  char* name;
  fluid_inst_t* inst;
  fluid_zone_range_t range;
  fluid_gen_t gen[GEN_LAST];
  fluid_mod_t * mod; /* List of modulators */
};

fluid_preset_zone_t* new_fluid_preset_zone(char* name);
void delete_fluid_preset_zone(fluid_preset_zone_t* zone);
fluid_preset_zone_t* fluid_preset_zone_next(fluid_preset_zone_t* preset);
int fluid_preset_zone_import_sfont(fluid_preset_zone_t* zone, SFZone* sfzone, fluid_defsfont_t* sfont);
fluid_inst_t* fluid_preset_zone_get_inst(fluid_preset_zone_t* zone);

/*
 * fluid_inst_t
 */
struct _fluid_inst_t
{
  char name[21];
  fluid_inst_zone_t* global_zone;
  fluid_inst_zone_t* zone;
};

fluid_inst_t* new_fluid_inst(void);
int fluid_inst_import_sfont(fluid_preset_zone_t* zonePZ, fluid_inst_t* inst,
							SFInst *sfinst, fluid_defsfont_t* sfont);
void delete_fluid_inst(fluid_inst_t* inst);
int fluid_inst_set_global_zone(fluid_inst_t* inst, fluid_inst_zone_t* zone);
int fluid_inst_add_zone(fluid_inst_t* inst, fluid_inst_zone_t* zone);
fluid_inst_zone_t* fluid_inst_get_zone(fluid_inst_t* inst);
fluid_inst_zone_t* fluid_inst_get_global_zone(fluid_inst_t* inst);

/*
 * fluid_inst_zone_t
 */
struct _fluid_inst_zone_t
{
  fluid_inst_zone_t* next;
  char* name;
  fluid_sample_t* sample;
  fluid_zone_range_t range;
  fluid_gen_t gen[GEN_LAST];
  fluid_mod_t * mod; /* List of modulators */
};


fluid_inst_zone_t* new_fluid_inst_zone(char* name);
void delete_fluid_inst_zone(fluid_inst_zone_t* zone);
fluid_inst_zone_t* fluid_inst_zone_next(fluid_inst_zone_t* zone);
int fluid_inst_zone_import_sfont(fluid_preset_zone_t* zonePZ,
								 fluid_inst_zone_t* zone, SFZone *sfzone, fluid_defsfont_t* sfont);
fluid_sample_t* fluid_inst_zone_get_sample(fluid_inst_zone_t* zone);



int fluid_sample_import_sfont(fluid_sample_t* sample, SFSample* sfsample, fluid_defsfont_t* sfont);
int fluid_sample_in_rom(fluid_sample_t* sample);


#endif  /* _FLUID_SFONT_H */
