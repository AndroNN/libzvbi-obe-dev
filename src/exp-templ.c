/*
 *  Template for export modules
 *
 *  Placed in the public domain.
 */

/* $Id: exp-templ.c,v 1.1 2002/01/12 16:18:37 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "export.h"

typedef struct tmpl_instance
{
	/* Common to all export modules */
	vbi_export		export;

	/* Our private stuff */

	/* Options */
	int			flip;
	int			day;
	int			prime;
	double			quality;
	char *			comment;
	int			weekday;

	int			counter;
} tmpl_instance;

static vbi_export *
tmpl_new(void)
{
	tmpl_instance *tmpl;

	if (!(tmpl = calloc(1, sizeof(*tmpl))))
		return NULL;

	/*
	 *  The caller will initialize tmpl->export.class for us
	 *  and reset all options to their defaults, we only
	 *  have to initialize our private stuff.
	 */

	tmpl->counter = 0;

	return &tmpl->export;
}

static void
tmpl_delete(vbi_export *e)
{
	/* convert e -> tmpl_instance.export to -> tmpl_instance
           (one could also "tmpl = (tmpl_instance *) e;" but it's less safe) */
	tmpl_instance *tmpl = PARENT(e, tmpl_instance, export);

	/* Uninitialize our private stuff and options */

	if (tmpl->comment)
		free(tmpl->comment);

	free(tmpl);
}

/* convenience */
#define elements(array) (sizeof(array) / sizeof(array[0]))

/* N_(), _() are NLS functions, see info gettext. */
static const char *
string_menu_items[] = {
	N_("Sunday"), N_("Monday"), N_("Tuesday"),
	N_("Wednesday"), N_("Thursday"), N_("Friday"), N_("Saturday")
};

static int
int_menu_items[] = {
	1, 3, 5, 7, 11, 13, 17, 19
};

static vbi_option_info
tmpl_options[] = {
	VBI_OPTION_BOOL_INITIALIZER
	  /*
	   *  Option keywords must be unique within their module
	   *  and shall contain only "AZaz09-_" (be filesystem safe that is).
	   *  Note "network", "creator" and "reveal" are reserved generic
	   *  options, filtered by the export api functions.
	   */
	  ("flip", N_("Boolean option"),
           FALSE, N_("This is a boolean option")),
	VBI_OPTION_INT_RANGE_INITIALIZER
	  ("day", N_("Select a month day"),
	   /* default, min, max, step, has no tooltip */
	      13,       1,   31,  1,      NULL),
	VBI_OPTION_INT_MENU_INITIALIZER
	  ("prime", N_("Select a prime"),
	   0, int_menu_items, elements(int_menu_items),
	   N_("Default is the first, '1'")),
	VBI_OPTION_REAL_RANGE_INITIALIZER
	  ("quality", N_("Compression quality"),
	   100, 1, 100, 0.01, NULL),
	/* VBI_OPTION_REAL_MENU_INITIALIZER like int */
	VBI_OPTION_STRING_INITIALIZER
	  ("comment", N_("Add a comment"),
	   "default comment", N_("Another tooltip")),
	VBI_OPTION_MENU_INITIALIZER
	  ("weekday", N_("Select a weekday"),
	   2, string_menu_items, 7, N_("Default is Tuesday"))
};

/*
 *  Enumerate our options (optional if we have no options).
 *  Instead of using a table one could also dynamically create
 *  vbi_option_info's in tmpl_instance.
 */
static vbi_option_info *
option_enum(vbi_export *e, int index)
{
	/* Enumeration 0 ... n */
	if (index < 0 || index >= elements(tmpl_options))
		return NULL;

	return tmpl_options + index;
}

/*
 *  Get an option (optional if we have no options).
 */
static vbi_bool
option_get(vbi_export *e, const char *keyword, vbi_option_value *value)
{
	tmpl_instance *tmpl = PARENT(e, tmpl_instance, export);

	if (strcmp(keyword, "flip") == 0) {
		value->num = tmpl->flip;
	} else if (strcmp(keyword, "day") == 0) {
		value->num = tmpl->day;
	} else if (strcmp(keyword, "prime") == 0) {
		value->num = tmpl->prime;
	} else if (strcmp(keyword, "quality") == 0) {
		value->dbl = tmpl->quality;
	} else if (strcmp(keyword, "comment") == 0) {
		if (!(value->str = vbi_export_strdup(e, NULL,
			tmpl->comment ? tmpl->comment : "")))
			return FALSE;
	} else if (strcmp(keyword, "weekday") == 0) {
		value->num = tmpl->weekday;
	} else {
		vbi_export_unknown_option(e, keyword);
		return FALSE;
	}

	return TRUE; /* success */
}

/*
 *  Set an option (optional if we have no options).
 */
static vbi_bool
option_set(vbi_export *e, const char *keyword, va_list args)
{
	tmpl_instance *tmpl = PARENT(e, tmpl_instance, export);

	if (strcmp(keyword, "flip") == 0) {
		tmpl->flip = !!va_arg(args, int);
	} else if (strcmp(keyword, "day") == 0) {
		int day = va_arg(args, int);

		/* or clamp */
		if (day < 1 || day > 31) {
			vbi_export_invalid_option(e, keyword, day);
			return FALSE;
		}
		tmpl->day = day;
	} else if (strcmp(keyword, "prime") == 0) {
		int i, value = va_arg(args, int);
		unsigned int d, dmin = UINT_MAX;

		/* or return an error */
		for (i = 0; i < elements(int_menu_items); i++)
			if ((d = abs(int_menu_items[i] - value)) < dmin) {
				tmpl->prime = int_menu_items[i];
				dmin = d;
			}
	} else if (strcmp(keyword, "quality") == 0) {
		double quality = va_arg(args, double);

		/* or return an error */
		if (quality < 1)
			quality = 1;
		else if (quality > 100)
			quality = 100;

		tmpl->quality = quality;
	} else if (strcmp(keyword, "comment") == 0) {
		char *comment = va_arg(args, char *);

		/* Note the option remains unchanged in case of error */
		if (!vbi_export_strdup(e, &tmpl->comment, comment))
			return FALSE;
	} else if (strcmp(keyword, "weekday") == 0) {
		int day = va_arg(args, int);

		/* or return an error */
		tmpl->weekday = day % 7;
	} else {
		vbi_export_unknown_option(e, keyword);
		return FALSE;
	}

	return TRUE; /* success */
}

/*
 *  The output function, mandatory.
 */
static vbi_bool
export(vbi_export *e, FILE *fp, vbi_page *pg)
{
	tmpl_instance *tmpl = PARENT(e, tmpl_instance, export);

	/*
	 *  Write @pg to @fp, that's all.
	 */

	tmpl->counter++; /* just for fun */

	/*
	 *  Should any of the module functions return unsuccessful
	 *  they better post a description of the problem.
	 *  Parameters like printf, no linefeeds '\n' please.
	 */
	/*
	vbi_export_error_printf(_("Writing failed: %s"), strerror(errno));
         */

	return FALSE; /* no success (since we didn't write anything) */
}

/*
 *  Let's describe us.
 *  You can leave away assignments unless mandatory.
 */
vbi_export_class
vbi_export_class_tmpl = {
	.public = {
		/* The mandatory keyword must be unique and shall
                   contain only "AZaz09-_" */
		.keyword	= "templ",
		.label		= N_("Template"),
		.tooltip	= N_("This is just an export template"),

		.mime_type	= "misc/example",
		.extension	= "tmpl",
	},

	/* Functions to allocate and free a tmpl_class vbi_export instance.
	   When you omit these, libzvbi will allocate a bare struct vbi_export */
	.new			= tmpl_new,
	.delete			= tmpl_delete,

	/* Functions to enumerate, read and write options */
	.option_enum		= option_enum,
	.option_get		= option_get,
	.option_set		= option_set,

	/* Function to export a page, mandatory */
	.export			= export
};

/*
 *  This is a constructor calling vbi_register_export_module().
 *  (Commented out since we don't want the example module listed.)
 */
#if 1
VBI_AUTOREG_EXPORT_MODULE(vbi_export_class_tmpl)
#endif
