#include <libdank/objects/logctx.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/intervaltree.h>

typedef struct interval_tree {
	struct interval_tree *l,*r;
	interval ival;
	void *data;
} interval_tree;

static interval_tree *
create_interval_tree_node(const interval *ival,void *v){
	interval_tree *ret;

	if( (ret = Malloc("interval tree node",sizeof(*ret))) ){
		ret->l = ret->r = NULL;
		ret->ival = *ival;
		ret->data = v;
	}
	return ret;
}

// Tests for distinctness of intervals (non-overlapping, *not* mere inequality)
static inline int
intervals_distinct(const interval *i1,const interval *i2){
	// & vs && is an optimization to avoid branching, not an error!
	return (i1->ubound < i2->lbound) & (i2->ubound < i1->lbound);
}

static inline int
intervals_equal(const interval *i1,const interval *i2){
	return (i1->lbound == i2->lbound) & (i1->ubound == i2->ubound);
}

int insert_interval_tree(interval_tree **it,const interval *ival,void *v){
	if(*it){
		if(ival->ubound < (*it)->ival.lbound){
			return insert_interval_tree(&(*it)->l,ival,v);
		}else if((*it)->ival.ubound < ival->lbound){
			return insert_interval_tree(&(*it)->r,ival,v);
		}else{
			bitch("Supplied overlapping intervals\n");
			return -1;
		}
	}else{
		if((*it = create_interval_tree_node(ival,v)) == NULL){
			return -1;
		}
	}
	return 0;
}

int replace_interval_tree(interval_tree **it,const interval *ival,void *v,
				void(*freefxn)(void *)){
	if(*it){
		if(ival->ubound < (*it)->ival.lbound){
			return replace_interval_tree(&(*it)->l,ival,v,freefxn);
		}else if((*it)->ival.ubound < ival->lbound){
			return replace_interval_tree(&(*it)->r,ival,v,freefxn);
		}else if(intervals_equal(&(*it)->ival,ival)){
			if(freefxn){
				freefxn((*it)->data);
			}
			(*it)->data = v;
		}else{
			bitch("Supplied overlapping intervals\n");
			return -1;
		}
	}else{
		if((*it = create_interval_tree_node(ival,v)) == NULL){
			return -1;
		}
	}
	return 0;
}

static void
splice_out_node(interval_tree **it){
	interval_tree *l,*r;

	l = (*it)->l;
	r = (*it)->r;
	Free(*it);
	*it = r;
	if(l){
		if(r){
			while(r->l){
				r = r->l;
			}
			r->l = l;
		}else{
			*it = l;
		}
	}
}

int remove_interval_tree(interval_tree **it,const interval *ival,
				void (*freefxn)(void *)){
	if(*it){
		if(ival->ubound < (*it)->ival.lbound){
			return remove_interval_tree(&(*it)->l,ival,freefxn);
		}else if((*it)->ival.ubound < ival->lbound){
			return remove_interval_tree(&(*it)->r,ival,freefxn);
		}else if(intervals_equal(&(*it)->ival,ival)){
			if(freefxn){
				freefxn((*it)->data);
			}
			splice_out_node(it);
			return 0;
		}else{
			bitch("Supplied overlapping intervals\n");
		}
	}
	return -1;
}

int extract_interval_tree(interval_tree **it,const interval *ival,void **v){
	if(*it){
		if(ival->ubound < (*it)->ival.lbound){
			return extract_interval_tree(&(*it)->l,ival,v);
		}else if((*it)->ival.ubound < ival->lbound){
			return extract_interval_tree(&(*it)->r,ival,v);
		}else if(intervals_equal(&(*it)->ival,ival)){
			if(v){
				*v = (*it)->data;
			}
			splice_out_node(it);
			return 0;
		}else{
			bitch("Supplied overlapping intervals\n");
		}
	}
	return -1;
}

static inline int
key_in_interval(const interval *ival,uint32_t key){
	// & vs && is an optimization to avoid branching, not an error!
	return (key >= ival->lbound) & (key <= ival->ubound);
}

void *lookup_interval_tree(const interval_tree *it,uint32_t key){
	if(it){
		if(key_in_interval(&it->ival,key)){
			return it->data;
		}else if(key < it->ival.lbound){
			return lookup_interval_tree(it->l,key);
		}else{
			return lookup_interval_tree(it->r,key);
		}
	}
	return NULL;
}

unsigned depth_interval_tree(const interval_tree *it){
	if(it){
		unsigned lm,rm;

		lm = depth_interval_tree(it->l);
		rm = depth_interval_tree(it->r);
		return (lm > rm ? lm : rm) + 1;
	}
	return 0;
}

unsigned population_interval_tree(const interval_tree *it){
	if(it){
		return population_interval_tree(it->l) +
			population_interval_tree(it->r) + 1;
	}
	return 0;
}

// balancing helper -- linearize the interval tree into a sorted linked list
// (->l will be NULL for all nodes; ->r will be non-NULL for all but one (the
// last) node). Takes a pointer to an interval tree, which will be reset to the
// head of the list. Returns a pointer to the end of the list's (non-NULL) ->r.
static interval_tree **
linearize_interval_tree(interval_tree **it){
	interval_tree **join,*cur;

	if((cur = *it) == NULL){
		return NULL;
	}
	if( (join = linearize_interval_tree(&cur->l)) ){
		*it = cur->l;
		cur->l = NULL;
		*join = cur;
	}
	if((join = linearize_interval_tree(&cur->r)) == NULL){
		join = &cur->r;
	}
	return join;
}

static interval_tree **
vectorize_list(interval_tree *it,unsigned count){
	interval_tree **ret;

	if( (ret = Malloc("tree vector",sizeof(*ret) * count)) ){
		unsigned i;

		for(i = 0 ; i < count ; ++i){
			ret[i] = it;
			it = it->r;
		}
	}
	return ret;
}

static interval_tree *
bbstize_vector(interval_tree **it,unsigned count){
	interval_tree *ret;

	if(count == 0){
		return NULL;
	}
	ret = it[count / 2];
	ret->l = bbstize_vector(it,count / 2);
	ret->r = bbstize_vector(it + (count / 2) + 1,count - (count / 2) - 1);
	return ret;
}

void balance_interval_tree(interval_tree **it){
	interval_tree **tmp;
	unsigned depth,upop;
	float pop;

	pop = upop = population_interval_tree(*it);
	depth = depth_interval_tree(*it);
	nag("Current depth: %u (%2.2f%% of N)\n",depth,depth / pop * 100);
	linearize_interval_tree(it);
	depth = depth_interval_tree(*it);
	nag("Current depth: %u (%2.2f%% of N)\n",depth,depth / pop * 100);
	// FIXME if this allocation fails, we leave the tree in its pessimal
	// form! surely we can rebalance in O(1) state...
	if( (tmp = vectorize_list(*it,upop)) ){
		*it = bbstize_vector(tmp,upop);
		Free(tmp);
		depth = depth_interval_tree(*it);
		nag("Current depth: %u (%2.2f%% of N)\n",depth,depth / pop * 100);
	}
}

void free_interval_tree(interval_tree **it,void (*freefxn)(void *)){
	if(*it){
		if(freefxn){
			freefxn((*it)->data);
		}
		free_interval_tree(&(*it)->l,freefxn);
		free_interval_tree(&(*it)->r,freefxn);
		Free(*it);
		*it = NULL;
	}
}
