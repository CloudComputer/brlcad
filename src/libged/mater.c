/*                         M A T E R . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file mater.c
 *
 * The mater command.
 *
 */

#include "ged.h"


int
ged_mater(struct rt_wdb	*wdbp, int argc, const char *argv[])
{
    int status = GED_OK;
    register struct directory *dp;
    int r=0, g=0, b=0;
    char inherit;
    struct rt_db_internal	intern;
    struct rt_comb_internal	*comb;
    static const char *usage = "region_name shader r g b inherit";

    GED_CHECK_DBI_NULL(wdbp->dbip, GED_ERROR);
    GED_CHECK_READ_ONLY(wdbp->dbip, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&wdbp->wdb_result_str, 0);
    wdbp->wdb_result = GED_RESULT_NULL;
    wdbp->wdb_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	wdbp->wdb_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&wdbp->wdb_result_str, "Usage: %s %s", argv[0], usage);
	return GED_OK;
    }

    if (argc != 7) {
	bu_vls_printf(&wdbp->wdb_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if ((dp = db_lookup(wdbp->dbip,  argv[1], LOOKUP_NOISY)) == DIR_NULL) {
	bu_vls_printf(&wdbp->wdb_result_str, "%s: not found", argv[1]);
	return GED_ERROR;
    }

    if ((dp->d_flags & DIR_COMB) == 0) {
	bu_vls_printf(&wdbp->wdb_result_str, "%s: not a combination", dp->d_namep);
	return GED_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(&wdbp->wdb_result_str, "Database read error, aborting");
	return GED_ERROR;
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    /* Material */
    bu_vls_trunc(&comb->shader, 0);
    if (bu_shader_to_tcl_list(argv[2], &comb->shader)) {
	bu_vls_printf(&wdbp->wdb_result_str, "Problem with shader string: %s", argv[2]);
	rt_db_free_internal(&intern, &rt_uniresource);
	return GED_ERROR;
    }

    /* Color */
    if (sscanf(argv[3], "%d", &r) != 1 || r < 0 || 255 < r) {
	bu_vls_printf(&wdbp->wdb_result_str, "Bad color value - %s", argv[3]);
	rt_db_free_internal(&intern, &rt_uniresource);
	return GED_ERROR;
    }

    if (sscanf(argv[4], "%d", &g) != 1 || g < 0 || 255 < g) {
	bu_vls_printf(&wdbp->wdb_result_str, "Bad color value - %s", argv[4]);
	rt_db_free_internal(&intern, &rt_uniresource);
	return GED_ERROR;
    }

    if (sscanf(argv[5], "%d", &b) != 1 || b < 0 || 255 < b) {
	bu_vls_printf(&wdbp->wdb_result_str, "Bad color value - %s", argv[5]);
	rt_db_free_internal(&intern, &rt_uniresource);
	return GED_ERROR;
    }

    comb->rgb[0] = r;
    comb->rgb[1] = g;
    comb->rgb[2] = b;
    comb->rgb_valid = 1;

    inherit = *argv[6];

    switch (inherit) {
	case '1':
	    comb->inherit = 1;
	    break;
	case '0':
	    comb->inherit = 0;
	    break;
	default:
	    bu_vls_printf(&wdbp->wdb_result_str, "Inherit value must be 0 or 1");
	    rt_db_free_internal(&intern, &rt_uniresource);
	    return GED_ERROR;
    }

    if (rt_db_put_internal(dp, wdbp->dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(&wdbp->wdb_result_str, "Database write error, aborting\n");
	return GED_ERROR;
    }

    return GED_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
