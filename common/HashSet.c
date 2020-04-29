#include <common/HashSet.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h> // memset

static const uint32_t DEFAULT_TABLE_SIZE = 1 << 8U;  // Required to be a power of 2 !
static const float MAX_LOAD_FACTOR       = 0.4f;

typedef struct hop_hash_set
{
   const void** table;
   uint32_t capacity;
   uint32_t count;
} hop_hash_set;

static inline float load_factor( hop_hash_set_t set ) { return (float)set->count / set->capacity; }

static inline uint64_t hash_func( const void* value )
{
   return (uint64_t)value;
}

static inline uint32_t quad_probe( uint64_t hash_value, uint32_t it, uint32_t table_size )
{
   // Using quadratic probing function (x^2 + x) / 2
   return ( hash_value + ( ( it * it + it ) >> 2 ) ) % table_size;
}

// Insert value inside the hash set without incrementing the count. Used while rehashing as
// well as within the public insert function
static int insert_internal( hop_hash_set_t hs, const void* value )
{
   const uint64_t hash_value = hash_func( value );
   uint32_t iteration        = 0;
   while( iteration < hs->capacity )
   {
      const uint32_t idx         = quad_probe( hash_value, iteration++, hs->capacity );
      const void* existing_value = hs->table[idx];
      if( existing_value == value )
      {
         return 0;  // Value already inserted. Return insertion failure
      }
      else if( existing_value == NULL )
      {
         hs->table[idx] = value;
         return 1;
      }
   }

   return 0;
}

static void rehash( hop_hash_set_t hs )
{
   const void** prev_table      = hs->table;
   const uint32_t prev_capacity = hs->capacity;

   hs->capacity = prev_capacity * 2;
   hs->table    = (const void**)calloc( hs->capacity, sizeof( const void* ) );

   for( uint32_t i = 0; i < prev_capacity; ++i )
   {
      if( prev_table[i] != NULL ) insert_internal( hs, prev_table[i] );
   }

   free( prev_table );
}

hop_hash_set_t hop_hash_set_create()
{
   hop_hash_set* hs = (hop_hash_set*)calloc( 1, sizeof( hop_hash_set ) );
   if( !hs ) return NULL;

   hs->table = (const void**)calloc( DEFAULT_TABLE_SIZE, sizeof( const void* ) );
   if( !hs->table )
   {
      free( hs );
      return NULL;
   }

   hs->capacity = DEFAULT_TABLE_SIZE;
   return hs;
}

void hop_hash_set_destroy( hop_hash_set_t set )
{
   if( set )
   {
      free( set->table );
   }
   free( set );
}

int hop_hash_set_insert( hop_hash_set_t hs, const void* value )
{
   const int inserted = insert_internal( hs, value );
   if( inserted )
   {
      ++hs->count;
      if( load_factor( hs ) > MAX_LOAD_FACTOR )
      {
         rehash( hs );
      }
   }
   return inserted;
}

int hop_hash_set_count( hop_hash_set_t set ) { return set->count; }

void hop_hash_set_clear( hop_hash_set_t set )
{
   if( set )
   {
      set->count = 0;
      if( set->table )
      {
         memset( set->table, 0, set->capacity * sizeof( const void* ) );
      }
   }
}
