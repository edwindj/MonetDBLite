/* -*- c-basic-offset:4; c-indentation-style:"k&r"; indent-tabs-mode:nil -*- */
/**
 * Copyright Notice:
 * -----------------
 *
 *  The contents of this file are subject to the MonetDB Public
 *  License Version 1.0 (the "License"); you may not use this file
 *  except in compliance with the License. You may obtain a copy of
 *  the License at http://monetdb.cwi.nl/Legal/MonetDBLicense-1.0.html
 *
 *  Software distributed under the License is distributed on an "AS
 *  IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 *  implied. See the License for the specific language governing
 *  rights and limitations under the License.
 *
 *  The Original Code is the ``Pathfinder'' system. The Initial
 *  Developer of the Original Code is the Database & Information
 *  Systems Group at the University of Konstanz, Germany. Portions
 *  created by U Konstanz are Copyright (C) 2000-2004 University
 *  of Konstanz. All Rights Reserved.
 *
 *  Contributors:
 *          Torsten Grust <torsten.grust@uni-konstanz.de>
 *          Maurice van Keulen <M.van.Keulen@bigfoot.com>
 *          Jens Teubner <jens.teubner@uni-konstanz.de>
 *
 * $Id$
 */

#include "pathfinder.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "milprint_summer.h"

#include "array.h"
#include "pfstrings.h"
#include "oops.h"
#include "subtyping.h"

static void
translate2MIL (FILE *f, int act_level, int counter, PFcnode_t *c);

/**
 * Test if two types are castable into each other
 *
 * @param t1 the type from which it is casted
 * @param t2 the type to which it is casted
 * @return the result of the castable test
 */
static bool
castable (PFty_t t1, PFty_t t2)
{
    t2 = PFty_defn (t2);
    if (t2.type == ty_choice ||
        (t2.type == ty_opt && (PFty_child (t2)).type == ty_choice))
        return false;
    else
        return (PFty_subtype (t1, PFty_opt (PFty_atomic ())) &&
                PFty_subtype (t2, PFty_opt (PFty_atomic ())));
}

/* enumeration of supported types */
enum kind_co {
    co_int,
    co_dbl,
    co_dec,
    co_str,
    co_bool
};
typedef enum kind_co kind_co;

/* container for type information */
struct type_co {
    kind_co kind;      /* kind of the container */
    char *table;       /* variable name of the corresponding table of values */
    char *mil_type;    /* implementation type */
    char *mil_cast;    /* the MIL constant representing the type */
    char *name;        /* the full name of the type to generate error messages */
};
typedef struct type_co type_co;

static type_co
int_container ()
{
    type_co new_co;
    new_co.kind = co_int;
    new_co.table = "int_values";
    new_co.mil_type = "int";
    new_co.mil_cast = "INT";
    new_co.name = "integer";

    return new_co;
}

static type_co
dbl_container ()
{
    type_co new_co;
    new_co.kind = co_dbl;
    new_co.table = "dbl_values";
    new_co.mil_type = "dbl";
    new_co.mil_cast = "DBL";
    new_co.name = "double";

    return new_co;
}

static type_co
dec_container ()
{
    type_co new_co;
    new_co.kind = co_dec;
    new_co.table = "dec_values";
    new_co.mil_type = "dbl";
    new_co.mil_cast = "DEC";
    new_co.name = "decimal";

    return new_co;
}

static type_co
str_container ()
{
    type_co new_co;
    new_co.kind = co_str;
    new_co.table = "str_values";
    new_co.mil_type = "str";
    new_co.mil_cast = "STR";
    new_co.name = "string";

    return new_co;
}

static type_co
bool_container ()
{
    type_co new_co;
    new_co.kind = co_bool;
    new_co.table = 0;
    new_co.mil_type = "bit";
    new_co.mil_cast = "BOOL";
    new_co.name = "boolean";

    return new_co;
}

/**
 * init introduces the initial MIL variables
 * 
 * @param f the Stream the MIL code is printed to
 */
static void
init (FILE *f)
{
    fprintf(f,
            "# init ()\n"
            /* pathfinder functions (scj, doc handling) are made visible
               in the server */
            "# module(\"pathfinder\");\n"
            "# module(\"pf_support\");\n"
            "# module(\"aggrX3\");\n"
            "# module(\"xtables\");\n"
            "# module(\"malalgebra\");\n"

            /* a new working set is created */
            "var ws := create_ws();\n"
            /* the first loop is initialized */
            "var loop000 := bat(void,oid).seqbase(0@0);\n"
            "loop000.insert(0@0, 1@0);\n"
             /* variable environment vars */
            "var vu_fid;\n"
            "var vu_vid;\n"
            "var inner000 := loop000;\n"
            "var outer000 := loop000;\n"
            "var v_vid000 := bat(void,oid).access(BAT_APPEND).seqbase(0@0);\n"
            "var v_iter000 := bat(void,oid).access(BAT_APPEND).seqbase(0@0);\n"
            "var v_pos000 := bat(void,oid).access(BAT_APPEND).seqbase(0@0);\n"
            "var v_item000 := bat(void,oid).access(BAT_APPEND).seqbase(0@0);\n"
            "var v_kind000 := bat(void,int).access(BAT_APPEND).seqbase(0@0);\n"

             /* value containers for literal values */
            "var str_values := bat(void,str).seqbase(0@0).access(BAT_WRITE);\n"
            "str_values.reverse().key(true);\n"
            "var int_values := bat(void,int).seqbase(0@0).access(BAT_WRITE);\n"
            "int_values.reverse().key(true);\n"
            "var dbl_values := bat(void,dbl).seqbase(0@0).access(BAT_WRITE);\n"
            "dbl_values.reverse().key(true);\n"
            "var dec_values := bat(void,dbl).seqbase(0@0).access(BAT_WRITE);\n"
            "dec_values.reverse().key(true);\n"

            /* reference for empty attribute construction */
            "str_values.insert(0@0,\"\");\n"
            "var EMPTY_STRING := 0@0;\n"

             /* variable binding for loop-lifting of the empty sequence */
            "var empty_bat := bat(void,oid).seqbase(0@0);\n"
            "empty_bat.access(BAT_READ);\n"
            "var empty_kind_bat := bat(void,int).seqbase(0@0);\n"
            "empty_kind_bat.access(BAT_READ);\n"

             /* variables for (intermediate) results */
            "var iter;\n"
            "var pos;\n"
            "var item;\n"
            "var kind;\n"

             /* boolean mapping */
            "var bool_not := bat(oid,oid).insert(0@0,1@0).insert(1@0,0@0);\n"
           );
    fprintf(f,
            "# create nil values\n"
            "var nil_oid_oid := nil;\n"
            "var nil_oid_int := nil;\n"
            "var nil_oid_dbl := nil;\n"
            "var nil_oid_str := nil;\n"
            "var nil_oid_bit := nil;\n"
            "var nil_oid_chr := nil;\n"
            "var nil_oid_bat := nil;\n"
            "var nil_int_oid := nil;\n"
            "var nil_dbl_oid := nil;\n"
            "var nil_str_oid := nil;\n"
            "var nil_bit_oid := nil;\n"
            "var nil_chr_oid := nil;\n"
            "var nil_bat_oid := nil;\n"
            "var nil_oid := nil;\n"
            "var nil_int := nil;\n"
            "var nil_dbl := nil;\n"
            "var nil_str := nil;\n"
            "var nil_bit := nil;\n"
            "var nil_chr := nil;\n");            
}

/**
 * the variables iter, pos, item, kind are used to
 * create an human readable output (iter|pos|item),
 * by converting the underlying value of item|kind
 * into a string
 * 
 * @param f the Stream the MIL code is printed to
 */
static void
print_output (FILE *f)
{
    fprintf(f, 
            "{ # print_output ()\n"
            /* the values of the different kinds are combined
               by inserting the converted bats into 'output_item' */
            "var output_item := bat(oid, str);\n"
  
            /* gets string values for string kind */ 
            "var temp_kind_oid_nil := kind.get_type(STR);\n"
            "var temp_kind_oid_oid := temp_kind_oid_nil.mirror().leftfetchjoin(item);\n"
            "temp_kind_oid_nil := nil_oid_oid;\n"
            "var str_kind_oid_str := temp_kind_oid_oid.leftfetchjoin(str_values);\n"
            "temp_kind_oid_oid := nil_oid_oid;\n"
            "output_item.insert(str_kind_oid_str);\n"
            "str_kind_oid_str := nil_oid_str;\n"
  
            /* gets the node information for node kind */
            "temp_kind_oid_oid := kind.get_type(ELEM).mark(0@0).reverse();\n"
            "var backup_oids := temp_kind_oid_oid.reverse();\n"
            "var temp1_frag := temp_kind_oid_oid.leftfetchjoin(kind).get_fragment();\n"
            "var oid_pre := temp_kind_oid_oid.leftfetchjoin(item);\n"
            "temp_kind_oid_oid := nil_oid_oid;\n"
            "var node_kind_oid_str;\n"
            /* distinguishes between TEXT and ELEMENT nodes */
            "{\n"
            "var oid_kind := mposjoin(oid_pre, temp1_frag, ws.fetch(PRE_KIND));\n"
/* FIXME: line should stay as long as BUG 1032273 is not solved */ "oid_kind.access(BAT_READ);\n"
            "var oid_elems := oid_kind.ord_uselect(ELEMENT).mark(0@0).reverse();\n"
            "var oid_texts := oid_kind.ord_uselect(TEXT).mark(0@0).reverse();\n"
            "oid_kind := nil_oid_chr;\n"
            "var e_pres := oid_elems.leftfetchjoin(oid_pre);\n"
            "var e_frags := oid_elems.leftfetchjoin(temp1_frag);\n"
            "var t_pres := oid_texts.leftfetchjoin(oid_pre);\n"
            "var t_frags := oid_texts.leftfetchjoin(temp1_frag);\n"
            /* creates string output for ELEMENT nodes */
            "node_kind_oid_str := [str](e_pres);\n"
            "node_kind_oid_str := node_kind_oid_str.[+](\" of frag: \");\n"
            "node_kind_oid_str := node_kind_oid_str.[+](e_frags.[str]());\n"
            "node_kind_oid_str := node_kind_oid_str.[+](\" (node) name: \");\n"
            "node_kind_oid_str := node_kind_oid_str.[+](mposjoin(mposjoin(e_pres, e_frags, ws.fetch(PRE_PROP)), "
                                                       "mposjoin(e_pres, e_frags, ws.fetch(PRE_FRAG)), "
                                                       "ws.fetch(QN_LOC)));\n"
            "node_kind_oid_str := node_kind_oid_str.[+](\"; size: \");\n"
            "node_kind_oid_str := node_kind_oid_str.[+](mposjoin(e_pres, e_frags, ws.fetch(PRE_SIZE)));\n"
            "node_kind_oid_str := node_kind_oid_str.[+](\"; level: \");\n"
            "node_kind_oid_str := node_kind_oid_str.[+]([int](mposjoin(e_pres, e_frags, ws.fetch(PRE_LEVEL))));\n"
            "e_pres := nil_oid_oid;\n"
            "e_frags := nil_oid_oid;\n"
            /* creates string output for TEXT nodes */
            "var tnode_kind_oid_str := [str](t_pres);\n"
            "tnode_kind_oid_str := tnode_kind_oid_str.[+](\" of frag: \");\n"
            "tnode_kind_oid_str := tnode_kind_oid_str.[+](t_frags.[str]());\n"
            "tnode_kind_oid_str := tnode_kind_oid_str.[+](\" (text-node) value: '\");\n"
            "tnode_kind_oid_str := tnode_kind_oid_str.[+](mposjoin(mposjoin(t_pres, t_frags, ws.fetch(PRE_PROP)), "
                                                         "mposjoin(t_pres, t_frags, ws.fetch(PRE_FRAG)), "
                                                         "ws.fetch(PROP_TEXT)));\n"
            "tnode_kind_oid_str := tnode_kind_oid_str.[+](\"'; level: \");\n"
            "tnode_kind_oid_str := tnode_kind_oid_str.[+]([int](mposjoin(t_pres, t_frags, ws.fetch(PRE_LEVEL))));\n"
            "t_pres := nil_oid_oid;\n"
            "t_frags := nil_oid_oid;\n"
            /* combines the two node outputs */
            "if (oid_elems.count() = 0) { node_kind_oid_str := tnode_kind_oid_str; } "
            "else { if (oid_texts.count() != 0) "
            "{\n"
            "var res_mu := merged_union(oid_elems, oid_texts, "
                                       "node_kind_oid_str.reverse().mark(0@0).reverse(), "
                                       "tnode_kind_oid_str.reverse().mark(0@0).reverse());\n"
            "node_kind_oid_str := res_mu.fetch(1);\n"
            "res_mu := nil_oid_bat;\n"
            "}}\n"
            "oid_elems := nil_oid_oid;\n"
            "oid_texts := nil_oid_oid;\n"
            "tnode_kind_oid_str := nil_oid_str;\n"
            "}\n"
            "oid_pre := nil_oid_oid;\n"
            "temp1_frag := nil_oid_oid;\n"
            "output_item.insert(backup_oids.leftfetchjoin(node_kind_oid_str));\n"
            "backup_oids := nil_oid_oid;\n"
            "node_kind_oid_str := nil_oid_str;\n"
  
            /* gets the attribute information for attribute kind */
            "temp_kind_oid_oid := kind.get_type(ATTR).mark(0@0).reverse();\n"
            "backup_oids := temp_kind_oid_oid.reverse();\n"
            "temp1_frag := temp_kind_oid_oid.leftfetchjoin(kind).get_fragment();\n"
            "var oid_attr := temp_kind_oid_oid.leftfetchjoin(item);\n"
            "temp_kind_oid_oid := nil_oid_oid;\n"
            "var attr_kind_oid_str := [str](oid_attr);\n"
            "attr_kind_oid_str := attr_kind_oid_str.[+](\" (attr) owned by: \");\n"
            "var owner_str := oid_attr.mposjoin(temp1_frag, ws.fetch(ATTR_OWN)).[str]();\n"
            /* translates attributes without owner differently */
            "{\n"
            "var nil_bool := owner_str.[isnil]();\n"
            "var no_owner_str := nil_bool.ord_uselect(true).mark(0@0).reverse();\n"
            "var with_owner_str := nil_bool.ord_uselect(false).mark(0@0).reverse();\n"
            "nil_bool := nil_oid_bit;\n"
            "var res_mu := merged_union(with_owner_str, no_owner_str, "
                                       "with_owner_str.leftfetchjoin(owner_str), "
                                       "no_owner_str.project(\"nil\"));\n"
            "with_owner_str := nil_oid_str;\n"
            "no_owner_str := nil_oid_str;\n"
            "owner_str := res_mu.fetch(1);\n"
            "res_mu := nil_oid_bat;\n"
            "if (owner_str.count() != attr_kind_oid_str.count()) "
            "{ ERROR (\"thinking error in attribute output printing\"); }\n"
            "}\n"
            "attr_kind_oid_str := attr_kind_oid_str.[+](owner_str);\n"
            "attr_kind_oid_str := attr_kind_oid_str.[+](\" of frag: \");\n"
            "attr_kind_oid_str := attr_kind_oid_str.[+](oid_attr.mposjoin(temp1_frag, ws.fetch(ATTR_FRAG)));\n"
            "attr_kind_oid_str := attr_kind_oid_str.[+](\"; \");\n"
            "attr_kind_oid_str := attr_kind_oid_str.[+](mposjoin(mposjoin(oid_attr, temp1_frag, ws.fetch(ATTR_QN)), "
                                                       "mposjoin(oid_attr, temp1_frag, ws.fetch(ATTR_FRAG)), "
                                                       "ws.fetch(QN_LOC)));\n"
            "attr_kind_oid_str := attr_kind_oid_str.[+](\"='\");\n"
            "attr_kind_oid_str := attr_kind_oid_str.[+](mposjoin(mposjoin(oid_attr, temp1_frag, ws.fetch(ATTR_PROP)), "
                                                       "mposjoin(oid_attr, temp1_frag, ws.fetch(ATTR_FRAG)), "
                                                       "ws.fetch(PROP_VAL)));\n"
            "attr_kind_oid_str := attr_kind_oid_str.[+](\"'\");\n"
            "owner_str := nil_oid_str;\n"
            "oid_attr := nil_oid_oid;\n"
            "temp1_frag := nil_oid_oid;\n"
            "output_item.insert(backup_oids.leftfetchjoin(attr_kind_oid_str));\n"
            "backup_oids := nil_oid_oid;\n"
            "attr_kind_oid_str := nil_oid_str;\n"
  
            /* gets the information for qname kind */
            "temp_kind_oid_oid := kind.get_type(QNAME).mirror();\n"
            "var oid_qnID := temp_kind_oid_oid.leftfetchjoin(item);\n"
            "temp_kind_oid_oid := nil_oid_oid;\n"
            "var qn_kind_oid_str := [str](oid_qnID);\n"
            "qn_kind_oid_str := qn_kind_oid_str.[+](\" (qname) '\");\n"
            "qn_kind_oid_str := qn_kind_oid_str.[+](oid_qnID.leftfetchjoin(ws.fetch(QN_NS).fetch(WS)));\n"
            "qn_kind_oid_str := qn_kind_oid_str.[+](\":\");\n"
            "qn_kind_oid_str := qn_kind_oid_str.[+](oid_qnID.leftfetchjoin(ws.fetch(QN_LOC).fetch(WS)));\n"
            "qn_kind_oid_str := qn_kind_oid_str.[+](\"'\");\n"
            "oid_qnID := nil_oid_oid;\n"
            "output_item.insert(qn_kind_oid_str);\n"
            "qn_kind_oid_str := nil_oid_str;\n"
  
            /* gets the information for boolean kind */
            "var bool_strings := bat(oid,str).insert(0@0,\"false\").insert(1@0,\"true\");\n"
            "temp_kind_oid_oid := kind.get_type(BOOL);\n"
            "temp_kind_oid_oid := temp_kind_oid_oid.mirror().leftfetchjoin(item);\n"
            "var bool_kind_oid_str := temp_kind_oid_oid.leftfetchjoin(bool_strings);\n"
            "temp_kind_oid_oid := nil_oid_oid;\n"
            "bool_strings := nil_oid_str;\n"
            "output_item.insert(bool_kind_oid_str);\n"
            "bool_kind_oid_str := nil_oid_str;\n"
  
            /* gets the information for integer kind */
            "temp_kind_oid_oid := kind.get_type(INT);\n"
            "temp_kind_oid_oid := temp_kind_oid_oid.mirror().leftfetchjoin(item);\n"
            "var temp1_int := temp_kind_oid_oid.leftfetchjoin(int_values);\n"
            "temp_kind_oid_oid := nil_oid_oid;\n"
            "var int_kind_oid_str := [str](temp1_int);\n"
            "temp1_int := nil_oid_int;\n"
            "output_item.insert(int_kind_oid_str);\n"
            "int_kind_oid_str := nil_oid_str;\n"
  
            /* gets the information for double kind */
            "temp_kind_oid_oid := kind.get_type(DBL);\n"
            "temp_kind_oid_oid := temp_kind_oid_oid.mirror().leftfetchjoin(item);\n"
            "var temp1_dbl := temp_kind_oid_oid.leftfetchjoin(dbl_values);\n"
            "temp_kind_oid_oid := nil_oid_oid;\n"
            "var dbl_kind_oid_str := [str](temp1_dbl);\n"
            "temp1_dbl := nil_oid_dbl;\n"
            "output_item.insert(dbl_kind_oid_str);\n"
            "dbl_kind_oid_str := nil_oid_str;\n"
  
            /* gets the information for decimal kind */
            "temp_kind_oid_oid := kind.get_type(DEC);\n"
            "temp_kind_oid_oid := temp_kind_oid_oid.mirror().leftfetchjoin(item);\n"
            "var temp1_dec := temp_kind_oid_oid.leftfetchjoin(dec_values);\n"
            "temp_kind_oid_oid := nil_oid_oid;\n"
            "var dec_kind_oid_str := [str](temp1_dec);\n"
            "temp1_dec := nil_oid_dbl;\n"
            "output_item.insert(dec_kind_oid_str);\n"
            "dec_kind_oid_str := nil_oid_str;\n"
  
            /* debugging output
            "print (iter, pos, item, kind);\n"
            "print (output_item);\n"
            */
            /* prints the result in a readable way */
            "printf(\"#====================#\\n\");\n"
            "printf(\"#====== result ======#\\n\");\n"
            "printf(\"#====================#\\n\");\n"
            "print (iter, pos, output_item);\n"
            "output_item := nil_oid_str;\n"
  
            /* prints the documents and the working set if 
               they have not to much elements/attributes and if
               there are not to many */
            "printf(\"#====================#\\n\");\n"
            "printf(\"#=== working set ====#\\n\");\n"
            "printf(\"#====================#\\n\");\n"
            "if (ws.fetch(PRE_SIZE).count() < 5) {\n"
            "printf(\"#- loaded documents -#\\n\");\n"
            "ws.fetch(DOC_LOADED).print();\n"
            "var i := 0;\n"
            "while (i < ws.fetch(PRE_SIZE).count()) {\n"
            "        if (i = 0) { print(\"WS\"); }\n"
            "        else { ws.fetch(DOC_LOADED).fetch(oid(i)).print(); }\n"
            "        printf(\"---- attributes ----\\n\");\n"
            "        if (ws.fetch(ATTR_OWN).fetch(i).count() < 100) {\n"
            "                var attribute := mposjoin(ws.fetch(ATTR_QN).fetch(i), "
                                                      "ws.fetch(ATTR_FRAG).fetch(i), "
                                                      "ws.fetch(QN_LOC));\n"
            "                attribute := attribute.[+](\"='\");\n"
            "                attribute := attribute.[+](mposjoin(ws.fetch(ATTR_PROP).fetch(i), "
                                                                "ws.fetch(ATTR_FRAG).fetch(i), "
                                                                "ws.fetch(PROP_VAL)));\n"
            "                attribute := attribute.[+](\"'\");\n"
            "                attribute := attribute.reverse().mark(0@0).reverse();\n"
            "                print(ws.fetch(ATTR_OWN).fetch(i), attribute);\n"
            "                attribute := nil_oid_str;\n"
            "        } else {\n"
            "                print(ws.fetch(ATTR_OWN).fetch(i).count());\n"
            "        }\n"
            "        printf(\"----- elements -----\\n\");\n"
            "        if (ws.fetch(PRE_SIZE).fetch(i).count() < 100) {\n"
            /* have to handle TEXT and ELEMENT nodes different because
               otherwise fetch causes error */
            "                ws.fetch(PRE_KIND).fetch(i).access(BAT_READ);\n"
            "                var elems := ws.fetch(PRE_KIND).fetch(i).ord_uselect(ELEMENT).mark(0@0).reverse();\n"
            "                var e_props := elems.leftfetchjoin(ws.fetch(PRE_PROP).fetch(i));\n"
            "                var e_frags := elems.leftfetchjoin(ws.fetch(PRE_FRAG).fetch(i));\n"
            "                var e_qns := mposjoin(e_props, e_frags, ws.fetch(QN_LOC));\n"
            "                e_props := nil_oid_oid;\n"
            "                e_frags := nil_oid_oid;\n"
            "                var texts := ws.fetch(PRE_KIND).fetch(i).ord_uselect(TEXT).mark(0@0).reverse();\n"
            "                var t_props := texts.leftfetchjoin(ws.fetch(PRE_PROP).fetch(i));\n"
            "                var t_frags := texts.leftfetchjoin(ws.fetch(PRE_FRAG).fetch(i));\n"
            "                var t_qns := mposjoin(t_props, t_frags, ws.fetch(PROP_TEXT));\n"
            "                t_props := nil_oid_oid;\n"
            "                t_frags := nil_oid_oid;\n"
            "                var t_names := texts.project(\"(TEXT) '\").[+](t_qns)[+](\"'\");\n"
            "                t_names := t_names.reverse().mark(0@0).reverse();\n"
            "                var res_mu := merged_union(elems, texts, e_qns, t_names);\n"
            "                elems := nil_oid_oid;\n"
            "                texts := nil_oid_oid;\n"
            "                e_qns := nil_oid_str;\n"
            "                t_qns := nil_oid_str;\n"
            "                t_names := nil_oid_str;\n"
            "                ws.fetch(PRE_KIND).fetch(i).access(BAT_WRITE);\n"
            "                var names := res_mu.fetch(0).reverse().leftfetchjoin(res_mu.fetch(1));\n"
            "                print(ws.fetch(PRE_SIZE).fetch(i), "
                                  "ws.fetch(PRE_LEVEL).fetch(i).[int](), "
                                  "names);\n"
            "                res_mu := nil_oid_bat;\n"
            "                names := nil_oid_str;\n"
            "        } else {\n"
            "                print(ws.fetch(PRE_SIZE).fetch(i).count());\n"
            "        }\n"
            "i :+= 1;\n"
            "}\n"
            "i := nil_int;\n"
            "} else {\n"
            "print(\"to much content in the WS to print it for debugging purposes\");\n"
            "if (ws.fetch(DOC_LOADED).count() > 25) \n"
            "{ printf(\"# (number of loaded documents: %%i) #\\n\", ws.fetch(DOC_LOADED).count()); } "
            "else {\n"
            "printf(\"#- loaded documents -#\\n\");\n"
            "ws.fetch(DOC_LOADED).print();\n"
            "}\n"
            "}\n"
            "} # end of print_output ()\n"
           );
}

/**
 * translateEmpty translates the empty sequence and gives back 
 * empty bats for the intermediate result (iter|pos|item|kind)
 * 
 * @param f the Stream the MIL code is printed to
 */
static void
translateEmpty (FILE *f)
{
    fprintf(f,
            "# translateEmpty ()\n"
            "iter := empty_bat;\n"
            "pos := empty_bat;\n"
            "item := empty_bat;\n"
            "kind := empty_kind_bat;\n");
}

/**
 * cleanUpLevel sets all variables needed for 
 * a new scope introduced by a for-expression
 * back to nil
 * 
 * @param f the Stream the MIL code is printed to
 * @param act_level the level of the for-scope
 */
static void
cleanUpLevel (FILE *f, int act_level)
{
    fprintf(f, "# cleanUpLevel ()\n");
    fprintf(f, "inner%03u := nil_oid_oid;\n", act_level);
    fprintf(f, "outer%03u := nil_oid_oid;\n", act_level);
    fprintf(f, "loop%03u := nil_oid_oid;\n", act_level);

    fprintf(f, "v_vid%03u := nil_oid_oid;\n", act_level);
    fprintf(f, "v_iter%03u := nil_oid_oid;\n", act_level);
    fprintf(f, "v_pos%03u := nil_oid_oid;\n", act_level);
    fprintf(f, "v_item%03u := nil_oid_oid;\n", act_level);
    fprintf(f, "v_kind%03u := nil_oid_int;\n", act_level);
}
                                                                                                                                                        
/**
 * translateVar looks up a variable in the 
 * actual scope and binds it values to the intermediate
 * result (iter|pos|item|kind)
 * 
 * @param f the Stream the MIL code is printed to
 * @param act_level the level of the for-scope
 * @param c the core node of the variable
 */
static void
translateVar (FILE *f, int act_level, PFcnode_t *c)
{
    fprintf(f, "{ # translateVar (c)\n");
    fprintf(f, "var vid := v_vid%03u.ord_uselect(%i@0);\n", 
            act_level, c->sem.var->vid);
    fprintf(f, "vid := vid.mark(0@0).reverse();\n");
    fprintf(f, "iter := vid.leftfetchjoin(v_iter%03u);\n", act_level);
    fprintf(f, "pos := vid.leftfetchjoin(v_pos%03u);\n", act_level);
    fprintf(f, "item := vid.leftfetchjoin(v_item%03u);\n", act_level);
    fprintf(f, "kind := vid.leftfetchjoin(v_kind%03u);\n", act_level);
    fprintf(f, "vid := nil_oid_oid;\n");
    fprintf(f, "} # end of translateVar (c)\n");
}

/**
 * saveResult binds a intermediate result to a set of
 * variables, which are not used
 * (saveResult should be used in pairs with deleteResult)
 *
 * @param f the Stream the MIL code is printed to
 * @param counter the actual offset of saved variables
 */
static void
saveResult (FILE *f, int counter)
{
    fprintf(f, "{ # saveResult%i () : int\n", counter);
    fprintf(f, "var iter%03u := iter;\n", counter);
    fprintf(f, "var pos%03u := pos;\n", counter);
    fprintf(f, "var item%03u := item;\n", counter);
    fprintf(f, "var kind%03u := kind;\n", counter);
    fprintf(f,
            "iter := nil_oid_oid;\n"
            "pos := nil_oid_oid;\n"
            "item := nil_oid_oid;\n"
            "kind := nil_oid_int;\n"
            "# end of saveResult%i () : int\n", counter
           );
}

/**
 * deleteResult deletes a saved intermediate result and
 * gives free the offset to be reused (return value)
 * (deleteResult should be used in pairs with saveResult)
 *
 * @param f the Stream the MIL code is printed to
 * @param counter the actual offset of saved variables
 */
static void
deleteResult (FILE *f, int counter)
{
    fprintf(f, "# deleteResult%i ()\n", counter);
    fprintf(f, "iter%03u := nil_oid_oid;\n", counter);
    fprintf(f, "pos%03u := nil_oid_oid;\n", counter);
    fprintf(f, "item%03u := nil_oid_oid;\n", counter);
    fprintf(f, "kind%03u := nil_oid_int;\n", counter);
    fprintf(f, "} # end of deleteResult%i ()\n", counter);
}

/**
 * translateSeq combines two intermediate results and saves in 
 * the intermediate result (iter|pos|item|kind) sorted by 
 * iter (under the condition, that the incoming iters of each
 * input where sorted already)
 * 
 * @param f the Stream the MIL code is printed to
 * @param i the offset of the first intermediate result
 */
static void
translateSeq (FILE *f, int i)
{
    /* pruning of the two cases where one of
       the intermediate results is empty */
    fprintf(f,
            "if (iter.count() = 0) {\n"
            "        iter := iter%03u;\n"
            "        pos := pos%03u;\n"
            "        item := item%03u;\n"
            "        kind := kind%03u;\n",
            i, i, i, i);
    fprintf(f, 
            "} else { if (iter%03u.count() != 0)\n",
            i);
    fprintf(f,
            "{ # translateSeq (counter)\n"
            /* FIXME: tests if input is sorted is needed because of merged union*/
            "iter%03u.chk_order(false);\n"
            "iter.chk_order(false);\n" 
            "var merged_result := merged_union "
            "(iter%03u, iter, item%03u, item, kind%03u, kind);\n",
            i, i, i, i);
    fprintf(f,
            "iter := merged_result.fetch(0);\n"
            "item := merged_result.fetch(1);\n"
            "kind := merged_result.fetch(2);\n"
            "merged_result := nil_oid_bat;\n"
            "pos := iter.mark_grp(iter.tunique().project(1@0));\n"
            "} # end of translateSeq (counter)\n"
            "}\n");
}

/**
 * project creates the variables for the next for-scope
 *
 * @param f the Stream the MIL code is printed to
 * @param act_level the level of the for-scope
 */
static void
project (FILE *f, int act_level)
{
    fprintf(f, "# project ()\n");
    fprintf(f, "var outer%03u := iter;\n", act_level);
    fprintf(f, "iter := iter.mark(1@0);\n");
    fprintf(f, "var inner%03u := iter;\n", act_level);
    fprintf(f, "pos := iter.project(1@0);\n");
    fprintf(f, "var loop%03u := inner%03u;\n", act_level, act_level);

    fprintf(f, "var v_vid%03u;\n", act_level);
    fprintf(f, "var v_iter%03u;\n", act_level);
    fprintf(f, "var v_pos%03u;\n", act_level);
    fprintf(f, "var v_item%03u;\n", act_level);
    fprintf(f, "var v_kind%03u;\n", act_level);
}

/**
 * getExpanded looks up the variables with which are expanded
 * (because needed) in the next deeper for-scope nesting 
 *
 * @param f the Stream the MIL code is printed to
 * @param act_level the level of the for-scope
 * @param fid the number of the for-scope to look up
 *        the list of variables which have to be expanded
 */
static void
getExpanded (FILE *f, int act_level, int fid)
{
    fprintf(f, 
            "{ # getExpanded (fid)\n"
            "var vu_nil := vu_fid.ord_uselect(%i@0);\n",
            fid);
    fprintf(f,
            "var vid_vu := vu_vid.reverse();\n"
            "var oid_nil := vid_vu.leftjoin(vu_nil);\n"
            "vid_vu := nil_oid_oid;\n"
            "vu_nil := nil_oid_oid;\n"
            "expOid := v_vid%03u.leftjoin(oid_nil);\n",
            /* the vids from the nesting before are looked up */
            act_level - 1);
    fprintf(f,
            "oid_nil := nil_oid_oid;\n"
            "expOid := expOid.mirror();\n"
            "} # end of getExpanded (fid)\n");
}

/**
 * expand joins inner_outer and iter and sorts out the
 * variables which shouldn't be expanded by joining with expOid
 *
 * @param f the Stream the MIL code is printed to
 * @param act_level the level of the for-scope
 */
static void
expand (FILE *f, int act_level)
{
    fprintf(f,
            "{ # expand ()\n"
            "var expOid_iter := expOid.leftfetchjoin(v_iter%03u);\n",
            /* the iters from the nesting before are looked up */
            act_level - 1); 
    fprintf(f,
            "expOid := nil_oid_oid;\n"
            "var iter_expOid := expOid_iter.reverse();\n"
            "expOid_iter := nil_oid_oid;\n"
            "var oidMap_expOid := outer%03u.leftjoin(iter_expOid);\n",
            act_level);
    fprintf(f,
            "iter_expOid := nil_oid_oid;\n"
            "var expOid_oidMap := oidMap_expOid.reverse();\n"
            "oidMap_expOid := nil_oid_oid;\n"
            "expOid_iter := expOid_oidMap.leftfetchjoin(inner%03u);\n",
            act_level);
    fprintf(f,
            "expOid_oidMap := nil_oid_oid;\n"
            "v_iter%03u := expOid_iter;\n",
            act_level);
    /* oidNew_expOid is the relation which maps from old scope to the
       new scope */
    fprintf(f,
            "oidNew_expOid := expOid_iter.mark(0@0).reverse();\n"
            "expOid_iter := nil_oid_oid;\n"
            "} # end of expand ()\n");
}

/**
 * join maps the five columns (vid|iter|pos|item|kind) to the next scope
 * and reserves the double size in the bats for inserts from let-expressions
 *
 * @param f the Stream the MIL code is printed to
 * @param act_level the level of the for-scope
 */
static void
join (FILE *f, int act_level)
{
    fprintf(f, "{ # join ()\n");
    fprintf(f, "v_iter%03u := v_iter%03u.reverse().mark(0@0).reverse();\n",
            act_level, act_level);
    fprintf(f, "var new_v_iter := v_iter%03u;\n", act_level);
    fprintf(f, "v_iter%03u := bat(void,oid,count(new_v_iter)*2);\n", act_level);
    fprintf(f, "v_iter%03u.seqbase(0@0);\n", act_level);
    fprintf(f, "v_iter%03u.access(BAT_APPEND);\n", act_level);
    fprintf(f, "v_iter%03u.insert(new_v_iter);\n", act_level);
    fprintf(f, "new_v_iter := nil_oid_oid;\n");

    fprintf(f, "var new_v_vid := oidNew_expOid.leftjoin(v_vid%03u);\n",
            act_level - 1);
    fprintf(f, "v_vid%03u := bat(void,oid,count(new_v_vid)*2);\n", act_level);
    fprintf(f, "v_vid%03u.seqbase(0@0);\n", act_level);
    fprintf(f, "v_vid%03u.access(BAT_APPEND);\n", act_level);
    fprintf(f, "v_vid%03u.insert(new_v_vid);\n", act_level);
    fprintf(f, "new_v_vid := nil_oid_oid;\n");

    fprintf(f, "var new_v_pos := oidNew_expOid.leftjoin(v_pos%03u);\n",
            act_level - 1);
    fprintf(f, "v_pos%03u := bat(void,oid,count(new_v_pos)*2);\n", act_level);
    fprintf(f, "v_pos%03u.seqbase(0@0);\n", act_level);
    fprintf(f, "v_pos%03u.access(BAT_APPEND);\n", act_level);
    fprintf(f, "v_pos%03u.insert(new_v_pos);\n", act_level);
    fprintf(f, "new_v_pos := nil_oid_oid;\n");

    fprintf(f, "var new_v_item := oidNew_expOid.leftjoin(v_item%03u);\n",
            act_level - 1);
    fprintf(f, "v_item%03u := bat(void,oid,count(new_v_item)*2);\n", act_level);
    fprintf(f, "v_item%03u.seqbase(0@0);\n", act_level);
    fprintf(f, "v_item%03u.access(BAT_APPEND);\n", act_level);
    fprintf(f, "v_item%03u.insert(new_v_item);\n", act_level);
    fprintf(f, "new_v_item := nil_oid_oid;\n");

    fprintf(f, "var new_v_kind := oidNew_expOid.leftjoin(v_kind%03u);\n",
            act_level - 1);
    fprintf(f, "v_kind%03u := bat(void,int,count(new_v_kind)*2);\n", act_level);
    fprintf(f, "v_kind%03u.seqbase(0@0);\n", act_level);
    fprintf(f, "v_kind%03u.access(BAT_APPEND);\n", act_level);
    fprintf(f, "v_kind%03u.insert(new_v_kind);\n", act_level);
    fprintf(f, "new_v_kind := nil_oid_int;\n");

    fprintf(f, "oidNew_expOid := nil_oid;\n");
    fprintf(f, "} # end of join ()\n");

    /*
    fprintf(f, "print (\"testoutput in join() expanded to level %i\");\n",
            act_level);
    fprintf(f, "print (v_vid%03u, v_iter%03u, v_pos%03u, v_kind%03u);\n",
            act_level, act_level, act_level, act_level);
    */
}

/**
 * mapBack joins back the intermediate result to their old
 * iter values after the execution of the body of the for-expression
 *
 * @param f the Stream the MIL code is printed to
 * @param act_level the level of the for-scope
 */
static void
mapBack (FILE *f, int act_level)
{
    fprintf(f,
            "{ # mapBack ()\n"
            /* the iters are mapped back to the next outer scope */
            "var iter_oidMap := inner%03u.reverse();\n",
            act_level);
    fprintf(f,
            "var oid_oidMap := iter.leftfetchjoin(iter_oidMap);\n"
            "iter_oidMap := nil_oid_oid;\n"
            "iter := oid_oidMap.leftfetchjoin(outer%03u);\n",
            act_level);
    fprintf(f,
            "oid_oidMap := nil_oid_oid;\n"
            "pos := iter.mark_grp(iter.tunique().project(1@0));\n"
            "# item := item;\n"
            "# kind := kind;\n"
            "} # end of mapBack ()\n"
           );
}

/**
 * createNewVarTable creates new bats for the next for-scope
 * in case no variables will be expanded
 *
 * @param f the Stream the MIL code is printed to
 * @param act_level the level of the for-scope
 */
static void
createNewVarTable (FILE *f, int act_level)
{
    fprintf(f, "# createNewVarTable ()\n");
    fprintf(f,
            "v_iter%03u := bat(void,oid).seqbase(0@0).access(BAT_APPEND);\n",
            act_level);
    fprintf(f,
            "v_vid%03u := bat(void,oid).seqbase(0@0).access(BAT_APPEND);\n",
            act_level);
    fprintf(f,
            "v_pos%03u := bat(void,oid).seqbase(0@0).access(BAT_APPEND);\n",
            act_level);
    fprintf(f,
            "v_item%03u := bat(void,oid).seqbase(0@0).access(BAT_APPEND);\n",
            act_level);
    fprintf(f,
            "v_kind%03u := bat(void,int).seqbase(0@0).access(BAT_APPEND);\n",
            act_level);
}

/**
 * append appends the information of a variable to the 
 * corresponding column of the variable environment
 *
 * @param f the Stream the MIL code is printed to
 * @param name the columnname
 * @param type_ the tail type of the column
 * @param level the actual level of the for-scope
 */
static void
append (FILE *f, char *name, int level, char *type_)
{
    fprintf(f, "{ # append (%s, level)\n", name);
    fprintf(f, "var seqb := oid(v_%s%03u.count());\n",name, level);
    fprintf(f, "var temp_%s := %s.reverse().mark(seqb).reverse();\n", name, name);
    fprintf(f, "seqb := nil_oid;\n");
    fprintf(f, "v_%s%03u.insert(temp_%s);\n", name, level, name);
    fprintf(f, "temp_%s := nil_oid_%s;\n", name, type_);
    fprintf(f, "} # append (%s, level)\n", name);
}

/**
 * insertVar adds a intermediate result (iter|pos|item|kind) to
 * the variable environment in the actual for-scope (let-expression)
 * 
 * @param f the Stream the MIL code is printed to
 * @param act_level the level of the for-scope
 * @param vid the key value of the intermediate result which is later used to
 *        look it up again 
 */
static void
insertVar (FILE *f, int act_level, int vid)
{
    fprintf(f,
            "{ # insertVar (vid)\n"
            "var vid := iter.project(%i@0);\n",
            vid);

    append (f, "vid", act_level, "oid");
    append (f, "iter", act_level, "oid");
    append (f, "pos", act_level, "oid");
    append (f, "item", act_level, "oid");
    append (f, "kind", act_level, "int");

    fprintf(f, "vid := nil_oid_oid;\n");
    /*
    fprintf(f, 
            "print(\"testoutput in insertVar(%i@0) expanded to level %i\");\n",
            vid, act_level);
    fprintf(f, 
            "print (v_vid%03u, v_iter%03u, v_pos%03u, v_kind%03u);\n",
            act_level, act_level, act_level, act_level);
    */
    fprintf(f, "} # insertVar (vid)\n");
}

/**
 * translateConst translates the loop-lifting of a Constant
 * (before calling a variable 'itemID' with an oid has to be bound)
 *
 * @param f the Stream the MIL code is printed to
 * @param act_level the level of the for-scope
 * @param kind the kind of the item
 */
static void
translateConst (FILE *f, int act_level, char *kind)
{
    fprintf(f,
            "# translateConst (kind)\n"
            "iter := loop%03u.reverse().mark(0@0).reverse();\n"
            "pos := iter.project(1@0);\n"
            "item := iter.project(itemID);\n"
            "kind := iter.project(%s);\n",
            act_level, kind);
}

/**
 * loop_liftedSCJ is the loop-lifted version of the staircasejoin,
 * which translates the attribute step and calls the iterative version
 * of the loop-lifted staircasejoin for the other axis
 * FIXME: self axis is missing
 *
 * @param f the Stream the MIL code is printed to
 * @param axis the string containing the axis information
 * @param kind the string containing the kind information
 * @param ns the string containing the qname namespace information
 * @param loc the string containing the qname local part information
 */
static void
loop_liftedSCJ (FILE *f, char *axis, char *kind, char *ns, char *loc)
{
    /* iter|pos|item input contains only nodes (kind=ELEM) */
    fprintf(f, "# loop_liftedSCJ (axis, kind, ns, loc)\n");

    if (!strcmp (axis, "attribute"))
    {
        fprintf(f,
            "{ # attribute axis\n"
            /* get all unique iter|item combinations */
            "var unq := CTgroup(iter).CTgroup(item)"
                       ".CTgroup(kind).tunique().mark(0@0).reverse();\n"
            /* if unique destroys the order a sort is needed */
            /*
            "iter_item := iter_item.sort();\n"
            */
            "var oid_iter := unq.leftfetchjoin(iter);\n"
            "var oid_item := unq.leftfetchjoin(item);\n"
            "var oid_frag := unq.leftfetchjoin(kind.get_fragment());\n"
            "unq := nil_oid_oid;\n"
            /* get the attribute ids from the pre values */
            "var temp1 := mvaljoin (oid_item, oid_frag, ws.fetch(ATTR_OWN));\n"
            "oid_item := nil_oid_oid;\n"
            "oid_frag := temp1.mark(0@0).reverse().leftfetchjoin(oid_frag);\n"
            "var oid_attr := temp1.reverse().mark(0@0).reverse();\n"
            "oid_iter := temp1.mark(0@0).reverse().leftfetchjoin(oid_iter);\n"
            "temp1 := nil_oid_oid;\n"
            "var temp1_str; # only needed for name test\n"
           );

        if (ns)
        {
            fprintf(f,
                    "temp1_str := mposjoin(mposjoin(oid_attr, oid_frag, ws.fetch(ATTR_QN)), "
                                          "mposjoin(oid_attr, oid_frag, ws.fetch(ATTR_FRAG)), "
                                          "ws.fetch(QN_NS));\n"
                    "temp1 := temp1_str.ord_uselect(\"%s\");\n"
                    "temp1_str := nil_oid_str;\n",
                    ns);
            fprintf(f,
                    "temp1 := temp1.mark(0@0).reverse();\n"
                    "oid_attr := temp1.leftfetchjoin(oid_attr);\n"
                    "oid_frag := temp1.leftfetchjoin(oid_frag);\n"
                    "oid_iter := temp1.leftfetchjoin(oid_iter);\n"
                    "temp1 := nil_oid_oid;\n");
        }
        if (loc)
        {
            fprintf(f,
                    "temp1_str := mposjoin(mposjoin(oid_attr, oid_frag, ws.fetch(ATTR_QN)), "
                                      "mposjoin(oid_attr, oid_frag, ws.fetch(ATTR_FRAG)), "
                                      "ws.fetch(QN_LOC));\n"
                    "temp1 := temp1_str.ord_uselect(\"%s\");\n"
                    "temp1_str := nil_oid_str;\n",
                    loc);
            fprintf(f,
                    "temp1 := temp1.mark(0@0).reverse();\n"
                    "oid_attr := temp1.leftfetchjoin(oid_attr);\n"
                    "oid_frag := temp1.leftfetchjoin(oid_frag);\n"
                    "oid_iter := temp1.leftfetchjoin(oid_iter);\n"
                    "temp1 := nil_oid_oid;\n");
        }

        /* add '.reverse().mark(0@0).reverse()' to be sure that the head of 
           the results is void */
        fprintf(f,
                "res_scj := bat(void,bat).seqbase(0@0);\n"
                "res_scj.insert(nil, oid_iter.reverse().mark(0@0).reverse());\n"
                "oid_iter := nil_oid_oid;\n"
                "res_scj.insert(nil, oid_attr.reverse().mark(0@0).reverse());\n"
                "oid_attr := nil_oid_oid;\n"
                "res_scj.insert(nil, oid_frag.reverse().mark(0@0).reverse());\n"
                "oid_frag := nil_oid_oid;\n"
                "} # end of attribute axis\n");
    }
    else
    {
        /* FIXME: in case iter is not sorted pf:distinct-doc-order 
                  should be called */
        if (kind)
        {
            fprintf(f,
                    "res_scj := loop_lifted_%s_step_with_kind_test_joined"
                    "(iter, item, kind.get_fragment(), ws, %s);\n",
                    axis, kind);
        }
        else if (ns && loc)
        {
            fprintf(f,
                    "res_scj := loop_lifted_%s_step_with_nsloc_test_joined"
                    "(iter, item, kind.get_fragment(), ws, \"%s\", \"%s\");\n",
                    axis, ns, loc);
        }
        else if (loc)
        {
            fprintf(f,
                    "res_scj := loop_lifted_%s_step_with_loc_test_joined"
                    "(iter, item, kind.get_fragment(), ws, \"%s\");\n",
                    axis, loc);
        }
        else if (ns)
        {
            fprintf(f,
                    "res_scj := loop_lifted_%s_step_with_ns_test_joined"
                    "(iter, item, kind.get_fragment(), ws, \"%s\");\n", 
                    axis, ns);
        }
        else
        {
            fprintf(f,
                    "res_scj := loop_lifted_%s_step_joined"
                    "(iter, item, kind.get_fragment(), ws);\n", 
                    axis);
        }
    }
}

/**
 * translateLocsteps finds the right parameters for the staircasejoin
 * and calls it with this parameters
 *
 * @param f the Stream the MIL code is printed to
 * @param c the Core node containing the step information
 */
static void
translateLocsteps (FILE *f, PFcnode_t *c)
{
    char *axis, *ns, *loc;

    fprintf(f, 
            "{ # translateLocsteps (c)\n"
            /* variable for the (iterative) scj */
            "var res_scj;"
  
            /* make this path step only for nodes */
            /* this shouldn't be necessary because
               distinct-doc-order makes sure that only nodes
               are used for a path step
            "var sel_ls := kind.get_type(ELEM);\n"
            "if (sel_ls.count() != kind.count())\n"
            "{    ERROR(\"location step only allows "
                 "nodes as input parameter\"); }\n"
            "sel_ls := sel_ls.mark(0@0).reverse();\n"
            "item := sel_ls.leftfetchjoin(item);\n"
            "iter := sel_ls.leftfetchjoin(iter);\n"
            "kind := sel_ls.leftfetchjoin(kind);\n"
            "sel_ls := nil_oid_oid;\n"
            */
           );

    switch (c->kind)
    {
        case c_ancestor:
            axis = "ancestor";
            break;
        case c_ancestor_or_self:
            axis = "ancestor_or_self";
            break;
        case c_attribute:
            axis = "attribute";
            break;
        case c_child:
            axis = "child";
            break;
        case c_descendant:
            axis = "descendant";
            break;
        case c_descendant_or_self:
            axis = "descendant_or_self";
            break;
        case c_following:
            axis = "following";
            break;
        case c_following_sibling:
            axis = "following_sibling";
            break;
        case c_parent:
            axis = "parent";
            break;
        case c_preceding:
            axis = "preceding";
            break;
        case c_preceding_sibling:
            axis = "preceding_sibling";
            break;
        case c_self:
            axis = "attribute";
            break;
        default:
            PFoops (OOPS_FATAL, "XPath axis is not supported in MIL-translation");
    }

    switch (c->child[0]->kind)
    {
        case c_namet:
            ns = c->child[0]->sem.qname.ns.uri;
            loc = c->child[0]->sem.qname.loc;

            /* translate wildcard '*' as 0 and missing ns as "" */
            if (!ns)
                ns = "";
            else if (!strcmp (ns,"*"))
                ns = 0;

            /* translate wildcard '*' as 0 */
            if (loc && (!strcmp(loc,"*")))
                loc = 0;

            loop_liftedSCJ (f, axis, 0, ns, loc); 
            break;
        case c_kind_node:
            loop_liftedSCJ (f, axis, 0, 0, 0);
            break;
        case c_kind_comment:
            loop_liftedSCJ (f, axis, "COMMENT", 0, 0);
            break;
        case c_kind_text:
            loop_liftedSCJ (f, axis, "TEXT", 0, 0);
            break;
        case c_kind_pi:
            loop_liftedSCJ (f, axis, "PI", 0, 0);
            break;
        case c_kind_doc:
            loop_liftedSCJ (f, axis, "DOCUMENT", 0, 0);
            break;
        case c_kind_elem:
            loop_liftedSCJ (f, axis, "ELEMENT", 0, 0);
            break;
        case c_kind_attr:
            loop_liftedSCJ (f, axis, "ATTRIBUTE", 0, 0);
            break;
        default:
            PFoops (OOPS_FATAL, "illegal node test in MIL-translation");
            break;
    }

    /* res_scj = iter|item bat */
    fprintf(f,
            "iter := res_scj.fetch(0);\n"
            "pos := iter.mark_grp(iter.tunique().project(1@0));\n"
            "item := res_scj.fetch(1);\n"
           );
    if (!strcmp (axis, "attribute"))
            fprintf(f, "kind := res_scj.fetch(2).get_kind(ATTR);\n");
    else
            fprintf(f, "kind := res_scj.fetch(2).get_kind(ELEM);\n");

    fprintf(f,
            "res_scj := nil_oid_bat;\n"
            "} # end of translateLocsteps (c)\n"
           );
}

/**
 * addValues inserts values into a table which are not already in
 * in the table (where the tail is supposed to be key(true)) and gives
 * back the offsets for all values
 *
 * @param f the Stream the MIL code is printed to
 * @param tablename the variable name where the entries are inserted
 * @param varname the variable which holds the input entries and
 *        gets the offsets of the added items
 * @param var_type the type of the entered values
 * @param result_var the variable, to which the result is bound
 */
static void
addValues (FILE *f, 
           type_co t_co,
           char *varname,
           char *result_var)
{
    fprintf(f, "%s.seqbase(nil);\n", t_co.table);
    fprintf(f, "var ins_vals := %s.reverse().mark(nil).reverse();\n", varname);
    fprintf(f, "%s.insert(ins_vals);\n", t_co.table);
    fprintf(f, "ins_vals := nil_oid_%s;\n", t_co.mil_type);
    fprintf(f, "%s.seqbase(0@0);\n", t_co.table);
    /* get the offsets of the values */
    fprintf(f, "%s := %s.leftjoin(%s.reverse());\n", 
            result_var, varname, t_co.table);
}

/**
 * createEnumeration creates the Enumeration needed for the
 * changes item and inserts if needed
 * the int values to 'int_values'
 *
 * @param f the Stream the MIL code is printed to
 */
static void
createEnumeration (FILE *f, int act_level)
{
    fprintf(f,
            "{ # createEnumeration ()\n"
            /* the head of item has to be void */
            "var ints_cE := outer%03u.mark_grp(outer%03u.tunique().project(1@0)).[int]();\n",
            act_level, act_level);
    addValues (f, int_container(), "ints_cE", "item");
    fprintf(f,
            "item := item.reverse().mark(0@0).reverse();\n"
            "ints_cE := nil_oid_int;\n"
            "iter := inner%03u.reverse().mark(0@0).reverse();\n"
            "pos := iter.project(1@0);\n"
            /* change kind information to int */
            "kind := kind.project(INT);\n"
            "} # end of createEnumeration ()\n",
            act_level);
}

/**
 * castQName casts strings to QNames
 * - only strings are allowed 
 * - doesn't test text any further 
 * - translates only into string into local part
 *
 * @param f the Stream the MIL code is printed to
 */
static void
castQName (FILE *f)
{
    fprintf(f,
            "{ # castQName ()\n"
            "var qnames := kind.get_type(QNAME);\n"
            "var counted_items := kind.count();\n"
            "var counted_qn := qnames.count();\n"
            "if (counted_items != counted_qn)\n"
            "{\n"
            "var strings := kind.ord_uselect(STR);\n"
            "if (counted_items != (strings.count() + counted_qn)) "
            "{ ERROR (\"only strings and qnames can be"
            "casted to qnames\"); }\n"
            "counted_items := nil_int;\n"

            "var oid_oid := strings.mark(0@0).reverse();\n"
            "strings := nil_oid_oid;\n"
            "var oid_item := oid_oid.leftfetchjoin(item);\n"
            /* get all the unique strings */
            "strings := oid_item.tunique().mark(0@0).reverse();\n"
            "var oid_str := strings.leftfetchjoin(str_values);\n"
            "strings := nil_oid_oid;\n"

            /* string name is only translated into local name, because
               no URIs for the namespace are available */
            "var prop_id := ws.fetch(QN_NS).fetch(WS).ord_uselect(\"\");\n"
            "var prop_name := prop_id.mirror().leftfetchjoin(ws.fetch(QN_LOC).fetch(WS));\n"
            "prop_id := nil_oid_oid;\n"

            /* find all strings which are not in the qnames of the WS */
            "var str_oid := oid_str.reverse().kdiff(prop_name.reverse());\n"
            "oid_str := nil_oid_str;\n"
            "prop_name := nil_oid_str;\n"
            "oid_str := str_oid.mark(oid(ws.fetch(QN_LOC).fetch(WS).count())).reverse();\n"
            "str_oid := nil_str_oid;\n"
            /* add the strings as local part of the qname into the working set */
            "ws.fetch(QN_LOC).fetch(WS).insert(oid_str);\n"
            "oid_str := oid_str.project(\"\");\n"
            "ws.fetch(QN_NS).fetch(WS).insert(oid_str);\n"
            "oid_str := nil_oid_str;\n"

            /* get all the possible matching names from the updated working set */
            "prop_id := ws.fetch(QN_NS).fetch(WS).ord_uselect(\"\");\n"
            "prop_name := prop_id.mirror().leftfetchjoin(ws.fetch(QN_LOC).fetch(WS));\n"
            "prop_id := nil_oid_oid;\n"

            "oid_str := oid_item.leftfetchjoin(str_values);\n"
            "oid_item := nil_oid_oid;\n"
            /* get property ids for each string */
            "var oid_prop := oid_str.leftjoin(prop_name.reverse());\n"
            "oid_str := nil_oid_str;\n"
            "prop_name := nil_oid_str;\n"
            /* oid_prop now contains the items with property ids
               which were before strings */
            "if (counted_qn = 0)\n"
            /* the only possible input kind is string -> oid_oid=void|void */
            "{    item := oid_prop.reverse().mark(0@0).reverse(); } "
            "else {\n"
            /* qnames and newly generated qnames are merged 
               (first 2 parameters are the oids for the sorting) */
            "    var res_mu := merged_union"
                        "(oid_oid, "
                         "qnames.mark(0@0).reverse(), "
                         "oid_prop.reverse().mark(0@0).reverse(), "
                         "qnames.mark(0@0).reverse().leftfetchjoin(item));\n"
            "    item := res_mu.fetch(1);\n"
            "    res_mu := nil_oid_bat;\n"
            "}\n"
            "oid_oid := nil_oid_oid;\n"
            "oid_prop := nil_oid_oid;\n"
            "qnames := nil_oid_oid;\n"
            "counted_qn := nil_int;\n"

            "kind := item.project(QNAME);\n"
            "}\n"
            "} # end of castQName ()\n"
           );
    }

/**
 * loop_liftedElemConstr creates new elements with their 
 * element and attribute subtree copies as well as the attribute 
 * contents
 *
 * @param f the Stream the MIL code is printed to
 * @param i the counter of the actual saved result (elem name)
 */
static void
loop_liftedElemConstr (FILE *f, int i)
{
    fprintf(f,
            "{ # loop_liftedElemConstr (counter)\n"
            "var root_level;\n"
            "var root_size;\n"
            "var root_kind;\n"
            "var root_frag;\n"
            "var root_prop;\n"

 /* attr */ "var preNew_preOld;\n"
 /* attr */ "var preNew_frag;\n"
 /* attr */ "var attr := kind.get_type(ATTR).mark(0@0).reverse();\n"
 /* attr */ "var attr_iter := attr.leftfetchjoin(iter);\n"
 /* attr */ "var attr_item := attr.leftfetchjoin(item);\n"
 /* attr */ "var attr_frag := attr.leftfetchjoin(kind).get_fragment();\n"
 /* attr */ "attr := nil_oid_oid;\n"

            /* there can be only nodes and attributes - everything else
               should cause an error */

            "var nodes := kind.get_type(ELEM);\n"
            /* if no nodes are found we jump right to the end and only
               have to execute the stuff for the root construction */
            "if (nodes.count() != 0) {\n"
    
            "var oid_oid := nodes.mark(0@0).reverse();\n"
            "nodes := nil_oid_oid;\n"
            "var node_items := oid_oid.leftfetchjoin(item);\n"
            "var node_frags := oid_oid.leftfetchjoin(kind).get_fragment();\n"
            /* set iter to a distinct list and therefore don't
               prune any node */
            "var iter_input := oid_oid.mirror();\n"

            /* get all subtree copies */
            "var res_scj := loop_lifted_descendant_or_self_step_unjoined"
            "(iter_input, node_items, node_frags, ws);\n"

            "iter_input := nil_oid_oid;\n"
            /* variables for the result of the scj */
            "var pruned_input := res_scj.fetch(0);\n"
            /* pruned_input comes as ctx|iter */
            "var ctx_dn_item := res_scj.fetch(1);\n"
            "var ctx_dn_frag := res_scj.fetch(2);\n"
            "res_scj := nil_oid_bat;\n"
            /* res_ec is the iter|dn table resulting from the scj */
            "var res_item := pruned_input.reverse().leftjoin(ctx_dn_item);\n"
            /* create content_iter as sorting argument for the merged union */
            "var content_void := res_item.mark(0@0).reverse();\n"
            "var content_iter := content_void.leftfetchjoin(oid_oid).leftfetchjoin(iter);\n"
            "content_void := nil_oid_oid;\n"
            /* only the dn_items and dn_frags from the joined result are needed
               in the following (getting the values for content_size, 
               content_prop, ...) and the input for a mposjoin has to be void */
            "res_item := res_item.reverse().mark(0@0).reverse();\n"
            "var res_frag := pruned_input.reverse().leftjoin(ctx_dn_frag);\n"
            "res_frag := res_frag.reverse().mark(0@0).reverse();\n"

            /* create subtree copies for all bats except content_level */
            "var content_size := mposjoin(res_item, res_frag, "
                                         "ws.fetch(PRE_SIZE));\n"
            "var content_prop := mposjoin(res_item, res_frag, "
                                         "ws.fetch(PRE_PROP));\n"
            "var content_kind := mposjoin(res_item, res_frag, "
                                         "ws.fetch(PRE_KIND));\n"
            "var content_frag := mposjoin(res_item, res_frag, "
                                         "ws.fetch(PRE_FRAG));\n"

 /* attr */ /* content_pre is needed for attribute subtree copies */
 /* attr */ "var content_pre := res_item;\n"
 /* attr */ /* as well as content_frag_pre */
 /* attr */ "var content_frag_pre := res_frag;\n"

            "res_item := nil_oid_oid;\n"
            "res_frag := nil_oid_oid;\n"

            /* change the level of the subtree copies */
            /* get the level of the content root nodes */
            /* - unique is needed, if pruned_input has more than once an ctx value
               - join with iter between pruned_input and item is not needed, because
               in this case pruned_input has the void column as iter value */
            "nodes := pruned_input.kunique();\n" /* creates unique ctx-node list */
            "var temp_ec_item := nodes.reverse().mark(0@0).reverse();\n"
            "temp_ec_item := temp_ec_item.leftfetchjoin(node_items);\n"
            "var temp_ec_frag := nodes.reverse().mark(0@0).reverse();\n"
            "temp_ec_frag := temp_ec_frag.leftfetchjoin(node_frags);\n"
            "nodes := nodes.mark(0@0);\n"
            "var contentRoot_level := mposjoin(temp_ec_item, "
                                              "temp_ec_frag, "
                                              "ws.fetch(PRE_LEVEL));\n"
            "contentRoot_level := nodes.leftfetchjoin(contentRoot_level);\n"
            "temp_ec_item := nil_oid_oid;\n"
            "temp_ec_frag := nil_oid_oid;\n"
            "nodes := nil_oid_oid;\n"

            "temp_ec_item := ctx_dn_item.reverse().mark(0@0).reverse();\n"
            "temp_ec_frag := ctx_dn_frag.reverse().mark(0@0).reverse();\n"
            "nodes := ctx_dn_item.mark(0@0);\n"
            "var content_level := mposjoin(temp_ec_item, temp_ec_frag, "
                                          "ws.fetch(PRE_LEVEL));\n"
            "content_level := nodes.leftfetchjoin(content_level);\n"
            "content_level := content_level.[-](contentRoot_level);\n"
            "contentRoot_level := nil_oid_chr;\n"
            "content_level := content_level.[+](chr(1));\n"
            /* join is made after the multiplex, because the level has to be
               change only once for each dn-node. With the join the multiplex
               is automatically expanded */
            "content_level := pruned_input.reverse().leftjoin(content_level);\n"
            "content_level := content_level.reverse().mark(0@0).reverse();\n"

            /* printing output for debugging purposes */
            /*
            "print(\"content\");\n"
            "print(content_iter, content_size, [int](content_level), "
            "[int](content_kind), content_prop, content_pre, content_frag_pre);\n"
            */

            /* get the maximum level of the new constructed nodes
               and set the maximum of the working set */
            "{\n"
            "var height := int(content_level.max()) + 1;\n"
            "ws.fetch(HEIGHT).replace(WS, max(ws.fetch(HEIGHT).fetch(WS), height));\n"
            "height := nil_int;\n"
            "}\n"

            /* calculate the sizes for the root nodes */
            "var contentRoot_size := mposjoin(node_items, node_frags, "
                                             "ws.fetch(PRE_SIZE)).[+](1);\n"
            "var size_oid := contentRoot_size.reverse();\n"
            "contentRoot_size := nil_oid_int;\n"
            "size_oid := size_oid.leftfetchjoin(oid_oid);\n"
            "oid_oid := nil_oid_oid;\n"
            "var size_iter := size_oid.leftfetchjoin(iter);\n"
            "size_oid := nil_int_oid;\n"
            "var iter_size := size_iter.reverse();\n"
            "size_iter := nil_int_oid;\n"
            /* sums up all the sizes into an size for each iter */
            /* every element must have a name, but elements don't need
               content. Therefore the second argument of the grouped
               sum has to be from the names result */
           "iter_size := {sum}(iter_size, iter%03u.tunique());\n",
           i);

    fprintf(f,
            "root_level := iter_size.project(chr(0));\n"
            "root_size := iter_size;\n"
            "root_kind := iter_size.project(ELEMENT);\n"
            "root_prop := iter%03u.reverse().leftfetchjoin(item%03u);\n"
            "root_frag := iter_size.project(WS);\n",
            i, i);

    fprintf(f,
            "root_level := root_level.reverse().mark(0@0).reverse();\n"
            "root_size := root_size.reverse().mark(0@0).reverse();\n"
            "root_kind := root_kind.reverse().mark(0@0).reverse();\n"
            "root_prop := root_prop.reverse().mark(0@0).reverse();\n"
            "root_frag := root_frag.reverse().mark(0@0).reverse();\n"
            "var root_iter := iter_size.mark(0@0).reverse();\n"
            "iter_size := nil_oid_int;\n"

 /* attr */ /* root_pre is a dummy needed for merge union with content_pre */
 /* attr */ "var root_pre := root_iter.project(nil);\n"
 /* attr */ /* as well as root_frag_pre */
 /* attr */ "var root_frag_pre := root_iter.project(nil);\n"

            /* printing output for debugging purposes */
            /*
            "print(\"root\");\n"
            "print(root_iter, root_size, [int](root_level), [int](root_kind), root_prop);\n"
            */

            /* merge union root and nodes */
            "{\n"
            /* FIXME: tests if input is sorted is needed because of merged union*/
            "root_iter.chk_order(false);\n"
            "content_iter.chk_order(false);\n" 
/* FIXME: doesn't work until bug 1023816 is solved 
   FIXME: doesn't work until bug 1023816 is solved 
   FIXME: doesn't work until bug 1023816 is solved 
   FIXME: doesn't work until bug 1023816 is solved 
   FIXME: doesn't work until bug 1023816 is solved 
            "var merged_result := merged_union ("
            "root_iter, content_iter, root_size, content_size, "
            "root_level, content_level, root_kind, content_kind, "
            "root_prop, content_prop, root_frag, content_frag, "
 / * attr * / "root_pre, content_pre, root_frag_pre, content_frag_pre);\n"
            "root_iter := nil_oid_oid;\n"
            "content_iter := nil_oid_oid;\n"
            "root_size := merged_result.fetch(1);\n"
            "content_size := nil_oid_int;\n"
            "root_level := merged_result.fetch(2);\n"
            "content_level := nil_oid_chr;\n"
            "root_kind := merged_result.fetch(3);\n"
            "content_kind := nil_oid_chr;\n"
            "root_prop := merged_result.fetch(4);\n"
            "content_prop := nil_oid_oid;\n"
            "root_frag := merged_result.fetch(5);\n"
            "content_frag := nil_oid_oid;\n"
            "root_pre := merged_result.fetch(6);\n"
            "content_pre := nil_oid_oid;\n"
            "root_frag_pre := merged_result.fetch(7);\n"
            "content_frag_pre := nil_oid_oid;\n"
            "merged_result := nil_oid_bat;\n"
*/
/* temporary solution */
            "var merged_result := merged_union ("
            "root_iter, content_iter, root_size, content_size, "
            "root_level, content_level, root_kind, content_kind, "
            "root_prop, content_prop, root_frag, content_frag);\n"
            "root_size := merged_result.fetch(1);\n"
            "content_size := nil_oid_int;\n"
            "root_level := merged_result.fetch(2);\n"
            "content_level := nil_oid_chr;\n"
            "root_kind := merged_result.fetch(3);\n"
            "content_kind := nil_oid_chr;\n"
            "root_prop := merged_result.fetch(4);\n"
            "content_prop := nil_oid_oid;\n"
            "root_frag := merged_result.fetch(5);\n"
            "content_frag := nil_oid_oid;\n"

            "merged_result := nil_oid_bat;\n"
            "merged_result := merged_union ("
            "root_iter, content_iter, "
 /* attr */ "root_pre, content_pre, root_frag_pre, content_frag_pre);\n"
            "root_iter := nil_oid_oid;\n"
            "content_iter := nil_oid_oid;\n"
            "root_pre := merged_result.fetch(1);\n"
            "content_pre := nil_oid_oid;\n"
            "root_frag_pre := merged_result.fetch(2);\n"
            "content_frag_pre := nil_oid_oid;\n"
            "merged_result := nil_oid_bat;\n"
/* end of temporary solution */
            /* printing output for debugging purposes */
            /* 
            "merged_result.print();\n"
            "print(\"merged (root & content)\");\n"
            "print(root_size, [int](root_level), [int](root_kind), root_prop);\n"
            */
            "}\n"
   
    
 /* attr */ /* preNew_preOld has in the tail old pre */
 /* attr */ /* values merged with nil values */
 /* attr */ "preNew_preOld := root_pre;\n"
 /* attr */ "root_pre := nil_oid_oid;\n"
 /* attr */ "preNew_frag := root_frag_pre;\n"
 /* attr */ "root_frag_pre := nil_oid_oid;\n"

            "} else { # if (nodes.count() != 0) ...\n"
           );

    fprintf(f, "root_level := item%03u.project(chr(0));\n", i);
    fprintf(f, "root_size := item%03u.project(0);\n", i);
    fprintf(f, "root_kind := item%03u.project(ELEMENT);\n", i);
    fprintf(f, "root_prop := item%03u;\n", i);
    fprintf(f, "root_frag := item%03u.project(WS);\n", i);

 /* attr */ fprintf(f,
 /* attr */ "preNew_preOld := item%03u.project(nil);\n", i);
 /* attr */ fprintf(f,
 /* attr */ "preNew_preOld := preNew_preOld.reverse().mark(0@0).reverse();\n"
 /* attr */ "preNew_frag := preNew_preOld.reverse().mark(0@0).reverse();\n"

            "root_level := root_level.reverse().mark(0@0).reverse();\n"
            "root_size := root_size.reverse().mark(0@0).reverse();\n"
            "root_kind := root_kind.reverse().mark(0@0).reverse();\n"
            "root_prop := root_prop.reverse().mark(0@0).reverse();\n"
            "root_frag := root_frag.reverse().mark(0@0).reverse();\n"

            "} # end of else in 'if (nodes.count() != 0)'\n"

            /* set the offset for the new created trees */
            "{\n"
            "var seqb := oid(count(ws.fetch(PRE_SIZE).fetch(WS)));\n"
            "root_level.seqbase(seqb);\n"
            "root_size.seqbase(seqb);\n"
            "root_kind.seqbase(seqb);\n"
            "root_prop.seqbase(seqb);\n"
            "root_frag.seqbase(seqb);\n"
            /* get the new pre values */
 /* attr */ "preNew_preOld.seqbase(seqb);\n"
 /* attr */ "preNew_frag.seqbase(seqb);\n"
            "seqb := nil_oid;\n"
            "}\n"
            /* insert the new trees into the working set */
            "ws.fetch(PRE_LEVEL).fetch(WS).insert(root_level);\n"
            "ws.fetch(PRE_SIZE).fetch(WS).insert(root_size);\n"
            "ws.fetch(PRE_KIND).fetch(WS).insert(root_kind);\n"
            "ws.fetch(PRE_PROP).fetch(WS).insert(root_prop);\n"
            "ws.fetch(PRE_FRAG).fetch(WS).insert(root_frag);\n"

            /* printing output for debugging purposes */
            /*
            "print(\"actual working set\");\n"
            "print(Tpre_size, [int](Tpre_level), [int](Tpre_kind), Tpre_prop);\n"
            */

            /* save the new roots for creation of the intermediate result */
            "var roots := root_level.ord_uselect(chr(0));\n"
            "roots := roots.mark(0@0).reverse();\n"

            /* resetting the temporary variables */
            "root_level := nil_oid_chr;\n"
            "root_size := nil_oid_int;\n"
            "root_prop := nil_oid_oid;\n"
            "root_kind := nil_oid_chr;\n"
            "root_frag := nil_oid_oid;\n"

            /* adding the new constructed roots to the WS_FRAG bat of the
               working set, that a following (preceding) step can check
               the fragment boundaries */
            "{ # adding new fragments to the WS_FRAG bat\n"
            "var seqb := oid(count(ws.fetch(WS_FRAG)));\n"
            "var new_pres := roots.reverse().mark(seqb).reverse();\n"
            "seqb := nil_oid;\n"
            "ws.fetch(WS_FRAG).insert(new_pres);\n"
            "new_pres := nil_oid_oid;\n"
            "}\n"
           );

            /* return the root elements in iter|pos|item|kind representation */
            /* should contain for each iter exactly 1 root element
               unless there is a thinking error */
    fprintf(f,
            "iter := iter%03u;\n"
            "pos := roots.mark(1@0);\n"
            "item := roots;\n"
            "kind := roots.project(ELEM);\n",
            i);

 /* attr */ /* attr translation */
 /* attr */ /* 1. step: add subtree copies of attributes */
    fprintf(f,
            "{ # create attribute subtree copies\n"
            /* get the attributes of the subtree copy elements */
            /* because also nil values from the roots are used for matching
               and 'select(nil)' inside mvaljoin gives back all the attributes
               not bound to a pre value first all root pre values have to
               be thrown out */
            "var content_preNew_preOld := preNew_preOld.ord_select(nil,nil);\n"
            "var oid_preOld := content_preNew_preOld.reverse().mark(0@0).reverse();\n"
            "var oid_preNew := content_preNew_preOld.mark(0@0).reverse();\n"
            "content_preNew_preOld := nil_oid_oid;\n"
            "var oid_frag := oid_preNew.leftfetchjoin(preNew_frag);\n"
            "var temp_attr := mvaljoin(oid_preOld, oid_frag, ws.fetch(ATTR_OWN));\n"
            "oid_preOld := nil_oid_oid;\n"
            "var oid_attr := temp_attr.reverse().mark(0@0).reverse();\n"
            "oid_frag := temp_attr.reverse().leftfetchjoin(oid_frag);\n"
            "oid_frag := oid_frag.reverse().mark(0@0).reverse();\n"
            "oid_preNew := temp_attr.reverse().leftfetchjoin(oid_preNew);\n"
            "oid_preNew := oid_preNew.reverse().mark(0@0).reverse();\n"
            "temp_attr := nil_oid_oid;\n"

            "var seqb := oid(ws.fetch(ATTR_QN).fetch(WS).count());\n"

            /* get the values of the QN/OID offsets for the reference to the
               string values */
            "var attr_qn := mposjoin(oid_attr, oid_frag, ws.fetch(ATTR_QN));\n"
            "var attr_oid := mposjoin(oid_attr, oid_frag, ws.fetch(ATTR_PROP));\n"
            "oid_attr := nil_oid_oid;\n"
            "attr_qn.seqbase(seqb);\n"
            "attr_oid.seqbase(seqb);\n"
            "oid_preNew.seqbase(seqb);\n"
            "oid_frag.seqbase(seqb);\n"
            "seqb := nil_oid;\n"

            /* insert into working set WS the attribute subtree copies 
               only 'offsets' where to find strings are copied 
               (QN/FRAG, OID/FRAG) */
            "ws.fetch(ATTR_QN).fetch(WS).insert(attr_qn);\n"
            "ws.fetch(ATTR_PROP).fetch(WS).insert(attr_oid);\n"
            "ws.fetch(ATTR_OWN).fetch(WS).insert(oid_preNew);\n"
            "ws.fetch(ATTR_FRAG).fetch(WS).insert(oid_frag);\n"
            "attr_qn := nil_oid_oid;\n"
            "attr_oid := nil_oid_oid;\n"
            "oid_preNew := nil_oid_oid;\n"
            "oid_frag := nil_oid_oid;\n"

            "} # end of create attribute subtree copies\n"
           );

 /* attr */ /* 2. step: add attribute bindings of new root nodes */
    fprintf(f,
            "{ # create attribute root entries\n"
            /* use iter, qn and frag to find unique combinations */
            "var unq_attrs := CTgroup(attr_iter)"
                             ".CTgroup(mposjoin(attr_item, attr_frag, ws.fetch(ATTR_QN)))"
                             ".CTgroup(mposjoin(attr_item, attr_frag, ws.fetch(ATTR_FRAG)))"
                             ".tunique();\n"
            /* test uniqueness */
            "if (unq_attrs.count() != attr_iter.count())\n"
            "{\n"
            "   if (item%03u.count() > 0) {\n"
            "      ERROR (\"attributes are not unique in element"
            " construction of '%%s' within each iter\",\n"
            "             item%03u.leftfetchjoin(ws.fetch(QN_LOC).fetch(WS)).fetch(0));\n"
            "   }\n"
            "   else {\n"
            "     ERROR (\"attributes are not unique in element"
            " construction within each iter\");\n"
            "   }\n"
            "}\n"
            "unq_attrs := nil_oid_oid;\n",
            i, i);

            /* insert it into the WS after everything else */
    fprintf(f,
            "var seqb := oid(ws.fetch(ATTR_QN).fetch(WS).count());\n"
            /* get old QN reference and copy it into the new attribute */
            "var attr_qn := mposjoin(attr_item, attr_frag, ws.fetch(ATTR_QN));\n"
            "attr_qn.seqbase(seqb);\n"
            /* get old OID reference and copy it into the new attribute */
            "var attr_oid := mposjoin(attr_item, attr_frag, ws.fetch(ATTR_PROP));\n"
            "attr_oid.seqbase(seqb);\n"
            /* get the iters and their corresponding new pre value (roots) and
               multiply them for all the attributes */
            "var attr_own := iter%03u.reverse().leftfetchjoin(roots);\n"
            "roots := nil_oid_oid;\n"
            "attr_own := attr_iter.leftjoin(attr_own);\n"
            "attr_iter := nil_oid_oid;\n"
            "attr_own := attr_own.reverse().mark(seqb).reverse();\n",
            i);
            /* use the old FRAG values as reference */
    fprintf(f,
            "attr_frag.seqbase(seqb);\n"
            "seqb := nil_oid;\n"
  
            "ws.fetch(ATTR_QN).fetch(WS).insert(attr_qn);\n"
            "attr_qn := nil_oid_oid;\n"
            "ws.fetch(ATTR_PROP).fetch(WS).insert(attr_oid);\n"
            "attr_oid := nil_oid_oid;\n"
            "ws.fetch(ATTR_OWN).fetch(WS).insert(attr_own);\n"
            "attr_own := nil_oid_oid;\n"
            "ws.fetch(ATTR_FRAG).fetch(WS).insert(attr_frag);\n"
            "attr_frag := nil_oid_oid;\n"
  
            "} # end of create attribute root entries\n"
  
            /* printing output for debugging purposes */
            /*
            "print(\"Theight\"); Theight.print();\n"
            "print(\"Tdoc_pre\"); Tdoc_pre.print();\n"
            "print(\"Tdoc_name\"); Tdoc_name.print();\n"
            */
  
            "} # end of loop_liftedElemConstr (counter)\n"
           );
}


/**
 * loop-liftedAttrConstr creates new attributes which
 * are not connected to element nodes
 *
 * @param f the Stream the MIL code is printed to
 * @param act_level the level of the for-scope
 * @param i the counter of the actual saved result (attr name)
 */
static void
loop_liftedAttrConstr (FILE *f, int act_level, int i)
{
    fprintf(f,
            /* test qname and add "" for each empty item */
            "{ # loop_liftedAttrConstr (int i)\n"
            "if (iter%03u.count() != loop%03u.count())\n"
            "{    ERROR (\"empty tagname is not allowed in "
                        "attribute construction\"); }\n"
            "if (iter.count() != loop%03u.count())\n"
            "{\n"
            "var difference := loop%03u.reverse().kdiff(iter.reverse());\n"
            "difference := difference.mark(0@0).reverse();\n"
            "var res_mu := merged_union(iter, difference, item, "
                                       "difference.project(EMPTY_STRING));\n"
            "difference := nil_oid_oid;\n"
            "item := res_mu.fetch(1);\n"
            "res_mu := nil_oid_bat;\n"
            "}\n",
            i, act_level, act_level, act_level);

    fprintf(f,
            "var ws_prop_val := ws.fetch(PROP_VAL).fetch(WS);\n"
            /* add strings to PROP_VAL table (but keep the tail of PROP_VAL
               unique */
            "var unq := item.tunique().mark(0@0).reverse();\n"
            "var unq_str := unq.leftfetchjoin(str_values);\n"
            "unq := nil_oid_oid;\n"
            "var str_unq := unq_str.reverse().kdiff(ws_prop_val.reverse());\n"
            "unq_str := nil_oid_str;\n"
            "var seqb := oid(int(ws_prop_val.seqbase()) + ws_prop_val.count());\n"
            "unq_str := str_unq.mark(seqb).reverse();\n"
            "str_unq := nil_str_oid;\n"
            "seqb := nil_oid;\n"
            "ws_prop_val.insert(unq_str);\n"
            "unq_str := nil_oid_str;\n"
            /* get the property values of the strings */
            "var strings := item.leftfetchjoin(str_values);\n"
            "var attr_oid := strings.leftjoin(ws_prop_val.reverse());\n"
            "strings := nil_oid_str;\n"
            "seqb := oid(ws.fetch(ATTR_OWN).fetch(WS).count());\n"
            "attr_oid := attr_oid.reverse().mark(seqb).reverse();\n"
            /* add the new attribute properties */
            "ws.fetch(ATTR_PROP).fetch(WS).insert(attr_oid);\n"
            "attr_oid := nil_oid_oid;\n"
            "var qn := item%03u.reverse().mark(seqb).reverse();\n"
            "ws.fetch(ATTR_QN).fetch(WS).insert(qn);\n"
            "ws.fetch(ATTR_FRAG).fetch(WS).insert(qn.project(WS));\n"
            "ws.fetch(ATTR_OWN).fetch(WS).insert(qn.mark(nil));\n"
            "qn := nil_oid_oid;\n"
            /* get the intermediate result */
            "iter := iter%03u;\n"
            "pos := pos%03u;\n"
            "item := iter%03u.mark(seqb);\n"
            "seqb := nil_oid;\n"
            "kind := kind%03u.project(ATTR);\n"
            "} # end of loop_liftedAttrConstr (int i)\n",
            i, i, i, i, i);
}

/**
 * loop_liftedTextConstr takes strings and creates new text 
 * nodes out of it and adds them to the working set
 *
 * @param f the Stream the MIL code is printed to
 */
static void
loop_liftedTextConstr (FILE *f)
{
    fprintf(f,
            "{ # adding new strings to text node content and create new nodes\n"
            "var ws_prop_text := ws.fetch(PROP_TEXT).fetch(WS);\n"
            "var unq := item.tunique().mark(0@0).reverse();\n"
            "var unq_str := unq.leftfetchjoin(str_values);\n"
            "unq := nil_oid_oid;\n"
            "var str_unq := unq_str.reverse().kdiff(ws_prop_text.reverse());\n"
            "unq_str := nil_oid_str;\n"
            "var seqb := oid(int(ws_prop_text.seqbase()) + ws_prop_text.count());\n"
            "unq_str := str_unq.mark(seqb).reverse();\n"
            "str_unq := nil_str_oid;\n"
            "seqb := nil_oid;\n"
            "ws_prop_text.insert(unq_str);\n"
            "unq_str := nil_oid_str;\n"
            /* get the property values of the strings */
            "var strings := item.leftfetchjoin(str_values);\n"
            "var newPre_prop := strings.leftjoin(ws_prop_text.reverse());\n"
            "strings := nil_oid_str;\n"
            "seqb := oid(ws.fetch(PRE_KIND).fetch(WS).count());\n"
            "newPre_prop := newPre_prop.reverse().mark(seqb).reverse();\n"

            "ws.fetch(PRE_PROP).fetch(WS).insert(newPre_prop);\n"
            "ws.fetch(PRE_SIZE).fetch(WS).insert(newPre_prop.project(0));\n"
            "ws.fetch(PRE_LEVEL).fetch(WS).insert(newPre_prop.project(chr(0)));\n"
            "ws.fetch(PRE_KIND).fetch(WS).insert(newPre_prop.project(TEXT));\n"
            "ws.fetch(PRE_FRAG).fetch(WS).insert(newPre_prop.project(WS));\n"
            "newPre_prop := nil_oid_oid;\n"
            "item := item.mark(seqb);\n"
            "seqb := nil_oid;\n"
            "kind := kind.project(ELEM);\n"
            "}\n"

            /* adding the new constructed roots to the WS_FRAG bat of the
               working set, that a following (preceding) step can check
               the fragment boundaries */
            "{ # adding new fragments to the WS_FRAG bat\n"
            "var seqb := ws.fetch(WS_FRAG).count();\n"
            "seqb := oid(seqb);\n"
            "var new_pres := item.reverse().mark(seqb).reverse();\n"
            "seqb := nil_oid;\n"
            "ws.fetch(WS_FRAG).insert(new_pres);\n"
            "new_pres := nil_oid;\n"
            /* get the maximum level of the new constructed nodes
               and set the maximum of the working set */
            "ws.fetch(HEIGHT).replace(WS, max(ws.fetch(HEIGHT).fetch(WS), 1));\n"
            "}\n"
           );
}

/*
 * translateIfThen translates either then or else block of an if-then-else
 *
 * to avoid more than one expansion of the subtree for each 
 * branch three branches (PHASES) are added in MIL. They
 * avoid the expansion of the variable environment and of the 
 * subtree if the if-clause produces either only true or only
 * false values. If then- or else-clause is empty (c_empty)
 * the function will only be called for the other.
 *
 *  '-' = not      |   skip  |  c_empty 
 *        executed | 0  1  2 | then  else
 *  PHASE 1 (then) |    -  - |  -
 *  PHASE 2 (then) |       - |  -
 *  PHASE 3 (then) |    -  - |  -
 *  PHASE 1 (else) |    -  - |        -
 *  PHASE 2 (else) |    -    |        -
 *  PHASE 3 (else) |    -  - |        -
 *
 * @param f the Stream the MIL code is printed to
 * @param act_level the level of the for-scope
 * @param counter the actual offset of saved variables
 * @param c the then/else expression
 * @param then is the boolean saving if the branch (then/else)
 * @param bool_res the number, where the result of the if-clause
 *        is saved 
 */
static void
translateIfThen (FILE *f, int act_level, int counter,
                 PFcnode_t *c, int then, int bool_res)
{
    act_level++;
    fprintf(f, "{ # translateIfThen\n");

    /* initial setting of new 'scope' */
    fprintf(f, "var loop%03u := loop%03u;\n", act_level, act_level-1);
    fprintf(f, "var inner%03u := inner%03u;\n", act_level, act_level-1);
    fprintf(f, "var outer%03u := outer%03u;\n", act_level, act_level-1);
    fprintf(f, "var v_vid%03u := v_vid%03u;\n", act_level, act_level-1);
    fprintf(f, "var v_iter%03u := v_iter%03u;\n", act_level, act_level-1);
    fprintf(f, "var v_pos%03u := v_pos%03u;\n", act_level, act_level-1);
    fprintf(f, "var v_item%03u := v_item%03u;\n", act_level, act_level-1);
    fprintf(f, "var v_kind%03u := v_kind%03u;\n", act_level, act_level-1);

    /* 1. PHASE: create all mapping stuff to next 'scope' */
    fprintf(f, "if (skip = 0)\n{\n");
    /* output for debugging
    fprintf(f, "\"PHASE 1 of %s-clause active\".print();\n",then?"then":"else");
    */

    /* get the right set of sequences, which have to be processed */
    if (!then)
            fprintf(f, "selected := item%03u.ord_uselect(0@0);\n", bool_res);

    fprintf(f, "iter := selected.mirror().join(iter%03u);\n", bool_res);
    fprintf(f, "iter := iter.reverse().mark(0@0).reverse();\n");
    fprintf(f, "outer%03u := iter;\n", act_level);
    fprintf(f, "iter := iter.mark(1@0);\n");
    fprintf(f, "inner%03u := iter;\n", act_level);
    fprintf(f, "loop%03u := inner%03u;\n", act_level, act_level);
    fprintf(f, "iter := nil_oid_oid;\n");

    /* - in a first version no variables are pruned
         at an if-then-else node 
       - if-then-else is executed more or less like a for loop */
    fprintf(f, "var expOid := v_iter%03u.mirror();\n", act_level);
    fprintf(f, "var oidNew_expOid;\n");
    expand (f, act_level);
    join (f, act_level);
    fprintf(f, "expOid := nil_oid_oid;\n");

    fprintf(f, "}\n");

    /* 2. PHASE: execute then/else expression if there are 
       true/false values in the boolean expression */
    if (then)
            fprintf(f, "if (skip != 1)\n{\n");
    else
            fprintf(f, "if (skip != 2)\n{\n");
    /* output for debugging
    fprintf(f, "\"PHASE 2 of %s-clause active\".print();\n",then?"then":"else");
    */

    translate2MIL (f, act_level, counter, c);
    fprintf(f, "} ");
    fprintf(f, "else\n{\n");
    translateEmpty (f);
    fprintf(f, "}\n");

    /* 3. PHASE: create all mapping stuff from to actual 'scope' */
    fprintf(f, "if (skip = 0)\n{\n");
    /* output for debugging
    fprintf(f, "\"PHASE 3 of %s-clause active\".print();\n",then?"then":"else");
    */
    mapBack (f, act_level);
    fprintf(f, "}\n");

    cleanUpLevel (f, act_level);
    fprintf(f, "} # end of translateIfThen\n");
    act_level--;
}

static void
fn_boolean (FILE *f, int act_level, PFty_t input_type)
{
    fprintf(f,
            "{ # translate fn:boolean (item*) as boolean\n"
            "var iter_count := {count}(iter.reverse(), loop%03u.reverse());\n"
            "var trues := iter_count.[!=](0);\n",
            act_level);

    if (PFty_subtype (input_type, PFty_star(PFty_integer ())))
    {
        fprintf(f,
                "trues.access(BAT_WRITE);\n"
                "var test := iter_count.ord_uselect(1).mirror();\n"
                "test := test.leftjoin(iter.reverse());\n"
                "var test_item := test.leftfetchjoin(item);\n"
                "test := nil_oid_oid;\n"
                "var test_int := test_item.leftfetchjoin(int_values);\n"
                "test_item := nil_oid_oid;\n"
                "var test_falses := test_int.ord_uselect(0);\n"
                "test_int := nil_oid_int;\n"
                "var falses := test_falses.project(false);\n"
                "trues.replace(falses);\n"
                "test_falses := nil_oid_oid;\n"
                "falses := nil_oid_bit;\n"
                "trues.access(BAT_READ);\n");
    }
    else if (PFty_subtype (input_type, PFty_star(PFty_double ())))
    {
        fprintf(f,
                "trues.access(BAT_WRITE);\n"
                "var test := iter_count.ord_uselect(1).mirror();\n"
                "test := test.leftjoin(iter.reverse());\n"
                "var test_item := test.leftfetchjoin(item);\n"
                "test := nil_oid_oid;\n"
                "var test_dbl := test_item.leftfetchjoin(dbl_values);\n"
                "test_item := nil_oid_oid;\n"
                "var test_falses := test_dbl.ord_uselect(dbl(0));\n"
                "test_dbl := nil_oid_dbl;\n"
                "var falses := test_falses.project(false);\n"
                "test_falses := nil_oid_oid;\n"
                "trues.replace(falses);\n"
                "falses := nil_oid_bit;\n"
                "trues.access(BAT_READ);\n");
    }
    else if (PFty_subtype (input_type, PFty_star(PFty_decimal ())))
    {
        fprintf(f,
                "trues.access(BAT_WRITE);\n"
                "var test := iter_count.ord_uselect(1).mirror();\n"
                "test := test.leftjoin(iter.reverse());\n"
                "var test_item := test.leftfetchjoin(item);\n"
                "test := nil_oid_oid;\n"
                "var test_dec := test_item.leftfetchjoin(dec_values);\n"
                "test_item := nil_oid_oid;\n"
                "var test_falses := test_dec.ord_uselect(dbl(0));\n"
                "test_dec := nil_oid_dbl;\n"
                "var falses := test_falses.project(false);\n"
                "test_falses := nil_oid_oid;\n"
                "trues.replace(falses);\n"
                "falses := nil_oid_bit;\n"
                "trues.access(BAT_READ);\n");
    }
    else if (PFty_subtype (input_type, PFty_star(PFty_string ())))
    {
        fprintf(f,
                "trues.access(BAT_WRITE);\n"
                "var test := iter_count.ord_uselect(1).mirror();\n"
                "test := test.leftjoin(iter.reverse());\n"
                "var test_item := test.leftfetchjoin(item);\n"
                "test := nil_oid_oid;\n"
                "var test_str_ := test_item.leftfetchjoin(str_values);\n"
                "test_item := nil_oid_oid;\n"
                "var test_falses := test_str_.ord_uselect(\"\");\n"
                "test_str_ := nil_oid_str;\n"
                "var falses := test_falses.project(false);\n"
                "test_falses := nil_oid_oid;\n"
                "trues.replace(falses);\n"
                "falses := nil_oid_bit;\n"
                "trues.access(BAT_READ);\n");
    }
    else if (PFty_subtype (input_type, PFty_star(PFty_boolean ())))
    {
        /* this branch never occurs, because it gets optimized away :) */
        fprintf(f,
                "trues.access(BAT_WRITE);\n"
                "var test := iter_count.ord_uselect(1).mirror();\n"
                "test := test.leftjoin(iter.reverse());\n"
                "var test_item := test.leftfetchjoin(item);\n"
                "test := nil_oid_oid;\n"
                "var test_falses := test_item.ord_uselect(0@0);\n"
                "test_item := nil_oid_oid;\n"
                "var falses := test_falses.project(false);\n"
                "test_falses := nil_oid_oid;\n"
                "trues.replace(falses);\n"
                "falses := nil_oid_bit;\n"
                "trues.access(BAT_READ);\n");

    }
    else if (PFty_subtype (input_type, PFty_star(PFty_atomic ())))
    {

        /* FIXME: rewrite stuff two use only one column instead of oid|oid */
        fprintf(f,
                "trues.access(BAT_WRITE);\n"
                "var test := iter_count.ord_uselect(1).mirror();\n"
                "iter := iter.reverse();\n"
                "item := iter.leftfetchjoin(item);\n"
                "kind := iter.leftfetchjoin(kind);\n"
                "var test_kind := test.leftjoin(kind);\n"
                "test := nil_oid_oid;\n"
                "var str_test := test_kind.ord_uselect(STR);\n"
                "var int_test := test_kind.ord_uselect(INT);\n"
                "var dbl_test := test_kind.ord_uselect(DBL);\n"
                "var dec_test := test_kind.ord_uselect(DEC);\n"
                "var bool_test := test_kind.ord_uselect(BOOL);\n"
                "test := nil_oid_int;\n"
                "str_test := str_test.mirror();\n"
                "int_test := int_test.mirror();\n"
                "dbl_test := dbl_test.mirror();\n"
                "dec_test := dec_test.mirror();\n"
                "bool_test := bool_test.mirror();\n"
                "str_test := str_test.leftjoin(item);\n"
                "int_test := int_test.leftjoin(item);\n"
                "dec_test := dec_test.leftjoin(item);\n"
                "dbl_test := dbl_test.leftjoin(item);\n"
                "bool_test := bool_test.leftjoin(item);\n"
                "var test_str_ := str_test.leftfetchjoin(str_values);\n"
                "var test_int := int_test.leftfetchjoin(int_values);\n"
                "var test_dec := dec_test.leftfetchjoin(dec_values);\n"
                "var test_dbl := dbl_test.leftfetchjoin(dbl_values);\n"
                "bool_test := bool_test.ord_uselect(0@0);\n"
                "str_test := test_str_.ord_uselect(\"\");\n"
                "int_test := test_int.ord_uselect(0);\n"
                "dec_test := test_dec.ord_uselect(dbl(0));\n"
                "dbl_test := test_dbl.ord_uselect(dbl(0));\n"
                "test_str_ := nil_oid_str;\n"
                "test_int := nil_oid_int;\n"
                "test_dec := nil_oid_dbl;\n"
                "test_dbl := nil_oid_dbl;\n"
                "var str_falses := str_test.project(false);\n"
                "var int_falses := int_test.project(false);\n"
                "var dec_falses := dec_test.project(false);\n"
                "var dbl_falses := dbl_test.project(false);\n"
                "var bool_falses := bool_test.project(false);\n"
                "str_test := nil_oid_oid;\n"
                "int_test := nil_oid_oid;\n"
                "dec_test := nil_oid_oid;\n"
                "dbl_test := nil_oid_oid;\n"
                "bool_test := nil_oid_oid;\n"
                "trues.replace(str_falses);\n"
                "trues.replace(int_falses);\n"
                "trues.replace(dec_falses);\n"
                "trues.replace(dbl_falses);\n"
                "trues.replace(bool_falses);\n"
                "str_falses := nil_oid_bit;\n"
                "int_falses := nil_oid_bit;\n"
                "dec_falses := nil_oid_bit;\n"
                "dbl_falses := nil_oid_bit;\n"
                "bool_falses := nil_oid_bit;\n"
                "trues.access(BAT_READ);\n");
    }
    fprintf(f,
            "iter := iter_count.mark(0@0).reverse();\n"
            "iter_count := nil_oid_int;\n"
            "pos := iter.project(1@0);\n"
            "kind := iter.project(BOOL);\n"
            "item := trues.[oid]().reverse().mark(0@0).reverse();\n"
            "trues := nil_oid_bit;\n"
            "} # end of translate fn:boolean (item*) as boolean\n");
}

/**
 * testCastComplete tests if the result of a Cast
 * also contains empty sequences and produces an
 * error if empty sequences are found
 *
 * @param f the Stream the MIL code is printed to
 * @param act_level the level of the for-scope
 * @param type_ the type to which it should be casted
 */
static void
testCastComplete (FILE *f, int act_level, PFty_t type_)
{
    fprintf(f,
            "if (iter.count() != loop%03u.count())\n"
            "{    ERROR (\"'%s' doesn't allow empty sequences to be casted\"); }\n",
            act_level, PFty_str (type_));
}

/**
 * evaluateCastBlock casts a block from a given type to the target_type type
 *
 * @param f the Stream the MIL code is printed to
 * @param original_type is the type of the items which are casted
 * @param table is the table where the actual item values are saved
 * @param target_type is the type to which it is casted
 */
static void
evaluateCastBlock (FILE *f, type_co ori, char *cast, char *target_type)
{
    fprintf(f,
            "{\n"
            "var part_kind := kind.ord_uselect(%s);\n"
            "var oid_oid := part_kind.mark(0@0).reverse();\n"
            "part_kind := nil_oid_oid;\n"
            "var part_item := oid_oid.leftfetchjoin(item);\n",
            ori.mil_cast);
    if (ori.kind != co_bool)
        fprintf(f,
                "var part_%s := part_item.leftfetchjoin(%s);\n",
                ori.mil_type, ori.table);
    else
        fprintf(f,
                "var part_%s := part_item;\n",
                ori.mil_type);

    fprintf(f,
            "part_item := nil_oid_oid;\n"
            "var part_val := part_%s.%s;\n",
            ori.mil_type, cast);
    fprintf(f,
            "var res_mu := merged_union(_oid, oid_oid, _val, part_val);\n"
            "oid_oid := nil_oid_oid;\n"
            "part_%s := nil_oid_%s;\n"
            "part_val := nil_oid_%s;\n"
            "_oid := res_mu.fetch(0);\n"
            "_val := res_mu.fetch(1);\n"
            "res_mu := nil_oid_bat;\n"
            "}\n",
            ori.mil_type, ori.mil_type, target_type);
}

/**
 * evaluateCast casts from a given type to the target_type type
 *
 * @param f the Stream the MIL code is printed to
 * @param ori the struct containing the information about the 
 *        input type
 * @param target the struct containing the information about 
 *        the cast type
 * @param cast the command to execute the cast
 */
static void
evaluateCast (FILE *f,
              type_co ori,
              type_co target,
              char *cast)
{
    
    if (ori.kind != co_bool)
        fprintf(f,
                "var ori_val := item.leftfetchjoin(%s);\n"
                "var cast_val := ori_val.%s;\n"
                "ori_val := nil_oid_%s;\n",
                ori.table, cast, ori.mil_type);
    else
        fprintf(f, "var cast_val := item.%s;\n", cast);

    fprintf(f,
            "if (cast_val.ord_uselect(%s(nil)).count() != 0)\n"
            "{    ERROR (\"couldn't cast all values from %s to %s\"); }\n",
            target.mil_type, ori.name, target.name);

    if (target.kind != co_bool)
        addValues (f, target, "cast_val", "item");
    else
        fprintf(f, "item := cast_val.[oid]();\n");

    fprintf(f,
            "cast_val := nil_oid_%s;\n"
            "item := item.reverse().mark(0@0).reverse();\n"
            "kind := kind.project(%s);\n",
            target.mil_type, target.mil_cast);
}

/**
 * translateCast2INT takes an intermediate result
 * and casts all possible types to INT. It produces
 * an error if an iteration contains more than one
 * value
 *
 * @param f the Stream the MIL code is printed to
 */
static void
translateCast2INT (FILE *f, PFty_t input_type)
{
    if (PFty_eq (input_type, PFty_integer ()));
    else if (PFty_eq (input_type, PFty_decimal ()))
        evaluateCast (f, dec_container(), int_container(), "[int]()");
    else if (PFty_eq (input_type, PFty_double ()))
        evaluateCast (f, dbl_container(), int_container(), "[int]()");
    else if (PFty_eq (input_type, PFty_string ()) ||
             PFty_eq (input_type, PFty_untypedAtomic ()))
        evaluateCast (f, str_container(), int_container(), "[int]()");
    else if (PFty_eq (input_type, PFty_boolean ()))
        evaluateCast (f, bool_container(), int_container(), "[int]()");
    else /* handles the choice type */
    {
        fprintf(f,
                "var _oid := kind.ord_uselect(INT);\n"
                "_oid := _oid.mark(0@0).reverse();\n"
                "var part_item := _oid.leftfetchjoin(item);\n"
                "var _val := part_item.leftfetchjoin(int_values);\n"
                "part_item := nil_oid_oid;\n");
 
        evaluateCastBlock (f, bool_container(), "[int]()", "int");
        evaluateCastBlock (f, dec_container(), "[int]()", "int");
        evaluateCastBlock (f, dbl_container(), "[int]()", "int");
        evaluateCastBlock (f, str_container(), "[int]()", "int");

        fprintf(f,
                "if (_val.ord_uselect(int(nil)).count() != 0)\n"
                "{    ERROR (\"couldn't cast all values to integer\"); }\n");
 
        addValues(f, int_container(), "_val", "item");
        fprintf(f,
                "item := item.reverse().mark(0@0).reverse();\n"
                "kind := _oid.project(INT);\n");
    }
}

/**
 * translateCast2DEC takes an intermediate result
 * and casts all possible types to DEC. It produces
 * an error if an iteration contains more than one
 * value
 *
 * @param f the Stream the MIL code is printed to
 */
static void
translateCast2DEC (FILE *f, PFty_t input_type)
{
    if (PFty_eq (input_type, PFty_integer ()))
        evaluateCast (f, int_container(), dec_container(), "[dbl]()");
    else if (PFty_eq (input_type, PFty_decimal ()));
    else if (PFty_eq (input_type, PFty_double ()))
        evaluateCast (f, dbl_container(), dec_container(), "[dbl]()");
    else if (PFty_eq (input_type, PFty_string ()) ||
             PFty_eq (input_type, PFty_untypedAtomic ()))
        evaluateCast (f, str_container(), dec_container(), "[dbl]()");
    else if (PFty_eq (input_type, PFty_boolean ()))
        evaluateCast (f, bool_container(), dec_container(), "[dbl]()");
    else /* handles the choice type */ 
    {
        fprintf(f,
                "var _oid := kind.ord_uselect(DEC);\n"
                "_oid := _oid.mark(0@0).reverse();\n"
                "var part_item := _oid.leftfetchjoin(item);\n"
                "var _val := part_item.leftfetchjoin(dec_values);\n"
                "part_item := nil_oid_oid;\n");
 
        evaluateCastBlock (f, bool_container(), "[dbl]()", "dbl");
        evaluateCastBlock (f, int_container(), "[dbl]()", "dbl");
        evaluateCastBlock (f, dbl_container(), "[dbl]()", "dbl");
        evaluateCastBlock (f, str_container(), "[dbl]()", "dbl");
 
        fprintf(f,
                "if (_val.ord_uselect(dbl(nil)).count() != 0)\n"
                "{    ERROR (\"couldn't cast all values to decimal\"); }\n");
 
        addValues(f, dec_container(), "_val", "item");
        fprintf(f,
                "item := item.reverse().mark(0@0).reverse();\n"
                "kind := _oid.project(DEC);\n");
    }
}

/**
 * translateCast2DBL takes an intermediate result
 * and casts all possible types to DBL. It produces
 * an error if an iteration contains more than one
 * value
 *
 * @param f the Stream the MIL code is printed to
 */
static void
translateCast2DBL (FILE *f, PFty_t input_type)
{
    if (PFty_eq (input_type, PFty_integer ()))
        evaluateCast (f, int_container(), dbl_container(), "[dbl]()");
    else if (PFty_eq (input_type, PFty_decimal ()))
        evaluateCast (f, dec_container(), dbl_container(), "[dbl]()");
    else if (PFty_eq (input_type, PFty_double ()));
    else if (PFty_eq (input_type, PFty_string ()) || 
             PFty_eq (input_type, PFty_untypedAtomic ()))
        evaluateCast (f, str_container(), dbl_container(), "[dbl]()");
    else if (PFty_eq (input_type, PFty_boolean ()))
        evaluateCast (f, bool_container(), dbl_container(), "[dbl]()");
    else /* handles the choice type */ 
    {
        fprintf(f,
                "var _oid := kind.ord_uselect(DBL);\n"
                "_oid := _oid.mark(0@0).reverse();\n"
                "var part_item := _oid.leftfetchjoin(item);\n"
                "var _val := part_item.leftfetchjoin(dbl_values);\n"
                "part_item := nil_oid_oid;\n");
 
        evaluateCastBlock (f, bool_container(), "[dbl]()", "dbl");
        evaluateCastBlock (f, dec_container(), "[dbl]()", "dbl");
        evaluateCastBlock (f, int_container(), "[dbl]()", "dbl");
        evaluateCastBlock (f, str_container(), "[dbl]()", "dbl");
 
        fprintf(f,
                "if (_val.ord_uselect(dbl(nil)).count() != 0)\n"
                "{    ERROR (\"couldn't cast all values to double\"); }\n");
 
        addValues(f, dbl_container(), "_val", "item");
        fprintf(f,
                "item := item.reverse().mark(0@0).reverse();\n"
                "kind := _oid.project(DBL);\n");
    }
}

/**
 * translateCast2STR takes an intermediate result
 * and casts all possible types to STR. It produces
 * an error if an iteration contains more than one
 * value
 *
 * @param f the Stream the MIL code is printed to
 */
static void
translateCast2STR (FILE *f, PFty_t input_type)
{
    if (PFty_eq (input_type, PFty_integer ()))
        evaluateCast (f, int_container(), str_container(), "[str]()");
    else if (PFty_eq (input_type, PFty_decimal ()))
        evaluateCast (f, dec_container(), str_container(), "[str]()");
    else if (PFty_eq (input_type, PFty_double ()))
        evaluateCast (f, dbl_container(), str_container(), "[str]()");
    else if (PFty_eq (input_type, PFty_string ()) ||
             PFty_eq (input_type, PFty_untypedAtomic ()));
    else if (PFty_eq (input_type, PFty_boolean ()))
        evaluateCast (f, bool_container(), str_container(), "[str]()");
    else /* handles the choice type */ 
    {
        fprintf(f,
                "var _oid := kind.ord_uselect(STR);\n"
                "_oid := _oid.mark(0@0).reverse();\n"
                "var part_item := _oid.leftfetchjoin(item);\n"
                "var _val := part_item.leftfetchjoin(str_values);\n"
                "part_item := nil_oid_oid;\n");
 
        evaluateCastBlock (f, bool_container(), "[str]()", "str");
        evaluateCastBlock (f, dec_container(), "[str]()", "str");
        evaluateCastBlock (f, dbl_container(), "[str]()", "str");
        evaluateCastBlock (f, int_container(), "[str]()", "str");
 
        fprintf(f,
                "if (_val.ord_uselect(str(nil)).count() != 0)\n"
                "{    ERROR (\"couldn't cast all values to string\"); }\n");
 
        addValues(f, str_container(), "_val", "item");
        fprintf(f,
                "item := item.reverse().mark(0@0).reverse();\n"
                "kind := _oid.project(STR);\n");
    }
}

/**
 * translateCast2BOOL takes an intermediate result
 * and casts all possible types to BOOL. It produces
 * an error if an iteration contains more than one
 * value
 *
 * @param f the Stream the MIL code is printed to
 */
static void
translateCast2BOOL (FILE *f, PFty_t input_type)
{
    if (PFty_eq (input_type, PFty_integer ()))
        evaluateCast (f, int_container(), bool_container(), "[bit]()");
    else if (PFty_eq (input_type, PFty_decimal ()))
        evaluateCast (f, dec_container(), bool_container(), "[bit]()");
    else if (PFty_eq (input_type, PFty_double ()))
        evaluateCast (f, dbl_container(), bool_container(), "[bit]()");
    else if (PFty_eq (input_type, PFty_string ()) ||
             PFty_eq (input_type, PFty_untypedAtomic ()))
        evaluateCast (f, str_container(), bool_container(), "[!=](\"\")");
    else if (PFty_eq (input_type, PFty_boolean ()));
    else /* handles the choice type */ 
    {
        fprintf(f,
                "var _oid := kind.ord_uselect(BOOL);\n"
                "_oid := _oid.mark(0@0).reverse();\n"
                "var part_item := _oid.leftfetchjoin(item);\n"
                "var _val := part_item.[bit]();\n"
                "part_item := nil_oid_oid;\n");
 
        evaluateCastBlock (f, int_container(), "[bit]()", "bit");
        evaluateCastBlock (f, dec_container(), "[bit]()", "bit");
        evaluateCastBlock (f, dbl_container(), "[bit]()", "bit");
        evaluateCastBlock (f, bool_container(), "[!=](\"\")", "bit");
 
        fprintf(f,
                "if (_val.ord_uselect(bit(nil)).count() != 0)\n"
                "{    ERROR (\"couldn't cast all values to boolean\"); }\n");
 
        fprintf(f,
                "item := _val.[oid]();\n"
                "kind := _oid.project(BOOL);\n");
    }
}

/**
 * translateCast decides wether the cast can be evaluated
 * and then calls the specific cast function
 *
 * @param f the Stream the MIL code is printed to
 * @param act_level the level of the for-scope
 * @param c the Core node containing the rest of the subtree
 */
static void
translateCast (FILE *f, int act_level, PFcnode_t *c)
{
    PFty_t cast_type = PFty_defn (c->child[0]->sem.type);
    PFty_t input_type = PFty_defn (c->child[1]->type);
    int cast_optional = 0;
     
    if (cast_type.type == ty_opt)
    {
        cast_type = PFty_child (cast_type);
        cast_optional = 1;
    }

    if (input_type.type == ty_opt) 
        input_type = PFty_child (input_type);

    fprintf(f,
            "{ # cast from %s to %s\n", 
            PFty_str(input_type), PFty_str(cast_type));

    switch (cast_type.type)
    {
        case ty_boolean:
            translateCast2BOOL (f, input_type);
            break;
        case ty_integer:
            translateCast2INT (f, input_type);
            break;
        case ty_decimal:
            translateCast2DEC (f, input_type);
            break;
        case ty_double:
            translateCast2DBL (f, input_type);
            break;
        case ty_untypedAtomic:
        case ty_string:
            translateCast2STR (f, input_type);
            break;
        default:
            PFoops (OOPS_TYPECHECK,
                    "can't cast type '%s' to type '%s'",
                    PFty_str(c->child[1]->type),
                    PFty_str(c->child[0]->sem.type));
            break;
    }

    if (!cast_optional)
        testCastComplete (f, act_level, c->child[0]->sem.type);

    fprintf(f,
            "} # end of cast from %s to %s\n", 
            PFty_str(input_type), PFty_str(cast_type));
}

/**
 * evaluateOp evaluates a operation and gives back a new
 * item column with the references to the updated values.
 * It doesn't test, if the intermediate results are aligned
 * and therefore doesn't work, if either the order of iter
 * is not given or some rows (like in the optional case)
 * are missing.
 *
 * @param f the Stream the MIL code is printed to
 * @param counter the actual offset of saved variables
 * @param operator the operator which is evaluated
 * @param t_co the container containing the type information of the
 *        values
 * @param div enables the test wether a division is made
 */
static void
evaluateOp (FILE *f, int counter, char *operator, type_co t_co, char *div)
{
    fprintf(f, "{ # '%s' calculation\n", operator);
    /* FIXME: assume that both intermediate results are aligned 
              otherwise this doesn't work */
    fprintf(f, "var val_snd := item.leftfetchjoin(%s);\n", t_co.table);
    /* item%03u is the older (first) argument and has to be the first operand
       for the evaluation */
    fprintf(f, "var val_fst := item%03u.leftfetchjoin(%s);\n",
            counter, t_co.table);
    if (div)
        fprintf(f, 
                "if (val_snd.ord_uselect(%s).count() > 0)\n"
                "{   ERROR (\"division by 0 is forbidden\"); }\n",
                div);
    fprintf(f, "val_fst := val_fst.[%s](val_snd);\n", operator);
    addValues(f, t_co, "val_fst", "item");
    fprintf(f, "item := item.reverse().mark(0@0).reverse();\n");
    fprintf(f, "val_fst := nil_oid_%s;\n", t_co.mil_type);
    fprintf(f, "val_snd := nil_oid_%s;\n", t_co.mil_type);
    fprintf(f, "} # end of '%s' calculation\n", operator);
}

/**
 * evaluateOpOpt evaluates a operation and gives back a new 
 * intermediate result. It first gets the iter values and joins
 * the two input with their iter values, which makes it possible
 * to evaluate operations where the operands are empty
 *
 * @param f the Stream the MIL code is printed to
 * @param counter the actual offset of saved variables
 * @param operator the operator which is evaluated
 * @param t_co the container containing the type information of the
 *        values
 * @param kind the type of the new intermediate result
 * @param div enables the test wether a division is made
 */
static void
evaluateOpOpt (FILE *f, int counter, char *operator,
               type_co t_co, char *kind, char *div)
{
    fprintf(f, "{ # '%s' calculation with optional type\n", operator);
    fprintf(f, "var val_snd := item.leftfetchjoin(%s);\n", t_co.table);
    fprintf(f, "val_snd := iter.reverse().leftfetchjoin(val_snd);\n");
    fprintf(f, "var val_fst := item%03u.leftfetchjoin(%s);\n",
            counter, t_co.table);
    fprintf(f, "val_fst := iter%03u.reverse().leftfetchjoin(val_fst);\n",
            counter);
    if (div)
        fprintf(f, 
                "if (val_snd.ord_uselect(%s).count() > 0)\n"
                "{   ERROR (\"division by 0 is forbidden\") };\n",
                div);
    /* item%03u is the older (first) argument and has to be the first operand
       for the evaluation */
    fprintf(f, "val_fst := val_fst.[%s](val_snd);\n", operator);
    fprintf(f, "iter := val_fst.mark(0@0).reverse();\n");
    fprintf(f, "pos := iter.project(1@0);\n");
    fprintf(f, "kind := iter.project(%s);\n", kind);
    addValues(f, t_co, "val_fst", "item");
    fprintf(f, "item := item.reverse().mark(0@0).reverse();\n");
    fprintf(f, "val_fst := nil_oid_%s;\n", t_co.mil_type);
    fprintf(f, "val_snd := nil_oid_%s;\n", t_co.mil_type);
    fprintf(f, "} # end of '%s' calculation with optional type\n", operator);
}

/**
 * translateOperation takes a operator and a core tree node
 * containing two arguments and calls according to the input
 * type a helper function, which evaluates the operation on
 * the intermediate results of the input
 *
 * @param f the Stream the MIL code is printed to
 * @param act_level the level of the for-scope
 * @param counter the actual offset of saved variables
 * @param operator the operator which is evaluated
 * @args the head of the argument list
 * @param div enables the test wether a division is made
 */
static void
translateOperation (FILE *f, int act_level, int counter, 
                    char *operator, PFcnode_t *args, bool div)
{
    PFty_t expected = args->type;

    /* translate the subtrees */
    translate2MIL (f, act_level, counter, args->child[0]);
    counter++;
    saveResult (f, counter); 
    translate2MIL (f, act_level, counter, args->child[1]->child[0]);

    /* evaluate the operation */
    if (PFty_eq(expected, PFty_integer()))
    {
        evaluateOp (f, counter, operator,
                    int_container(), div?"0":0);
    }
    else if (PFty_eq(expected, PFty_double()))
    {
        evaluateOp (f, counter, operator,                                                                                                                    
                    dbl_container(), div?"dbl(0)":0);
    }
    else if (PFty_eq(expected, PFty_decimal()))
    {
        evaluateOp (f, counter, operator,                                                                                                                    
                    dec_container(), div?"dbl(0)":0);
    }
    else if (PFty_eq(expected, PFty_opt(PFty_integer())))
    {
        evaluateOpOpt (f, counter, operator,
                       int_container(), "INT", div?"0":0);
    }
    else if (PFty_eq(expected, PFty_opt(PFty_double())))
    {
        evaluateOpOpt (f, counter, operator,                                                                                                                 
                       dbl_container(), "DBL", div?"dbl(0)":0);
    }
    else if (PFty_eq(expected, PFty_opt(PFty_decimal())))
    {
        evaluateOpOpt (f, counter, operator,                                                                                                                 
                       dec_container(), "DEC", div?"dbl(0)":0);
    }
    else
        PFlog("thinking error: result type '%s' is not supported",
              PFty_str(expected));

    /* clear the intermediate result of the second subtree */
    deleteResult (f, counter);
}

/**
 * evaluateComp evaluates a comparison and gives back a new
 * item column with the boolean values.
 * It doesn't test, if the intermediate results are aligned
 * and therefore doesn't work, if either the order of iter
 * is not given or some rows (like in the optional case)
 * are missing.
 *
 * @param f the Stream the MIL code is printed to
 * @param counter the actual offset of saved variables
 * @param comp the comparison which is evaluated
 * @param table the name of the table where the values are saved
 */
static void
evaluateComp (FILE *f, int counter, char *operator, type_co t_co)
{
    fprintf(f, "{ # '%s' comparison\n", operator);
    /* FIXME: assume that both intermediate results are aligned 
              otherwise this doesn't work */
    if (t_co.kind != co_bool)
    {
        fprintf(f, "var val_snd := item.leftfetchjoin(%s);\n", t_co.table);
        /* item%03u is the older (first) argument and has to be
           the first operand for the evaluation */
        fprintf(f, "var val_fst := item%03u.leftfetchjoin(%s);\n",
                counter, t_co.table);
    }
    else
    {
        fprintf(f, "var val_snd := item;\n");
        /* item%03u is the older (first) argument and has to be
           the first operand for the evaluation */
        fprintf(f, "var val_fst := item%03u;\n", counter);
    }
    fprintf(f, "var val_bool := val_fst.[%s](val_snd);\n", operator);
    fprintf(f, "val_fst := nil_oid_%s;\n", t_co.mil_type);
    fprintf(f, "val_snd := nil_oid_%s;\n", t_co.mil_type);
    fprintf(f, "item := val_bool.[oid]();\n");
    fprintf(f, "val_bool := nil_oid_bit;\n");
    fprintf(f, "kind := kind.project(BOOL);\n");
    fprintf(f, "} # end of '%s' comparison\n", operator);
}

/**
 * evaluateCompOpt evaluates a comparison and gives back a new 
 * intermediate result. It first gets the iter values and joins
 * the two input with their iter values, which makes it possible
 * to evaluate comparisons where the operands are empty
 *
 * @param f the Stream the MIL code is printed to
 * @param counter the actual offset of saved variables
 * @param comp the comparison which is evaluated
 * @param table the name of the table where the values are saved
 */
static void
evaluateCompOpt (FILE *f, int counter, char *operator, type_co t_co)
{
    fprintf(f, "{ # '%s' comparison with optional type\n", operator);
    if (t_co.kind != co_bool)
    {
        fprintf(f, "var val_snd := item.leftfetchjoin(%s);\n", t_co.table);
        fprintf(f, "val_snd := iter.reverse().leftfetchjoin(val_snd);\n");
        /* item%03u is the older (first) argument and has to be
           the first operand for the evaluation */
        fprintf(f, "var val_fst := item%03u.leftfetchjoin(%s);\n",
                counter, t_co.table);
        fprintf(f, "val_fst := iter%03u.reverse().leftfetchjoin(val_fst);\n",
                counter);
    }
    else
    {
        fprintf(f, "var val_snd := item;\n");
        /* item%03u is the older (first) argument and has to be
           the first operand for the evaluation */
        fprintf(f, "var val_fst := item%03u;\n", counter);
    }
    fprintf(f, "var val_bool := val_fst.[%s](val_snd);\n", operator);
    fprintf(f, "val_fst := nil_oid_%s;\n", t_co.mil_type);
    fprintf(f, "val_snd := nil_oid_%s;\n", t_co.mil_type);
    fprintf(f,
            "iter := val_bool.mark(0@0).reverse();\n"
             "pos := iter.project(1@0);\n"
             "item := val_bool.[oid]();\n"
             "val_bool := nil_oid_bit;\n"
             "item := item.reverse().mark(0@0).reverse();\n"
             "kind := iter.project(BOOL);\n"
             "} # end of '%s' comparison with optional type\n",
             operator);
}

/**
 * translateComparison takes a operator and a core tree node
 * containing two arguments and calls according to the input
 * type a helper function, which evaluates the comparison on
 * the intermediate results of the input
 *
 * @param f the Stream the MIL code is printed to
 * @param act_level the level of the for-scope
 * @param counter the actual offset of saved variables
 * @param comp the comparison which is evaluated
 * @args the head of the argument list
 */
static void
translateComparison (FILE *f, int act_level, int counter, 
                    char *comp, PFcnode_t *args)
{
    PFty_t expected = args->type;

    /* translate the subtrees */
    translate2MIL (f, act_level, counter, args->child[0]);
    counter++;
    saveResult (f, counter); 
    translate2MIL (f, act_level, counter, args->child[1]->child[0]);

    /* evaluate the comparison */
    if (PFty_eq(expected, PFty_integer()))
    {
        evaluateComp (f, counter, comp, int_container());
    }
    else if (PFty_eq(expected, PFty_double()))
    {
        evaluateComp (f, counter, comp, dbl_container());
    }
    else if (PFty_eq(expected, PFty_decimal()))
    {
        evaluateComp (f, counter, comp, dec_container());
    }
    else if (PFty_eq(expected, PFty_boolean()))
    {
        evaluateComp (f, counter, comp, bool_container());
    }
    else if (PFty_eq(expected, PFty_string()))
    {
        evaluateComp (f, counter, comp, str_container());
    }
    else if (PFty_eq(expected, PFty_opt(PFty_integer())))
    {
        evaluateCompOpt (f, counter, comp, int_container());
    }
    else if (PFty_eq(expected, PFty_opt(PFty_double())))
    {
        evaluateCompOpt (f, counter, comp, dbl_container());
    }
    else if (PFty_eq(expected, PFty_opt(PFty_decimal())))
    {
        evaluateCompOpt (f, counter, comp, dec_container());
    }
    else if (PFty_eq(expected, PFty_opt(PFty_boolean())))
    {
        evaluateCompOpt (f, counter, comp, bool_container());
    }
    else if (PFty_eq(expected, PFty_opt(PFty_string())))
    {
        evaluateCompOpt (f, counter, comp, str_container());
    }
    else
        PFlog("thinking error: first argument is of type %s",
              PFty_str(expected));

    /* clear the intermediate result of the second subtree */
    deleteResult (f, counter);
}

/**
 * combine_strings concatenates all the strings of each iter
 * and adds the newly created strings to the string container
 * 'str_values'
 *
 * @param f the Stream the MIL code is printed to
 */
static void
combine_strings (FILE *f)
{
    fprintf(f,
            "{ # combine_strings\n"
            "var iter_item := iter.reverse().leftfetchjoin(item);\n"
            "var iter_str := iter_item.leftfetchjoin(str_values);\n"
            "iter_item := nil_oid_oid;\n"
            "iter_str := iter_str.string_join(iter.project(\" \"));\n"
            "iter := iter_str.mark(0@0).reverse();\n"
            "pos := iter.mark(1@0);\n"
            "kind := iter.project(STR);\n");
    addValues (f, str_container(), "iter_str", "item");
    fprintf(f,
            "iter_str := nil_oid_str;\n"
            "item := item.reverse().mark(0@0).reverse();\n"
            "} # end of combine_strings\n");
}

/**
 * typed_value gets the string value(s) of a node or an attribute
 * if the boolean tv is set to true for a node a list of untyped
 * values (internally handled as strings) is the new intermediate
 * result. Otherwise the values are concatenated to one string
 * (string-value function).
 *
 * @param f the Stream the MIL code is printed to
 * @param tv the boolean indicating wether typed-value or
 *        string-value is called
 */
static void
typed_value (FILE *f, bool tv)
{
    /* to avoid executing to much code there are three cases:
       - only elements
       - only attributes
       - elements and attributes 
       This makes of course the code listed here bigger :) */
    fprintf(f,
            "{ # typed-value\n"
            /* save input iters to return empty string for rows
               which had no text content */
            "var input_iter := iter;\n"
            "var kind_elem := kind.get_type(ELEM);\n"
            "var item_str;\n"
            /* only elements */
            "if (kind_elem.count() = kind.count())\n"
            "{\n"
                "kind_elem := nil_oid_oid;\n"
                "var frag := kind.get_fragment();\n"
                /* to get all text nodes a scj is performed */
                "var res_scj := "
                "loop_lifted_descendant_or_self_step_with_kind_test_unjoined"
                "(iter, item, frag, ws, TEXT);\n"
                "iter := nil_oid_oid;\n"
                "item := nil_oid_oid;\n"
                "frag := nil_oid_oid;\n"
                /* variables for the result of the scj */
                "var pruned_input := res_scj.fetch(0);\n"
                /* pruned_input comes as ctx|iter */
                "var ctx_dn_item := res_scj.fetch(1);\n"
                "var ctx_dn_frag := res_scj.fetch(2);\n"
                "res_scj := nil_oid_bat;\n"
                /* combine pruned_input and ctx|dn */
                "pruned_input := pruned_input.reverse().leftjoin(ctx_dn_item.mark(0@0));\n"
                "item := ctx_dn_item.reverse().mark(0@0).reverse();\n"
                "frag := ctx_dn_frag.reverse().mark(0@0).reverse();\n"
                "ctx_dn_item := nil_oid_oid;\n"
                "ctx_dn_frag := nil_oid_oid;\n"
                /* get the string values of the text nodes */
                "item_str := mposjoin(mposjoin(item, frag, ws.fetch(PRE_PROP)), "
                                     "mposjoin(item, frag, ws.fetch(PRE_FRAG)), "
                                     "ws.fetch(PROP_TEXT));\n"
                "item := nil_oid_oid;\n"
                "frag := nil_oid_oid;\n"
                /* for the result of the scj join with the string values */
                "var iter_item := pruned_input.leftfetchjoin(item_str);\n"
                "item_str := nil_oid_str;\n");
    if (!tv)
        fprintf(f,"iter_item := iter_item.string_join(iter_item.reverse().tunique().project(\"\"));\n");

    fprintf(f,
                "pruned_input := nil_oid_oid;\n"
                "iter := iter_item.mark(0@0).reverse();\n"
                "item_str := iter_item.reverse().mark(0@0).reverse();\n"
            "} "
            "else\n"
            "{\n"
                "var kind_attr := kind.get_type(ATTR);\n"
                /* only attributes */
                "if (kind_attr.count() = kind.count())\n"
                "{\n"
                    "kind_attr := nil_oid_oid;\n"
                    "var frag := kind.get_fragment();\n"
                    "item_str := mposjoin(mposjoin(item, frag, ws.fetch(ATTR_PROP)), "
                                         "mposjoin(item, frag, ws.fetch(ATTR_FRAG)), "
                                         "ws.fetch(PROP_VAL));\n"
                    "item := nil_oid_oid;\n"
                    "frag := nil_oid_oid;\n"
                "} "
                "else\n"
                "{\n"
                    /* handles attributes and elements */
                    /* get attribute string values */
                    "kind_attr := kind_attr.mark(0@0).reverse();\n"
                    "var item_attr := kind_attr.leftfetchjoin(item);\n"
                    "var iter_attr := kind_attr.leftfetchjoin(iter);\n"
                    "var frag := kind_attr.leftfetchjoin(kind).get_fragment();\n"
                    "kind_attr := nil_oid_oid;\n"
                    "var item_attr_str "
                        ":= mposjoin(mposjoin(item_attr, frag, ws.fetch(ATTR_PROP)), "
                                    "mposjoin(item_attr, frag, ws.fetch(ATTR_FRAG)), "
                                    "ws.fetch(PROP_VAL));\n"
                    "item_attr := nil_oid_oid;\n"
                    "frag := nil_oid_oid;\n"
                    /* get element string values */
                    "kind_elem := kind_elem.mark(0@0).reverse();\n"
                    "iter := kind_elem.leftfetchjoin(iter);\n"
                    "frag := kind_elem.leftfetchjoin(kind).get_fragment();\n"
                    "item := kind_elem.leftfetchjoin(item);\n"
                    "kind_elem := nil_oid_oid;\n"
                    /* to get all text nodes a scj is performed */
                    "var res_scj := "
                    "loop_lifted_descendant_or_self_step_with_kind_test_unjoined"
                    "(iter, item, frag, ws, TEXT);\n"
                    "iter := nil_oid_oid;\n"
                    "item := nil_oid_oid;\n"
                    "frag := nil_oid_oid;\n"
                    /* variables for the result of the scj */
                    "var pruned_input := res_scj.fetch(0);\n"
                    /* pruned_input comes as ctx|iter */
                    "var ctx_dn_item := res_scj.fetch(1);\n"
                    "var ctx_dn_frag := res_scj.fetch(2);\n"
                    "res_scj := nil_oid_bat;\n"
                    /* combine pruned_input and ctx|dn */
                    "pruned_input := pruned_input.reverse().leftjoin(ctx_dn_item.mark(0@0));\n"
                    "item := ctx_dn_item.reverse().mark(0@0).reverse();\n"
                    "frag := ctx_dn_frag.reverse().mark(0@0).reverse();\n"
                    "ctx_dn_item := nil_oid_oid;\n"
                    "ctx_dn_frag := nil_oid_oid;\n"
                    /* get the string values of the text nodes */
                    "item_str := mposjoin(mposjoin(item, frag, ws.fetch(PRE_PROP)), "
                                         "mposjoin(item, frag, ws.fetch(PRE_FRAG)), "
                                         "ws.fetch(PROP_TEXT));\n"
                    "item := nil_oid_oid;\n"
                    "frag := nil_oid_oid;\n"
                    /* for the result of the scj join with the string values */
                    "var iter_item := pruned_input.leftfetchjoin(item_str);\n"
                    "pruned_input := nil_oid_oid;\n"
                    "iter := iter_item.mark(0@0).reverse();\n"
                    "item_str := iter_item.reverse().mark(0@0).reverse();\n"
                    /* merge strings from element and attribute */
                    "var res_mu := merged_union (iter, iter_attr, item_str, item_attr_str);\n"
                    "iter := res_mu.fetch(0);\n"
                    "item_str := res_mu.fetch(1);\n"
                    "res_mu := nil_oid_bat;\n"
                    "iter_item := iter.reverse().leftfetchjoin(item_str);\n"
                    "iter := nil_oid_oid;\n"
                    "item_str := nil_oid_str;\n");

    if (!tv)
        fprintf(f,  "iter_item := iter_item.string_join(iter_item.reverse().tunique().project(\"\"));\n");

    fprintf(f,
                    "iter := iter_item.mark(0@0).reverse();\n"
                    "item_str := iter_item.reverse().mark(0@0).reverse();\n"
                "}\n"
            "}\n");

    addValues (f, str_container(), "item_str", "item");
    fprintf(f,
            "item_str := nil_oid_str;\n"
            "item := item.reverse().mark(0@0).reverse();\n"
            /* adds empty strings if an element had no string content */
            "if (iter.count() != input_iter.count())\n"
            "{\n"
            "var difference := input_iter.reverse().kdiff(iter.reverse());\n"
            "difference := difference.mark(0@0).reverse();\n"
            "var res_mu := merged_union(iter, difference, item, "
                                       "difference.project(EMPTY_STRING));\n"
            "item := res_mu.fetch(1);\n"
            "res_mu := nil_oid_bat;\n"
            "iter := input_iter;\n"
            "}\n"
            "input_iter := nil_oid_oid;\n"
            "pos := iter.mark_grp(iter.tunique().project(1@0));\n"
            "kind := iter.project(STR);\n"
            "} # end of typed-value\n");
}

/**
 * the loop lifted version of fn_data searches only for 
 * values, which are not atomic and calles string-value
 * for them and combines the both sort of types in the end
 *
 * @param f the Stream the MIL code is printed to
 */
static void
fn_data (FILE *f)
{
    fprintf(f,
            /* split atomic from node types */
            "var atomic := kind.get_type_atomic();\n"
            "atomic := atomic.mark(0@0).reverse();\n"
            "var iter_atomic := atomic.leftfetchjoin(iter);\n"
            "var pos_atomic := atomic.leftfetchjoin(pos);\n"
            "var item_atomic := atomic.leftfetchjoin(item);\n"
            "var kind_atomic := atomic.leftfetchjoin(kind);\n"

            "var node := kind.get_type_node();\n"
            "node := node.mark(0@0).reverse();\n"
            "var iter_node := node.leftfetchjoin(iter);\n"
            "iter := node.mirror();\n"
            "pos := node.leftfetchjoin(pos);\n"
            "item := node.leftfetchjoin(item);\n"
            "kind := node.leftfetchjoin(kind);\n");
    typed_value (f, false);
    fprintf(f,
            /* every input row of typed-value gives back exactly
               one output row - therefore a mapping is not necessary */
            "var res_mu := merged_union (node, atomic, "
                                        "iter_node, iter_atomic, "
                                        "item, item_atomic, "
                                        "kind, kind_atomic);\n"
            "node := nil_oid_oid;\n"
            "atomic := nil_oid_oid;\n"
            "iter_node := nil_oid_oid;\n"
            "iter_atomic := nil_oid_oid;\n"
            "item := nil_oid_oid;\n"
            "item_atomic := nil_oid_oid;\n"
            "kind := nil_oid_int;\n"
            "kind_atomic := nil_oid_int;\n"
            "iter := res_mu.fetch(1);\n"
            "item := res_mu.fetch(2);\n"
            "kind := res_mu.fetch(3);\n"
            "res_mu := nil_oid_bat;\n"
            "pos := iter.mark_grp(iter.tunique().project(1@0));\n");
}

/**
 * is2ns is the translation of the item-sequence-to-node-sequence
 * function. It does basically four steps. The first step is to
 * get all text-nodes and every other nodes, as well as all other
 * atomic nodes. The second step calls 'translateCast2STR' for
 * the atomic nodes. The third step is to generate new text-nodes
 * where all the rules for building the content of an element
 * are applied within a function 'combine-text-string'. The fourth
 * step is to create text-nodes out of it and combine the other
 * nodes and the attributes with them.
 *
 */
static void
is2ns (FILE *f, int counter, PFty_t input_type)
{
    counter++;
    saveResult (f, counter);
    fprintf(f,
            "{ # item-sequence-to-node-sequence\n"
            "var nodes_order;\n"
            "{\n"
            "iter := iter%03u;\n"
            "pos := pos%03u;\n"
            "item := item%03u;\n"
            "kind := kind%03u;\n"
            /* get all text-nodes */
            "var elem := kind.get_type(ELEM);\n"
            "elem := elem.mark(0@0).reverse();\n"
            "var kind_elem := elem.leftfetchjoin(kind);\n"
            "var frag_elem := kind_elem.get_fragment();\n"
            "kind_elem := nil_oid_int;\n"
            "var item_elem := elem.leftfetchjoin(item);\n"
            "var kind_node := mposjoin (item_elem, frag_elem, ws.fetch(PRE_KIND));\n"
/* FIXME: line should stay as long as BUG 1032273 is not solved */ "kind_node.access(BAT_READ);\n"
            "var text := kind_node.ord_uselect(TEXT).mark(0@0).reverse();\n"
            "var item_text := text.leftfetchjoin(item_elem);\n"
            "var frag_text := text.leftfetchjoin(frag_elem);\n"
            "item_elem := nil_oid_oid;\n"
            "frag_elem := nil_oid_oid;\n"
            "var text_str := mposjoin (mposjoin (item_text, frag_text, ws.fetch(PRE_PROP)), "
                                      "mposjoin (item_text, frag_text, ws.fetch(PRE_FRAG)), "
                                      "ws.fetch(PROP_TEXT));\n"
            "item_text := nil_oid_oid;\n"
            "frag_text := nil_oid_oid;\n"
            "var str_text := text_str.reverse().leftfetchjoin(text);\n"
            "text_str := nil_oid_str;\n"
            "text := nil_oid_oid;\n"
            "var texts := str_text.leftfetchjoin(elem).reverse();\n"
            "str_text := nil_str_oid;\n"
            "var texts_order := texts.mark(0@0).reverse();\n"
            "texts := texts.reverse().mark(0@0).reverse();\n"
            /* 2@0 is text node constant for combine_text_string */
            "var texts_const := texts.project(2@0);\n"

            /* get all other nodes and create empty strings for them */
            "var nodes := kind_node.[!=](TEXT).ord_uselect(true).project(\"\");\n"
            "kind_node := nil_oid_chr;\n"
            "nodes := nodes.reverse().leftfetchjoin(elem).reverse();\n"
            "elem := nil_oid_oid;\n"
            "nodes_order := nodes.mark(0@0).reverse();\n"
            "nodes := nodes.reverse().mark(0@0).reverse();\n"
            /* 1@0 is node constant for combine_text_string */
            "var nodes_const := nodes.project(1@0);\n"

            "var res_mu_is2ns := merged_union (nodes_order, texts_order, "
                                              "nodes, texts, "
                                              "nodes_const, texts_const);\n"
            "nodes := nil_oid_str;\n"
            "nodes_const := nil_oid_oid;\n"
            "texts_order := nil_oid_oid;\n"
            "texts := nil_oid_str;\n"
            "texts_const := nil_oid_oid;\n"
            "var input_order := res_mu_is2ns.fetch(0);\n"
            "var input_str := res_mu_is2ns.fetch(1);\n"
            "var input_const := res_mu_is2ns.fetch(2);\n"
            "res_mu_is2ns := nil_oid_bat;\n"

            /* get all the atomic values and cast them to string */
            "var atomic := kind.get_type_atomic();\n"
            "atomic := atomic.mark(0@0).reverse();\n"
            "var iter_atomic := atomic.leftfetchjoin(iter);\n"
            "iter := atomic.mirror();\n"
            "pos := atomic.leftfetchjoin(pos);\n"
            "item := atomic.leftfetchjoin(item);\n"
            "kind := atomic.leftfetchjoin(kind);\n",
            counter, counter, counter, counter);
    translateCast2STR (f, input_type);
    fprintf(f,
            "res_mu_is2ns := merged_union (input_order, atomic, "
                                          "input_str, item.leftfetchjoin(str_values), "
                                          /* 3@0 is string constant for combine_text_string */
                                          "input_const, item.project(3@0));\n"
            "atomic := nil_oid_oid;\n"
            "input_order := res_mu_is2ns.fetch(0);\n"
            "input_str := res_mu_is2ns.fetch(1);\n"
            "input_const := res_mu_is2ns.fetch(2);\n"
            "res_mu_is2ns := nil_oid_bat;\n"
            "var input_iter := input_order.leftfetchjoin(iter%03u);\n"
            "var result_size := iter%03u.tunique().count() + nodes_order.count() + 1;\n"
            /* doesn't believe, that iter as well as input_order are ordered on h & t */
            "input_iter.chk_order(false);\n"
            /* apply the rules for the content of element construction */
            "var result_str := combine_text_string "
                              "(input_iter, input_const, input_str, result_size);\n"
            "input_iter := nil_oid_oid;\n"
            "input_const := nil_oid_oid;\n"
            "input_str := nil_oid_str;\n"
            "result_size := nil_int;\n"
            "var result_order := result_str.mark(0@0).reverse();\n"
            "result_order := result_order.leftfetchjoin(input_order);\n"
            "result_str := result_str.reverse().mark(0@0).reverse();\n",
            counter, counter);
    /* instead of adding the values first to string, then create new text nodes
       before making subtree copies in the element construction a new type text
       nodes could be created, which saves only the offset of the string in the
       text-node table and has a different handling in the element construction.
       At least some copying of strings could be avoided :) */
    addValues (f, str_container(), "result_str", "item");
    fprintf(f,
            "result_str := nil_oid_str;\n"
            "iter := result_order;\n"
            "pos := result_order.mark(1@0);\n"
            "result_order := nil_oid_oid;\n"
            "item := item.reverse().mark(0@0).reverse();\n"
            "kind := iter.project(STR);\n"
            "}\n");
    loop_liftedTextConstr (f); 
    fprintf(f,
            "var res_mu_is2ns := merged_union (iter, nodes_order, "
                                              "item, nodes_order.leftfetchjoin(item%03u), "
                                              "kind, nodes_order.leftfetchjoin(kind%03u));\n"
            "nodes_order := nil_oid_oid;\n"
            "var attr := kind%03u.get_type(ATTR).mark(0@0).reverse();\n"
            "var item_attr := attr.leftfetchjoin(item%03u);\n"
            "var kind_attr := attr.leftfetchjoin(kind%03u);\n"
            "res_mu_is2ns := merged_union (res_mu_is2ns.fetch(0), attr, "
                                          "res_mu_is2ns.fetch(1), item_attr, "
                                          "res_mu_is2ns.fetch(2), kind_attr);\n"
            "attr := nil_oid_oid;\n"
            "item_attr := nil_oid_oid;\n"
            "kind_attr := nil_oid_int;\n"
            "iter := res_mu_is2ns.fetch(0).leftfetchjoin(iter%03u);\n"
            "item := res_mu_is2ns.fetch(1);\n"
            "kind := res_mu_is2ns.fetch(2);\n"
            "pos := iter.mark_grp(iter.tunique().project(1@0));\n"
            "res_mu_is2ns := nil_oid_bat;\n"
            "} # end of item-sequence-to-node-sequence\n",
            counter, counter, counter, counter, counter, counter);
    deleteResult (f, counter);
}

/**
 * translateFunction translates the builtin functions
 *
 * @param f the Stream the MIL code is printed to
 * @param act_level the level of the for-scope
 * @param counter the actual offset of saved variables
 * @param fnQname the name of the function
 * @args the head of the argument list
 */
static void
translateFunction (FILE *f, int act_level, int counter, 
                   PFqname_t fnQname, PFcnode_t *args)
{
    if (!PFqname_eq(fnQname,PFqname (PFns_fn,"doc")))
    {
        translate2MIL (f, act_level, counter, args->child[0]);
        /* FIXME: expects strings otherwise something stupid happens */
        fprintf(f,
                "{ # translate fn:doc (string?) as document?\n"
                "var docs := item.tunique().mark(0@0).reverse();\n"
                "var doc_str := docs.leftfetchjoin(str_values);\n"
                "docs := nil_oid_oid;\n"
                "doc_str := doc_str.reverse().kdiff(ws.fetch(DOC_LOADED).reverse())"
                        ".mark(0@0).reverse();\n"
                "doc_str@batloop () {\n"
                "ws := add_doc(ws, $t);\n"
                "}\n"
                "doc_str := nil_oid_str;\n"
                "doc_str := item.leftfetchjoin(str_values);\n"
                "var frag := doc_str.leftjoin(ws.fetch(DOC_LOADED).reverse());\n"
                "doc_str := nil_oid_str;\n"
                "frag := frag.reverse().mark(0@0).reverse();\n"
                "kind := get_kind(frag, ELEM);\n"
                "frag := nil_oid_oid;\n"
                "item := kind.project(0@0);\n"
                "} # end of translate fn:doc (string?) as document?\n"
               );
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_pf,"distinct-doc-order")))
    {
        translate2MIL (f, act_level, counter, args->child[0]);
        fprintf(f,
                "{ # translate pf:distinct-doc-order (node*) as node*\n"
                /* FIXME: is this right? */
                "if (kind.count() != kind.get_type(ELEM).count()) "
                "{ ERROR (\"function pf:distinct-doc-order expects only nodes\"); }\n"
                /* delete duplicates */
                "var temp_ddo := CTgroup(iter).CTgroup(item).CTgroup(kind);\n"
                "temp_ddo := temp_ddo.tunique().mark(0@0).reverse();\n"
                "iter := temp_ddo.leftfetchjoin(iter);\n"
                "item := temp_ddo.leftfetchjoin(item);\n"
                "kind := temp_ddo.leftfetchjoin(kind);\n"
                "temp_ddo := nil_oid_oid;\n"
                /* sort by iter, frag, pre */
                "var sorting := iter.reverse().sort().reverse();\n"
                "sorting := sorting.CTrefine(kind);"
                "sorting := sorting.CTrefine(item);"
                "sorting := sorting.mark(0@0).reverse();\n"
                "iter := sorting.leftfetchjoin(iter);\n"
                "pos := iter.mark(1@0);\n"
                "item := sorting.leftfetchjoin(item);\n"
                "kind := sorting.leftfetchjoin(kind);\n"
                "sorting := nil_oid_oid;\n"
                "} # end of translate pf:distinct-doc-order (node*) as node*\n"
               );
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_fn,"exactly-one")))
    {
        translate2MIL (f, act_level, counter, args->child[0]);
        fprintf(f,
                "if (iter.tunique().count() != loop%03u.count()) "
                "{ ERROR (\"function fn:exactly-one expects "
                "exactly one value per iteration\"); }\n",
                act_level);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_fn,"zero-or-one")))
    {
        translate2MIL (f, act_level, counter, args->child[0]);
        fprintf(f,
                "if (iter.tunique().count() != iter.count()) "
                "{ ERROR (\"function fn:zero-or-one expects "
                "zero or one value per iteration\"); }\n");
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_fn,"count")))
    {
        translate2MIL (f, act_level, counter, args->child[0]);
        fprintf(f,
                "{ # translate fn:count (item*) as integer\n"
                /* counts for all iters the number of items */
                /* uses the actual loop, to collect the iters, which are translated 
                   into empty sequences */
                "var iter_count := {count}(iter.reverse(),loop%03u.reverse());\n"
                "iter_count := iter_count.reverse().mark(0@0).reverse();\n",
                act_level);
        addValues (f, int_container(), "iter_count", "item");
        fprintf(f,
                "item := item.reverse().mark(0@0).reverse();\n"
                "iter_count := nil_oid_int;\n"
                "iter := loop%03u.reverse().mark(0@0).reverse();\n"
                "pos := iter.project(1@0);\n"
                "kind := iter.project(INT);\n"
                "} # end of translate fn:count (item*) as integer\n",
                act_level);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_fn,"empty")))
    {
        translate2MIL (f, act_level, counter, args->child[0]);
        fprintf(f,
                "{ # translate fn:empty (item*) as boolean\n"
                "var iter_count := {count}(iter.reverse(),loop%03u.reverse());\n"
                "var iter_bool := iter_count.[=](0).[oid]();\n"
                "iter_count := nil_oid_int;\n"
                "item := iter_bool.reverse().mark(0@0).reverse();\n"
                "iter_bool := nil_oid_bit;\n"
                "iter := loop%03u.reverse().mark(0@0).reverse();\n"
                "pos := iter.project(1@0);\n"
                "kind := iter.project(BOOL);\n"
                "} # end of translate fn:empty (item*) as boolean\n",
                act_level, act_level);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_fn,"not")))
    {
        translate2MIL (f, act_level, counter, args->child[0]);
        fprintf(f,
                "# translate fn:not (boolean) as boolean\n"
                "item := item.leftfetchjoin(bool_not);\n"
               );
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_fn,"boolean")))
    {
        translate2MIL (f, act_level, counter, args->child[0]);
        fn_boolean (f, act_level, args->type);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_fn,"contains")))
    {
        translate2MIL (f, act_level, counter, args->child[0]);
        counter++;
        saveResult (f, counter);
        translate2MIL (f, act_level, counter, args->child[1]->child[0]);
        fprintf(f,
                "{ # fn:contains (string?, string?) as boolean\n"
                "var strings;\n"
                "var search_strs;\n"
                "if (iter%03u.count() != loop%03u.count())\n"
                "{\n"
                "var difference := loop%03u.reverse().kdiff(iter%03u.reverse());\n"
                "difference := difference.mark(0@0).reverse();\n"
                "var res_mu := merged_union(iter%03u, difference, item%03u, "
                                           "difference.project(EMPTY_STRING));\n"
                "difference := nil_oid_oid;\n"
                "strings := res_mu.fetch(1).leftfetchjoin(str_values);\n"
                "res_mu := nil_oid_bat;\n"
                "} "
                "else {\n"
                "strings := item%03u.leftfetchjoin(str_values);\n"
                "}\n"

                "if (iter.count() != loop%03u.count())\n"
                "{\n"
                "var difference := loop%03u.reverse().kdiff(iter.reverse());\n"
                "difference := difference.mark(0@0).reverse();\n"
                "var res_mu := merged_union(iter, difference, item, "
                                           "difference.project(EMPTY_STRING));\n"
                "difference := nil_oid_oid;\n"
                "search_strs := res_mu.fetch(1).leftfetchjoin(str_values);\n"
                "res_mu := nil_oid_bat;\n"
                "} "
                "else {\n"
                "search_strs := item.leftfetchjoin(str_values);\n"
                "}\n"

                "item := strings.[search](search_strs).[!=](-1).[oid]();\n"
                "strings := nil_oid_str;\n"
                "search_strs := nil_oid_str;\n"
                "iter := loop%03u.reverse().mark(0@0).reverse();\n"
                "pos := iter.project(1@0);\n"
                "kind := iter.project(BOOL);\n"
                "} # end of fn:contains (string?, string?) as boolean\n",
                counter, act_level, 
                act_level, counter,
                counter, counter,
                counter,
                act_level,
                act_level,
                act_level);
        deleteResult (f, counter);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_op,"and")))
    {
        translate2MIL (f, act_level, counter, args->child[0]);
        counter++;
        saveResult (f, counter);
        translate2MIL (f, act_level, counter, args->child[1]->child[0]);
        fprintf(f,
                "item := item.[int]().[and](item%03u.[int]()).[oid]();\n", counter);
        deleteResult (f, counter);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_op,"or")))
    {
        translate2MIL (f, act_level, counter, args->child[0]);
        counter++;
        saveResult (f, counter);
        translate2MIL (f, act_level, counter, args->child[1]->child[0]);
        fprintf(f,
                "item := item.[int]().[or](item%03u.[int]()).[oid]();\n", counter);
        deleteResult (f, counter);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_fn,"position")))
    {
        /* FIXME: */
        PFlog ("function position is NOT correct!! "
               "it builds position according to actual nesting not"
               " according to '.'");
        createEnumeration (f, act_level);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_fn,"last")))
    {
        fprintf(f,
                "{ # fn:last ()\n"
                /* I'm not 100% sure what I actually programmed here, 
                   but it works at least with the actual translation
                   of the predicates */
                "var ints_cE := outer%03u.mark_grp(outer%03u.tunique().project(1@0));\n"
                "var last_rows := {max}(outer%03u.reverse().[int](),outer%03u.reverse()).[oid]();\n"
                "last_rows := last_rows.leftfetchjoin(ints_cE);\n"
                "ints_cE := nil_oid_oid;\n"
                "iter := last_rows.mark(0@0).reverse();\n"
                "pos := iter.project(1@0);\n"
                "kind := iter.project(INT);\n"
                "last_rows := last_rows.reverse().mark(0@0).reverse();\n"
                "var ints_last := last_rows.[int]();\n"
                "last_rows := nil_oid_oid;\n",
                act_level, act_level, act_level, act_level);
        addValues (f, int_container(), "ints_last", "item");
        fprintf(f,
                "item := item.reverse().mark(0@0).reverse();\n"
                "ints_last := nil_oid_int;\n"
                "} # end of fn:last ()\n");
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_pf,"typed-value")))
    {
        translate2MIL (f, act_level, counter, args->child[0]);
        typed_value (f, true);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_pf,"string-value")))
    {
        translate2MIL (f, act_level, counter, args->child[0]);
        typed_value (f, false);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_fn,"data")))
    {
        translate2MIL (f, act_level, counter, args->child[0]);
        fn_data (f);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_fn,"string")))
    {
        translate2MIL (f, act_level, counter, args->child[0]);
        typed_value (f, false);
        translateCast2STR (f, args->type);
        fprintf(f,
                "if (iter.count() != loop%03u.count())\n"
                "{\n"
                "var difference := loop%03u.reverse().kdiff(iter.reverse());\n"
                "difference := difference.mark(0@0).reverse();\n"
                "var res_mu := merged_union(iter, difference, item, "
                                           "difference.project(EMPTY_STRING));\n"
                "difference := nil_oid_oid;\n"
                "item := res_mu.fetch(1);\n"
                "res_mu := nil_oid_bat;\n"
                "iter := loop%03u.reverse().mark(0@0).reverse();\n"
                "pos := iter.project(1@0);\n"
                "kind := iter.project(STR);\n"
                "}\n",
                act_level, act_level, act_level);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_fn,"string-join")))
    {
        translate2MIL (f, act_level, counter, args->child[0]);
        counter++;
        saveResult (f, counter); 
        translate2MIL (f, act_level, counter, args->child[1]->child[0]);
        fprintf(f,
                "{ # string-join (string*, string)\n "
                "var iter_item := iter%03u.reverse().leftfetchjoin(item%03u);\n"
                "var iter_item_str := iter_item.leftfetchjoin(str_values);\n"
                "iter_item := nil_oid_oid;\n"
                "var iter_sep := iter.reverse().leftfetchjoin(item);\n"
                "var iter_sep_str := iter_sep.leftfetchjoin(str_values);\n"
                "iter_sep := nil_oid_oid;\n"
                "iter_item_str := string_join(iter_item_str, iter_sep_str);\n"
                "iter_sep_str := nil_oid_str;\n"
                "iter := iter_item_str.mark(0@0).reverse();\n"
                "iter_item_str := iter_item_str.reverse().mark(0@0).reverse();\n",
                counter, counter);
        addValues(f, str_container(), "iter_item_str", "item");
        fprintf(f,
                "iter_item_str := nil_oid_str;\n"
                "item := item.reverse().mark(0@0).reverse();\n"
                "pos := iter.project(1@0);\n"
                "kind := iter.project(STR);\n"
                "} # end of string-join (string*, string)\n ");
        deleteResult (f, counter);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_fn,"concat")))
    {
        translate2MIL (f, act_level, counter, args->child[0]);
        counter++;
        saveResult (f, counter); 
        translate2MIL (f, act_level, counter, args->child[1]->child[0]);
        fprintf(f,
                "{ # concat (string, string)\n "
                "var iter_item := iter%03u.reverse().leftfetchjoin(item%03u);\n"
                "var fst_iter_str := iter_item.leftfetchjoin(str_values);\n"
                "iter_item := nil_oid_oid;\n"
                "var iter_item := iter.reverse().leftfetchjoin(item);\n"
                "var snd_iter_str := iter_item.leftfetchjoin(str_values);\n"
                "iter_item := nil_oid_oid;\n"
                "fst_iter_str := fst_iter_str[+](snd_iter_str);\n"
                "snd_iter_str := nil_oid_str;\n"
                "iter := fst_iter_str.mark(0@0).reverse();\n"
                "fst_iter_str := fst_iter_str.reverse().mark(0@0).reverse();\n",
                counter, counter);
        addValues(f, str_container(), "fst_iter_str", "item");
        fprintf(f,
                "fst_iter_str := nil_oid_str;\n"
                "item := item.reverse().mark(0@0).reverse();\n"
                "pos := iter.project(1@0);\n"
                "kind := iter.project(STR);\n"
                "} # end of concat (string, string)\n ");
        deleteResult (f, counter);
    }
    /* calculation functions just call an extra function with
       their operator argument to avoid code duplication */
    else if (!PFqname_eq(fnQname,PFqname (PFns_op,"plus")))
    {
        translateOperation (f, act_level, counter, "+", args, false);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_op,"minus")))
    {
        translateOperation (f, act_level, counter, "-", args, false);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_op,"times")))
    {
        translateOperation (f, act_level, counter, "*", args, false);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_op,"div")))
    {
        translateOperation (f, act_level, counter, "/", args, true);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_op,"mod")))
    {
        translateOperation (f, act_level, counter, "%", args, true);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_op,"idiv")))
    {
        /* the semantics of idiv are a normal div operation
           followed by a cast to integer */
        translateOperation (f, act_level, counter, "/", args, true);
        translateCast2INT (f, args->type);
        testCastComplete(f, act_level, PFty_integer ());
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_op,"eq")))
    {
        translateComparison (f, act_level, counter, "=", args);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_op,"ne")))
    {
        translateComparison (f, act_level, counter, "!=", args);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_op,"ge")))
    {
        translateComparison (f, act_level, counter, ">=", args);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_op,"gt")))
    {
        translateComparison (f, act_level, counter, ">", args);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_op,"le")))
    {
        translateComparison (f, act_level, counter, "<=", args);
    }
    else if (!PFqname_eq(fnQname,PFqname (PFns_op,"lt")))
    {
        translateComparison (f, act_level, counter, "<", args);
    }
    else if (!PFqname_eq(fnQname,
                         PFqname (PFns_pf,"item-sequence-to-node-sequence")))
    {
        translate2MIL (f, act_level, counter, args->child[0]);
        is2ns (f, counter, args->type);
    }
    else if (!PFqname_eq(fnQname,
                         PFqname (PFns_pf,"item-sequence-to-untypedAtomic")))
    {
        translate2MIL (f, act_level, counter, args->child[0]);
        fn_data (f);
        translateCast2STR (f, args->type);
        combine_strings (f);
    }
    else if (!PFqname_eq(fnQname,
                         PFqname (PFns_pf,"merge-adjacent-text-nodes")))
    {
        translate2MIL (f, act_level, counter, args->child[0]);
        is2ns (f, counter, args->type);
    }
    else if (!PFqname_eq(fnQname,
                         PFqname (PFns_fn,"distinct-values")))
    {
        translate2MIL (f, act_level, counter, args->child[0]);
        fprintf(f,
                "{ # translate fn:distinct-values (atomic*) as atomic*\n"
                "var sorting := CTgroup(iter).CTgroup(item).CTgroup(kind);\n"
                "sorting := sorting.tunique().mark(0@0).reverse();\n"
                "iter := sorting.leftfetchjoin(iter);\n"
                "pos := iter.mark_grp(iter.tunique().project(1@0));\n"
                "item := sorting.leftfetchjoin(item);\n"
                "kind := sorting.leftfetchjoin(kind);\n"
                "sorting := nil_oid_oid;\n"
                "} # end of translate fn:distinct-values (atomic*) as atomic*\n");
    }
    else 
    {
        PFlog("function %s is not supported and therefore ignored",
              PFqname_str (fnQname));
        fprintf(f,
                "# empty intermediate result "
                "instead of unsupported function %s\n",
                PFqname_str (fnQname));
        translateEmpty (f);
    }
}

/**
 * translate2MIL prints the MIL-expressions, for the following
 * core nodes:
 * c_var, c_seq, c_for, c_let
 * c_lit_str, c_lit_dec, c_lit_dbl, c_lit_int
 * c_empty, c_true, c_false
 * c_locsteps, (axis: c_ancestor, ...)
 * (tests: c_namet, c_kind_node, ...)
 * c_ifthenelse,
 * (constructors: c_elem, ...)
 * c_typesw, c_cases, c_case, c_seqtype, c_seqcast
 *
 * the following list is not supported so far:
 * c_nil
 * c_apply, c_arg,
 * c_error, c_root
 *
 * @param f the Stream the MIL code is printed to
 * @param act_level the level of the for-scope
 * @param counter the actual offset of saved variables
 * @param c the Core node containing the rest of the subtree
 */
static void
translate2MIL (FILE *f, int act_level, int counter, PFcnode_t *c)
{
    char *ns, *loc;
    int bool_res;

    assert(c);
    switch (c->kind)
    {
        case c_var:
            translateVar(f, act_level, c);
            break;
        case c_seq:
            translate2MIL (f, act_level, counter, c->child[0]);
            counter++;
            saveResult (f, counter);

            translate2MIL (f, act_level, counter, c->child[1]);

            translateSeq (f, counter);
            deleteResult (f, counter);
            break;
        case c_let:
            if (c->child[0]->sem.var->used)
            {
                translate2MIL (f, act_level, counter, c->child[1]);
                insertVar (f, act_level, c->child[0]->sem.var->vid);
            }

            translate2MIL (f, act_level, counter, c->child[2]);
            break;
        case c_for:
            translate2MIL (f, act_level, counter, c->child[2]);
            /* not allowed to overwrite iter,pos,item */

            act_level++;
            fprintf(f, "{ # for-translation\n");
            project (f, act_level);

            fprintf(f, "var expOid;\n");
            getExpanded (f, act_level, c->sem.num);
            fprintf(f,
                    "if (expOid.count() != 0) {\n"
                    "var oidNew_expOid;\n");
                    expand (f, act_level);
                    join (f, act_level);
            fprintf(f, "} else {\n");
                    createNewVarTable (f, act_level);
            fprintf(f, 
                    "} # end if\n"
                    "expOid := nil_oid_oid;\n");

            if (c->child[0]->sem.var->used)
                insertVar (f, act_level, c->child[0]->sem.var->vid);
            if ((c->child[1]->kind == c_var)
                && (c->child[1]->sem.var->used))
            {
                /* changes item and kind and inserts if needed
                   new int values to 'int_values' bat */
                createEnumeration (f, act_level);
                insertVar (f, act_level, c->child[1]->sem.var->vid);
            }
            /* end of not allowed to overwrite iter,pos,item */

            translate2MIL (f, act_level, counter, c->child[3]);
            
            mapBack (f, act_level);
            cleanUpLevel (f, act_level);
            fprintf(f, "} # end of for-translation\n");
            break;
        case c_ifthenelse:
            translate2MIL (f, act_level, counter, c->child[0]);
            counter ++;
            saveResult (f, counter);
            bool_res = counter;
            fprintf(f, "{ # ifthenelse-translation\n");
            /* idea:
            select trues
            if (trues = count) or (trues = 0)
                 only give back one of the results
            else
                 do the whole stuff
            */
            fprintf(f,
                    "var selected := item%03u.ord_uselect(1@0);\n"
                    "var skip := 0;\n"
                    "if (selected.count() = item%03u.count()) "
                    "{ skip := 2; } "
                    "else { if (selected.count() = 0) "
                    "{ skip := 1; }}\n",
                    bool_res, bool_res);
            /* if at compile time one argument is already known to
               be empty don't do the other */
            if (c->child[2]->kind == c_empty)
            {
                    translateIfThen (f, act_level, counter, 
                                     c->child[1], 1, bool_res);
            }
            else if (c->child[1]->kind == c_empty)
            {
                    translateIfThen (f, act_level, counter,
                                     c->child[2], 0, bool_res);
            }
            else
            {
                    translateIfThen (f, act_level, counter,
                                     c->child[1], 1, bool_res);
                    counter++;
                    saveResult (f, counter);
                    translateIfThen (f, act_level, counter,
                                     c->child[2], 0, bool_res);
                    translateSeq (f, counter);
                    deleteResult (f, counter);
                    counter--;
            }
            fprintf(f, "} # end of ifthenelse-translation\n");
            deleteResult (f, counter);
            break;
        case c_locsteps:
            translate2MIL (f, act_level, counter, c->child[1]);
            translateLocsteps (f, c->child[0]);
            break;
        case c_elem:
            translate2MIL (f, act_level, counter, c->child[0]);
            if (c->child[0]->kind != c_tag)
            {
                castQName (f);
            }
            counter++;
            saveResult (f, counter);

            translate2MIL (f, act_level, counter, c->child[1]);

            loop_liftedElemConstr (f, counter);
            deleteResult (f, counter);
            break;
        case c_attr:
            translate2MIL (f, act_level, counter, c->child[0]);

            if (c->child[0]->kind != c_tag)
            {
                castQName (f);
            }

            counter++;
            saveResult (f, counter);

            translate2MIL (f, act_level, counter, c->child[1]);

            loop_liftedAttrConstr (f, act_level, counter);
            deleteResult (f, counter);
            break;
        case c_tag:
            ns = c->sem.qname.ns.uri;
            loc = c->sem.qname.loc;

            /* translate missing ns as "" */
            if (!ns)
                ns = "";

            fprintf(f,
                    "{ # tagname-translation\n"
                    "var propID := ws.fetch(QN_NS).fetch(WS)"
                        ".ord_uselect(\"%s\").mirror();\n"
                    "var prop_str := propID"
                        ".leftfetchjoin(ws.fetch(QN_LOC).fetch(WS));\n"
                    "propID := prop_str.ord_uselect(\"%s\");\n"
                    "prop_str := nil_oid_str;\n"
                    "var itemID;\n",
                    ns, loc);

            fprintf(f,
                    "if (propID.count() = 0)\n"
                    "{\n"
                    "itemID := oid(ws.fetch(QN_LOC).fetch(WS).count());\n"
                    "ws.fetch(QN_NS).fetch(WS).insert(itemID,\"%s\");\n"
                    "ws.fetch(QN_LOC).fetch(WS).insert(itemID,\"%s\");\n"
                    "} else { "
                    "itemID := propID.reverse().fetch(0); }\n",
                    ns, loc);

            /* translateConst needs a bound variable itemID */
            translateConst (f, act_level, "QNAME");
            fprintf(f,
                    "propID := nil_oid_oid;\n"
                    "itemID := nil_oid;\n"
                    "} # end of tagname-translation\n"
                   );
            break;
        case c_text:
            translate2MIL (f, act_level, counter, c->child[0]);
            loop_liftedTextConstr (f);
            break;
        case c_lit_str:
            fprintf(f,
                    "{\n"
                    "str_values.seqbase(nil);\n"
                    "str_values.insert(nil,\"%s\");\n"
                    "str_values.seqbase(0@0);\n"
                    "var itemID := str_values.ord_uselect(\"%s\");\n"
                    "itemID := itemID.reverse().fetch(0);\n",
                    PFesc_string (c->sem.str),
                    PFesc_string (c->sem.str));
            /* translateConst needs a bound variable itemID */
            translateConst(f, act_level, "STR");
            fprintf(f, 
                    "itemID := nil_str;\n"
                    "}\n");
            break;
        case c_lit_int:
            fprintf(f,
                    "{\n"
                    "int_values.seqbase(nil);\n"
                    "int_values.insert(nil,%u);\n"
                    "int_values.seqbase(0@0);\n"
                    "var itemID := int_values.ord_uselect(%u);\n"
                    "itemID := itemID.reverse().fetch(0);\n",
                    c->sem.num, c->sem.num);
            /* translateConst needs a bound variable itemID */
            translateConst(f, act_level, "INT");
            fprintf(f, 
                    "itemID := nil_int;\n"
                    "}\n");
            break;
        case c_lit_dec:
            fprintf(f,
                    "{\n"
                    "dec_values.seqbase(nil);\n"
                    "dec_values.insert(nil,dbl(%g));\n"
                    "dec_values.seqbase(0@0);\n"
                    "var itemID := dec_values.ord_uselect(dbl(%g));\n"
                    "itemID := itemID.reverse().fetch(0);\n",
                    c->sem.dec, c->sem.dec);
            /* translateConst needs a bound variable itemID */
            translateConst(f, act_level, "DEC");
            fprintf(f, 
                    "itemID := nil_dbl;\n"
                    "}\n");
            break;
        case c_lit_dbl:
            fprintf(f,
                    "{\n"
                    "dbl_values.seqbase(nil);\n"
                    "dbl_values.insert(nil,dbl(%g));\n"
                    "dbl_values.seqbase(0@0);\n"
                    "var itemID := dbl_values.ord_uselect(dbl(%g));\n"
                    "itemID := itemID.reverse().fetch(0);\n",
                    c->sem.dbl, c->sem.dbl);
            /* translateConst needs a bound variable itemID */
            translateConst(f, act_level, "DBL");
            fprintf(f, 
                    "itemID := nil_dbl;\n"
                    "}\n");
            break;
        case c_true:
            fprintf(f,
                    "{\n"
                    "var itemID := 1@0;\n");
            /* translateConst needs a bound variable itemID */
            translateConst(f, act_level, "BOOL");
            fprintf(f, 
                    "itemID := nil_oid;\n"
                    "}\n");
            break;
        case c_false:
            fprintf(f,
                    "{\n"
                    "var itemID := 0@0;\n");
            /* translateConst needs a bound variable itemID */
            translateConst(f, act_level, "BOOL");
            fprintf(f, 
                    "itemID := nil_oid;\n"
                    "}\n");
            break;
        case c_root:
            /* root gets the pre value and fragment of the
               document which is last loaded */
            /* FIXME: if a document is already loaded FRAG is not
                      changed if the document is referenced 
                      - fn:doc would need to work in a iterative
                      way */
            fprintf(f,
                    "{\n"
                    "var itemID := 0@0;\n");
            /* translateConst needs a bound variable itemID */
            translateConst(f, act_level, "ELEM");
            fprintf(f,
                    "kind := kind.project(ws.fetch(FRAG).fetch(0))"
                    ".get_kind(ELEM);\n"
                    "itemID := nil_oid;\n"
                    "}\n");
            break;
        case c_empty:
            translateEmpty (f);
            break;
        case c_seqcast:
            translate2MIL (f, act_level, counter, c->child[1]);
            translateCast (f, act_level, c);
            break;
        case c_apply:
            translateFunction (f, act_level, counter, 
                               c->sem.fun->qname, c->child[0]);
            break;
        case c_typesw:
            PFlog("typeswitch occured");
        case c_nil:
            PFlog("nil occured");
        case c_arg:
            PFlog("arg occured");
        default: 
            PFoops (OOPS_WARNING, "not supported feature is translated");
            break;
    }
}

/**
 * noConstructor tests if constructors are used in core tree
 * and report 0 if at least one is found and 1 else
 *
 * @param c the core tree, which is tested on constructor containment
 * @return 1 if no constructor is found - else 0
 */
static int
noConstructor (PFcnode_t *c)
{
    int i;
    if (c->kind == c_elem || c->kind == c_attr || c->kind == c_text ||
        c->kind == c_doc || c->kind == c_comment || c->kind == c_pi)
        return 0;
    else 
        for (i = 0; i < PFCNODE_MAXCHILD && c->child[i]; i++)
            if (!noConstructor (c->child[i]))
                return 0;

    return 1;
}

/**
 * Walk core tree @a e and replace occurrences of variable @a v
 * by core tree @a a (i.e., compute e[a/v]).
 *
 * @note Only copies the single node @a a for each occurence of
 *       @a v. Only use this function if you
 *       - either call it only with atoms @a a,
 *       - or copy @a a at most once (i.e. @a v occurs at most
 *         once in @a e).
 *       Otherwise we might come into deep trouble as children
 *       of @a e would have more than one parent afterwards...
 *
 * @param v variable to replace
 * @param a core tree to insert for @a v
 * @param e core tree to walk over
 * @return modified core tree
 */
static void
replace_var (PFvar_t *v, PFcnode_t *a, PFcnode_t *e)
{
  unsigned short int i;
                                                                                                                                                             
  assert (v && a && e);
                                                                                                                                                             
  if (e->kind == c_var && e->sem.var == v)
      *e = *a;
  else
      for (i = 0; (i < PFCNODE_MAXCHILD) && e->child[i]; i++)
          replace_var (v, a, e->child[i]);
}

/**
 * var_is_used tests how often a variable is used in a tree
 * and gives back the number of usages.
 *
 * @param v variable to replace
 * @param e core tree to walk over
 */
static int
var_is_used (PFvar_t *v, PFcnode_t *e)
{
  int i;
  int usage = 0;

  assert (v && e);

  if (e->kind == c_var && e->sem.var == v)
      return 1;
  else
      for (i = 0; (i < PFCNODE_MAXCHILD) && e->child[i]; i++)
          usage += var_is_used (v, e->child[i]);

  return usage;
}

/**
 * simplifyCoreTree walks over a given tree and simplifies
 * some core nodes and appends extra information (e.g.
 * fake functions like 'item_sequence_to_node_sequence')
 *
 * @param c core tree to walk over
 * @return modified core tree
 */
static void
simplifyCoreTree (PFcnode_t *c)
{
    unsigned int i;
    PFfun_t *fun;
    PFcnode_t *new_node;
    PFty_t expected, opt_expected;
    PFty_t cast_type, input_type, opt_cast_type;

    assert(c);
    switch (c->kind)
    {
        case c_var:
            break;
        case c_seq:
            /* prunes every empty node in the sequence construction */
            simplifyCoreTree (c->child[0]);
            simplifyCoreTree (c->child[1]);

            if ((c->child[0]->kind == c_empty)
                && (c->child[1]->kind != c_empty))
            {
                new_node = PFcore_empty ();
                new_node->type = PFty_empty ();
                *c = *new_node;
            }
            else if (c->child[0]->kind == c_empty)
                *c = *(c->child[1]);
            else if (c->child[1]->kind == c_empty)
                *c = *(c->child[0]);
            break;
        case c_let:
            if ((i = var_is_used (c->child[0]->sem.var, c->child[2])))
            {
                simplifyCoreTree (c->child[1]);
                simplifyCoreTree (c->child[2]);
                /* removes let statements, which are used only once and contain
                   no constructor */
                if (i == 1 &&
                    noConstructor (c->child[1]))
                {
                    replace_var (c->child[0]->sem.var, c->child[1], c->child[2]);
                    *c = *(c->child[2]);
                    simplifyCoreTree (c);
                }
                /* remove all let statements, which are only bound to a literal
                   or another variable */
                else if (c->child[1]->kind == c_lit_str ||
                         c->child[1]->kind == c_lit_int ||
                         c->child[1]->kind == c_lit_dec ||
                         c->child[1]->kind == c_lit_dbl ||
                         c->child[1]->kind == c_true ||
                         c->child[1]->kind == c_false ||
                         c->child[1]->kind == c_empty ||
                         c->child[1]->kind == c_var)
                {
                    replace_var (c->child[0]->sem.var, c->child[1], c->child[2]);
                    *c = *(c->child[2]);
                    simplifyCoreTree (c);
                }
                /* remove let statements, whose body contains only
                   the bound variable */
                else if (c->child[2]->kind == c_var && 
                         c->child[2]->sem.var == c->child[0]->sem.var)
                {
                    *c = *(c->child[1]);
                }
            }
            /* removes let statement, which are not used */
            else
            {
                *c = *(c->child[2]);
                simplifyCoreTree (c);
            }
            break;
        case c_for:
            simplifyCoreTree (c->child[2]);
            simplifyCoreTree (c->child[3]);
            input_type = PFty_defn(c->child[2]->type);

            if (c->child[3]->kind == c_var && 
                c->child[3]->sem.var == c->child[0]->sem.var)
            {
                *c = *(c->child[2]);
            }
            /* removes for expressions, which loop only over one literal */
            /* FIXME: doesn't work if the following type is possible here:
                      '(integer, decimal)?' */
            else if (PFty_subtype (input_type, PFty_opt (PFty_item ())) &&
                     input_type.type != ty_plus &&
                     input_type.type != ty_star &&
                     input_type.type != ty_choice &&
                     input_type.type != ty_all &&
                     input_type.type != ty_seq)
            {
                /* if for expression contains a positional variable
                   and it is used it it replaced by the integer 0 */
                if (c->child[1]->kind == c_var)
                {
                    new_node = PFcore_num (1);
                    new_node->type = PFty_integer ();
                    replace_var (c->child[1]->sem.var, new_node, c->child[3]);
                }

                if (PFty_eq (input_type, PFty_empty ()))
                {
                    new_node = PFcore_empty ();
                    new_node->type = PFty_empty ();
                    *c = *new_node;
                }
                else if (PFty_subtype (input_type, PFty_opt (PFty_atomic ())))
                {
                    replace_var (c->child[0]->sem.var, c->child[2], c->child[3]);
                    *c = *(c->child[3]);
                }
                else 
                {
                    new_node = PFcore_let (c->child[0],
                                           c->child[2],
                                           c->child[3]);
                    new_node->type = c->child[3]->type;
                    new_node->child[0]->type = c->child[1]->type;
                    *c = *new_node;
                }
                simplifyCoreTree (c);
            }
            /* remove for expression, whose body contains only
                   the bound variable */
            break;
        case c_seqcast:
            simplifyCoreTree (c->child[1]);
            /* debugging information */
            /*
            PFlog("input type: %s",
                  PFty_str (c->child[1]->type));
            PFlog("cast type: %s",
                  PFty_str (c->child[0]->sem.type));
            */
            cast_type = c->child[0]->sem.type;
            input_type = c->child[1]->type;
            opt_cast_type = cast_type;

            if (cast_type.type == ty_opt)
                opt_cast_type = PFty_child (cast_type);

            /* if casts are nested only the most outest
               cast has to be evaluated */
            if (c->child[1]->kind == c_seqcast)
            {
                assert (c->child[1]->child[1]);
                c->child[1] = c->child[1]->child[1];
                input_type = c->child[1]->type;
            }

            /* removes casts which are not necessary:
               - if types are the same
               - if the cast type has only a additional
                 occurence indicator compared to the
                 input type */
            if (PFty_eq (input_type, cast_type) ||
                PFty_eq (input_type, opt_cast_type) ||
                (cast_type.type == ty_opt &&
                 PFty_eq (input_type, PFty_empty ())))
            {
                *c = *(c->child[1]);
            }
            /* tests if a type is castable */
            else if (castable (input_type, cast_type));
            else if (!PFty_subtype (input_type, cast_type))
                PFoops (OOPS_TYPECHECK,
                        "can't cast type '%s' to type '%s'",
                        PFty_str(input_type),
                        PFty_str(cast_type));
            else
            {
                *c = *(c->child[1]);
                PFlog ("cast from '%s' to '%s' ignored",
                       PFty_str (input_type),
                       PFty_str (cast_type));
            }
            break;
        case c_typesw:
            /* prunes the typeswitches where the branch is already
               known at compiletime and reports error otherwise */
            cast_type = c->child[1]->child[0]->child[0]->sem.type;

            if (PFty_subtype (c->child[0]->type, cast_type))
            {
                PFlog("typeswitch removed: ``%s'' is subtype of ``%s''",
                      PFty_str(c->child[0]->type), PFty_str(cast_type));
                *c = *(c->child[1]->child[0]->child[1]);
            }
            else if (PFty_disjoint (c->child[0]->type, cast_type))
            {
                PFlog("typeswitch removed: ``%s'' is disjoint from ``%s''",
                      PFty_str(c->child[0]->type), PFty_str(cast_type));
                *c = *(c->child[2]);
            }
            else
                PFoops (OOPS_TYPECHECK,
                        "couldn't solve typeswitch at compile time;"
                        " don't know if '%s' is subtype of '%s'",
                        PFty_str(c->child[0]->type), PFty_str(cast_type));
        
            simplifyCoreTree (c);
            break;
        case c_apply:
            /* handle the promotable types explicitly by casting them */
            fun = c->sem.fun;
            if (fun->arity == 1)
                simplifyCoreTree (c->child[0]->child[0]);

            if ((!PFqname_eq(fun->qname,PFqname (PFns_fn,"boolean")) || 
                 !PFqname_eq(fun->qname,PFqname (PFns_pf,"item-sequence-to-node-sequence"))) &&
                PFty_subtype(c->child[0]->type, fun->ret_ty))
            {
                /* don't use function - omit apply and arg node */
                *c = *(c->child[0]->child[0]);
                simplifyCoreTree (c);
            }
            else if (!PFqname_eq(fun->qname,PFqname (PFns_pf,"item-sequence-to-untypedAtomic")) &&
                     (PFty_subtype (c->child[0]->type, fun->ret_ty) ||
                      PFty_subtype (c->child[0]->type, PFty_string ())))
            {
                /* don't use function - omit apply and arg node */
                *c = *(c->child[0]->child[0]);
                simplifyCoreTree (c);
            }
            else if (!PFqname_eq(fun->qname,PFqname (PFns_pf,"distinct-doc-order")) &&
                     PFty_subtype (c->child[0]->type, PFty_opt(PFty_node ())))
            {
                /* don't use function - omit apply and arg node */
                *c = *(c->child[0]->child[0]);
                simplifyCoreTree (c);
            }
            else if (!PFqname_eq(fun->qname,PFqname (PFns_fn,"empty")) &&
                     c->child[0]->child[0]->kind == c_ifthenelse &&
                     PFty_subtype (c->child[0]->child[0]->child[1]->type,
                                   PFty_atomic ()) &&
                     PFty_subtype (c->child[0]->child[0]->child[2]->type,
                                   PFty_empty ()))
            {
                c->sem.fun = PFcore_function (PFqname (PFns_fn, "not"));
                c->child[0]->child[0] = c->child[0]->child[0]->child[0];
                c->child[0]->type = c->child[0]->child[0]->type;
                simplifyCoreTree (c);
            }
            else if (!PFqname_eq(fun->qname,PFqname (PFns_fn,"empty")) &&
                     c->child[0]->child[0]->kind == c_ifthenelse &&
                     PFty_subtype (c->child[0]->child[0]->child[1]->type,
                                   PFty_empty ()) &&
                     PFty_subtype (c->child[0]->child[0]->child[2]->type,
                                   PFty_atomic ()))
            {
                *c = *(c->child[0]->child[0]->child[0]);
                simplifyCoreTree (c);
            }
            else
            {
                c = c->child[0];
                for (i = 0; i < fun->arity; i++, c = c->child[1])
                {
                    expected = (fun->par_ty)[i];
 
                    if (expected.type == ty_opt ||
                        expected.type == ty_star ||
                        expected.type == ty_plus)
                        opt_expected = PFty_child (expected);
                    else
                        opt_expected = expected;
 
                    if (PFty_subtype (opt_expected, PFty_atomic ()) &&
                        !PFty_eq (c->child[0]->type, expected))
                    {
                        c->child[0] = PFcore_seqcast (PFcore_seqtype (expected),
                                                      c->child[0]);
                        /* type new code, to avoid multiple casts */
                        c->type                     =
                        c->child[0]->type           =
                        c->child[0]->child[0]->type = expected;
                        simplifyCoreTree (c->child[0]);
                    }
                }
            }
            break;
        case c_ifthenelse:
            simplifyCoreTree (c->child[0]);
            simplifyCoreTree (c->child[1]);
            simplifyCoreTree (c->child[2]);

            if (c->child[0]->kind == c_apply &&
                !PFqname_eq(c->child[0]->sem.fun->qname,
                            PFqname (PFns_fn,"not")))
            {
                c->child[0] = c->child[0]->child[0]->child[0];
                new_node = c->child[2];
                c->child[2] = c->child[1];
                c->child[1] = new_node;
            }
            break;
        case c_text:
            simplifyCoreTree (c->child[0]);
            /* substitutes empty text nodes by empty nodes */
            if (c->child[0]->kind == c_empty)
                *c = *(PFcore_empty ());
            break;
        default: 
            for (i = 0; i < PFCNODE_MAXCHILD && c->child[i]; i++)
                simplifyCoreTree (c->child[i]);
            break;
    }
}

/* the fid increasing for every for node */ 
#define FID 0
/* the actual fid saves the last active fid, to prune the 
   scopes of the later used variables */
#define ACT_FID 1
/* the vid increasing for every new variable binding */
#define VID 2

/**
 * in update_expansion for a variable usage all fids between
 * the definition of the variable and its usage are added
 * to the var_usage bat
 *
 * @param f the Stream the MIL code is printed to
 * @param c the variable core node
 * @param way the path of the for ids (active for-expression)
 */
static void
update_expansion (FILE *f, PFcnode_t *c,  PFarray_t *way)
{
    int m;
    PFvar_t *var;

    assert(c->sem.var);
    var = c->sem.var;

    for (m = PFarray_last (way) - 1; m >= 0 
         && *(int *) PFarray_at (way, m) > var->base; m--)
    {
        fprintf(f,
                "var_usage.insert(%i@0,%i@0);\n",
                var->vid,
                *(int *) PFarray_at (way, m));
    }
}

/**
 * in append_lev for each variable a vid (variable id) and
 * for each for expression a fid (for id) is added;
 * for each variable usage the needed fids are added to a 
 * bat 'var_usage'
 *
 * @param f the Stream the MIL code is printed to
 * @param c a core tree node
 * @param way an array containing the path of the for ids
 *        (active for-expression)
 * @param counter an array containing 3 values for fid, act_fid and vid
 * @return the updated counter
 */
static PFarray_t *
append_lev (FILE *f, PFcnode_t *c,  PFarray_t *way, PFarray_t *counter)
{
    unsigned int i;
    int fid, act_fid, vid;

    if (c->kind == c_var) 
    {
       /* inserts fid|vid combinations into var_usage bat */
       update_expansion (f, c, way);
       /* the field used is for pruning the MIL code and
          avoid translation of variables which are later
          not used */
       c->sem.var->used += 1;
    }

    /* only in for and let variables can be bound */
    else if (c->kind == c_for)
    {
       if (c->child[2])
           counter = append_lev (f, c->child[2], way, counter);
       
       (*(int *) PFarray_at (counter, FID))++;
       fid = *(int *) PFarray_at (counter, FID);

       c->sem.num = fid;
       *(int *) PFarray_add (way) = fid;
       act_fid = fid;

       vid = *(int *) PFarray_at (counter, VID);
       c->child[0]->sem.var->base = act_fid;
       c->child[0]->sem.var->vid = vid;
       c->child[0]->sem.var->used = 0;
       (*(int *) PFarray_at (counter, VID))++;

       if (c->child[1]->kind == c_var)
       {
            vid = *(int *) PFarray_at (counter, VID);
            c->child[1]->sem.var->base = act_fid;
            c->child[1]->sem.var->vid = vid;
            c->child[1]->sem.var->used = 0;
            (*(int *) PFarray_at (counter, VID))++;
       }

       if (c->child[3])
           counter = append_lev (f, c->child[3], way, counter);
       
       *(int *) PFarray_at (counter, ACT_FID) = *(int *) PFarray_top (way);
       PFarray_del (way);
    }

    else if (c->kind == c_let)
    {
       if (c->child[1])
           counter = append_lev (f, c->child[1], way, counter);

       act_fid = *(int *) PFarray_at (counter, ACT_FID);
       vid = *(int *) PFarray_at (counter, VID);
       c->child[0]->sem.var->base = act_fid;
       c->child[0]->sem.var->vid = vid;
       c->child[0]->sem.var->used = 0;
       (*(int *) PFarray_at (counter, VID))++;

       if (c->child[2])
           counter = append_lev (f, c->child[2], way, counter);
    }

    else 
    {
       for (i = 0; i < PFCNODE_MAXCHILD && c->child[i]; i++)
          counter = append_lev (f, c->child[i], way, counter);
    } 

    return counter;
}

/**
 * first MIL generation from Pathfinder Core
 *
 * first to each `for' and `var' node additional
 * information is appended. With this information
 * the core tree is translated into MIL.
 *
 * @param f the Stream the MIL code is printed to
 * @param c the root of the core tree
 */
void
PFprintMILtemp (FILE *f, PFcnode_t *c)
{
    PFarray_t *way, *counter;

    way = PFarray (sizeof (int));
    counter = PFarray (sizeof (int));
    *(int *) PFarray_add (counter) = 0; 
    *(int *) PFarray_add (counter) = 0; 
    *(int *) PFarray_add (counter) = 0; 

    /* resolves nodes, which are not supported and prunes 
       code which is node needed (e.g. casts, let-bindings) */
    simplifyCoreTree (c);

#define TIMINGS 1
#if TIMINGS
    fprintf(f,
            "module(alarm);\n"
            "var tries := 3;\n"
            "var rep := 0;\n"
            "while (rep < tries) {\n"
            "var timer := time();\n"
            "rep := rep+1;\n");
#endif

    /* some bats and module get initialized, variables get bound */
    init (f);

    /* append_lev appends information to the core nodes and
       creates a var_usage table, which is later split in vu_fid
       and vu_vid */
    fprintf(f,
            "{\n"
            "var var_usage := bat(oid,oid);\n"); /* [vid, fid] */
    append_lev (f, c, way, counter);
    /* the contents of var_usage will be sorted by fid and
       then refined (sorted) by vid */
    fprintf(f,
            "var_usage := var_usage.unique().reverse();\n"
            "var_usage.access(BAT_READ);\n"
            "vu_fid := var_usage.mark(1000@0).reverse();\n"
            "vu_vid := var_usage.reverse().mark(1000@0).reverse();\n"
            "var_usage := nil_oid_oid;\n"
            "var sorting := vu_fid.reverse().sort().reverse();\n"
            "sorting := sorting.CTrefine(vu_vid);\n"
            "sorting := sorting.mark(1000@0).reverse();\n"
            "vu_vid := sorting.leftfetchjoin(vu_vid);\n"
            "vu_fid := sorting.leftfetchjoin(vu_fid);\n"
            "sorting := nil_oid_oid;\n"
            "}\n");


    /* recursive translation of the core tree */
    translate2MIL (f, 0, 0, c);

#if TIMINGS
    fprintf(f,
            "fprintf(stderr(),\"### time for run %%i: %%i msec\\n\", rep, time() - timer);\n"
            "timer := nil_int;\n"
            "if (rep = tries)\n"
            "{\n");

/* fprintf(f, "print_result(\"xml\",ws,item,kind,int_values,dbl_values,dec_values,str_values);\n"); */

    print_output (f);

    fprintf(f,
            "}\n"
            "}\n"
            "rep := nil; # nil_int is not declared here\n"
            "tries := nil; # nil_int is not declared here\n");
#else
    /* print result in iter|pos|item representation */
    print_output (f);
#endif


    fprintf(f, "print(\"mil-programm without crash finished :)\");\n");

}
/* vim:set shiftwidth=4 expandtab: */
