/*
 * The contents of this file are subject to the MonetDB Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at
 * http://monetdb.cwi.nl/Legal/MonetDBPL-1.0.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Monet Database System.
 *
 * The Initial Developer of the Original Code is CWI.
 * Portions created by CWI are Copyright (C) 1997-2002 CWI.
 * All Rights Reserved.
 *
 * Contributor(s):
 * 		Martin Kersten  <Martin.Kersten@cwi.nl>
 * 		Peter Boncz  <Peter.Boncz@cwi.nl>
 * 		Niels Nes  <Niels.Nes@cwi.nl>
 * 		Stefan Manegold  <Stefan.Manegold@cwi.nl>
 */

#include "catalog.h"
#include "statement.h"
#include "mem.h"

#include "comm.h"
#include <string.h>

typedef struct cc {
	stream *in;
	stream *out;
	context *lc;
} cc;

extern stmt *sqlexecute( context *lc, char *buf );

static int key_cmp( key *k, long *id )
{
	if (k && id && k->id == *id)
		return 0;
	return 1;
}

static char *readblock( stream *s ){
	int len = 0;
	int size = BLOCK + 1;
	char *buf = NEW_ARRAY(char, size ), *start = buf;

	while ((len = s->read(s, start, 1, BLOCK)) == BLOCK){
		size += BLOCK;
		buf = RENEW_ARRAY(char, buf, size);
		start = buf + size - BLOCK - 1;
		*start = '\0';
	}
	start += len;
	*start = '\0';
	return buf;
}

static void send_gettypes( catalog *cat ){
	char buf[BUFSIZ];
	cc *i = (cc*)cat->cc;

	sprintf(buf, "types_export(Output);\n" );
	i->out->write(i->out, buf, strlen(buf), 1);
	i->out->flush(i->out);
}

static void send_getschema( catalog *cat ){
	char buf[BUFSIZ];
	cc *i = (cc*)cat->cc;

	sprintf(buf, "mvc_export_schema(myc, Output);\n");
	i->out->write(i->out, buf, strlen(buf), 1);
	i->out->flush(i->out);
}


void gettypes( catalog *c ){
	stream *s = ((cc*)c->cc)->in;
	int i, tcnt;
	char *buf, *start, *n;

	send_gettypes( c );

	buf = readblock(s);
	n = start = buf;

	tcnt = strtol(n,&n,10);
	for(i=0;i<tcnt;i++){
	    char *sqlname, *name, *cast;

	    n = strchr(start = n+1, ','); *n = '\0';
	    sqlname = start;

	    n = strchr(start = n+1, ','); *n = '\0';
	    name = start;

	    n = strchr(start = n+1, '\n'); *n = '\0';
	    cast = start;

	    sql_create_type( sqlname, name );
	}
	/* TODO load proper type cast table */

	tcnt = strtol(n+1,&n,10);
	for(i=0;i<tcnt;i++){
	    char *tname, *imp, *intype, *result;

	    n = strchr(start = n+1, ','); *n = '\0';
	    tname = start;

	    n = strchr(start = n+1, ','); *n = '\0';
	    imp = start;

	    n = strchr(start = n+1, ','); *n = '\0';
	    intype = start;

	    n = strchr(start = n+1, '\n'); *n = '\0';
	    result = start;

	    sql_create_aggr( tname, imp, intype, result );
	}

	tcnt = strtol(n+1,&n,10);
	for(i=0;i<tcnt;i++){
	    char *tname, *imp, *tpe1, *tpe2, *tpe3, *res;

	    n = strchr(start = n+1, ','); *n = '\0';
	    tname = start;

	    n = strchr(start = n+1, ','); *n = '\0';
	    imp = start;

	    n = strchr(start = n+1, ','); *n = '\0';
	    tpe1 = start;

	    n = strchr(start = n+1, ','); *n = '\0';
	    tpe2 = start;

	    n = strchr(start = n+1, ','); *n = '\0';
	    tpe3 = start;

	    n = strchr(start = n+1, '\n'); *n = '\0';
	    res = start;

	    sql_create_func( tname, imp, tpe1, tpe2, tpe3, res );
	}
	_DELETE(buf);
}

void getschema( catalog *c, char *schema, char *user ){
	list *keys = list_create(NULL);
	stream *s = ((cc*)c->cc)->in;
	context *lc = ((cc*)c->cc)->lc;
	int i, tcnt;
	char *buf, *start, *n;

	send_getschema(c);

	buf = readblock(s);
	n = start = buf;

	if (c->schemas) list_destroy( c->schemas );
	c->schemas = list_create((fdestroy)&cat_drop_schema);

	c->cur_schema = cat_create_schema( c, 0, schema, user );
	list_append( c->schemas, c->cur_schema );

	tcnt = strtol(n,&n,10);
	for(i=0;i<tcnt;i++){
	    long id;
	    char *tname;
	    int cnr, knr;
	    char *query;

	    n = strchr(start = n+1, ','); *n = '\0';
	    id = strtol(start, (char**)NULL, 10);

	    n = strchr(start = n+1, ','); *n = '\0';
	    tname = start;

	    n = strchr(start = n+1, ','); *n = '\0';
	    cnr = atoi(start);

	    n = strchr(start = n+1, ','); *n = '\0';
	    query = start;

	    n = strchr(start = n+1, '\n'); *n = '\0';
	    knr = atoi(start);

	    if (cnr){
		int j;
		table *t =
		       cat_create_table( c, id, c->cur_schema, tname, 0, NULL);

		for(j=0;j<cnr;j++){
			long id = 0;
			char *cname, *ctype, *def;
			int nll;

			n = strchr(start = n+1, ','); *n = '\0';
			id = strtol(start, (char**)NULL, 10);

			n = strchr(start = n+1, ','); *n = '\0';
			cname = start;

			n = strchr(start = n+1, ','); *n = '\0';
			ctype = start;

			n = strchr(start = n+1, ','); *n = '\0';
			def = start;

			n = strchr(start = n+1, '\n'); *n = '\0';
			nll = atoi(start);

			cat_create_column( c, id, t, cname, ctype, def, nll );
		}
	        if (knr){ /* keys */
		    int j;
		    for(j=0;j<knr;j++){
			node *m = NULL;
			key *k = NULL;
			long id = 0, rkid;
			int ci, type = 0, cnr = 0;

			n = strchr(start = n+1, ','); *n = '\0';
			id = strtol(start, (char**)NULL, 10);

			n = strchr(start = n+1, ','); *n = '\0';
			type = atoi(start);

			n = strchr(start = n+1, ','); *n = '\0';
			cnr = atoi(start);

			n = strchr(start = n+1, '\n'); *n = '\0';
			rkid = strtol(start, (char**)NULL, 10);

			if (rkid)
				m = list_find(keys, &rkid, (fcmp)&key_cmp);

			if (m){
				k = cat_table_add_key( t, type, m->data);
				list_remove_node( keys, m);
			} else {
				k = cat_table_add_key( t, type, NULL);
				list_append( keys, k);
			}
			k->id = id;
			for(ci = 0; ci<cnr; ci++){
				char *colname;
				column *col;
				n = strchr(start = n+1, '\n'); *n = '\0';
				colname = start;

				col = cat_bind_column(c, t, colname);
				assert(col);
				cat_key_add_column( k, col);
			}
		    }
	        }

	    } else {
		sqlexecute(lc, query );
	    }
	}
	_DELETE(buf);
}

static void cc_destroy( catalog *c ){
	types_exit();

	_DELETE(c->cc);
	c->cc = NULL;
}

catalog *catalog_create_stream( stream *in, context *lc ){
	cc *CC = NEW(cc);
	catalog *c = lc->cat;

	CC->in = in;
	CC->out = lc->out;
	CC->lc = lc;
	c->cc = (char*)CC;
	c->cc_getschemas = &getschema;
	c->cc_destroy = &cc_destroy;

	types_init( lc->debug );
	gettypes( c );
	return c;
}
