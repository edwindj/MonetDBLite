
#include "mem.h"
#include "scope.h"

static void destroy_vars( list *vars ){
	if (vars){
	  	node *n = vars->h;
	  	for( ; n; n = n->next ){
			var *v = (var*)n->data.sval;
			_DELETE(v->vname);
			_DELETE(n->data.sval);
	  	}
	  	list_destroy(vars);
	}
}

static void destroy_lifted( list *lifted ){
	if (lifted){
	  	node *n = lifted->h;
	  	for( ; n; n = n->next ){
			_DELETE(n->data.sval);
	  	}
		list_destroy( lifted);
	}
}

scope *scope_open( scope *p ){
	scope *s = NEW(scope);
	s->vars = list_create();
	s->lifted = list_create();
	s->p = p;
	s->nr = (p)?p->nr:0;
	return s;
}

scope *scope_close( scope *s ){
	scope *p = s->p;
	destroy_vars( s->vars );
	destroy_lifted( s->lifted );
	_DELETE(s);
	return p;
}

void scope_add_statement( scope *scp, statement *s, char *name ){
	var *v = NEW(var);
	v->type = type_statement;
	v->data.stval = s;
	v->vname = _strdup(name);
	v->nr = scp->nr++;
	list_append_string( scp->vars, (char*)v);
}

void scope_add_table( scope *scp, table *t, char *name ){
	var *v = NEW(var);
	v->type = type_table;
	v->data.tval = t;
	v->vname = _strdup(name);
	v->nr = scp->nr++;
	list_append_string( scp->vars, (char*)v);
}

static
void scope_lift( scope *s, column *c, var *v ){
	lifted *nw = NULL;
	node *n = s->lifted->h;
	for( ; n; n = n->next ){
		lifted *o = (lifted*)n->data.sval;
		if (strcmp( o->c->name, c->name ) == 0)
			return;
	}
	nw = NEW(lifted);
	nw->v = v; 
	nw->c = c;
	list_append_string(s->lifted, (char*)nw ); 
}

/* returns a list of vars */
list *scope_unique_lifted_vars( scope *s ){
	list *r = list_create();
	node *n = s->lifted->h;
	for( ; n; n = n->next ){
		node *m = r->h;
		int found = 0;
		for(; m && !found; m = m->next ){
			lifted *l = (lifted*)n->data.sval;
			if (l->v == (var*)m->data.sval)
				found = 1;
		}	
		if (!found){
			lifted *l = (lifted*)n->data.sval;
			list_append_string(r, (char*)l->v );
		}
	}
	return r;
}

var *scope_bind_table( scope *scp, char *name ){
	for( ; scp; scp = scp->p ){
		node *n = scp->vars->h; 
		for( ; n; n = n->next ){
			var *v = (var*)n->data.sval;
			if (v->type == type_table &&
				strcmp( v->vname, name) == 0){
				return v;	
			}
		}
	}
	return NULL;
}

static
column *bind_column( list *columns, char *name ){
	node *n = columns->h; 
	for( ; n; n = n->next ){
		column *c = n->data.cval;
		if (strcmp( c->name, name) == 0){
			return c;	
		}
	}
	return NULL;
}

column *scope_bind_column( scope *scp, char *name, var **b ){
	scope *start = scp;
	column *c = NULL;
	for( ; scp; scp = scp->p ){
	  node *n = scp->vars->h; 
	  for( ; n; n = n->next ){
	    var *v = (var*)n->data.sval;
	    if (v->type == type_table){
	      if ((c = bind_column(v->data.tval->columns, name)) != NULL){
		if (start != scp)
		  scope_lift(start, c, v );
		*b = v;
		return c;
	      }
	    }
	  }
	}
	return NULL;
}

column *scope_bind_table_column( scope *scp, char *tname, char *cname, var **b){
	scope *start = scp;
	column *c = NULL;
	for( ; scp; scp = scp->p ){
	  node *n = scp->vars->h; 
	  for( ; n; n = n->next ){
	    var *v = (var*)n->data.sval;
	    if (v->type == type_table){
	      if (strcmp(v->vname, tname) == 0)
	       if ((c = bind_column(v->data.tval->columns, cname)) != NULL){
		if (start != scp)
		  scope_lift(start, c, v );
		*b = v;
		return c;
	      }
	    }
	  }
	}
	return NULL;
}

statement *scope_bind_statement( scope *scp, char *name ){
	for( ; scp; scp = scp->p ){
	  node *n = scp->vars->h; 
	  for( ; n; n = n->next ){
	    var *v = (var*)n->data.sval;
	    if (v->type == type_statement){
	      if (strcmp(v->vname, name) == 0){
		return v->data.stval;
	      }
	    }
	  }
	}
	return NULL;
}

statement *scope_first_column( scope *scp ){
	for( ; scp; scp = scp->p ){
	  node *n = scp->vars->h; 
	  for( ; n; n = n->next ){
	    var *v = (var*)n->data.sval;
	    if (v->type == type_statement){
              return v->data.stval;
	    } else if (v->type == type_table){
	      return statement_column(v->data.tval->columns->h->data.cval,v);
	    }
	  }
	}
	return NULL;
}

/* first and only table */
var *scope_first_table( scope *scp ){
	for( ; scp; scp = scp->p ){
	  node *n = scp->vars->h; 
	  for( ; n; n = n->next ){
	    var *v = (var*)n->data.sval;
	    if (v->type == type_statement){
              return NULL;
	    } else if (v->type == type_table){
	      return v;
	    }
	  }
	}
	return NULL;
}

