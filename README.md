# libconsistent
Implementation of consistent hashing in C

# Support consistent hashing algorithms
- consistent hashing
- ketama consistent hashing
- jump consistent hashing

# How to install

# Usage
### consistent hashing
```c
cons_hash *cons = chash_create();
chash_add_server(cons, "1.1.1.1:1111");
server_info *chash_server = chash_get_server(cons, "test_key");
chash_del_server(cons, chash_server);
```
### ketama consistent hashing
```c
ketama_hash *ketama = ketama_create();
ketama_add_server(ketama, "1.1.1.1:1111");
server_info *ketama_server = ketama_get_server(ketama, "test_key");
ketama_del_server(ketama, ketama_server);

```
### jump consistent hashing
```c
jump_hash *jump = jump_create();
jump_add_server(jump, "1.1.1.1:1111");
server_info *jump_server = jump_get_server(jump, "test_key");
// jump hashing doesn't support jump_del_server api
```
# reference
