#ifndef OBJECTS_INTERVALTREE
#define OBJECTS_INTERVALTREE

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct interval_tree;

typedef struct interval {
	uint32_t lbound,ubound;
} interval;

// All intervals are closed. State will be retained, and must be valid across
// the interval tree's life; the intervals passed will be copied and thus
// needn't persist past the function call. Overlapping intervals are not (yet)
// supported. An unordered tree is used; balance_interval_tree() may be used to
// explicitly request a balancing.
int insert_interval_tree(struct interval_tree **,const interval *,void *);
int replace_interval_tree(struct interval_tree **,const interval *,void *,
				void (*)(void *));
int extract_interval_tree(struct interval_tree **,const interval *,void **);
int remove_interval_tree(struct interval_tree **,const interval *,
				void (*)(void *));
void *lookup_interval_tree(const struct interval_tree *,uint32_t);
void free_interval_tree(struct interval_tree **,void (*)(void *));

// Perform an O(N) time, O(N) space balancing.
void balance_interval_tree(struct interval_tree **);

// Diagnostic functions -- query the maximum depth, or population
unsigned depth_interval_tree(const struct interval_tree *);
unsigned population_interval_tree(const struct interval_tree *);

#ifdef __cplusplus
}
#endif

#endif
