/* Do not edit this file. It is produced from the corresponding .m4 source */
/*
 *	Copyright 1996, University Corporation for Atmospheric Research
 *      See netcdf/COPYRIGHT file for copying and redistribution conditions.
 */

#include "nc3internal.h"
#include "ncdispatch.h"
#include "nc3dispatch.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ncx.h"
#include "fbits.h"
#include "rnd.h"
#include "utf8proc.h"


/*
 * Free attr
 * Formerly
NC_free_attr()
 */
void
free_NC_attr(NC_attr *attrp)
{

	if(attrp == NULL)
		return;
	free_NC_string(attrp->name);
	free(attrp);
}


/*
 * How much space will 'nelems' of 'type' take in
 *  external representation (as the values of an attribute)?
 */
static size_t
ncx_len_NC_attrV(nc_type type, size_t nelems)
{
	switch(type) {
	case NC_BYTE:
	case NC_CHAR:
		return ncx_len_char(nelems);
	case NC_SHORT:
		return ncx_len_short(nelems);
	case NC_INT:
		return ncx_len_int(nelems);
	case NC_FLOAT:
		return ncx_len_float(nelems);
	case NC_DOUBLE:
		return ncx_len_double(nelems);
	case NC_UBYTE:
		return ncx_len_ubyte(nelems);
	case NC_USHORT:
		return ncx_len_ushort(nelems);
	case NC_UINT:
		return ncx_len_uint(nelems);
	case NC_INT64:
		return ncx_len_int64(nelems);
	case NC_UINT64:
		return ncx_len_uint64(nelems);
	default:
	        assert("ncx_len_NC_attr bad type" == 0);
	}
	return 0;
}


NC_attr *
new_x_NC_attr(
	NC_string *strp,
	nc_type type,
	size_t nelems)
{
	NC_attr *attrp;
	const size_t xsz = ncx_len_NC_attrV(type, nelems);
	size_t sz = M_RNDUP(sizeof(NC_attr));

	assert(!(xsz == 0 && nelems != 0));

	sz += xsz;

	attrp = (NC_attr *) malloc(sz);
	if(attrp == NULL )
		return NULL;

	attrp->xsz = xsz;

	attrp->name = strp;
	attrp->type = type;
	attrp->nelems = nelems;
	if(xsz != 0)
		attrp->xvalue = (char *)attrp + M_RNDUP(sizeof(NC_attr));
	else
		attrp->xvalue = NULL;

	return(attrp);
}


/*
 * Formerly
NC_new_attr(name,type,count,value)
 */
static NC_attr *
new_NC_attr(
	const char *uname,
	nc_type type,
	size_t nelems)
{
	NC_string *strp;
	NC_attr *attrp;

	char *name = (char *)utf8proc_NFC((const unsigned char *)uname);
	if(name == NULL)
	    return NULL;
	assert(name != NULL && *name != 0);

	strp = new_NC_string(strlen(name), name);
	free(name);
	if(strp == NULL)
		return NULL;

	attrp = new_x_NC_attr(strp, type, nelems);
	if(attrp == NULL)
	{
		free_NC_string(strp);
		return NULL;
	}

	return(attrp);
}


static NC_attr *
dup_NC_attr(const NC_attr *rattrp)
{
	NC_attr *attrp = new_NC_attr(rattrp->name->cp,
		 rattrp->type, rattrp->nelems);
	if(attrp == NULL)
		return NULL;
    if(attrp->xvalue != NULL && rattrp->xvalue != NULL)
    	(void) memcpy(attrp->xvalue, rattrp->xvalue, rattrp->xsz);
	return attrp;
}

/* attrarray */

/*
 * Free the stuff "in" (referred to by) an NC_attrarray.
 * Leaves the array itself allocated.
 */
void
free_NC_attrarrayV0(NC_attrarray *ncap)
{
	assert(ncap != NULL);

	if(ncap->nelems == 0)
		return;

	assert(ncap->value != NULL);

	{
		NC_attr **app = ncap->value;
		NC_attr *const *const end = &app[ncap->nelems];
		for( /*NADA*/; app < end; app++)
		{
			free_NC_attr(*app);
			*app = NULL;
		}
	}
	ncap->nelems = 0;
}


/*
 * Free NC_attrarray values.
 * formerly
NC_free_array()
 */
void
free_NC_attrarrayV(NC_attrarray *ncap)
{
	assert(ncap != NULL);

	if(ncap->nalloc == 0)
		return;

	assert(ncap->value != NULL);

	free_NC_attrarrayV0(ncap);

	free(ncap->value);
	ncap->value = NULL;
	ncap->nalloc = 0;
}


int
dup_NC_attrarrayV(NC_attrarray *ncap, const NC_attrarray *ref)
{
	int status = NC_NOERR;

	assert(ref != NULL);
	assert(ncap != NULL);

	if(ref->nelems != 0)
	{
		const size_t sz = ref->nelems * sizeof(NC_attr *);
		ncap->value = (NC_attr **) malloc(sz);
		if(ncap->value == NULL)
			return NC_ENOMEM;

		(void) memset(ncap->value, 0, sz);
		ncap->nalloc = ref->nelems;
	}

	ncap->nelems = 0;
	{
		NC_attr **app = ncap->value;
		const NC_attr **drpp = (const NC_attr **)ref->value;
		NC_attr *const *const end = &app[ref->nelems];
		for( /*NADA*/; app < end; drpp++, app++, ncap->nelems++)
		{
			*app = dup_NC_attr(*drpp);
			if(*app == NULL)
			{
				status = NC_ENOMEM;
				break;
			}
		}
	}

	if(status != NC_NOERR)
	{
		free_NC_attrarrayV(ncap);
		return status;
	}

	assert(ncap->nelems == ref->nelems);

	return NC_NOERR;
}


/*
 * Add a new handle on the end of an array of handles
 * Formerly
NC_incr_array(array, tail)
 */
static int
incr_NC_attrarray(NC_attrarray *ncap, NC_attr *newelemp)
{
	NC_attr **vp;

	assert(ncap != NULL);

	if(ncap->nalloc == 0)
	{
		assert(ncap->nelems == 0);
		vp = (NC_attr **) malloc(NC_ARRAY_GROWBY * sizeof(NC_attr *));
		if(vp == NULL)
			return NC_ENOMEM;

		ncap->value = vp;
		ncap->nalloc = NC_ARRAY_GROWBY;
	}
	else if(ncap->nelems +1 > ncap->nalloc)
	{
		vp = (NC_attr **) realloc(ncap->value,
			(ncap->nalloc + NC_ARRAY_GROWBY) * sizeof(NC_attr *));
		if(vp == NULL)
			return NC_ENOMEM;

		ncap->value = vp;
		ncap->nalloc += NC_ARRAY_GROWBY;
	}

	if(newelemp != NULL)
	{
		ncap->value[ncap->nelems] = newelemp;
		ncap->nelems++;
	}
	return NC_NOERR;
}


NC_attr *
elem_NC_attrarray(const NC_attrarray *ncap, size_t elem)
{
	assert(ncap != NULL);
	/* cast needed for braindead systems with signed size_t */
	if(ncap->nelems == 0 || (unsigned long) elem >= ncap->nelems)
		return NULL;

	assert(ncap->value != NULL);

	return ncap->value[elem];
}

/* End attarray per se */

/*
 * Given ncp and varid, return ptr to array of attributes
 *  else NULL on error
 */
static NC_attrarray *
NC_attrarray0(NC3_INFO* ncp, int varid)
{
	NC_attrarray *ap;

	if(varid == NC_GLOBAL) /* Global attribute, attach to cdf */
	{
		ap = &ncp->attrs;
	}
	else if(varid >= 0 && (size_t) varid < ncp->vars.nelems)
	{
		NC_var **vpp;
		vpp = (NC_var **)ncp->vars.value;
		vpp += varid;
		ap = &(*vpp)->attrs;
	} else {
		ap = NULL;
	}
	return(ap);
}


/*
 * Step thru NC_ATTRIBUTE array, seeking match on name.
 *  return match or NULL if Not Found or out of memory.
 */
NC_attr **
NC_findattr(const NC_attrarray *ncap, const char *uname)
{
	NC_attr **attrpp;
	size_t attrid;
	size_t slen;
	char *name;

	assert(ncap != NULL);

	if(ncap->nelems == 0)
		return NULL;

	attrpp = (NC_attr **) ncap->value;

	/* normalized version of uname */
	name = (char *)utf8proc_NFC((const unsigned char *)uname);
	if(name == NULL)
	    return NULL; /* TODO: need better way to indicate no memory */
	slen = strlen(name);

	for(attrid = 0; attrid < ncap->nelems; attrid++, attrpp++)
	{
		if(strlen((*attrpp)->name->cp) == slen &&
			strncmp((*attrpp)->name->cp, name, slen) == 0)
		{
		        free(name);
			return(attrpp); /* Normal return */
		}
	}
	free(name);
	return(NULL);
}


/*
 * Look up by ncid, varid and name, return NULL if not found
 */
static int
NC_lookupattr(int ncid,
	int varid,
	const char *name, /* attribute name */
	NC_attr **attrpp) /* modified on return */
{
	int status;
	NC* nc;
	NC3_INFO *ncp;
	NC_attrarray *ncap;
	NC_attr **tmp;

	status = NC_check_id(ncid, &nc);
	if(status != NC_NOERR)
		return status;
	ncp = NC3_DATA(nc);

	ncap = NC_attrarray0(ncp, varid);
	if(ncap == NULL)
		return NC_ENOTVAR;

	tmp = NC_findattr(ncap, name);
	if(tmp == NULL)
		return NC_ENOTATT;

	if(attrpp != NULL)
		*attrpp = *tmp;

	return NC_NOERR;
}

/* Public */

int
NC3_inq_attname(int ncid, int varid, int attnum, char *name)
{
	int status;
	NC* nc;
	NC3_INFO *ncp;
	NC_attrarray *ncap;
	NC_attr *attrp;

	status = NC_check_id(ncid, &nc);
	if(status != NC_NOERR)
		return status;
	ncp = NC3_DATA(nc);

	ncap = NC_attrarray0(ncp, varid);
	if(ncap == NULL)
		return NC_ENOTVAR;

	attrp = elem_NC_attrarray(ncap, (size_t)attnum);
	if(attrp == NULL)
		return NC_ENOTATT;

	(void) strncpy(name, attrp->name->cp, attrp->name->nchars);
	name[attrp->name->nchars] = 0;

	return NC_NOERR;
}


int
NC3_inq_attid(int ncid, int varid, const char *name, int *attnump)
{
	int status;
	NC *nc;
	NC3_INFO* ncp;
	NC_attrarray *ncap;
	NC_attr **attrpp;

	status = NC_check_id(ncid, &nc);
	if(status != NC_NOERR)
		return status;
	ncp = NC3_DATA(nc);

	ncap = NC_attrarray0(ncp, varid);
	if(ncap == NULL)
		return NC_ENOTVAR;


	attrpp = NC_findattr(ncap, name);
	if(attrpp == NULL)
		return NC_ENOTATT;

	if(attnump != NULL)
		*attnump = (int)(attrpp - ncap->value);

	return NC_NOERR;
}

int
NC3_inq_att(int ncid,
	int varid,
	const char *name, /* input, attribute name */
	nc_type *datatypep,
	size_t *lenp)
{
	int status;
	NC_attr *attrp;

	status = NC_lookupattr(ncid, varid, name, &attrp);
	if(status != NC_NOERR)
		return status;

	if(datatypep != NULL)
		*datatypep = attrp->type;
	if(lenp != NULL)
		*lenp = attrp->nelems;

	return NC_NOERR;
}


int
NC3_rename_att( int ncid, int varid, const char *name, const char *unewname)
{
	int status;
	NC *nc;
	NC3_INFO* ncp;
	NC_attrarray *ncap;
	NC_attr **tmp;
	NC_attr *attrp;
	NC_string *newStr, *old;
	char *newname;  /* normalized version */

			/* sortof inline clone of NC_lookupattr() */
	status = NC_check_id(ncid, &nc);
	if(status != NC_NOERR)
		return status;
	ncp = NC3_DATA(nc);

	if(NC_readonly(ncp))
		return NC_EPERM;

	ncap = NC_attrarray0(ncp, varid);
	if(ncap == NULL)
		return NC_ENOTVAR;

	status = NC_check_name(unewname);
	if(status != NC_NOERR)
		return status;

	tmp = NC_findattr(ncap, name);
	if(tmp == NULL)
		return NC_ENOTATT;
	attrp = *tmp;
			/* end inline clone NC_lookupattr() */

	if(NC_findattr(ncap, unewname) != NULL)
	{
		/* name in use */
		return NC_ENAMEINUSE;
	}

	old = attrp->name;
	newname = (char *)utf8proc_NFC((const unsigned char *)unewname);
	if(newname == NULL)
	    return NC_EBADNAME;
	if(NC_indef(ncp))
	{
		newStr = new_NC_string(strlen(newname), newname);
		free(newname);
		if( newStr == NULL)
			return NC_ENOMEM;
		attrp->name = newStr;
		free_NC_string(old);
		return NC_NOERR;
	}
	/* else */
	status = set_NC_string(old, newname);
	free(newname);
	if( status != NC_NOERR)
		return status;

	set_NC_hdirty(ncp);

	if(NC_doHsync(ncp))
	{
		status = NC_sync(ncp);
		if(status != NC_NOERR)
			return status;
	}

	return NC_NOERR;
}

int
NC3_del_att(int ncid, int varid, const char *uname)
{
	int status;
	NC *nc;
	NC3_INFO* ncp;
	NC_attrarray *ncap;
	NC_attr **attrpp;
	NC_attr *old = NULL;
	int attrid;
	size_t slen;

	status = NC_check_id(ncid, &nc);
	if(status != NC_NOERR)
		return status;
	ncp = NC3_DATA(nc);

	if(!NC_indef(ncp))
		return NC_ENOTINDEFINE;

	ncap = NC_attrarray0(ncp, varid);
	if(ncap == NULL)
		return NC_ENOTVAR;

	{
	char *name = (char *)utf8proc_NFC((const unsigned char *)uname);
	if(name == NULL)
	    return NC_ENOMEM;

			/* sortof inline NC_findattr() */
	slen = strlen(name);

	attrpp = (NC_attr **) ncap->value;
	for(attrid = 0; (size_t) attrid < ncap->nelems; attrid++, attrpp++)
	    {
		if( slen == (*attrpp)->name->nchars &&
			strncmp(name, (*attrpp)->name->cp, slen) == 0)
		{
			old = *attrpp;
			break;
		}
	    }
	free(name);
	}
	if( (size_t) attrid == ncap->nelems )
		return NC_ENOTATT;
			/* end inline NC_findattr() */

	/* shuffle down */
	for(attrid++; (size_t) attrid < ncap->nelems; attrid++)
	{
		*attrpp = *(attrpp + 1);
		attrpp++;
	}
	*attrpp = NULL;
	/* decrement count */
	ncap->nelems--;

	free_NC_attr(old);

	return NC_NOERR;
}


static int
ncx_pad_putn_Iuchar(void **xpp, size_t nelems, const uchar *tp, nc_type type)
{
	switch(type) {
	case NC_CHAR:
		return NC_ECHAR;
	case NC_BYTE:
		return ncx_pad_putn_schar_uchar(xpp, nelems, tp);
	case NC_SHORT:
		return ncx_pad_putn_short_uchar(xpp, nelems, tp);
	case NC_INT:
		return ncx_putn_int_uchar(xpp, nelems, tp);
	case NC_FLOAT:
		return ncx_putn_float_uchar(xpp, nelems, tp);
	case NC_DOUBLE:
		return ncx_putn_double_uchar(xpp, nelems, tp);
	case NC_UBYTE:
		return ncx_pad_putn_uchar_uchar(xpp, nelems, tp);
	case NC_USHORT:
		return ncx_putn_ushort_uchar(xpp, nelems, tp);
	case NC_UINT:
		return ncx_putn_uint_uchar(xpp, nelems, tp);
	case NC_INT64:
		return ncx_putn_longlong_uchar(xpp, nelems, tp);
	case NC_UINT64:
		return ncx_putn_ulonglong_uchar(xpp, nelems, tp);
	default:
                assert("ncx_pad_putn_Iuchar invalid type" == 0);
	}
	return NC_EBADTYPE;
}

static int
ncx_pad_getn_Iuchar(const void **xpp, size_t nelems, uchar *tp, nc_type type)
{
	switch(type) {
	case NC_CHAR:
		return NC_ECHAR;
	case NC_BYTE:
		return ncx_pad_getn_schar_uchar(xpp, nelems, tp);
	case NC_SHORT:
		return ncx_pad_getn_short_uchar(xpp, nelems, tp);
	case NC_INT:
		return ncx_getn_int_uchar(xpp, nelems, tp);
	case NC_FLOAT:
		return ncx_getn_float_uchar(xpp, nelems, tp);
	case NC_DOUBLE:
		return ncx_getn_double_uchar(xpp, nelems, tp);
	case NC_UBYTE:
		return ncx_pad_getn_uchar_uchar(xpp, nelems, tp);
	case NC_USHORT:
		return ncx_getn_ushort_uchar(xpp, nelems, tp);
	case NC_UINT:
		return ncx_getn_uint_uchar(xpp, nelems, tp);
	case NC_INT64:
		return ncx_getn_longlong_uchar(xpp, nelems, tp);
	case NC_UINT64:
		return ncx_getn_ulonglong_uchar(xpp, nelems, tp);
	default:
	        assert("ncx_pad_getn_Iuchar invalid type" == 0);
	}
	return NC_EBADTYPE;
}


static int
ncx_pad_putn_Ischar(void **xpp, size_t nelems, const schar *tp, nc_type type)
{
	switch(type) {
	case NC_CHAR:
		return NC_ECHAR;
	case NC_BYTE:
		return ncx_pad_putn_schar_schar(xpp, nelems, tp);
	case NC_SHORT:
		return ncx_pad_putn_short_schar(xpp, nelems, tp);
	case NC_INT:
		return ncx_putn_int_schar(xpp, nelems, tp);
	case NC_FLOAT:
		return ncx_putn_float_schar(xpp, nelems, tp);
	case NC_DOUBLE:
		return ncx_putn_double_schar(xpp, nelems, tp);
	case NC_UBYTE:
		return ncx_pad_putn_uchar_schar(xpp, nelems, tp);
	case NC_USHORT:
		return ncx_putn_ushort_schar(xpp, nelems, tp);
	case NC_UINT:
		return ncx_putn_uint_schar(xpp, nelems, tp);
	case NC_INT64:
		return ncx_putn_longlong_schar(xpp, nelems, tp);
	case NC_UINT64:
		return ncx_putn_ulonglong_schar(xpp, nelems, tp);
	default:
                assert("ncx_pad_putn_Ischar invalid type" == 0);
	}
	return NC_EBADTYPE;
}

static int
ncx_pad_getn_Ischar(const void **xpp, size_t nelems, schar *tp, nc_type type)
{
	switch(type) {
	case NC_CHAR:
		return NC_ECHAR;
	case NC_BYTE:
		return ncx_pad_getn_schar_schar(xpp, nelems, tp);
	case NC_SHORT:
		return ncx_pad_getn_short_schar(xpp, nelems, tp);
	case NC_INT:
		return ncx_getn_int_schar(xpp, nelems, tp);
	case NC_FLOAT:
		return ncx_getn_float_schar(xpp, nelems, tp);
	case NC_DOUBLE:
		return ncx_getn_double_schar(xpp, nelems, tp);
	case NC_UBYTE:
		return ncx_pad_getn_uchar_schar(xpp, nelems, tp);
	case NC_USHORT:
		return ncx_getn_ushort_schar(xpp, nelems, tp);
	case NC_UINT:
		return ncx_getn_uint_schar(xpp, nelems, tp);
	case NC_INT64:
		return ncx_getn_longlong_schar(xpp, nelems, tp);
	case NC_UINT64:
		return ncx_getn_ulonglong_schar(xpp, nelems, tp);
	default:
	        assert("ncx_pad_getn_Ischar invalid type" == 0);
	}
	return NC_EBADTYPE;
}


static int
ncx_pad_putn_Ishort(void **xpp, size_t nelems, const short *tp, nc_type type)
{
	switch(type) {
	case NC_CHAR:
		return NC_ECHAR;
	case NC_BYTE:
		return ncx_pad_putn_schar_short(xpp, nelems, tp);
	case NC_SHORT:
		return ncx_pad_putn_short_short(xpp, nelems, tp);
	case NC_INT:
		return ncx_putn_int_short(xpp, nelems, tp);
	case NC_FLOAT:
		return ncx_putn_float_short(xpp, nelems, tp);
	case NC_DOUBLE:
		return ncx_putn_double_short(xpp, nelems, tp);
	case NC_UBYTE:
		return ncx_pad_putn_uchar_short(xpp, nelems, tp);
	case NC_USHORT:
		return ncx_putn_ushort_short(xpp, nelems, tp);
	case NC_UINT:
		return ncx_putn_uint_short(xpp, nelems, tp);
	case NC_INT64:
		return ncx_putn_longlong_short(xpp, nelems, tp);
	case NC_UINT64:
		return ncx_putn_ulonglong_short(xpp, nelems, tp);
	default:
                assert("ncx_pad_putn_Ishort invalid type" == 0);
	}
	return NC_EBADTYPE;
}

static int
ncx_pad_getn_Ishort(const void **xpp, size_t nelems, short *tp, nc_type type)
{
	switch(type) {
	case NC_CHAR:
		return NC_ECHAR;
	case NC_BYTE:
		return ncx_pad_getn_schar_short(xpp, nelems, tp);
	case NC_SHORT:
		return ncx_pad_getn_short_short(xpp, nelems, tp);
	case NC_INT:
		return ncx_getn_int_short(xpp, nelems, tp);
	case NC_FLOAT:
		return ncx_getn_float_short(xpp, nelems, tp);
	case NC_DOUBLE:
		return ncx_getn_double_short(xpp, nelems, tp);
	case NC_UBYTE:
		return ncx_pad_getn_uchar_short(xpp, nelems, tp);
	case NC_USHORT:
		return ncx_getn_ushort_short(xpp, nelems, tp);
	case NC_UINT:
		return ncx_getn_uint_short(xpp, nelems, tp);
	case NC_INT64:
		return ncx_getn_longlong_short(xpp, nelems, tp);
	case NC_UINT64:
		return ncx_getn_ulonglong_short(xpp, nelems, tp);
	default:
	        assert("ncx_pad_getn_Ishort invalid type" == 0);
	}
	return NC_EBADTYPE;
}


static int
ncx_pad_putn_Iint(void **xpp, size_t nelems, const int *tp, nc_type type)
{
	switch(type) {
	case NC_CHAR:
		return NC_ECHAR;
	case NC_BYTE:
		return ncx_pad_putn_schar_int(xpp, nelems, tp);
	case NC_SHORT:
		return ncx_pad_putn_short_int(xpp, nelems, tp);
	case NC_INT:
		return ncx_putn_int_int(xpp, nelems, tp);
	case NC_FLOAT:
		return ncx_putn_float_int(xpp, nelems, tp);
	case NC_DOUBLE:
		return ncx_putn_double_int(xpp, nelems, tp);
	case NC_UBYTE:
		return ncx_pad_putn_uchar_int(xpp, nelems, tp);
	case NC_USHORT:
		return ncx_putn_ushort_int(xpp, nelems, tp);
	case NC_UINT:
		return ncx_putn_uint_int(xpp, nelems, tp);
	case NC_INT64:
		return ncx_putn_longlong_int(xpp, nelems, tp);
	case NC_UINT64:
		return ncx_putn_ulonglong_int(xpp, nelems, tp);
	default:
                assert("ncx_pad_putn_Iint invalid type" == 0);
	}
	return NC_EBADTYPE;
}

static int
ncx_pad_getn_Iint(const void **xpp, size_t nelems, int *tp, nc_type type)
{
	switch(type) {
	case NC_CHAR:
		return NC_ECHAR;
	case NC_BYTE:
		return ncx_pad_getn_schar_int(xpp, nelems, tp);
	case NC_SHORT:
		return ncx_pad_getn_short_int(xpp, nelems, tp);
	case NC_INT:
		return ncx_getn_int_int(xpp, nelems, tp);
	case NC_FLOAT:
		return ncx_getn_float_int(xpp, nelems, tp);
	case NC_DOUBLE:
		return ncx_getn_double_int(xpp, nelems, tp);
	case NC_UBYTE:
		return ncx_pad_getn_uchar_int(xpp, nelems, tp);
	case NC_USHORT:
		return ncx_getn_ushort_int(xpp, nelems, tp);
	case NC_UINT:
		return ncx_getn_uint_int(xpp, nelems, tp);
	case NC_INT64:
		return ncx_getn_longlong_int(xpp, nelems, tp);
	case NC_UINT64:
		return ncx_getn_ulonglong_int(xpp, nelems, tp);
	default:
	        assert("ncx_pad_getn_Iint invalid type" == 0);
	}
	return NC_EBADTYPE;
}


static int
ncx_pad_putn_Ifloat(void **xpp, size_t nelems, const float *tp, nc_type type)
{
	switch(type) {
	case NC_CHAR:
		return NC_ECHAR;
	case NC_BYTE:
		return ncx_pad_putn_schar_float(xpp, nelems, tp);
	case NC_SHORT:
		return ncx_pad_putn_short_float(xpp, nelems, tp);
	case NC_INT:
		return ncx_putn_int_float(xpp, nelems, tp);
	case NC_FLOAT:
		return ncx_putn_float_float(xpp, nelems, tp);
	case NC_DOUBLE:
		return ncx_putn_double_float(xpp, nelems, tp);
	case NC_UBYTE:
		return ncx_pad_putn_uchar_float(xpp, nelems, tp);
	case NC_USHORT:
		return ncx_putn_ushort_float(xpp, nelems, tp);
	case NC_UINT:
		return ncx_putn_uint_float(xpp, nelems, tp);
	case NC_INT64:
		return ncx_putn_longlong_float(xpp, nelems, tp);
	case NC_UINT64:
		return ncx_putn_ulonglong_float(xpp, nelems, tp);
	default:
                assert("ncx_pad_putn_Ifloat invalid type" == 0);
	}
	return NC_EBADTYPE;
}

static int
ncx_pad_getn_Ifloat(const void **xpp, size_t nelems, float *tp, nc_type type)
{
	switch(type) {
	case NC_CHAR:
		return NC_ECHAR;
	case NC_BYTE:
		return ncx_pad_getn_schar_float(xpp, nelems, tp);
	case NC_SHORT:
		return ncx_pad_getn_short_float(xpp, nelems, tp);
	case NC_INT:
		return ncx_getn_int_float(xpp, nelems, tp);
	case NC_FLOAT:
		return ncx_getn_float_float(xpp, nelems, tp);
	case NC_DOUBLE:
		return ncx_getn_double_float(xpp, nelems, tp);
	case NC_UBYTE:
		return ncx_pad_getn_uchar_float(xpp, nelems, tp);
	case NC_USHORT:
		return ncx_getn_ushort_float(xpp, nelems, tp);
	case NC_UINT:
		return ncx_getn_uint_float(xpp, nelems, tp);
	case NC_INT64:
		return ncx_getn_longlong_float(xpp, nelems, tp);
	case NC_UINT64:
		return ncx_getn_ulonglong_float(xpp, nelems, tp);
	default:
	        assert("ncx_pad_getn_Ifloat invalid type" == 0);
	}
	return NC_EBADTYPE;
}


static int
ncx_pad_putn_Idouble(void **xpp, size_t nelems, const double *tp, nc_type type)
{
	switch(type) {
	case NC_CHAR:
		return NC_ECHAR;
	case NC_BYTE:
		return ncx_pad_putn_schar_double(xpp, nelems, tp);
	case NC_SHORT:
		return ncx_pad_putn_short_double(xpp, nelems, tp);
	case NC_INT:
		return ncx_putn_int_double(xpp, nelems, tp);
	case NC_FLOAT:
		return ncx_putn_float_double(xpp, nelems, tp);
	case NC_DOUBLE:
		return ncx_putn_double_double(xpp, nelems, tp);
	case NC_UBYTE:
		return ncx_pad_putn_uchar_double(xpp, nelems, tp);
	case NC_USHORT:
		return ncx_putn_ushort_double(xpp, nelems, tp);
	case NC_UINT:
		return ncx_putn_uint_double(xpp, nelems, tp);
	case NC_INT64:
		return ncx_putn_longlong_double(xpp, nelems, tp);
	case NC_UINT64:
		return ncx_putn_ulonglong_double(xpp, nelems, tp);
	default:
                assert("ncx_pad_putn_Idouble invalid type" == 0);
	}
	return NC_EBADTYPE;
}

static int
ncx_pad_getn_Idouble(const void **xpp, size_t nelems, double *tp, nc_type type)
{
	switch(type) {
	case NC_CHAR:
		return NC_ECHAR;
	case NC_BYTE:
		return ncx_pad_getn_schar_double(xpp, nelems, tp);
	case NC_SHORT:
		return ncx_pad_getn_short_double(xpp, nelems, tp);
	case NC_INT:
		return ncx_getn_int_double(xpp, nelems, tp);
	case NC_FLOAT:
		return ncx_getn_float_double(xpp, nelems, tp);
	case NC_DOUBLE:
		return ncx_getn_double_double(xpp, nelems, tp);
	case NC_UBYTE:
		return ncx_pad_getn_uchar_double(xpp, nelems, tp);
	case NC_USHORT:
		return ncx_getn_ushort_double(xpp, nelems, tp);
	case NC_UINT:
		return ncx_getn_uint_double(xpp, nelems, tp);
	case NC_INT64:
		return ncx_getn_longlong_double(xpp, nelems, tp);
	case NC_UINT64:
		return ncx_getn_ulonglong_double(xpp, nelems, tp);
	default:
	        assert("ncx_pad_getn_Idouble invalid type" == 0);
	}
	return NC_EBADTYPE;
}


#ifdef IGNORE
static int
ncx_pad_putn_Ilong(void **xpp, size_t nelems, const long *tp, nc_type type)
{
	switch(type) {
	case NC_CHAR:
		return NC_ECHAR;
	case NC_BYTE:
		return ncx_pad_putn_schar_long(xpp, nelems, tp);
	case NC_SHORT:
		return ncx_pad_putn_short_long(xpp, nelems, tp);
	case NC_INT:
		return ncx_putn_int_long(xpp, nelems, tp);
	case NC_FLOAT:
		return ncx_putn_float_long(xpp, nelems, tp);
	case NC_DOUBLE:
		return ncx_putn_double_long(xpp, nelems, tp);
	case NC_UBYTE:
		return ncx_pad_putn_uchar_long(xpp, nelems, tp);
	case NC_USHORT:
		return ncx_putn_ushort_long(xpp, nelems, tp);
	case NC_UINT:
		return ncx_putn_uint_long(xpp, nelems, tp);
	case NC_INT64:
		return ncx_putn_longlong_long(xpp, nelems, tp);
	case NC_UINT64:
		return ncx_putn_ulonglong_long(xpp, nelems, tp);
	default:
                assert("ncx_pad_putn_Ilong invalid type" == 0);
	}
	return NC_EBADTYPE;
}

static int
ncx_pad_getn_Ilong(const void **xpp, size_t nelems, long *tp, nc_type type)
{
	switch(type) {
	case NC_CHAR:
		return NC_ECHAR;
	case NC_BYTE:
		return ncx_pad_getn_schar_long(xpp, nelems, tp);
	case NC_SHORT:
		return ncx_pad_getn_short_long(xpp, nelems, tp);
	case NC_INT:
		return ncx_getn_int_long(xpp, nelems, tp);
	case NC_FLOAT:
		return ncx_getn_float_long(xpp, nelems, tp);
	case NC_DOUBLE:
		return ncx_getn_double_long(xpp, nelems, tp);
	case NC_UBYTE:
		return ncx_pad_getn_uchar_long(xpp, nelems, tp);
	case NC_USHORT:
		return ncx_getn_ushort_long(xpp, nelems, tp);
	case NC_UINT:
		return ncx_getn_uint_long(xpp, nelems, tp);
	case NC_INT64:
		return ncx_getn_longlong_long(xpp, nelems, tp);
	case NC_UINT64:
		return ncx_getn_ulonglong_long(xpp, nelems, tp);
	default:
	        assert("ncx_pad_getn_Ilong invalid type" == 0);
	}
	return NC_EBADTYPE;
}

#endif

static int
ncx_pad_putn_Ilonglong(void **xpp, size_t nelems, const longlong *tp, nc_type type)
{
	switch(type) {
	case NC_CHAR:
		return NC_ECHAR;
	case NC_BYTE:
		return ncx_pad_putn_schar_longlong(xpp, nelems, tp);
	case NC_SHORT:
		return ncx_pad_putn_short_longlong(xpp, nelems, tp);
	case NC_INT:
		return ncx_putn_int_longlong(xpp, nelems, tp);
	case NC_FLOAT:
		return ncx_putn_float_longlong(xpp, nelems, tp);
	case NC_DOUBLE:
		return ncx_putn_double_longlong(xpp, nelems, tp);
	case NC_UBYTE:
		return ncx_pad_putn_uchar_longlong(xpp, nelems, tp);
	case NC_USHORT:
		return ncx_putn_ushort_longlong(xpp, nelems, tp);
	case NC_UINT:
		return ncx_putn_uint_longlong(xpp, nelems, tp);
	case NC_INT64:
		return ncx_putn_longlong_longlong(xpp, nelems, tp);
	case NC_UINT64:
		return ncx_putn_ulonglong_longlong(xpp, nelems, tp);
	default:
                assert("ncx_pad_putn_Ilonglong invalid type" == 0);
	}
	return NC_EBADTYPE;
}

static int
ncx_pad_getn_Ilonglong(const void **xpp, size_t nelems, longlong *tp, nc_type type)
{
	switch(type) {
	case NC_CHAR:
		return NC_ECHAR;
	case NC_BYTE:
		return ncx_pad_getn_schar_longlong(xpp, nelems, tp);
	case NC_SHORT:
		return ncx_pad_getn_short_longlong(xpp, nelems, tp);
	case NC_INT:
		return ncx_getn_int_longlong(xpp, nelems, tp);
	case NC_FLOAT:
		return ncx_getn_float_longlong(xpp, nelems, tp);
	case NC_DOUBLE:
		return ncx_getn_double_longlong(xpp, nelems, tp);
	case NC_UBYTE:
		return ncx_pad_getn_uchar_longlong(xpp, nelems, tp);
	case NC_USHORT:
		return ncx_getn_ushort_longlong(xpp, nelems, tp);
	case NC_UINT:
		return ncx_getn_uint_longlong(xpp, nelems, tp);
	case NC_INT64:
		return ncx_getn_longlong_longlong(xpp, nelems, tp);
	case NC_UINT64:
		return ncx_getn_ulonglong_longlong(xpp, nelems, tp);
	default:
	        assert("ncx_pad_getn_Ilonglong invalid type" == 0);
	}
	return NC_EBADTYPE;
}


static int
ncx_pad_putn_Iushort(void **xpp, size_t nelems, const ushort *tp, nc_type type)
{
	switch(type) {
	case NC_CHAR:
		return NC_ECHAR;
	case NC_BYTE:
		return ncx_pad_putn_schar_ushort(xpp, nelems, tp);
	case NC_SHORT:
		return ncx_pad_putn_short_ushort(xpp, nelems, tp);
	case NC_INT:
		return ncx_putn_int_ushort(xpp, nelems, tp);
	case NC_FLOAT:
		return ncx_putn_float_ushort(xpp, nelems, tp);
	case NC_DOUBLE:
		return ncx_putn_double_ushort(xpp, nelems, tp);
	case NC_UBYTE:
		return ncx_pad_putn_uchar_ushort(xpp, nelems, tp);
	case NC_USHORT:
		return ncx_putn_ushort_ushort(xpp, nelems, tp);
	case NC_UINT:
		return ncx_putn_uint_ushort(xpp, nelems, tp);
	case NC_INT64:
		return ncx_putn_longlong_ushort(xpp, nelems, tp);
	case NC_UINT64:
		return ncx_putn_ulonglong_ushort(xpp, nelems, tp);
	default:
                assert("ncx_pad_putn_Iushort invalid type" == 0);
	}
	return NC_EBADTYPE;
}

static int
ncx_pad_getn_Iushort(const void **xpp, size_t nelems, ushort *tp, nc_type type)
{
	switch(type) {
	case NC_CHAR:
		return NC_ECHAR;
	case NC_BYTE:
		return ncx_pad_getn_schar_ushort(xpp, nelems, tp);
	case NC_SHORT:
		return ncx_pad_getn_short_ushort(xpp, nelems, tp);
	case NC_INT:
		return ncx_getn_int_ushort(xpp, nelems, tp);
	case NC_FLOAT:
		return ncx_getn_float_ushort(xpp, nelems, tp);
	case NC_DOUBLE:
		return ncx_getn_double_ushort(xpp, nelems, tp);
	case NC_UBYTE:
		return ncx_pad_getn_uchar_ushort(xpp, nelems, tp);
	case NC_USHORT:
		return ncx_getn_ushort_ushort(xpp, nelems, tp);
	case NC_UINT:
		return ncx_getn_uint_ushort(xpp, nelems, tp);
	case NC_INT64:
		return ncx_getn_longlong_ushort(xpp, nelems, tp);
	case NC_UINT64:
		return ncx_getn_ulonglong_ushort(xpp, nelems, tp);
	default:
	        assert("ncx_pad_getn_Iushort invalid type" == 0);
	}
	return NC_EBADTYPE;
}


static int
ncx_pad_putn_Iuint(void **xpp, size_t nelems, const uint *tp, nc_type type)
{
	switch(type) {
	case NC_CHAR:
		return NC_ECHAR;
	case NC_BYTE:
		return ncx_pad_putn_schar_uint(xpp, nelems, tp);
	case NC_SHORT:
		return ncx_pad_putn_short_uint(xpp, nelems, tp);
	case NC_INT:
		return ncx_putn_int_uint(xpp, nelems, tp);
	case NC_FLOAT:
		return ncx_putn_float_uint(xpp, nelems, tp);
	case NC_DOUBLE:
		return ncx_putn_double_uint(xpp, nelems, tp);
	case NC_UBYTE:
		return ncx_pad_putn_uchar_uint(xpp, nelems, tp);
	case NC_USHORT:
		return ncx_putn_ushort_uint(xpp, nelems, tp);
	case NC_UINT:
		return ncx_putn_uint_uint(xpp, nelems, tp);
	case NC_INT64:
		return ncx_putn_longlong_uint(xpp, nelems, tp);
	case NC_UINT64:
		return ncx_putn_ulonglong_uint(xpp, nelems, tp);
	default:
                assert("ncx_pad_putn_Iuint invalid type" == 0);
	}
	return NC_EBADTYPE;
}

static int
ncx_pad_getn_Iuint(const void **xpp, size_t nelems, uint *tp, nc_type type)
{
	switch(type) {
	case NC_CHAR:
		return NC_ECHAR;
	case NC_BYTE:
		return ncx_pad_getn_schar_uint(xpp, nelems, tp);
	case NC_SHORT:
		return ncx_pad_getn_short_uint(xpp, nelems, tp);
	case NC_INT:
		return ncx_getn_int_uint(xpp, nelems, tp);
	case NC_FLOAT:
		return ncx_getn_float_uint(xpp, nelems, tp);
	case NC_DOUBLE:
		return ncx_getn_double_uint(xpp, nelems, tp);
	case NC_UBYTE:
		return ncx_pad_getn_uchar_uint(xpp, nelems, tp);
	case NC_USHORT:
		return ncx_getn_ushort_uint(xpp, nelems, tp);
	case NC_UINT:
		return ncx_getn_uint_uint(xpp, nelems, tp);
	case NC_INT64:
		return ncx_getn_longlong_uint(xpp, nelems, tp);
	case NC_UINT64:
		return ncx_getn_ulonglong_uint(xpp, nelems, tp);
	default:
	        assert("ncx_pad_getn_Iuint invalid type" == 0);
	}
	return NC_EBADTYPE;
}


static int
ncx_pad_putn_Iulonglong(void **xpp, size_t nelems, const ulonglong *tp, nc_type type)
{
	switch(type) {
	case NC_CHAR:
		return NC_ECHAR;
	case NC_BYTE:
		return ncx_pad_putn_schar_ulonglong(xpp, nelems, tp);
	case NC_SHORT:
		return ncx_pad_putn_short_ulonglong(xpp, nelems, tp);
	case NC_INT:
		return ncx_putn_int_ulonglong(xpp, nelems, tp);
	case NC_FLOAT:
		return ncx_putn_float_ulonglong(xpp, nelems, tp);
	case NC_DOUBLE:
		return ncx_putn_double_ulonglong(xpp, nelems, tp);
	case NC_UBYTE:
		return ncx_pad_putn_uchar_ulonglong(xpp, nelems, tp);
	case NC_USHORT:
		return ncx_putn_ushort_ulonglong(xpp, nelems, tp);
	case NC_UINT:
		return ncx_putn_uint_ulonglong(xpp, nelems, tp);
	case NC_INT64:
		return ncx_putn_longlong_ulonglong(xpp, nelems, tp);
	case NC_UINT64:
		return ncx_putn_ulonglong_ulonglong(xpp, nelems, tp);
	default:
                assert("ncx_pad_putn_Iulonglong invalid type" == 0);
	}
	return NC_EBADTYPE;
}

static int
ncx_pad_getn_Iulonglong(const void **xpp, size_t nelems, ulonglong *tp, nc_type type)
{
	switch(type) {
	case NC_CHAR:
		return NC_ECHAR;
	case NC_BYTE:
		return ncx_pad_getn_schar_ulonglong(xpp, nelems, tp);
	case NC_SHORT:
		return ncx_pad_getn_short_ulonglong(xpp, nelems, tp);
	case NC_INT:
		return ncx_getn_int_ulonglong(xpp, nelems, tp);
	case NC_FLOAT:
		return ncx_getn_float_ulonglong(xpp, nelems, tp);
	case NC_DOUBLE:
		return ncx_getn_double_ulonglong(xpp, nelems, tp);
	case NC_UBYTE:
		return ncx_pad_getn_uchar_ulonglong(xpp, nelems, tp);
	case NC_USHORT:
		return ncx_getn_ushort_ulonglong(xpp, nelems, tp);
	case NC_UINT:
		return ncx_getn_uint_ulonglong(xpp, nelems, tp);
	case NC_INT64:
		return ncx_getn_longlong_ulonglong(xpp, nelems, tp);
	case NC_UINT64:
		return ncx_getn_ulonglong_ulonglong(xpp, nelems, tp);
	default:
	        assert("ncx_pad_getn_Iulonglong invalid type" == 0);
	}
	return NC_EBADTYPE;
}



/* Common dispatcher for put cases */
static int
dispatchput(void **xpp, size_t nelems, const void* tp,
	    nc_type atype, nc_type memtype)
{
    switch (memtype) {
    case NC_CHAR:
        return ncx_pad_putn_text(xpp,nelems, (char *)tp);
    case NC_BYTE:
        return ncx_pad_putn_Ischar(xpp, nelems, (schar*)tp, atype);
    case NC_SHORT:
        return ncx_pad_putn_Ishort(xpp, nelems, (short*)tp, atype);
    case NC_INT:
          return ncx_pad_putn_Iint(xpp, nelems, (int*)tp, atype);
    case NC_FLOAT:
        return ncx_pad_putn_Ifloat(xpp, nelems, (float*)tp, atype);
    case NC_DOUBLE:
        return ncx_pad_putn_Idouble(xpp, nelems, (double*)tp, atype);
    case NC_UBYTE: /*Synthetic*/
        return ncx_pad_putn_Iuchar(xpp,nelems, (uchar *)tp, atype);
    case NC_INT64:
          return ncx_pad_putn_Ilonglong(xpp, nelems, (longlong*)tp, atype);
    case NC_USHORT:
          return ncx_pad_putn_Iushort(xpp, nelems, (ushort*)tp, atype);
    case NC_UINT:
          return ncx_pad_putn_Iuint(xpp, nelems, (uint*)tp, atype);
    case NC_UINT64:
          return ncx_pad_putn_Iulonglong(xpp, nelems, (ulonglong*)tp, atype);
    case NC_NAT:
        return NC_EBADTYPE;
    default:
        break;
    }
    return NC_EBADTYPE;
}

int
NC3_put_att(
	int ncid,
	int varid,
	const char *name,
	nc_type type,
	size_t nelems,
	const void *value,
	nc_type memtype)
{
    int status;
    NC *nc;
    NC3_INFO* ncp;
    NC_attrarray *ncap;
    NC_attr **attrpp;
    NC_attr *old = NULL;
    NC_attr *attrp;

    status = NC_check_id(ncid, &nc);
    if(status != NC_NOERR)
	return status;
    ncp = NC3_DATA(nc);

    if(NC_readonly(ncp))
	return NC_EPERM;

    ncap = NC_attrarray0(ncp, varid);
    if(ncap == NULL)
	return NC_ENOTVAR;

    status = nc3_cktype(nc->mode, type);
    if(status != NC_NOERR)
	return status;

    if(memtype == NC_NAT) memtype = type;

    if(memtype != NC_CHAR && type == NC_CHAR)
	return NC_ECHAR;
    if(memtype == NC_CHAR && type != NC_CHAR)
	return NC_ECHAR;

    /* cast needed for braindead systems with signed size_t */
    if((unsigned long) nelems > X_INT_MAX) /* backward compat */
	return NC_EINVAL; /* Invalid nelems */

    if(nelems != 0 && value == NULL)
	return NC_EINVAL; /* Null arg */

    attrpp = NC_findattr(ncap, name);

    /* 4 cases: exists X indef */

    if(attrpp != NULL) { /* name in use */
        if(!NC_indef(ncp)) {
	    const size_t xsz = ncx_len_NC_attrV(type, nelems);
            attrp = *attrpp; /* convenience */

	    if(xsz > attrp->xsz) return NC_ENOTINDEFINE;
	    /* else, we can reuse existing without redef */

	    attrp->xsz = xsz;
            attrp->type = type;
            attrp->nelems = nelems;

            if(nelems != 0) {
                void *xp = attrp->xvalue;
                status = dispatchput(&xp, nelems, (const void*)value, type, memtype);
            }

            set_NC_hdirty(ncp);

            if(NC_doHsync(ncp)) {
	        const int lstatus = NC_sync(ncp);
                /*
                 * N.B.: potentially overrides NC_ERANGE
                 * set by ncx_pad_putn_I$1
                 */
                if(lstatus != NC_NOERR) return lstatus;
            }

            return status;
        }
        /* else, redefine using existing array slot */
        old = *attrpp;
    } else {
        if(!NC_indef(ncp)) return NC_ENOTINDEFINE;

        if(ncap->nelems >= NC_MAX_ATTRS) return NC_EMAXATTS;
    }

    status = NC_check_name(name);
    if(status != NC_NOERR) return status;

    attrp = new_NC_attr(name, type, nelems);
    if(attrp == NULL) return NC_ENOMEM;

    if(nelems != 0) {
        void *xp = attrp->xvalue;
        status = dispatchput(&xp, nelems, (const void*)value, type, memtype);
    }

    if(attrpp != NULL) {
        *attrpp = attrp;
	if(old != NULL)
	        free_NC_attr(old);
    } else {
        const int lstatus = incr_NC_attrarray(ncap, attrp);
        /*
         * N.B.: potentially overrides NC_ERANGE
         * set by ncx_pad_putn_I$1
         */
        if(lstatus != NC_NOERR) {
           free_NC_attr(attrp);
           return lstatus;
        }
    }
    return status;
}

int
NC3_get_att(
	int ncid,
	int varid,
	const char *name,
	void *value,
	nc_type memtype)
{
    int status;
    NC_attr *attrp;
    const void *xp;

    status = NC_lookupattr(ncid, varid, name, &attrp);
    if(status != NC_NOERR) return status;

    if(attrp->nelems == 0) return NC_NOERR;

    if(memtype == NC_NAT) memtype = attrp->type;

    if(memtype != NC_CHAR && attrp->type == NC_CHAR)
	return NC_ECHAR;
    if(memtype == NC_CHAR && attrp->type != NC_CHAR)
	return NC_ECHAR;

    xp = attrp->xvalue;
    switch (memtype) {
    case NC_CHAR:
        return ncx_pad_getn_text(&xp, attrp->nelems , (char *)value);
    case NC_BYTE:
        return ncx_pad_getn_Ischar(&xp,attrp->nelems,(schar*)value,attrp->type);
    case NC_SHORT:
        return ncx_pad_getn_Ishort(&xp,attrp->nelems,(short*)value,attrp->type);
    case NC_INT:
          return ncx_pad_getn_Iint(&xp,attrp->nelems,(int*)value,attrp->type);
    case NC_FLOAT:
        return ncx_pad_getn_Ifloat(&xp,attrp->nelems,(float*)value,attrp->type);
    case NC_DOUBLE:
        return ncx_pad_getn_Idouble(&xp,attrp->nelems,(double*)value,attrp->type);
    case NC_INT64:
          return ncx_pad_getn_Ilonglong(&xp,attrp->nelems,(longlong*)value,attrp->type);
    case NC_UBYTE: /* Synthetic */
        return ncx_pad_getn_Iuchar(&xp, attrp->nelems , (uchar *)value, attrp->type);
    case NC_USHORT:
          return ncx_pad_getn_Iushort(&xp,attrp->nelems,(ushort*)value,attrp->type);
    case NC_UINT:
          return ncx_pad_getn_Iuint(&xp,attrp->nelems,(uint*)value,attrp->type);
    case NC_UINT64:
          return ncx_pad_getn_Iulonglong(&xp,attrp->nelems,(ulonglong*)value,attrp->type);

    case NC_NAT:
        return NC_EBADTYPE;
    default:
        break;
    }
    status =  NC_EBADTYPE;
    return status;
}
