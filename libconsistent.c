#include "libconsistent.h"
#include "md5.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct _continuum {
    uint32_t   hash;
    void      *node;
} continuum;

/* consistent hashing */
typedef struct _chash_node {
    continuum           cont;
    server_info        *srvr;
    struct _chash_node *prev;
    struct _chash_node *next;
} chash_node;

typedef struct _cons_hash {
    continuum  **continuum;
    int          contcount;
    int          arraysize;
    void        *node_head;
    int          nodecount;
} cons_hash;

/* ketama consistent hashing */
#define KETAMA_CONT_NUM 160
typedef struct _ketama_node {
    continuum            cont[KETAMA_CONT_NUM];
    server_info         *srvr;
    struct _ketama_node *prev;
    struct _ketama_node *next;
} ketama_node;

typedef struct _ketama_hash {
    continuum  **continuum;
    int          contcount;
    int          arraysize;
    void        *node_head;
    int          nodecount;
} ketama_hash;

/* jump consistent hashing */
typedef struct _jump_node {
    server_info       *srvr;
    uint64_t           shard_id;
} jump_node;

typedef struct _jump_hash {
    jump_node    *buckets;
    int           num_buckets;
    int           arraysize;
} jump_hash;

static void
hash_md5(const char *key, unsigned int keylen, unsigned char result[16])
{
    MD5 md5;
    MD5Open(&md5);
    MD5Digest(&md5, key, keylen);
    MD5Close(&md5, result);
}

static uint32_t
hash_uint32(const char *namestr, unsigned int namelen)
{
    unsigned char digest[16];
    hash_md5(namestr, namelen, digest);
    uint64_t hash = digest[3] << 24 |
                    digest[2] << 16 |
                    digest[1] << 8  |
                    digest[0];
   return hash;
}

static uint64_t
hash_uint64(const char *namestr, unsigned int namelen)
{
    unsigned char digest[16];
    hash_md5(namestr, namelen, digest);
    uint64_t hash = (uint64_t)digest[7] << 56 |
                    (uint64_t)digest[6] << 48 |
                    (uint64_t)digest[5] << 40 |
                    (uint64_t)digest[4] << 32 |
                    digest[3] << 24 |
                    digest[2] << 16 |
                    digest[1] << 8  |
                    digest[0];
   return hash;
}

server_info*
server_create(const char *name)
{
    server_info *srvr = malloc(sizeof(server_info));
    strcpy(srvr->name, name);
    return srvr;
}

void
server_delete(server_info *srvr)
{
    free(srvr);
}

static int
continuum_search(continuum **conts, int contcount, uint64_t hash)
{
    int mid, left, right;

    left  = 0;
    right = contcount-1;

    //TODO collision case..
    while (left <= right) {
        mid = (left+right)/2;
        if (conts[mid]->hash < hash) {
            left = mid+1;
        } else if (conts[mid]->hash > hash) {
            right = mid-1;
        } else {
            return mid;
        }
    }

    return -1;
}

static int
continuum_lookup(continuum **conts, int contcount, uint32_t hash)
{
    int mid, left, right;

    left  = 0;
    right = contcount-1;

    //TODO collision case..
    while (left <= right) {
        mid = (left+right)/2;
        if (conts[mid]->hash < hash) {
            left = mid+1;
        } else if (conts[mid]->hash > hash) {
            right = mid-1;
        } else {
            return mid;
        }
    }

    return left;
}

static void
continuum_insert(continuum **conts, int contcount, continuum *cont)
{
    int i;
    int target;

    target = continuum_lookup(conts, contcount, cont->hash);
    for (i = contcount; i > target; i--) {
        conts[i] = conts[i-1];
    }
    conts[i] = cont;
}

static void
continuum_delete(continuum **conts, int contcount, continuum *cont)
{
    int i, contid;

    contid = continuum_search(conts, contcount, cont->hash);
    for (i = contid; i < contcount-1; i++) {
        conts[i] = conts[i+1];
    }
}

static chash_node*
chash_node_create(const char *name)
{
    unsigned int namelen = strlen(name);
    if (namelen > MAX_NAME_LEN) {
        return NULL;
    }

    server_info *srvr = server_create(name);
    if (!srvr) {
        return NULL;
    }

    //TODO use free_list
    chash_node *node = malloc(sizeof(chash_node));
    if (!node) {
        free(srvr);
        return NULL;
    }
    memset(node, 0, sizeof(chash_node));
    node->srvr = srvr;
    node->cont.hash = hash_uint32(name, namelen);
    node->cont.node = node;
    return node;
}

static chash_node*
chash_node_search(cons_hash *cons, server_info *srvr)
{
    chash_node *node = cons->node_head;
    while (node) {
        if (node->srvr == srvr) {
            break;
        }
        node = node->next;
    }
    assert(node);
    return node;
}



static void
chash_node_delete(cons_hash *cons, chash_node *node)
{
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        assert(cons->node_head == node);
        cons->node_head = node->next;
    }
    if (node->next) {
        node->next->prev = node->prev;
    }
    server_delete(node->srvr);
    node->srvr = NULL;
}

static void
chash_node_insert(cons_hash *cons, chash_node *node)
{
    if (cons->node_head) {
        node->next = cons->node_head;
        ((chash_node*)cons->node_head)->prev = node;
    }
    cons->node_head = node;
}

/* chash consistent hashing */
cons_hash*
chash_create(void)
{
    cons_hash *cons = calloc(1, sizeof(cons_hash));
    if (!cons) {
        return NULL;
    }
    cons->arraysize = 16;
    cons->continuum = calloc(cons->arraysize, sizeof(continuum*));
    if (!cons->continuum) {
        free(cons);
        return NULL;
    }

    return cons;
}

server_info*
chash_add_server(cons_hash *consh, const char *name)
{
    chash_node *node;

    if (consh->nodecount >= consh->arraysize) {
        continuum **temp = consh->continuum;
        consh->continuum = realloc(consh->continuum,
                sizeof(continuum*)*(consh->arraysize*2));
        if (!consh->continuum) {
            consh->continuum = temp;
            return NULL;
        }
        consh->arraysize *= 2;
    }

    node = chash_node_create(name);
    if (!node) {
        return NULL;
    }
    continuum_insert(consh->continuum, consh->contcount, &node->cont);
    consh->contcount++;

    chash_node_insert(consh, node);
    consh->nodecount++;
    return node->srvr;
}

void
chash_del_server(cons_hash *consh, server_info *srvr)
{
    chash_node *node;

    node = chash_node_search(consh, srvr);

    continuum_delete(consh->continuum, consh->contcount, &node->cont);
    consh->contcount--;

    chash_node_delete(consh, node);
    consh->nodecount--;
}

server_info*
chash_get_server(cons_hash *consh, const char *key)
{
    uint32_t hash = hash_uint32(key, strlen(key));

    int i = continuum_lookup(consh->continuum, consh->contcount, hash);
    return ((chash_node*)consh->continuum[i]->node)->srvr;
}

void
chash_destroy(cons_hash *consh)
{
    chash_node *curr, *node;
    if (consh) {
        node = consh->node_head;
        while (node) {
            curr = node;
            node = node->next;
            free(curr->srvr);
            free(curr);
        }
        free(consh);
    }
}

static ketama_node*
ketama_node_create(const char *name)
{
    int i, j, length;
    uint32_t hash;
    unsigned char digest[16];
    char buf[MAX_NAME_LEN+1];

    unsigned int namelen = strlen(name);
    if (namelen > MAX_NAME_LEN) {
        return NULL;
    }

    server_info *srvr = server_create(name);
    if (!srvr) {
        return NULL;
    }

    //TODO use free_list
    ketama_node *node = malloc(sizeof(ketama_node));
    if (!node) {
        free(srvr);
        return NULL;
    }
    memset(node, 0, sizeof(ketama_node));
    for (i = 0; i < KETAMA_CONT_NUM; i++) {
        length = snprintf(buf, MAX_NAME_LEN, "%s-%d", name, i);
        hash_md5(buf, length, digest);
        for (j = 0; j < 4; j++) {
            hash = digest[3+j*4] << 24 |
                   digest[2+j*4] << 16|
                   digest[1+j*4] << 8|
                   digest[0+j*4];
        }
        node->cont[i].hash = hash;
        node->cont[i].node = node;
    }
    node->srvr = srvr;
    return node;
}

static ketama_node*
ketama_node_search(ketama_hash *ketamah, server_info *srvr)
{
    ketama_node *node = ketamah->node_head;
    while (node) {
        if (node->srvr == srvr) {
            break;
        }
        node = node->next;
    }
    assert(node);
    return node;
}

static void
ketama_node_delete(ketama_hash *ketamah, ketama_node *node)
{
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        assert(ketamah->node_head == node);
        ketamah->node_head = node->next;
    }
    if (node->next) {
        node->next->prev = node->prev;
    }
    server_delete(node->srvr);
    node->srvr = NULL;
}

static void
ketama_node_insert(ketama_hash *ketamah, ketama_node *node)
{
    if (ketamah->node_head) {
        node->next = ketamah->node_head;
        ((ketama_node*)ketamah->node_head)->prev = node;
    }
    ketamah->node_head = node;
}

ketama_hash*
ketama_create(void)
{
    ketama_hash *ketamah = calloc(1, sizeof(ketama_hash));
    if (!ketamah) {
        return NULL;
    }
    ketamah->arraysize = 16;
    ketamah->continuum = calloc(ketamah->arraysize, sizeof(continuum*));
    if (!ketamah->continuum) {
        free(ketamah);
        return NULL;
    }

    return ketamah;
}

server_info*
ketama_add_server(ketama_hash *ketamah, const char *name)
{
    int i;
    ketama_node *node;

    if (ketamah->nodecount >= ketamah->arraysize) {
        continuum **temp = ketamah->continuum;
        ketamah->continuum = realloc(ketamah->continuum,
                sizeof(continuum*)*(ketamah->arraysize*2));
        if (!ketamah->continuum) {
            ketamah->continuum = temp;
            return NULL;
        }
        ketamah->arraysize *= 2;
    }

    node = ketama_node_create(name);
    if (!node) {
        return NULL;
    }

    for (i = 0; i < KETAMA_CONT_NUM; i++) {
        continuum_insert(ketamah->continuum, ketamah->contcount, &node->cont[i]);
        ketamah->contcount++;
    }

    ketama_node_insert(ketamah, node);
    ketamah->nodecount++;
    return node->srvr;
}

void
ketama_del_server(ketama_hash *ketamah, server_info *srvr)
{
    int i;
    ketama_node *node;

    node = ketama_node_search(ketamah, srvr);

    for (i = 0; i < KETAMA_CONT_NUM; i++) {
        continuum_delete(ketamah->continuum, ketamah->contcount, &node->cont[i]);
        ketamah->contcount--;
    }

    ketama_node_delete(ketamah, node);
    ketamah->nodecount--;
}

server_info*
ketama_get_server(ketama_hash *ketamah, const char *key)
{
    uint32_t hash = hash_uint32(key, strlen(key));
    int i = continuum_lookup(ketamah->continuum, ketamah->contcount, hash);
    return ((ketama_node*)ketamah->continuum[i]->node)->srvr;
}

void
ketama_destroy(ketama_hash *ketamah)
{
    ketama_node *curr, *node;
    if (ketamah) {
        node = ketamah->node_head;
        while (node) {
            curr = node;
            node = node->next;
            free(curr->srvr);
            free(curr);
        }
        free(ketamah);
    }
}

static int32_t
hash_jump(uint64_t key, int32_t num_buckets) {
    int64_t b = -1, j = 0;
    while (j < num_buckets) {
        b = j;
        key = key * 2862933555777941757ULL + 1;
        j = (b + 1) * ((double)(1LL << 31) / (double)((key >> 33) + 1));
    }
    return b;
}

/* jump consistent hashing */
jump_hash*
jump_create(void)
{
    jump_hash *jumph = malloc(sizeof(jump_hash));
    if (!jumph) {
        return NULL;
    }
    jumph->num_buckets = 0;
    jumph->arraysize = 16;
    jumph->buckets = calloc(jumph->arraysize, sizeof(jump_node));
    if (!jumph->buckets) {
        free(jumph);
        return NULL;
    }

    return jumph;
}

server_info*
jump_add_server(jump_hash *jumph, const char *name)
{
    if (jumph->num_buckets >= jumph->arraysize) {
        jump_node *temp = jumph->buckets;
        jumph->buckets = realloc(jumph->buckets,
                sizeof(jump_node)*(jumph->arraysize*2));
        if (!jumph->buckets) {
            jumph->buckets = temp;
            return NULL;
        }
        jumph->arraysize *= 2;
    }

    int shard_id = jumph->num_buckets++;
    server_info *srvr = server_create(name);
    srvr->shard_id = shard_id;

    jump_node *node = &jumph->buckets[shard_id];
    node->srvr = srvr;
    node->shard_id = shard_id;

    return node->srvr;
}

server_info*
jump_get_server(jump_hash *jumph, const char *key)
{
    uint64_t hash_key = hash_uint64(key, strlen(key));
    uint32_t bucket_id = hash_jump(hash_key, jumph->num_buckets);

    jump_node *node = &jumph->buckets[bucket_id];

    return node->srvr;
}

void
jump_destroy(jump_hash *jumph)
{
    if (jumph) {
        int i;
        for (i = 0; i < jumph->num_buckets; i++) {
            free(jumph->buckets[i].srvr);
        }
        free(jumph->buckets);
        free(jumph);
    }
}
