#ifndef PERSISTENCE__H
#define PERSISTENCE__H

#define AOF_ALWAYS 1

#include "../engine/kvs_rbtree.h"
#include "../engine/kvs_array.h"
#include "../engine/kvs_hash.h"
#include "../utils/log.h"

int RDB();

int RDB_load(kvs_array_t *inst);

#endif