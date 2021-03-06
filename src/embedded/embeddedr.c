#include "monetdb_config.h"

#ifdef HAVE_EMBEDDED_R
#include "embeddedr.h"
#include "R_ext/Random.h"
#include "R_ext/Rallocators.h"
#include "monet_options.h"
#include "mal.h"
#include "mmath.h"
#include "mal_client.h"
#include "mal_linker.h"
#include "sql_scenario.h"
#include "gdk_utils.h"

/* we need the BAT-SEXP-BAT conversion in two places, here and in RAPI */
#include "converters.c.h"

int embedded_r_rand(void) {
	int ret;
	ret = (int) (unif_rand() * RAND_MAX);
	return ret;
}

static SEXP monetdb_error_R(char* err) {
	SEXP retChr = NULL, retVec = NA_STRING;
	if (!err) {
		return retVec;
	}
	PROTECT(retChr = RSTR(err));
	if (retChr) {
		retVec = ScalarString(retChr);
	}
	UNPROTECT(1);
	return retVec;
}

SEXP monetdb_query_R(SEXP connsexp, SEXP querysexp, SEXP executesexp, SEXP resultconvertsexp) {
	res_table* output = NULL;
	char* err = NULL;
	GetRNGstate();
	err = monetdb_query(R_ExternalPtrAddr(connsexp),
			(char*)CHAR(STRING_ELT(querysexp, 0)), LOGICAL(executesexp)[0], (void**)&output);
	if (err) { // there was an error
		PutRNGstate();
		return monetdb_error_R(err);
	}
	if (output && output->nr_cols > 0) {
		int i = 0, ncols = output->nr_cols;
		ssize_t nrows = -1;
		SEXP retlist = NULL, names = NULL;
		PROTECT(retlist = allocVector(VECSXP, ncols));
		if (!retlist) {
			UNPROTECT(1);
			return monetdb_error_R("Memory allocation failed");
		}
		PROTECT(names = NEW_STRING(ncols));
		if (!names) {
			UNPROTECT(2);
			return monetdb_error_R("Memory allocation failed");
		}

		for (i = 0; i < ncols; i++) {
			BAT* b = BATdescriptor(output->cols[i].b);
			SEXP varvalue = NULL;
			SEXP varname = PROTECT(RSTR(output->cols[i].name));
			int unfix = 1;
			if (!varname) {
				UNPROTECT(i * 2 + 3);
				return monetdb_error_R("Memory allocation failed");
			}
			if (nrows < 0) {
				nrows = BATcount(b);
			}
			if (!LOGICAL(resultconvertsexp)[0]) {
				BATsetcount(b, 0); // hehe
			}
			if (!(varvalue = bat_to_sexp(b, &unfix))) {
				UNPROTECT(i * 2 + 4);
				PutRNGstate();
				return monetdb_error_R("Conversion error");
			}
			SET_VECTOR_ELT(retlist, i, varvalue);
			SET_STRING_ELT(names, i, varname);
			if (unfix) {
				BBPunfix(b->batCacheid);
			}
		}
		SET_ATTR(retlist, install("__rows"), Rf_ScalarReal(nrows));
		monetdb_cleanup_result(R_ExternalPtrAddr(connsexp), output);
		SET_NAMES(retlist, names);
		/*
	PROTECT(tmp = mkString("data.frame"));
    setAttrib(data, R_ClassSymbol, tmp);
    UNPROTECT(1);
    if (length(row_names) == nr) {
	setAttrib(data, R_RowNamesSymbol, row_names);

	PROTECT(row_names = allocVector(INTSXP, 2));
	INTEGER(row_names)[0] = NA_INTEGER;
	INTEGER(row_names)[1] = nr;
	setAttrib(data, R_RowNamesSymbol, row_names);
	UNPROTECT(1);
		 */
		UNPROTECT(ncols * 2 + 2);
		PutRNGstate();
		return retlist;
	}
	PutRNGstate();
	return ScalarLogical(1);
}

SEXP monetdb_startup_R(SEXP dbdirsexp, SEXP silentsexp, SEXP sequentialsexp) {
	char* res = NULL;

	if (monetdb_is_initialized()) {
		error("MonetDBLite already initialized");
	}

#if defined(WIN32) && !defined(_WIN64)
	Rf_warning("MonetDBLite running in a 32-Bit Windows. This is not recommended.");
#endif
	GetRNGstate();
	res = monetdb_startup((char*) CHAR(STRING_ELT(dbdirsexp, 0)),
		LOGICAL(silentsexp)[0], LOGICAL(sequentialsexp)[0]);
	PutRNGstate();
	if (!res) {
		return ScalarLogical(1);
	}  else {
		return monetdb_error_R(res);
	}
}


SEXP monetdb_append_R(SEXP connsexp, SEXP schemasexp, SEXP namesexp, SEXP tabledatasexp) {
	const char *schema = NULL, *name = NULL;
	str msg;
	int col_ct, i;
	BAT *b = NULL;
	append_data *ad = NULL;
	int t_column_count;
	char** t_column_names = NULL;
	int* t_column_types = NULL;

	if (!IS_CHARACTER(schemasexp) || !IS_CHARACTER(namesexp)) {
		return ScalarInteger(-1);
	}
	GetRNGstate();
	schema = CHAR(STRING_ELT(schemasexp, 0));
	name = CHAR(STRING_ELT(namesexp, 0));
	col_ct = LENGTH(tabledatasexp);

	msg = monetdb_get_columns(R_ExternalPtrAddr(connsexp), schema, name, &t_column_count, &t_column_names, &t_column_types);
	if (msg != MAL_SUCCEED)
		goto wrapup;

	if (t_column_count != col_ct) {
		msg = GDKstrdup("Unequal number of columns"); // TODO: add counts here
		goto wrapup;
	}

	ad = GDKmalloc(col_ct * sizeof(append_data));
	assert(ad);

	for (i = 0; i < col_ct; i++) {
		SEXP ret_col = VECTOR_ELT(tabledatasexp, i);
		int bat_type = t_column_types[i];
		b = sexp_to_bat(ret_col, bat_type);
		if (b == NULL) {
			msg = createException(MAL, "embedded", "Could not convert column %i %s to type %i ", i, t_column_names[i], bat_type);
			goto wrapup;
		}
		ad[i].colname = t_column_names[i];
		ad[i].batid = b->batCacheid;
	}

	msg = monetdb_append(R_ExternalPtrAddr(connsexp), schema, name, ad, col_ct);

	wrapup:
		PutRNGstate();
		if (ad) {
			GDKfree(ad);
		}
		if (t_column_names) {
			GDKfree(t_column_names);
		}
		if (t_column_types) {
			GDKfree(t_column_types);
		}
		if (!msg) {
			return ScalarLogical(1);
		}
		return monetdb_error_R(msg);
}


SEXP monetdb_connect_R(void) {
	void* llconn = monetdb_connect();
	SEXP conn = NULL;
	if (!llconn) {
		error("Could not create connection.");
	}
	conn = PROTECT(R_MakeExternalPtr(llconn, R_NilValue, R_NilValue));
	R_RegisterCFinalizer(conn, (void (*)(SEXP)) monetdb_disconnect_R);
	UNPROTECT(1);
	return conn;
}

SEXP monetdb_disconnect_R(SEXP connsexp) {
	void* addr = R_ExternalPtrAddr(connsexp);
	if (addr) {
		monetdb_disconnect(addr);
		R_ClearExternalPtr(connsexp);
	}
	return R_NilValue;
}

SEXP monetdb_shutdown_R(void) {
	monetdb_shutdown();
	return R_NilValue;
}
#endif
