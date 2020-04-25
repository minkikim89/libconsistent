#ifndef LIBCONSISTENT_H
#define LIBCONSISTENT_H

#include <stdint.h>

#define MAX_NAME_LEN 256

typedef struct _server_info {
    char          name[MAX_NAME_LEN+1];
    int shard_id; //FIXME this is only for jump hashing
} server_info;

typedef struct _cons_hash cons_hash;
typedef struct _ketama_hash ketama_hash;
typedef struct _jump_hash jump_hash;

/* consistent hashing */
cons_hash   *chash_create(void);
server_info *chash_add_server(cons_hash *consh,
                              const char *name);
void         chash_del_server(cons_hash *consh,
                              server_info *srvr);
server_info* chash_get_server(cons_hash *consh,
                              const char *key);
void         chash_destroy(cons_hash *consh);

/* ketama consistent hashing */
ketama_hash *ketama_create(void);
server_info *ketama_add_server(ketama_hash *ketamah,
                               const char *name);
void         ketama_del_server(ketama_hash *ketamah,
                               server_info *srvr);
server_info *ketama_get_server(ketama_hash *ketamah,
                               const char *key);
void         ketama_destroy(ketama_hash *ketamah);

/* jump consistent hashing */
jump_hash    *jump_create(void);
server_info  *jump_add_server(jump_hash *jumph,
                              const char *name);
server_info  *jump_get_server(jump_hash *jumph,
                              const char *key);
void          jump_destroy(jump_hash *jumph);
#endif
