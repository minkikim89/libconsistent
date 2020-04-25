#include <stdio.h>
#include "libconsistent.h"

int main(int argc, char *argv[])
{
    /* consistent hashing */
    cons_hash *cons = chash_create();
    chash_add_server(cons, "1.1.1.1:1111");
    server_info *chash_server = chash_get_server(cons, "test_key");
    printf("name %s\n", chash_server->name);
    chash_del_server(cons, chash_server);

    /* ketama consistent hashing */
    ketama_hash *ketama = ketama_create();
    ketama_add_server(ketama, "2.2.2.2:2222");
    server_info *ketama_server = ketama_get_server(ketama, "test_key");
    printf("name %s\n", ketama_server->name);
    ketama_del_server(ketama, ketama_server);

    /* jump consistent hashing */
    jump_hash *jump = jump_create();
    jump_add_server(jump, "3.3.3.3:3333");
    server_info *jump_server = jump_get_server(jump, "test_key");
    printf("name %s\n", jump_server->name);

    return 0;
}
