#ifndef HOP_HASH_SET_H_
#define HOP_HASH_SET_H_

#ifdef __cplusplus
extern "C"
{
#endif

   typedef struct hop_hash_set* hop_hash_set_t;

   hop_hash_set_t hop_hash_set_create();
   void hop_hash_set_destroy( hop_hash_set_t set );
   void hop_hash_set_clear( hop_hash_set_t set );
   int hop_hash_set_insert( hop_hash_set_t set, const void* value );
   int hop_hash_set_count( hop_hash_set_t set );

#ifdef __cplusplus
}
#endif

#endif  // HOP_HASH_SET_H_