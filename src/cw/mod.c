/*
    This file is part of actube.

    actube is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libcapwap is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.

*/

/**
 * @file 
 * @brief Functions for modules (mods) management.
 */

#include <string.h>
#include <errno.h>
#include <dlfcn.h>


#include "action.h"
#include "mavl.h"
#include "dbg.h"
#include "log.h"


static void (*actions_registered_cb) (struct cw_Mod * capwap, struct cw_Mod * bindings,
				      struct cw_actiondef * actions) = NULL;





void mod_set_actions_registered_cb(void (*fun)
				    (struct cw_Mod *, struct cw_Mod *,
				     struct cw_actiondef *))
{
	actions_registered_cb = fun;
}


struct cache_item {
	const char *capwap;
	const char *bindings;
	struct cw_actiondef actions;
	struct cw_MsgSet * msgset;
};

static struct mavl *cache = NULL;

static int mod_null_register_actions(struct cw_actiondef *def, int mode)
{
	return 0;
}

/**
 * mod_null is a dummy mod 
 */
struct cw_Mod mod_null = {
	.name = "none",
	.register_actions = mod_null_register_actions,
	
};




static int cmp(const void *p1, const void *p2)
{
	struct cache_item *c1 = (struct cache_item *) p1;
	struct cache_item *c2 = (struct cache_item *) p2;

	int r;
	r = strcmp(c1->capwap, c2->capwap);
	if (r != 0)
		return r;

	return strcmp(c1->bindings, c2->bindings);
}

struct cw_actiondef *mod_cache_get(const char *capwap, const char *bindings)
{
	return NULL;
}


struct cw_actiondef *mod_cache_add(struct conn *conn, struct cw_Mod *c, struct cw_Mod *b)
{
	if (!cache) {
		cache = mavl_create(cmp, NULL);
		if (!cache) {
			cw_log(LOG_ERR, "Can't initialize mod cache: %s",
			       strerror(errno));
			return NULL;
		}
	}

	struct cache_item s;
	s.capwap = c->name;
	s.bindings = b->name;

	struct cache_item *i = mavl_get(cache, &s);
	if (i) {
		cw_dbg(DBG_INFO, "Using cached actions for %s,%s", c->name, b->name);
		return &(i->actions);
	}


	i = malloc(sizeof(struct cache_item));
	if (!i) {
		cw_log(LOG_ERR, "Can't allocate memory for mod cache item %s",
		       strerror(errno));
		return NULL;
	}

	cw_dbg(DBG_INFO, "Loading actions for %s,%s", c->name, b->name);
	memset(i, 0, sizeof(struct cache_item));
	if (c) {
		i->capwap = c->name;
		c->register_actions(&(i->actions), CW_MOD_MODE_CAPWAP);
	}
	if (b) {
		i->bindings = b->name;
		b->register_actions(&(i->actions), MOD_MODE_BINDINGS);
	}

	if (actions_registered_cb)
		actions_registered_cb(c, b, &(i->actions));

	mavl_add(cache, i);
	return &(i->actions);
}



/* static mavl to store modules */
static struct mavl * modlist = NULL;
static int mod_cmp(const void *e1, const void *e2){
	struct cw_Mod * m1 = e1;
	struct cw_Mod * m2 = e2;
	return strcmp(m1->name,m2->name);
}

static const char * mod_path="./";

void cw_mod_set_mod_path(const char * path){
	mod_path = path;
}

static int cw_mod_add(struct cw_Mod * mod ){
	if (modlist == NULL){
		modlist = mavl_create(mod_cmp,NULL);
		if (modlist==NULL){
			return 0;
		}
	}
	//mavl_add(modlist,modfn);
	return 1;
}

/**
 * @brief Load a module 
 * @param path
 * @param mod_name
 * @return 
 */
struct cw_Mod * cw_mod_load(const char * mod_name){
	/* if modlist is not initialized, initialize ... */
	if (modlist==NULL){
		modlist=mavl_create(mod_cmp,NULL);
		if (modlist==NULL){
			cw_log(LOG_ERROR, "Can't init modlist, no memory");
			return NULL;
		}
	}

	/* Search for the module in modlist, to see if it is
	 * already loaded or was statically linked */
	struct cw_Mod search;
	memset(&search,0,sizeof(search));
	search.name=mod_name;
	struct cw_Mod * mod;
	mod = mavl_find(modlist,&search);
	if (mod){
		return mod;
	}

	if (strlen(mod_name)>CW_MOD_MAX_MOD_NAME_LEN){
		cw_log(LOG_ERROR,"Mod name too long: %s (max allowed = %d",
			mod_name,CW_MOD_MAX_MOD_NAME_LEN);
		return NULL;
	}
	
	char mod_filename[CW_MOD_MAX_MOD_NAME_LEN+5];
	sprintf(mod_filename,"mod_%s",mod_name);

	/* we have to load the module dynamically */
	char * filename;
	filename = cw_filename(mod_path,mod_filename,".so");
	if (filename==NULL)
		return NULL;

	/* Open the DLL */
	void * handle;
	handle = dlopen(filename,RTLD_NOW);
	
	if (!handle){
		cw_log(LOG_ERROR,"Failed to load module: %s",dlerror());
		goto errX;
	}

	struct cw_Mod * (*mod_get_interface)();
	
	mod_get_interface = dlsym(handle,"mod_get_interface");
	
	if (!mod_get_interface){
		cw_log(LOG_ERROR,"Failed to load module: %s",dlerror());
		goto errX;
	}

	mod = mod_get_interface();
	mod->init();
errX:
	free(filename);
	return mod;
}