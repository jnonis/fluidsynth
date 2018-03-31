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


#include "fluid_defsfont.h"
#include "fluid_sfont.h"
#include "fluid_sys.h"
#include "fluid_synth.h"
#include "fluid_samplecache.h"

/* EMU8k/10k hardware applies this factor to initial attenuation generator values set at preset and
 * instrument level in a soundfont. We apply this factor when loading the generator values to stay
 * compatible as most existing soundfonts expect exactly this (strange, non-standard) behaviour. */
#define EMU_ATTENUATION_FACTOR (0.4f)


/***************************************************************
 *
 *                           SFONT LOADER
 */

/**
 * Creates a default soundfont2 loader that can be used with fluid_synth_add_sfloader().
 * By default every synth instance has an initial default soundfont loader instance.
 * Calling this function is usually only necessary to load a soundfont from memory, by providing custom callback functions via fluid_sfloader_set_callbacks().
 * 
 * @param settings A settings instance obtained by new_fluid_settings()
 * @return A default soundfont2 loader struct
 */
fluid_sfloader_t* new_fluid_defsfloader(fluid_settings_t* settings)
{
  fluid_sfloader_t* loader;
  fluid_return_val_if_fail(settings != NULL, NULL);

  loader = new_fluid_sfloader(fluid_defsfloader_load, delete_fluid_sfloader);
  
  if (loader == NULL)
  {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }

  fluid_sfloader_set_data(loader, settings);

  return loader;
}

fluid_sfont_t* fluid_defsfloader_load(fluid_sfloader_t* loader, const char* filename)
{
  fluid_defsfont_t* defsfont;
  fluid_sfont_t* sfont;

  defsfont = new_fluid_defsfont(fluid_sfloader_get_data(loader));

  if (defsfont == NULL) {
    return NULL;
  }

  if (fluid_defsfont_load(defsfont, &loader->file_callbacks, filename) == FLUID_FAILED) {
    delete_fluid_defsfont(defsfont);
    return NULL;
  }

  sfont = new_fluid_sfont(fluid_defsfont_sfont_get_name,
                          fluid_defsfont_sfont_get_preset,
                          fluid_defsfont_sfont_delete);
  if (sfont == NULL)
  {
    return NULL;
  }

  fluid_sfont_set_iteration_start(sfont, fluid_defsfont_sfont_iteration_start);
  fluid_sfont_set_iteration_next(sfont, fluid_defsfont_sfont_iteration_next);
  fluid_sfont_set_data(sfont, defsfont);

  return sfont;
}



/***************************************************************
 *
 *                           PUBLIC INTERFACE
 */

int fluid_defsfont_sfont_delete(fluid_sfont_t* sfont)
{
  if (delete_fluid_defsfont(fluid_sfont_get_data(sfont)) != FLUID_OK) {
    return -1;
  }
  delete_fluid_sfont(sfont);
  return 0;
}

const char* fluid_defsfont_sfont_get_name(fluid_sfont_t* sfont)
{
  return fluid_defsfont_get_name(fluid_sfont_get_data(sfont));
}

fluid_preset_t*
fluid_defsfont_sfont_get_preset(fluid_sfont_t* sfont, unsigned int bank, unsigned int prenum)
{
  fluid_preset_t* preset = NULL;
  fluid_defpreset_t* defpreset;
  fluid_defsfont_t* defsfont = fluid_sfont_get_data(sfont);

  defpreset = fluid_defsfont_get_preset(defsfont, bank, prenum);

  if (defpreset == NULL) {
    return NULL;
  }

  if (defsfont->preset_stack_size > 0) {
    defsfont->preset_stack_size--;
    preset = defsfont->preset_stack[defsfont->preset_stack_size];
  }
  if (!preset)
    preset = FLUID_NEW(fluid_preset_t);
  if (!preset) {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }

  preset->sfont = sfont;
  preset->data = defpreset;
  preset->free = fluid_defpreset_preset_delete;
  preset->get_name = fluid_defpreset_preset_get_name;
  preset->get_banknum = fluid_defpreset_preset_get_banknum;
  preset->get_num = fluid_defpreset_preset_get_num;
  preset->noteon = fluid_defpreset_preset_noteon;
  preset->notify = NULL;

  return preset;
}

void fluid_defsfont_sfont_iteration_start(fluid_sfont_t* sfont)
{
  fluid_defsfont_iteration_start(fluid_sfont_get_data(sfont));
}

int fluid_defsfont_sfont_iteration_next(fluid_sfont_t* sfont, fluid_preset_t* preset)
{
  preset->free = fluid_defpreset_preset_delete;
  preset->get_name = fluid_defpreset_preset_get_name;
  preset->get_banknum = fluid_defpreset_preset_get_banknum;
  preset->get_num = fluid_defpreset_preset_get_num;
  preset->noteon = fluid_defpreset_preset_noteon;
  preset->notify = NULL;

  return fluid_defsfont_iteration_next(fluid_sfont_get_data(sfont), preset);
}

fluid_sample_t *fluid_defsfont_sample_by_idx(fluid_defsfont_t *sfont, unsigned short idx)
{
    fluid_sample_t *sample;
    fluid_list_t *sample_list = sfont->sample;

    while (sample_list)
    {
        sample = (fluid_sample_t *)fluid_list_get(sample_list);
        if (sample->idx == idx)
        {
            return sample;
        }
        sample_list = fluid_list_next(sample_list);
    }

    return NULL;
}

void fluid_defpreset_preset_delete(fluid_preset_t* preset)
{
  fluid_defpreset_t* defpreset = fluid_preset_get_data(preset);
  fluid_defsfont_t* sfont = defpreset ? defpreset->sfont : NULL;

  if (sfont && sfont->preset_stack_size < sfont->preset_stack_capacity) {
     sfont->preset_stack[sfont->preset_stack_size] = preset;
     sfont->preset_stack_size++;
  }
  else
      delete_fluid_preset(preset);
}

const char* fluid_defpreset_preset_get_name(fluid_preset_t* preset)
{
  return fluid_defpreset_get_name(fluid_preset_get_data(preset));
}

int fluid_defpreset_preset_get_banknum(fluid_preset_t* preset)
{
  return fluid_defpreset_get_banknum(fluid_preset_get_data(preset));
}

int fluid_defpreset_preset_get_num(fluid_preset_t* preset)
{
  return fluid_defpreset_get_num(fluid_preset_get_data(preset));
}

int fluid_defpreset_preset_noteon(fluid_preset_t* preset, fluid_synth_t* synth,
				 int chan, int key, int vel)
{
  return fluid_defpreset_noteon(fluid_preset_get_data(preset), synth, chan, key, vel);
}


/***************************************************************
 *
 *                           SFONT
 */

/*
 * new_fluid_defsfont
 */
fluid_defsfont_t* new_fluid_defsfont(fluid_settings_t* settings)
{
  fluid_defsfont_t* sfont;
  int i;

  sfont = FLUID_NEW(fluid_defsfont_t);
  if (sfont == NULL) {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }

  FLUID_MEMSET(sfont, 0, sizeof(*sfont));
  
  fluid_settings_getint(settings, "synth.lock-memory", &sfont->mlock);

  /* Initialise preset cache, so we don't have to call malloc on program changes.
     Usually, we have at most one preset per channel plus one temporarily used,
     so optimise for that case. */
  fluid_settings_getint(settings, "synth.midi-channels", &sfont->preset_stack_capacity);
  sfont->preset_stack_capacity++;
  
  sfont->preset_stack = FLUID_ARRAY(fluid_preset_t*, sfont->preset_stack_capacity);
  if (!sfont->preset_stack) {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    FLUID_FREE(sfont);
    return NULL;
  }

  for (i = 0; i < sfont->preset_stack_capacity; i++) {
    sfont->preset_stack[i] = FLUID_NEW(fluid_preset_t);
    if (!sfont->preset_stack[i]) {
      FLUID_LOG(FLUID_ERR, "Out of memory");
      delete_fluid_defsfont(sfont);
      return NULL;
    }
    sfont->preset_stack_size++;
  }

  return sfont;
}

/*
 * delete_fluid_defsfont
 */
int delete_fluid_defsfont(fluid_defsfont_t* sfont)
{
  fluid_list_t *list;
  fluid_defpreset_t* preset;
  fluid_sample_t* sample;

  fluid_return_val_if_fail(sfont != NULL, FLUID_OK);
  
  /* Check that no samples are currently used */
  for (list = sfont->sample; list; list = fluid_list_next(list)) {
    sample = (fluid_sample_t*) fluid_list_get(list);
    if (sample->refcount != 0) {
      return FLUID_FAILED;
    }
  }

  if (sfont->filename != NULL) {
    FLUID_FREE(sfont->filename);
  }

  for (list = sfont->sample; list; list = fluid_list_next(list)) {
    delete_fluid_sample((fluid_sample_t*) fluid_list_get(list));
  }

  if (sfont->sample) {
    delete_fluid_list(sfont->sample);
  }

  if (sfont->sampledata != NULL) {
    fluid_cached_sampledata_unload(sfont->sampledata);
  }

  while (sfont->preset_stack_size > 0)
    FLUID_FREE(sfont->preset_stack[--sfont->preset_stack_size]);
  FLUID_FREE(sfont->preset_stack);

  preset = sfont->preset;
  while (preset != NULL) {
    sfont->preset = preset->next;
    delete_fluid_defpreset(preset);
    preset = sfont->preset;
  }

  FLUID_FREE(sfont);
  return FLUID_OK;
}

/*
 * fluid_defsfont_get_name
 */
const char* fluid_defsfont_get_name(fluid_defsfont_t* sfont)
{
  return sfont->filename;
}


/*
 * fluid_defsfont_load
 */
int fluid_defsfont_load(fluid_defsfont_t* sfont, const fluid_file_callbacks_t* fcbs, const char* file)
{
  SFData* sfdata;
  fluid_list_t *p;
  SFPreset* sfpreset;
  SFSample* sfsample;
  fluid_sample_t* sample;
  fluid_defpreset_t* preset = NULL;

  sfont->filename = FLUID_MALLOC(1 + FLUID_STRLEN(file));
  if (sfont->filename == NULL) {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return FLUID_FAILED;
  }
  FLUID_STRCPY(sfont->filename, file);

  /* The actual loading is done in the sfont and sffile files */
  sfdata = fluid_sffile_load(file, fcbs);
  if (sfdata == NULL) {
    FLUID_LOG(FLUID_ERR, "Couldn't load soundfont file");
    return FLUID_FAILED;
  }

  /* Keep track of the position and size of the sample data because
     it's loaded separately (and might be unoaded/reloaded in future) */
  sfont->samplepos = sfdata->samplepos;
  sfont->samplesize = sfdata->samplesize;
  sfont->sample24pos = sfdata->sample24pos;
  sfont->sample24size = sfdata->sample24size;

  /* load sample data in one block */
  if (fluid_defsfont_load_sampledata(sfont, fcbs) != FLUID_OK)
    goto err_exit;

  /* Create all the sample headers */
  p = sfdata->sample;
  while (p != NULL) {
    sfsample = (SFSample *) p->data;

    sample = new_fluid_sample();
    if (sample == NULL) goto err_exit;

    if (fluid_sample_import_sfont(sample, sfsample, sfont) != FLUID_OK)
      goto err_exit;

    fluid_defsfont_add_sample(sfont, sample);
    fluid_voice_optimize_sample(sample);
    p = fluid_list_next(p);
  }

  /* Load all the presets */
  p = sfdata->preset;
  while (p != NULL) {
    sfpreset = (SFPreset *) p->data;
    preset = new_fluid_defpreset(sfont);
    if (preset == NULL) goto err_exit;

    if (fluid_defpreset_import_sfont(preset, sfpreset, sfont) != FLUID_OK)
      goto err_exit;

    fluid_defsfont_add_preset(sfont, preset);
    p = fluid_list_next(p);
  }
  fluid_sffile_close (sfdata);

  return FLUID_OK;

err_exit:
  fluid_sffile_close (sfdata);
  delete_fluid_defpreset(preset);
  return FLUID_FAILED;
}

/* fluid_defsfont_add_sample
 *
 * Add a sample to the SoundFont
 */
int fluid_defsfont_add_sample(fluid_defsfont_t* sfont, fluid_sample_t* sample)
{
  sfont->sample = fluid_list_append(sfont->sample, sample);
  return FLUID_OK;
}

/* fluid_defsfont_add_preset
 *
 * Add a preset to the SoundFont
 */
int fluid_defsfont_add_preset(fluid_defsfont_t* sfont, fluid_defpreset_t* preset)
{
  fluid_defpreset_t *cur, *prev;
  if (sfont->preset == NULL) {
    preset->next = NULL;
    sfont->preset = preset;
  } else {
    /* sort them as we go along. very basic sorting trick. */
    cur = sfont->preset;
    prev = NULL;
    while (cur != NULL) {
      if ((preset->bank < cur->bank)
	  || ((preset->bank == cur->bank) && (preset->num < cur->num))) {
	if (prev == NULL) {
	  preset->next = cur;
	  sfont->preset = preset;
	} else {
	  preset->next = cur;
	  prev->next = preset;
	}
	return FLUID_OK;
      }
      prev = cur;
      cur = cur->next;
    }
    preset->next = NULL;
    prev->next = preset;
  }
  return FLUID_OK;
}

/*
 * fluid_defsfont_load_sampledata
 */
int
fluid_defsfont_load_sampledata(fluid_defsfont_t* sfont, const fluid_file_callbacks_t* fcbs)
{
  return fluid_cached_sampledata_load(sfont->filename,
                                      sfont->samplepos, sfont->samplesize, &sfont->sampledata,
                                      sfont->sample24pos, sfont->sample24size, &sfont->sample24data,
                                      sfont->mlock,
                                      fcbs);
}

/*
 * fluid_defsfont_get_preset
 */
fluid_defpreset_t* fluid_defsfont_get_preset(fluid_defsfont_t* sfont, unsigned int bank, unsigned int num)
{
  fluid_defpreset_t* preset = sfont->preset;
  while (preset != NULL) {
    if ((preset->bank == bank) && ((preset->num == num))) {
      return preset;
    }
    preset = preset->next;
  }
  return NULL;
}

/*
 * fluid_defsfont_iteration_start
 */
void fluid_defsfont_iteration_start(fluid_defsfont_t* sfont)
{
  sfont->iter_cur = sfont->preset;
}

/*
 * fluid_defsfont_iteration_next
 */
int fluid_defsfont_iteration_next(fluid_defsfont_t* sfont, fluid_preset_t* preset)
{
  if (sfont->iter_cur == NULL) {
    return 0;
  }

  preset->data = (void*) sfont->iter_cur;
  sfont->iter_cur = fluid_defpreset_next(sfont->iter_cur);
  return 1;
}

/***************************************************************
 *
 *                           PRESET
 */

/*
 * new_fluid_defpreset
 */
fluid_defpreset_t*
new_fluid_defpreset(fluid_defsfont_t* sfont)
{
  fluid_defpreset_t* preset = FLUID_NEW(fluid_defpreset_t);
  if (preset == NULL) {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }
  preset->next = NULL;
  preset->sfont = sfont;
  preset->name[0] = 0;
  preset->bank = 0;
  preset->num = 0;
  preset->global_zone = NULL;
  preset->zone = NULL;
  return preset;
}

/*
 * delete_fluid_defpreset
 */
void
delete_fluid_defpreset(fluid_defpreset_t* preset)
{
  fluid_preset_zone_t* zone;
  
  fluid_return_if_fail(preset != NULL);
  
    delete_fluid_preset_zone(preset->global_zone);
    preset->global_zone = NULL;
  
  zone = preset->zone;
  while (zone != NULL) {
    preset->zone = zone->next;
    delete_fluid_preset_zone(zone);
    zone = preset->zone;
  }
  FLUID_FREE(preset);
}

int
fluid_defpreset_get_banknum(fluid_defpreset_t* preset)
{
  return preset->bank;
}

int
fluid_defpreset_get_num(fluid_defpreset_t* preset)
{
  return preset->num;
}

const char*
fluid_defpreset_get_name(fluid_defpreset_t* preset)
{
  return preset->name;
}

/*
 * fluid_defpreset_next
 */
fluid_defpreset_t*
fluid_defpreset_next(fluid_defpreset_t* preset)
{
  return preset->next;
}


/*
 * fluid_defpreset_noteon
 */
int
fluid_defpreset_noteon(fluid_defpreset_t* preset, fluid_synth_t* synth, int chan, int key, int vel)
{
  fluid_preset_zone_t *preset_zone, *global_preset_zone;
  fluid_inst_t* inst;
  fluid_inst_zone_t *inst_zone, *global_inst_zone;
  fluid_sample_t* sample;
  fluid_voice_t* voice;
  fluid_mod_t * mod;
  fluid_mod_t * mod_list[FLUID_NUM_MOD]; /* list for 'sorting' preset modulators */
  int mod_list_count;
  int i;

  global_preset_zone = fluid_defpreset_get_global_zone(preset);

  /* run thru all the zones of this preset */
  preset_zone = fluid_defpreset_get_zone(preset);
  while (preset_zone != NULL) {

    /* check if the note falls into the key and velocity range of this
       preset */
    if (fluid_zone_inside_range(&preset_zone->range, key, vel)) {

      inst = fluid_preset_zone_get_inst(preset_zone);
      global_inst_zone = fluid_inst_get_global_zone(inst);

      /* run thru all the zones of this instrument */
      inst_zone = fluid_inst_get_zone(inst);
	  while (inst_zone != NULL) {
	/* make sure this instrument zone has a valid sample */
	sample = fluid_inst_zone_get_sample(inst_zone);
	if ((sample == NULL) || fluid_sample_in_rom(sample)) {
	  inst_zone = fluid_inst_zone_next(inst_zone);
	  continue;
	}

	/* check if the instrument zone is ignored and the note falls into
	   the key and velocity range of this  instrument zone.
	   An instrument zone must be ignored when its voice is already running
	   played by a legato passage (see fluid_synth_noteon_monopoly_legato()) */
	if (fluid_zone_inside_range(&inst_zone->range, key, vel)) {

	  /* this is a good zone. allocate a new synthesis process and initialize it */
	  voice = fluid_synth_alloc_voice_LOCAL(synth, sample, chan, key, vel, &inst_zone->range);
	  if (voice == NULL) {
	    return FLUID_FAILED;
	  }


	  /* Instrument level, generators */

	  for (i = 0; i < GEN_LAST; i++) {

	    /* SF 2.01 section 9.4 'bullet' 4:
	     *
	     * A generator in a local instrument zone supersedes a
	     * global instrument zone generator.  Both cases supersede
	     * the default generator -> voice_gen_set */

	    if (inst_zone->gen[i].flags){
	      fluid_voice_gen_set(voice, i, inst_zone->gen[i].val);

	    } else if ((global_inst_zone != NULL) && (global_inst_zone->gen[i].flags)) {
	      fluid_voice_gen_set(voice, i, global_inst_zone->gen[i].val);

	    } else {
	      /* The generator has not been defined in this instrument.
	       * Do nothing, leave it at the default.
	       */
	    }

	  } /* for all generators */

	  /* global instrument zone, modulators: Put them all into a
	   * list. */

	  mod_list_count = 0;

	  if (global_inst_zone){
	    mod = global_inst_zone->mod;
	    while (mod){
	      mod_list[mod_list_count++] = mod;
	      mod = mod->next;
	    }
	  }

	  /* local instrument zone, modulators.
	   * Replace modulators with the same definition in the list:
	   * SF 2.01 page 69, 'bullet' 8
	   */
	  mod = inst_zone->mod;

	  while (mod){

	    /* 'Identical' modulators will be deleted by setting their
	     *  list entry to NULL.  The list length is known, NULL
	     *  entries will be ignored later.  SF2.01 section 9.5.1
	     *  page 69, 'bullet' 3 defines 'identical'.  */

	    for (i = 0; i < mod_list_count; i++){
	      if (mod_list[i] && fluid_mod_test_identity(mod,mod_list[i])){
		mod_list[i] = NULL;
	      }
	    }

	    /* Finally add the new modulator to to the list. */
	    mod_list[mod_list_count++] = mod;
	    mod = mod->next;
	  }

	  /* Add instrument modulators (global / local) to the voice. */
	  for (i = 0; i < mod_list_count; i++){

	    mod = mod_list[i];

	    if (mod != NULL){ /* disabled modulators CANNOT be skipped. */

	      /* Instrument modulators -supersede- existing (default)
	       * modulators.  SF 2.01 page 69, 'bullet' 6 */
	      fluid_voice_add_mod(voice, mod, FLUID_VOICE_OVERWRITE);
	    }
	  }

	  /* Preset level, generators */

	  for (i = 0; i < GEN_LAST; i++) {

	    /* SF 2.01 section 8.5 page 58: If some generators are
	     * encountered at preset level, they should be ignored */
	    if ((i != GEN_STARTADDROFS)
		&& (i != GEN_ENDADDROFS)
		&& (i != GEN_STARTLOOPADDROFS)
		&& (i != GEN_ENDLOOPADDROFS)
		&& (i != GEN_STARTADDRCOARSEOFS)
		&& (i != GEN_ENDADDRCOARSEOFS)
		&& (i != GEN_STARTLOOPADDRCOARSEOFS)
		&& (i != GEN_KEYNUM)
		&& (i != GEN_VELOCITY)
		&& (i != GEN_ENDLOOPADDRCOARSEOFS)
		&& (i != GEN_SAMPLEMODE)
		&& (i != GEN_EXCLUSIVECLASS)
		&& (i != GEN_OVERRIDEROOTKEY)) {

	      /* SF 2.01 section 9.4 'bullet' 9: A generator in a
	       * local preset zone supersedes a global preset zone
	       * generator.  The effect is -added- to the destination
	       * summing node -> voice_gen_incr */

	      if (preset_zone->gen[i].flags) {
		fluid_voice_gen_incr(voice, i, preset_zone->gen[i].val);
	      } else if ((global_preset_zone != NULL) && global_preset_zone->gen[i].flags) {
		fluid_voice_gen_incr(voice, i, global_preset_zone->gen[i].val);
	      } else {
		/* The generator has not been defined in this preset
		 * Do nothing, leave it unchanged.
		 */
	      }
	    } /* if available at preset level */
	  } /* for all generators */


	  /* Global preset zone, modulators: put them all into a
	   * list. */
	  mod_list_count = 0;
	  if (global_preset_zone){
	    mod = global_preset_zone->mod;
	    while (mod){
	      mod_list[mod_list_count++] = mod;
	      mod = mod->next;
	    }
	  }

	  /* Process the modulators of the local preset zone.  Kick
	   * out all identical modulators from the global preset zone
	   * (SF 2.01 page 69, second-last bullet) */

	  mod = preset_zone->mod;
	  while (mod){
	    for (i = 0; i < mod_list_count; i++){
	      if (mod_list[i] && fluid_mod_test_identity(mod,mod_list[i])){
		mod_list[i] = NULL;
	      }
	    }

	    /* Finally add the new modulator to the list. */
	    mod_list[mod_list_count++] = mod;
	    mod = mod->next;
	  }

	  /* Add preset modulators (global / local) to the voice. */
	  for (i = 0; i < mod_list_count; i++){
	    mod = mod_list[i];
	    if ((mod != NULL) && (mod->amount != 0)) { /* disabled modulators can be skipped. */

	      /* Preset modulators -add- to existing instrument /
	       * default modulators.  SF2.01 page 70 first bullet on
	       * page */
	      fluid_voice_add_mod(voice, mod, FLUID_VOICE_ADD);
	    }
	  }

	  /* add the synthesis process to the synthesis loop. */
	  fluid_synth_start_voice(synth, voice);

	  /* Store the ID of the first voice that was created by this noteon event.
	   * Exclusive class may only terminate older voices.
	   * That avoids killing voices, which have just been created.
	   * (a noteon event can create several voice processes with the same exclusive
	   * class - for example when using stereo samples)
	   */
	}

	inst_zone = fluid_inst_zone_next(inst_zone);
      }
	}
    preset_zone = fluid_preset_zone_next(preset_zone);
  }

  return FLUID_OK;
}

/*
 * fluid_defpreset_set_global_zone
 */
int
fluid_defpreset_set_global_zone(fluid_defpreset_t* preset, fluid_preset_zone_t* zone)
{
  preset->global_zone = zone;
  return FLUID_OK;
}

/*
 * fluid_defpreset_import_sfont
 */
int
fluid_defpreset_import_sfont(fluid_defpreset_t* preset,
			     SFPreset* sfpreset,
			     fluid_defsfont_t* sfont)
{
  fluid_list_t *p;
  SFPresetZone* sfzone;
  fluid_preset_zone_t* zone;
  int count;
  char zone_name[256];
  if (FLUID_STRLEN(sfpreset->name) > 0) {
    FLUID_STRCPY(preset->name, sfpreset->name);
  } else {
    FLUID_SNPRINTF(preset->name, sizeof(preset->name), "Bank%d,Pre%d", sfpreset->bank, sfpreset->prenum);
  }
  preset->bank = sfpreset->bank;
  preset->num = sfpreset->prenum;
  p = sfpreset->zone;
  count = 0;
  while (p != NULL) {
    sfzone = (SFPresetZone *) p->data;
    FLUID_SNPRINTF(zone_name, sizeof(zone_name), "%s/%d", preset->name, count);
    zone = new_fluid_preset_zone(zone_name);
    if (zone == NULL) {
      return FLUID_FAILED;
    }
    if (fluid_preset_zone_import_sfont(zone, sfzone, sfont) != FLUID_OK) {
      delete_fluid_preset_zone(zone);
      return FLUID_FAILED;
    }
    if ((count == 0) && (fluid_preset_zone_get_inst(zone) == NULL)) {
      fluid_defpreset_set_global_zone(preset, zone);
    } else if (fluid_defpreset_add_zone(preset, zone) != FLUID_OK) {
      return FLUID_FAILED;
    }
    p = fluid_list_next(p);
    count++;
  }
  return FLUID_OK;
}

/*
 * fluid_defpreset_add_zone
 */
int
fluid_defpreset_add_zone(fluid_defpreset_t* preset, fluid_preset_zone_t* zone)
{
  if (preset->zone == NULL) {
    zone->next = NULL;
    preset->zone = zone;
  } else {
    zone->next = preset->zone;
    preset->zone = zone;
  }
  return FLUID_OK;
}

/*
 * fluid_defpreset_get_zone
 */
fluid_preset_zone_t*
fluid_defpreset_get_zone(fluid_defpreset_t* preset)
{
  return preset->zone;
}

/*
 * fluid_defpreset_get_global_zone
 */
fluid_preset_zone_t*
fluid_defpreset_get_global_zone(fluid_defpreset_t* preset)
{
  return preset->global_zone;
}

/***************************************************************
 *
 *                           PRESET_ZONE
 */

/*
 * fluid_preset_zone_next
 */
fluid_preset_zone_t*
fluid_preset_zone_next(fluid_preset_zone_t* preset)
{
  return preset->next;
}

/*
 * new_fluid_preset_zone
 */
fluid_preset_zone_t*
new_fluid_preset_zone(char *name)
{
  int size;
  fluid_preset_zone_t* zone = NULL;
  zone = FLUID_NEW(fluid_preset_zone_t);
  if (zone == NULL) {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }
  zone->next = NULL;
  size = 1 + FLUID_STRLEN(name);
  zone->name = FLUID_MALLOC(size);
  if (zone->name == NULL) {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    FLUID_FREE(zone);
    return NULL;
  }
  FLUID_STRCPY(zone->name, name);
  zone->inst = NULL;
  zone->range.keylo = 0;
  zone->range.keyhi = 128;
  zone->range.vello = 0;
  zone->range.velhi = 128;
  zone->range.ignore = FALSE; 

  /* Flag all generators as unused (default, they will be set when they are found
   * in the sound font).
   * This also sets the generator values to default, but that is of no concern here.*/
  fluid_gen_set_default_values(&zone->gen[0]);
  zone->mod = NULL; /* list of modulators */
  return zone;
}

/*
 * delete_fluid_preset_zone
 */
void
delete_fluid_preset_zone(fluid_preset_zone_t* zone)
{
  fluid_mod_t *mod, *tmp;

  fluid_return_if_fail(zone != NULL);
  
  mod = zone->mod;
  while (mod)	/* delete the modulators */
    {
      tmp = mod;
      mod = mod->next;
      delete_fluid_mod (tmp);
    }

  FLUID_FREE (zone->name);
  delete_fluid_inst (zone->inst);
  FLUID_FREE(zone);
}

/*
 * fluid_preset_zone_import_sfont
 */
int
fluid_preset_zone_import_sfont(fluid_preset_zone_t* zone, SFPresetZone *sfzone, fluid_defsfont_t* sfont)
{
  fluid_list_t *r;
  SFGen* sfgen;
  int count;
  for (count = 0, r = sfzone->gen; r != NULL; count++) {
    sfgen = (SFGen *) r->data;
    switch (sfgen->id) {
    case GEN_KEYRANGE:
      zone->range.keylo = sfgen->amount.range.lo;
      zone->range.keyhi = sfgen->amount.range.hi;
      break;
    case GEN_VELRANGE:
      zone->range.vello = sfgen->amount.range.lo;
      zone->range.velhi = sfgen->amount.range.hi;
      break;
    case GEN_ATTENUATION:
      /* EMU8k/10k hardware applies a scale factor to initial attenuation generator values set at
       * preset and instrument level */
      zone->gen[sfgen->id].val = (fluid_real_t) sfgen->amount.sword * EMU_ATTENUATION_FACTOR;
      zone->gen[sfgen->id].flags = GEN_SET;
      break;
    default:
      /* FIXME: some generators have an unsigne word amount value but i don't know which ones */
      zone->gen[sfgen->id].val = (fluid_real_t) sfgen->amount.sword;
      zone->gen[sfgen->id].flags = GEN_SET;
      break;
    }
    r = fluid_list_next(r);
  }
  if (sfzone->inst != NULL) {
    zone->inst = (fluid_inst_t*) new_fluid_inst();
    if (zone->inst == NULL) {
      FLUID_LOG(FLUID_ERR, "Out of memory");
      return FLUID_FAILED;
    }
    if (fluid_inst_import_sfont(zone, zone->inst, sfzone->inst, sfont) != FLUID_OK) {
      return FLUID_FAILED;
    }
  }

  /* Import the modulators (only SF2.1 and higher) */
  for (count = 0, r = sfzone->mod; r != NULL; count++) {

    SFMod* mod_src = (SFMod *)r->data;
    fluid_mod_t * mod_dest = new_fluid_mod();
    int type;

    if (mod_dest == NULL){
      return FLUID_FAILED;
    }
    mod_dest->next = NULL; /* pointer to next modulator, this is the end of the list now.*/

    /* *** Amount *** */
    mod_dest->amount = mod_src->amount;

    /* *** Source *** */
    mod_dest->src1 = mod_src->src & 127; /* index of source 1, seven-bit value, SF2.01 section 8.2, page 50 */
    mod_dest->flags1 = 0;

    /* Bit 7: CC flag SF 2.01 section 8.2.1 page 50*/
    if (mod_src->src & (1<<7)){
      mod_dest->flags1 |= FLUID_MOD_CC;
    } else {
      mod_dest->flags1 |= FLUID_MOD_GC;
    }

    /* Bit 8: D flag SF 2.01 section 8.2.2 page 51*/
    if (mod_src->src & (1<<8)){
      mod_dest->flags1 |= FLUID_MOD_NEGATIVE;
    } else {
      mod_dest->flags1 |= FLUID_MOD_POSITIVE;
    }

    /* Bit 9: P flag SF 2.01 section 8.2.3 page 51*/
    if (mod_src->src & (1<<9)){
      mod_dest->flags1 |= FLUID_MOD_BIPOLAR;
    } else {
      mod_dest->flags1 |= FLUID_MOD_UNIPOLAR;
    }

    /* modulator source types: SF2.01 section 8.2.1 page 52 */
    type=(mod_src->src) >> 10;
    type &= 63; /* type is a 6-bit value */
    if (type == 0){
      mod_dest->flags1 |= FLUID_MOD_LINEAR;
    } else if (type == 1){
      mod_dest->flags1 |= FLUID_MOD_CONCAVE;
    } else if (type == 2){
      mod_dest->flags1 |= FLUID_MOD_CONVEX;
    } else if (type == 3){
      mod_dest->flags1 |= FLUID_MOD_SWITCH;
    } else {
      /* This shouldn't happen - unknown type!
       * Deactivate the modulator by setting the amount to 0. */
      mod_dest->amount=0;
    }

    /* *** Dest *** */
    mod_dest->dest = mod_src->dest; /* index of controlled generator */

    /* *** Amount source *** */
    mod_dest->src2 = mod_src->amtsrc & 127; /* index of source 2, seven-bit value, SF2.01 section 8.2, p.50 */
    mod_dest->flags2 = 0;

    /* Bit 7: CC flag SF 2.01 section 8.2.1 page 50*/
    if (mod_src->amtsrc & (1<<7)){
      mod_dest->flags2 |= FLUID_MOD_CC;
    } else {
      mod_dest->flags2 |= FLUID_MOD_GC;
    }

    /* Bit 8: D flag SF 2.01 section 8.2.2 page 51*/
    if (mod_src->amtsrc & (1<<8)){
      mod_dest->flags2 |= FLUID_MOD_NEGATIVE;
    } else {
      mod_dest->flags2 |= FLUID_MOD_POSITIVE;
    }

    /* Bit 9: P flag SF 2.01 section 8.2.3 page 51*/
    if (mod_src->amtsrc & (1<<9)){
      mod_dest->flags2 |= FLUID_MOD_BIPOLAR;
    } else {
      mod_dest->flags2 |= FLUID_MOD_UNIPOLAR;
    }

    /* modulator source types: SF2.01 section 8.2.1 page 52 */
    type = (mod_src->amtsrc) >> 10;
    type &= 63; /* type is a 6-bit value */
    if (type == 0){
      mod_dest->flags2 |= FLUID_MOD_LINEAR;
    } else if (type == 1){
      mod_dest->flags2 |= FLUID_MOD_CONCAVE;
    } else if (type == 2){
      mod_dest->flags2 |= FLUID_MOD_CONVEX;
    } else if (type == 3){
      mod_dest->flags2 |= FLUID_MOD_SWITCH;
    } else {
      /* This shouldn't happen - unknown type!
       * Deactivate the modulator by setting the amount to 0. */
      mod_dest->amount=0;
    }

    /* *** Transform *** */
    /* SF2.01 only uses the 'linear' transform (0).
     * Deactivate the modulator by setting the amount to 0 in any other case.
     */
    if (mod_src->trans !=0){
      mod_dest->amount = 0;
    }

    /* Store the new modulator in the zone The order of modulators
     * will make a difference, at least in an instrument context: The
     * second modulator overwrites the first one, if they only differ
     * in amount. */
    if (count == 0){
      zone->mod = mod_dest;
    } else {
      fluid_mod_t * last_mod = zone->mod;

      /* Find the end of the list */
      while (last_mod->next != NULL){
	last_mod=last_mod->next;
      }

      last_mod->next = mod_dest;
    }

    r = fluid_list_next(r);
  } /* foreach modulator */

  return FLUID_OK;
}

/*
 * fluid_preset_zone_get_inst
 */
fluid_inst_t*
fluid_preset_zone_get_inst(fluid_preset_zone_t* zone)
{
  return zone->inst;
}


/***************************************************************
 *
 *                           INST
 */

/*
 * new_fluid_inst
 */
fluid_inst_t*
new_fluid_inst()
{
  fluid_inst_t* inst = FLUID_NEW(fluid_inst_t);
  if (inst == NULL) {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }
  inst->name[0] = 0;
  inst->global_zone = NULL;
  inst->zone = NULL;
  return inst;
}

/*
 * delete_fluid_inst
 */
void
delete_fluid_inst(fluid_inst_t* inst)
{
  fluid_inst_zone_t* zone;
  
  fluid_return_if_fail(inst != NULL);
  
    delete_fluid_inst_zone(inst->global_zone);
    inst->global_zone = NULL;
    
  zone = inst->zone;
  while (zone != NULL) {
    inst->zone = zone->next;
    delete_fluid_inst_zone(zone);
    zone = inst->zone;
  }
  FLUID_FREE(inst);
}

/*
 * fluid_inst_set_global_zone
 */
int
fluid_inst_set_global_zone(fluid_inst_t* inst, fluid_inst_zone_t* zone)
{
  inst->global_zone = zone;
  return FLUID_OK;
}

/*
 * fluid_inst_import_sfont
 */
int
fluid_inst_import_sfont(fluid_preset_zone_t* zonePZ, fluid_inst_t* inst, 
						SFInst *sfinst, fluid_defsfont_t* sfont)
{
  fluid_list_t *p;
  SFInstZone* sfzone;
  fluid_inst_zone_t* zone;
  char zone_name[256];
  int count;

  p = sfinst->zone;
  if (FLUID_STRLEN(sfinst->name) > 0) {
    FLUID_STRCPY(inst->name, sfinst->name);
  } else {
    FLUID_STRCPY(inst->name, "<untitled>");
  }

  count = 0;
  while (p != NULL) {

    sfzone = (SFInstZone *) p->data;
    FLUID_SNPRINTF(zone_name, sizeof(zone_name), "%s/%d", inst->name, count);

    zone = new_fluid_inst_zone(zone_name);
    if (zone == NULL) {
      return FLUID_FAILED;
    }

    if (fluid_inst_zone_import_sfont(zonePZ,zone, sfzone, sfont) != FLUID_OK) {
      delete_fluid_inst_zone(zone);
      return FLUID_FAILED;
    }

    if ((count == 0) && (fluid_inst_zone_get_sample(zone) == NULL)) {
      fluid_inst_set_global_zone(inst, zone);

    } else if (fluid_inst_add_zone(inst, zone) != FLUID_OK) {
      return FLUID_FAILED;
    }

    p = fluid_list_next(p);
    count++;
  }
  return FLUID_OK;
}

/*
 * fluid_inst_add_zone
 */
int
fluid_inst_add_zone(fluid_inst_t* inst, fluid_inst_zone_t* zone)
{
  if (inst->zone == NULL) {
    zone->next = NULL;
    inst->zone = zone;
  } else {
    zone->next = inst->zone;
    inst->zone = zone;
  }
  return FLUID_OK;
}

/*
 * fluid_inst_get_zone
 */
fluid_inst_zone_t*
fluid_inst_get_zone(fluid_inst_t* inst)
{
  return inst->zone;
}

/*
 * fluid_inst_get_global_zone
 */
fluid_inst_zone_t*
fluid_inst_get_global_zone(fluid_inst_t* inst)
{
  return inst->global_zone;
}

/***************************************************************
 *
 *                           INST_ZONE
 */

/*
 * new_fluid_inst_zone
 */
fluid_inst_zone_t*
new_fluid_inst_zone(char* name)
{
  int size;
  fluid_inst_zone_t* zone = NULL;
  zone = FLUID_NEW(fluid_inst_zone_t);
  if (zone == NULL) {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }
  zone->next = NULL;
  size = 1 + FLUID_STRLEN(name);
  zone->name = FLUID_MALLOC(size);
  if (zone->name == NULL) {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    FLUID_FREE(zone);
    return NULL;
  }
  FLUID_STRCPY(zone->name, name);
  zone->sample = NULL;
  zone->range.keylo = 0;
  zone->range.keyhi = 128;
  zone->range.vello = 0;
  zone->range.velhi = 128;
  zone->range.ignore = FALSE;
  /* Flag the generators as unused.
   * This also sets the generator values to default, but they will be overwritten anyway, if used.*/
  fluid_gen_set_default_values(&zone->gen[0]);
  zone->mod=NULL; /* list of modulators */
  return zone;
}

/*
 * delete_fluid_inst_zone
 */
void
delete_fluid_inst_zone(fluid_inst_zone_t* zone)
{
  fluid_mod_t *mod, *tmp;

  fluid_return_if_fail(zone != NULL);
  
  mod = zone->mod;
  while (mod)	/* delete the modulators */
    {
      tmp = mod;
      mod = mod->next;
      delete_fluid_mod (tmp);
    }

  FLUID_FREE (zone->name);
  FLUID_FREE(zone);
}

/*
 * fluid_inst_zone_next
 */
fluid_inst_zone_t*
fluid_inst_zone_next(fluid_inst_zone_t* zone)
{
  return zone->next;
}

/*
 * fluid_inst_zone_import_sfont
 */
int
fluid_inst_zone_import_sfont(fluid_preset_zone_t* preset_zone, fluid_inst_zone_t* zone,
                             SFInstZone *sfzone, fluid_defsfont_t* sfont)
{
  fluid_list_t *r;
  SFGen* sfgen;
  int count;

  for (count = 0, r = sfzone->gen; r != NULL; count++) {
    sfgen = (SFGen *) r->data;
    switch (sfgen->id) {
    case GEN_KEYRANGE:
      zone->range.keylo = sfgen->amount.range.lo;
      zone->range.keyhi = sfgen->amount.range.hi;
      break;
    case GEN_VELRANGE:
      zone->range.vello = sfgen->amount.range.lo;
      zone->range.velhi = sfgen->amount.range.hi;
      break;
    case GEN_ATTENUATION:
      /* EMU8k/10k hardware applies a scale factor to initial attenuation generator values set at
       * preset and instrument level */
      zone->gen[sfgen->id].val = (fluid_real_t) sfgen->amount.sword * EMU_ATTENUATION_FACTOR;
      zone->gen[sfgen->id].flags = GEN_SET;
      break;
    default:
      /* FIXME: some generators have an unsigned word amount value but
	 i don't know which ones */
      zone->gen[sfgen->id].val = (fluid_real_t) sfgen->amount.sword;
      zone->gen[sfgen->id].flags = GEN_SET;
      break;
    }
    r = fluid_list_next(r);
  }
  
  /* adjust instrument zone keyrange to integrate preset zone keyrange */
  if (preset_zone->range.keylo > zone->range.keylo) zone->range.keylo = preset_zone->range.keylo;
  if (preset_zone->range.keyhi < zone->range.keyhi) zone->range.keyhi = preset_zone->range.keyhi;
  /* adjust instrument zone to integrate  preset zone velrange */
  if (preset_zone->range.vello > zone->range.vello) zone->range.vello = preset_zone->range.vello;
  if (preset_zone->range.velhi < zone->range.velhi) zone->range.velhi = preset_zone->range.velhi;

  /* FIXME */
/*    if (zone->gen[GEN_EXCLUSIVECLASS].flags == GEN_SET) { */
/*      FLUID_LOG(FLUID_DBG, "ExclusiveClass=%d\n", (int) zone->gen[GEN_EXCLUSIVECLASS].val); */
/*    } */

  /* fixup sample pointer */
  if (sfzone->sample != NULL)
      zone->sample = fluid_defsfont_sample_by_idx(sfont, sfzone->sample->idx);

  /* Import the modulators (only SF2.1 and higher) */
  for (count = 0, r = sfzone->mod; r != NULL; count++) {
    SFMod* mod_src = (SFMod *) r->data;
    int type;
    fluid_mod_t* mod_dest;

    mod_dest = new_fluid_mod();
    if (mod_dest == NULL){
      return FLUID_FAILED;
    }

    mod_dest->next = NULL; /* pointer to next modulator, this is the end of the list now.*/

    /* *** Amount *** */
    mod_dest->amount = mod_src->amount;

    /* *** Source *** */
    mod_dest->src1 = mod_src->src & 127; /* index of source 1, seven-bit value, SF2.01 section 8.2, page 50 */
    mod_dest->flags1 = 0;

    /* Bit 7: CC flag SF 2.01 section 8.2.1 page 50*/
    if (mod_src->src & (1<<7)){
      mod_dest->flags1 |= FLUID_MOD_CC;
    } else {
      mod_dest->flags1 |= FLUID_MOD_GC;
    }

    /* Bit 8: D flag SF 2.01 section 8.2.2 page 51*/
    if (mod_src->src & (1<<8)){
      mod_dest->flags1 |= FLUID_MOD_NEGATIVE;
    } else {
      mod_dest->flags1 |= FLUID_MOD_POSITIVE;
    }

    /* Bit 9: P flag SF 2.01 section 8.2.3 page 51*/
    if (mod_src->src & (1<<9)){
      mod_dest->flags1 |= FLUID_MOD_BIPOLAR;
    } else {
      mod_dest->flags1 |= FLUID_MOD_UNIPOLAR;
    }

    /* modulator source types: SF2.01 section 8.2.1 page 52 */
    type = (mod_src->src) >> 10;
    type &= 63; /* type is a 6-bit value */
    if (type == 0){
      mod_dest->flags1 |= FLUID_MOD_LINEAR;
    } else if (type == 1){
      mod_dest->flags1 |= FLUID_MOD_CONCAVE;
    } else if (type == 2){
      mod_dest->flags1 |= FLUID_MOD_CONVEX;
    } else if (type == 3){
      mod_dest->flags1 |= FLUID_MOD_SWITCH;
    } else {
      /* This shouldn't happen - unknown type!
       * Deactivate the modulator by setting the amount to 0. */
      mod_dest->amount = 0;
    }

    /* *** Dest *** */
    mod_dest->dest=mod_src->dest; /* index of controlled generator */

    /* *** Amount source *** */
    mod_dest->src2=mod_src->amtsrc & 127; /* index of source 2, seven-bit value, SF2.01 section 8.2, page 50 */
    mod_dest->flags2 = 0;

    /* Bit 7: CC flag SF 2.01 section 8.2.1 page 50*/
    if (mod_src->amtsrc & (1<<7)){
      mod_dest->flags2 |= FLUID_MOD_CC;
    } else {
      mod_dest->flags2 |= FLUID_MOD_GC;
    }

    /* Bit 8: D flag SF 2.01 section 8.2.2 page 51*/
    if (mod_src->amtsrc & (1<<8)){
      mod_dest->flags2 |= FLUID_MOD_NEGATIVE;
    } else {
      mod_dest->flags2 |= FLUID_MOD_POSITIVE;
    }

    /* Bit 9: P flag SF 2.01 section 8.2.3 page 51*/
    if (mod_src->amtsrc & (1<<9)){
      mod_dest->flags2 |= FLUID_MOD_BIPOLAR;
    } else {
      mod_dest->flags2 |= FLUID_MOD_UNIPOLAR;
    }

    /* modulator source types: SF2.01 section 8.2.1 page 52 */
    type=(mod_src->amtsrc) >> 10;
    type &= 63; /* type is a 6-bit value */
    if (type == 0){
      mod_dest->flags2 |= FLUID_MOD_LINEAR;
    } else if (type == 1){
      mod_dest->flags2 |= FLUID_MOD_CONCAVE;
    } else if (type == 2){
      mod_dest->flags2 |= FLUID_MOD_CONVEX;
    } else if (type == 3){
      mod_dest->flags2 |= FLUID_MOD_SWITCH;
    } else {
      /* This shouldn't happen - unknown type!
       * Deactivate the modulator by setting the amount to 0. */
      mod_dest->amount = 0;
    }

    /* *** Transform *** */
    /* SF2.01 only uses the 'linear' transform (0).
     * Deactivate the modulator by setting the amount to 0 in any other case.
     */
    if (mod_src->trans !=0){
      mod_dest->amount = 0;
    }

    /* Store the new modulator in the zone
     * The order of modulators will make a difference, at least in an instrument context:
     * The second modulator overwrites the first one, if they only differ in amount. */
    if (count == 0){
      zone->mod=mod_dest;
    } else {
      fluid_mod_t * last_mod=zone->mod;
      /* Find the end of the list */
      while (last_mod->next != NULL){
	last_mod=last_mod->next;
      }
      last_mod->next=mod_dest;
    }

    r = fluid_list_next(r);
  } /* foreach modulator */
  return FLUID_OK;
}

/*
 * fluid_inst_zone_get_sample
 */
fluid_sample_t*
fluid_inst_zone_get_sample(fluid_inst_zone_t* zone)
{
  return zone->sample;
}


int
fluid_zone_inside_range(fluid_zone_range_t* range, int key, int vel)
{
    /* ignoreInstrumentZone is set in mono legato playing */
    int ignore_zone = range->ignore;
    
    /* Reset the 'ignore' request */
    range->ignore = FALSE;
    
  return !ignore_zone && ((range->keylo <= key) &&
	  (range->keyhi >= key) &&
	  (range->vello <= vel) &&
	  (range->velhi >= vel));
}

/***************************************************************
 *
 *                           SAMPLE
 */

/*
 * fluid_sample_in_rom
 */
int
fluid_sample_in_rom(fluid_sample_t* sample)
{
  return (sample->sampletype & FLUID_SAMPLETYPE_ROM);
}

/*
 * fluid_sample_import_sfont
 */
int
fluid_sample_import_sfont(fluid_sample_t* sample, SFSample* sfsample, fluid_defsfont_t* sfont)
{
  FLUID_STRCPY(sample->name, sfsample->name);
  sample->idx = sfsample->idx;
  sample->data = sfont->sampledata;
  sample->data24 = sfont->sample24data;
  sample->start = sfsample->start;
  sample->end = (sfsample->end > 0) ? sfsample->end - 1 : 0; /* marks last sample, contrary to SF spec. */
  sample->loopstart = sfsample->loopstart;
  sample->loopend = sfsample->loopend;
  sample->samplerate = sfsample->samplerate;
  sample->origpitch = sfsample->origpitch;
  sample->pitchadj = sfsample->pitchadj;
  sample->sampletype = sfsample->sampletype;

  if (fluid_sample_validate(sample, sfont->samplesize) == FLUID_FAILED)
  {
      return FLUID_OK;
  }

  if ((sample->sampletype & FLUID_SAMPLETYPE_OGG_VORBIS)
      && fluid_sample_decompress_vorbis(sample) == FLUID_FAILED)
  {
      return FLUID_FAILED;
  }

  fluid_sample_sanitize_loop(sample, sfont->samplesize);

  return FLUID_OK;
}
