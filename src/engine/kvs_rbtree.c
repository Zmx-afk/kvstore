


#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kvs_rbtree.h"
#include "../utils/Mm_pool.h"

rbtree_node *rbtree_mini(rbtree *T, rbtree_node *x) {
	while (x->left != T->nil) {
		x = x->left;
	}
	return x;
}

rbtree_node *rbtree_maxi(rbtree *T, rbtree_node *x) {
	while (x->right != T->nil) {
		x = x->right;
	}
	return x;
}

rbtree_node *rbtree_successor(rbtree *T, rbtree_node *x) {
	rbtree_node *y = x->parent;

	if (x->right != T->nil) {
		return rbtree_mini(T, x->right);
	}

	while ((y != T->nil) && (x == y->right)) {
		x = y;
		y = y->parent;
	}
	return y;
}


void rbtree_left_rotate(rbtree *T, rbtree_node *x) {

	rbtree_node *y = x->right;  // x  --> y  ,  y --> x,   right --> left,  left --> right

	x->right = y->left; //1 1
	if (y->left != T->nil) { //1 2
		y->left->parent = x;
	}

	y->parent = x->parent; //1 3
	if (x->parent == T->nil) { //1 4
		T->root = y;
	} else if (x == x->parent->left) {
		x->parent->left = y;
	} else {
		x->parent->right = y;
	}

	y->left = x; //1 5
	x->parent = y; //1 6
}


void rbtree_right_rotate(rbtree *T, rbtree_node *y) {

	rbtree_node *x = y->left;

	y->left = x->right;
	if (x->right != T->nil) {
		x->right->parent = y;
	}

	x->parent = y->parent;
	if (y->parent == T->nil) {
		T->root = x;
	} else if (y == y->parent->right) {
		y->parent->right = x;
	} else {
		y->parent->left = x;
	}

	x->right = y;
	y->parent = x;
}

void rbtree_insert_fixup(rbtree *T, rbtree_node *z) {

	while (z->parent->color == RED) { //z ---> RED
		if (z->parent == z->parent->parent->left) {
			rbtree_node *y = z->parent->parent->right;
			if (y->color == RED) {
				z->parent->color = BLACK;
				y->color = BLACK;
				z->parent->parent->color = RED;

				z = z->parent->parent; //z --> RED
			} else {

				if (z == z->parent->right) {
					z = z->parent;
					rbtree_left_rotate(T, z);
				}

				z->parent->color = BLACK;
				z->parent->parent->color = RED;
				rbtree_right_rotate(T, z->parent->parent);
			}
		}else {
			rbtree_node *y = z->parent->parent->left;
			if (y->color == RED) {
				z->parent->color = BLACK;
				y->color = BLACK;
				z->parent->parent->color = RED;

				z = z->parent->parent; //z --> RED
			} else {
				if (z == z->parent->left) {
					z = z->parent;
					rbtree_right_rotate(T, z);
				}

				z->parent->color = BLACK;
				z->parent->parent->color = RED;
				rbtree_left_rotate(T, z->parent->parent);
			}
		}
		
	}

	T->root->color = BLACK;
}


void rbtree_insert(rbtree *T, rbtree_node *z) {

	rbtree_node *y = T->nil;
	rbtree_node *x = T->root;

	while (x != T->nil) {
		y = x;
		int cmp = memcmp(z->key.data, x->key.data, z->key.len);
		if (cmp < 0) //z的key比x的key小
		{
			x = x->left;
		} 
		else if (cmp>0) 
		{
			x = x->right;
		} else //重复key节点
		{
			return ;
		}

	}

	z->parent = y;
	//判断树是否是空树
	if (y == T->nil) 
	{
		T->root = z;
	} 
	else if (memcmp(z->key.data, y->key.data,z->key.len) < 0) 
	{
		y->left = z;
	}
	else 
	{
		y->right = z;	
	}

	z->left = T->nil;
	z->right = T->nil;
	z->color = RED;

	rbtree_insert_fixup(T, z);
}

void rbtree_delete_fixup(rbtree *T, rbtree_node *x) {

	while ((x != T->root) && (x->color == BLACK)) {
		if (x == x->parent->left) {

			rbtree_node *w= x->parent->right;
			if (w->color == RED) {
				w->color = BLACK;
				x->parent->color = RED;

				rbtree_left_rotate(T, x->parent);
				w = x->parent->right;
			}

			if ((w->left->color == BLACK) && (w->right->color == BLACK)) {
				w->color = RED;
				x = x->parent;
			} else {

				if (w->right->color == BLACK) {
					w->left->color = BLACK;
					w->color = RED;
					rbtree_right_rotate(T, w);
					w = x->parent->right;
				}

				w->color = x->parent->color;
				x->parent->color = BLACK;
				w->right->color = BLACK;
				rbtree_left_rotate(T, x->parent);

				x = T->root;
			}

		} else {

			rbtree_node *w = x->parent->left;
			if (w->color == RED) {
				w->color = BLACK;
				x->parent->color = RED;
				rbtree_right_rotate(T, x->parent);
				w = x->parent->left;
			}

			if ((w->left->color == BLACK) && (w->right->color == BLACK)) {
				w->color = RED;
				x = x->parent;
			} else {

				if (w->left->color == BLACK) {
					w->right->color = BLACK;
					w->color = RED;
					rbtree_left_rotate(T, w);
					w = x->parent->left;
				}

				w->color = x->parent->color;
				x->parent->color = BLACK;
				w->left->color = BLACK;
				rbtree_right_rotate(T, x->parent);

				x = T->root;
			}

		}
	}

	x->color = BLACK;
}

rbtree_node *rbtree_delete(rbtree *T, rbtree_node *z) {

	rbtree_node *y = T->nil;
	rbtree_node *x = T->nil;

	if ((z->left == T->nil) || (z->right == T->nil)) {
		y = z;
	} else {
		y = rbtree_successor(T, z);
	}

	if (y->left != T->nil) {
		x = y->left;
	} else if (y->right != T->nil) {
		x = y->right;
	}

	x->parent = y->parent;
	if (y->parent == T->nil) {
		T->root = x;
	} else if (y == y->parent->left) {
		y->parent->left = x;
	} else {
		y->parent->right = x;
	}

	if (y != z) {
		z->key = y->key;
		z->value = y->value;
	}

	if (y->color == BLACK) {
		rbtree_delete_fixup(T, x);
	}

	return y;
}

rbtree_node *rbtree_search(rbtree *T, kvs_blob_t *key) {
    rbtree_node *node = T->root;
    while (node != T->nil) {
        int cmp;
        // 1. 先比长度（长度不同，直接分出大小）
        if (key->len != node->key.len) {
            cmp = (key->len > node->key.len) ? 1 : -1;
        } else {
            // 2. 长度相同，再比内容
            cmp = memcmp(key->data, node->key.data, key->len);
        }

        if (cmp < 0) {
            node = node->left;
        } else if (cmp > 0) {
            node = node->right;
        } else {
            return node; // 长度相同 且 内容相同，才是真的找到
        }
    }
    return T->nil;
}

void rbtree_traversal(rbtree *T, rbtree_node *node) {
	if (node != T->nil) {
		rbtree_traversal(T, node->left);

		printf("key:%.*s, value:%.*s, color:%d\n",
			node->key.len, (char*)node->key.data,  // 打印二进制key
			(int)strlen((char*)node->value.data), (char*)node->value.data,  // 打印value
			node->color); 
		rbtree_traversal(T, node->right);
	}
}



typedef struct _rbtree kvs_rbtree_t; 


// 5 + 2
kvs_rbtree_t* kvs_rbtree_create(void) 
{
	/*
		create a new kvs_rbtree_t
	*/
	kvs_rbtree_t *inst = malloc(sizeof(kvs_rbtree_t));
	if(!inst) return NULL;
	memset(inst,0,sizeof(kvs_rbtree_t));

	/*
		create mm_pool
	*/
	inst->pool = (mp_pool_t*)malloc(mm_pool_size());
	if(!inst->pool)
	{
		free(inst);
		return NULL;
	}
	
	if(mp_create(inst->pool, 4096)!=0)
	{
		free(inst->pool);
		free(inst);
		return NULL;
	}

	inst->nil = (rbtree_node*)kvs_malloc(inst->pool,sizeof(rbtree_node));
	if (!inst->nil) {
		mp_destory(inst->pool);
		free(inst->pool);
		free(inst);
		return NULL;
	}

	//初始化nil节点
	inst->nil->color = BLACK;
	inst->root = inst->nil;


	printf("rbtree实例建立完毕\n");
	return inst;

}

void kvs_rbtree_destory(kvs_rbtree_t *inst) {
	if (inst == NULL)
		return;

	// 先销毁内存池（会释放 nil 节点 + 所有树节点）
	if (inst->pool != NULL) {
		mp_destory(inst->pool);
		free(inst->pool);
	}

	// 最后释放树结构体本身
	free(inst);
}


int kvs_rbtree_set(kvs_rbtree_t *inst,kvs_blob_t *key, kvs_blob_t *value) {

	if (!inst || !key || !value) return -1;

	if(key->len <=0 || value->len <=0) 
	{
		printf("长度不对\n");
		printf("keylen:%d,valuelen:%d\n",key->len,value->len);
		return -1; 
	}	
	rbtree_node *node = (rbtree_node*)kvs_malloc(inst->pool,sizeof(rbtree_node));
		
	node->key.data = kvs_malloc(inst->pool,key->len);
	if (!node->key.data) return -2;
	node->key.len = key->len;
	memcpy(node->key.data,key->data,key->len);


	node->value.data = kvs_malloc(inst->pool,value->len);
	if (!node->value.data) return -2;
	node->value.len =value->len;
	memcpy(node->value.data,value->data,value->len);


	rbtree_insert(inst, node);

	return 0;
}


kvs_blob_t* kvs_rbtree_get(kvs_rbtree_t *inst, kvs_blob_t *key)  {

	if (!inst || !key) return NULL;
	rbtree_node *node = rbtree_search(inst, key);
	if (!node) return NULL; // no exist
	if (node == inst->nil) return NULL;

	return (kvs_blob_t*)&node->value;
	
}

int kvs_rbtree_del(kvs_rbtree_t *inst,kvs_blob_t *key) {

	if (!inst || !key) return -1;

	rbtree_node *node = rbtree_search(inst, key);
	if (node == inst->nil) return 1; // no exist
	
	rbtree_delete(inst, node);
	//free(cur);

	return 0;
}

int kvs_rbtree_mod(kvs_rbtree_t *inst, kvs_blob_t *key, kvs_blob_t *value) {

	if (!inst || !key || !value) return -1;

    rbtree_node *node = rbtree_search(inst, key);
    if (!node || node == inst->nil) return 1; // 不存在

    // 1. 分配新内存（防止分配失败时旧数据已丢失）
    void *new_data = kvs_malloc(inst->pool, value->len);
    if (!new_data) return -2;

    // 2. 拷贝新数据
    memcpy(new_data, value->data, value->len);

    // 3. 释放旧内存
    kvs_free(inst->pool, node->value.data);

    // 4. 更新指针和长度
    node->value.data = new_data;
    node->value.len = value->len; 

    return 0;

}

int kvs_rbtree_exist(kvs_rbtree_t *inst,kvs_blob_t *key) {

	if (!inst || !key) return -1;

	rbtree_node *node = rbtree_search(inst, key);
	if (!node) return 1; // no exist
	if (node == inst->nil) return 1;

	return 0;
}


