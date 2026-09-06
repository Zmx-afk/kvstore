#ifndef PERSISTENCE__H
#define PERSISTENCE__H

#include "../engine/kvs_rbtree.h"
#include "../engine/kvs_array.h"
#include "../engine/kvs_hash.h"
#include "../utils/log.h"


int RDB_sync();
int RDB_async();

int RDB_load(kvs_array_t *inst);

#endif