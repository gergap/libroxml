#ifndef ROXML_MEMORY_H
#define ROXML_MEMORY_H

#include <roxml_internal.h>

memory_cell_t head_cell;

/** \brief alloc memory function
 *
 * \fn roxml_malloc(int size, int num, int type)
 * this function allocate some memory that will be reachable at
 * any time by libroxml memory manager
 * \param size the size of memory to allocate for each elem
 * \param num the number of element
 * \param type the kind of pointer
 */
ROXML_INT void *roxml_malloc(int size, int num, int type);

#endif /* ROXML_MEMORY_H */
