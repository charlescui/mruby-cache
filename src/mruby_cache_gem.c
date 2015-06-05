#include "mruby.h"
#include "mruby/class.h"
#include "mruby/value.h"
#include "mruby/hash.h"
#include "mruby/data.h"
#include "mruby/gc.h"
#include "mruby/variable.h"
#include "mruby/string.h"
#include "mruby/array.h"
#include <sys/types.h>
#include <unistd.h>
#include "localmemcache.h"

#define DONE mrb_gc_arena_restore(mrb, 0)

/* :nodoc: */
long long_value(mrb_value i) { return mrb_nil_p(i) ? 0 : (long)mrb_fixnum(i); }
/* :nodoc: */
double double_value(mrb_value i) { return mrb_float(i); }

/* :nodoc: */
char *rstring_ptr(mrb_value s) { 
  char* r = mrb_nil_p(s) ? "nil" : RSTRING_PTR(s); 
  return r ? r : "nil";
}

/* :nodoc: */
char *rstring_ptr_null(mrb_value s) { 
  char* r = mrb_nil_p(s) ? NULL : RSTRING_PTR(s); 
  return r ? r : NULL;
}

/* :nodoc: */
int bool_value(mrb_value v) { return (mrb_type(v)); }

/* :nodoc: */
mrb_value
lmc_ruby_string2(mrb_state *mrb, const char *s, size_t l) { 
  return s ? mrb_str_new(mrb, s, l) : mrb_nil_value(); 
}

/* :nodoc: */
mrb_value
lmc_ruby_string(mrb_state *mrb, const char *s) { 
  return lmc_ruby_string2(mrb, s + sizeof(size_t), *(size_t *) s);
}

typedef struct {
  local_memcache_t *lmc;
  int open;
} rb_lmc_handle_t;

static struct RClass *Cache;
static mrb_value lmc_rb_sym_namespace;
static mrb_value lmc_rb_sym_filename;
static mrb_value lmc_rb_sym_size_mb;
static mrb_value lmc_rb_sym_min_alloc_size;
static mrb_value lmc_rb_sym_force;

/* :nodoc: */
void __rb_lmc_raise_exception(mrb_state *mrb, const char *error_type, const char *m) {
  mrb_sym eid;
  mrb_value k;
  eid = mrb_intern_cstr(mrb, error_type);
  k = mrb_mod_cv_get(mrb, Cache, eid);
  mrb_raise(mrb, mrb_class_ptr(k), m);
}

/* :nodoc: */
void rb_lmc_raise_exception(mrb_state *mrb, lmc_error_t *e) {
  __rb_lmc_raise_exception(mrb, e->error_type, e->error_str);
}

/* :nodoc: */
local_memcache_t *rb_lmc_check_handle_access(mrb_state *mrb, rb_lmc_handle_t *h) {
  if (!h || (h->open == 0) || !h->lmc) {
    __rb_lmc_raise_exception(mrb, "MemoryPoolClosed", "Pool is closed");
    return 0;
  }
  return h->lmc;
}

#define LMC_CACHE_KEY            "$lmc_cache"

static const struct mrb_data_type lmc_cache_type = { 
    LMC_CACHE_KEY, mrb_free
};

mrb_value
bool_local_memcache_delete(local_memcache_t *lmc, char *key, size_t n_key) {
	return (local_memcache_delete(lmc, key, n_key) == 1) ? mrb_true_value() : mrb_false_value();
}

/* :nodoc: */
void lmc_check_dict(mrb_state *mrb, mrb_value o){
  if (mrb_type(o) != MRB_TT_HASH) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "expected a Hash");
  }
}

/* :nodoc: */
mrb_value
Cache__new2(mrb_state *mrb, mrb_value self){
  mrb_value o;
  mrb_get_args(mrb, "o", &o);
  lmc_check_dict(mrb, o);
  lmc_error_t e;
  local_memcache_t *l = local_memcache_create(
      rstring_ptr_null(mrb_hash_get(mrb, o, lmc_rb_sym_namespace)),
      rstring_ptr_null(mrb_hash_get(mrb, o, lmc_rb_sym_filename)), 
      double_value(mrb_hash_get(mrb, o, lmc_rb_sym_size_mb)),
      long_value(mrb_hash_get(mrb, o, lmc_rb_sym_min_alloc_size)), &e);
  if (!l)  rb_lmc_raise_exception(mrb, &e);
  rb_lmc_handle_t *h = (rb_lmc_handle_t *)mrb_malloc(mrb, sizeof(rb_lmc_handle_t));
  if (!h) mrb_raise(mrb, E_RUNTIME_ERROR, "memory allocation error");
  h->lmc = l;
  h->open = 1;
  return mrb_obj_value((void *)Data_Wrap_Struct(mrb, mrb_class_ptr(self), &lmc_cache_type, h));
}

/* :nodoc: */
local_memcache_t *get_Cache(mrb_state *mrb, mrb_value self) {
  rb_lmc_handle_t *h;
  Data_Get_Struct(mrb, self, &lmc_cache_type, h);
  return rb_lmc_check_handle_access(mrb, h);
}

/*
 * call-seq: Cache.drop(*args)
 *
 * Deletes a memory pool.  If the :force option is set, locked semaphores are
 * removed as well.
 *
 * WARNING: Do only call this method with the :force option if you are sure
 * that you really want to remove this memory pool and no more processes are
 * still using it.
 *
 * If you delete a pool and other processes still have handles open on it, the
 * status of these handles becomes undefined.  There's no way for a process to
 * know when a handle is not valid anymore, so only delete a memory pool if
 * you are sure that all handles are closed.
 *
 * valid options for drop are 
 * [:namespace] 
 * [:filename] 
 * [:force] 
 *
 * The memory pool must be specified by either setting the :filename or
 * :namespace option.  The default for :force is false.
 */
mrb_value
Cache__drop(mrb_state *mrb, mrb_value self){
  mrb_value o;
  mrb_get_args(mrb, "o", &o);
  lmc_check_dict(mrb, o);
  lmc_error_t e;
  if (!local_memcache_drop_namespace(
      rstring_ptr_null(mrb_hash_get(mrb, o, lmc_rb_sym_namespace)), 
      rstring_ptr_null(mrb_hash_get(mrb, o, lmc_rb_sym_filename)),
      bool_value(mrb_hash_get(mrb, o, lmc_rb_sym_force)), &e)) {
    rb_lmc_raise_exception(mrb, &e); 
  }
  return mrb_nil_value();
}

/* :nodoc: */
mrb_value
Cache__enable_test_crash(mrb_state *mrb, mrb_value self){
  srand(getpid());
  lmc_test_crash_enabled = 1;
  return mrb_nil_value();
}

/* :nodoc: */
mrb_value
Cache__disable_test_crash(mrb_state *mrb, mrb_value self){
  lmc_test_crash_enabled = 0;
  return mrb_nil_value();
}

/* 
 *  call-seq:
 *     lmc.get(key)   ->   string value or nil
 *     lmc[key]       ->   string value or nil
 *
 *  Retrieve string value from hashtable.
 */
mrb_value
Cache__get(mrb_state *mrb, mrb_value self) {
  size_t l;
  mrb_value key;
  mrb_get_args(mrb, "o", &key);
  const char* r = __local_memcache_get(get_Cache(mrb, self), 
      RSTRING_PTR(key), RSTRING_LEN(key), &l);
  mrb_value rr = lmc_ruby_string2(mrb, r, l);
  lmc_unlock_shm_region("local_memcache_get", get_Cache(mrb, self));
  return rr;
}

/* 
 *  call-seq:
 *     lmc.set(key, value)   ->   Qnil
 *     lmc[key]=value        ->   Qnil
 *
 *  Set value for key in hashtable.  Value and key will be converted to
 *  string.
 */
mrb_value
Cache__set(mrb_state *mrb, mrb_value self){
  local_memcache_t *lmc = get_Cache(mrb, self);
  mrb_value key, value;
  mrb_get_args(mrb, "oo", &key, &value);
  if (!local_memcache_set(lmc, RSTRING_PTR(key), RSTRING_LEN(key), RSTRING_PTR(value), RSTRING_LEN(value))) { 
    rb_lmc_raise_exception(mrb, &lmc->error); 
  }
  return mrb_nil_value();
}


/*
 *  call-seq: 
 *     lmc.clear -> Qnil
 *
 *  Clears content of hashtable.
 */
mrb_value
Cache__clear(mrb_state *mrb, mrb_value self){
  local_memcache_t *lmc = get_Cache(mrb, self);
  if (!local_memcache_clear(lmc)) rb_lmc_raise_exception(mrb, &lmc->error); 
  return mrb_nil_value();
}

/* 
 *  call-seq:
 *     lmc.delete(key)   ->   Qnil
 *
 *  Deletes key from hashtable.  The key is converted to string.
 */
mrb_value
Cache__delete(mrb_state *mrb, mrb_value self){
  mrb_value arg;
  mrb_get_args(mrb, "o", &arg);
  return bool_local_memcache_delete(get_Cache(mrb, self), RSTRING_PTR(arg), RSTRING_LEN(arg));
}

/* 
 *  call-seq:
 *     lmc.close()   ->   Qnil
 *
 *  Releases hashtable.
 */
mrb_value
Cache__close(mrb_state *mrb, mrb_value self){
  lmc_error_t e;
  rb_lmc_handle_t *h;
  Data_Get_Struct(mrb, self, &lmc_cache_type, h);
  if (!local_memcache_free(rb_lmc_check_handle_access(mrb, h), &e)) 
      rb_lmc_raise_exception(mrb, &e);
  h->open = 0;
  return mrb_nil_value();
}

/* 
 *  call-seq:
 *     lmc.size -> number
 *
 *  Number of pairs in the hashtable.
 */
mrb_value
Cache__size(mrb_state *mrb, mrb_value self) {
  local_memcache_t *lmc = get_Cache(mrb, self);
  ht_hash_t *ht = lmc->base + lmc->va_hash;
  return mrb_fixnum_value(ht->size);
}

/* 
 *  call-seq:
 *     lmc.shm_status -> hash
 *
 *  Some status information on the shared memory:
 *
 *    :total_bytes # the total size of the shm in bytes 
 *    :used_bytes  # how many bytes are used in this shm 
 *                 # For exmpty namespaces this will reflect the amount
 *                 # of memory used for the hash buckets and some other 
 *                 # administrative data structures.
 *    :free_bytes  # how many bytes are free 
 */

mrb_value
Cache__shm_status(mrb_state *mrb, mrb_value self) {
  mrb_value hash = mrb_hash_new(mrb);
  
  local_memcache_t *lmc = get_Cache(mrb, self);
  if (!lmc_lock_shm_region("shm_status", lmc)) return mrb_nil_value();
  lmc_mem_status_t ms = lmc_status(lmc->base, "shm_status");
  if (!lmc_unlock_shm_region("shm_status", lmc)) return mrb_nil_value();

  mrb_hash_set(mrb, hash, mrb_symbol_value(mrb_intern_cstr(mrb, "free_bytes")), 
      mrb_fixnum_value(ms.total_free_mem));
  mrb_hash_set(mrb, hash, mrb_symbol_value(mrb_intern_cstr(mrb, "total_bytes")), 
      mrb_fixnum_value(ms.total_shm_size));
  mrb_hash_set(mrb, hash, mrb_symbol_value(mrb_intern_cstr(mrb, "used_bytes")), mrb_fixnum_value(
      ms.total_shm_size - ms.total_free_mem));
  mrb_hash_set(mrb, hash, mrb_symbol_value(mrb_intern_cstr(mrb, "free_chunks")), mrb_fixnum_value(
      ms.free_chunks));
  mrb_hash_set(mrb, hash, mrb_symbol_value(mrb_intern_cstr(mrb, "largest_chunk")), mrb_fixnum_value(
      ms.largest_chunk));
  return hash;
}

/* 
 * internal, do not use
 */
mrb_value
Cache__check_consistency(mrb_state *mrb, mrb_value self) {
  lmc_error_t e;
  rb_lmc_handle_t *h;
  Data_Get_Struct(mrb, self, &lmc_cache_type, h);
  return local_memcache_check_consistency(rb_lmc_check_handle_access(mrb, h), &e) ? 
      mrb_true_value() : mrb_false_value();
}

/*
 * Document-class: Cache
 * 
 * <code>Cache</code> provides for a Hashtable of strings in shared
 * memory (via a memory mapped file), which thus can be shared between
 * processes on a computer.  Here is an example of its usage:
 *
 *   $lm = Cache.new :namespace => "viewcounters"
 *   $lm[:foo] = 1
 *   $lm[:foo]          # -> "1"
 *   $lm.delete(:foo)
 *
 * <code>Cache</code> can also be used as a persistent key value
 * database, just use the :filename instead of the :namespace parameter.
 *
 *   $lm = Cache.new :filename => "my-database.lmc"
 *   $lm[:foo] = 1
 *   $lm[:foo]          # -> "1"
 *   $lm.delete(:foo)
 *
 *  == Default sizes of memory pools
 *
 *  The default size for memory pools is 1024 (MB). It cannot be changed later,
 *  so choose a size that will provide enough space for all your data.  You
 *  might consider setting this size to the maximum filesize of your
 *  filesystem.  Also note that while these memory pools may look large on your
 *  disk, they really aren't, because with sparse files only those parts of the
 *  file which contain non-null data actually use disk space.
 *
 *  == Automatic recovery from crashes
 *
 *  In case a process is terminated while accessing a memory pool, other
 *  processes will wait for the lock up to 2 seconds, and will then try to
 *  resume the aborted operation.  This can also be done explicitly by using
 *  Cache.check(options).
 *
 *  == Clearing memory pools
 *
 *  Removing memory pools can be done with Cache.drop(options). 
 *
 *  == Environment
 *  
 *  If you use the :namespace parameter, the .lmc file for your namespace will
 *  reside in /var/tmp/Cache.  This can be overriden by setting the
 *  LMC_NAMESPACES_ROOT_PATH variable in the environment.
 *
 *  == Storing Ruby Objects
 *
 *  If you want to store Ruby objects instead of just strings, consider 
 *  using Cache::SharedObjectStorage.
 *
 */
void 
mrb_mruby_cache_gem_init(mrb_state *mrb) {
  lmc_init();
  Cache = mrb_define_class(mrb, "Cache", mrb->object_class);
  mrb_define_singleton_method(mrb, (struct RObject*)Cache, "_new", Cache__new2, MRB_ARGS_REQ(1));
  mrb_define_singleton_method(mrb, (struct RObject*)Cache, "drop", 
      Cache__drop, MRB_ARGS_REQ(1));
  mrb_define_singleton_method(mrb, (struct RObject*)Cache, "disable_test_crash", 
      Cache__disable_test_crash, MRB_ARGS_NONE());
  mrb_define_singleton_method(mrb, (struct RObject*)Cache, "enable_test_crash", 
      Cache__enable_test_crash, MRB_ARGS_NONE());
  mrb_define_method(mrb, Cache, "get", Cache__get, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, Cache, "[]", Cache__get, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, Cache, "delete", Cache__delete, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, Cache, "set", Cache__set, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, Cache, "clear", Cache__clear, MRB_ARGS_NONE());
  mrb_define_method(mrb, Cache, "[]=", Cache__set, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, Cache, "close", Cache__close, MRB_ARGS_NONE());
  mrb_define_method(mrb, Cache, "size", Cache__size, MRB_ARGS_NONE());
  mrb_define_method(mrb, Cache, "shm_status", Cache__shm_status, MRB_ARGS_NONE());
  mrb_define_method(mrb, Cache, "check_consistency", Cache__check_consistency, MRB_ARGS_NONE());
  // mrb_define_method(mrb, Cache, "keys", Cache__keys, MRB_ARGS_NONE());

  lmc_rb_sym_namespace = (mrb_str_new_cstr(mrb, "namespace"));
  lmc_rb_sym_filename = (mrb_str_new_cstr(mrb, "filename"));
  lmc_rb_sym_size_mb = (mrb_str_new_cstr(mrb, "size_mb"));
  lmc_rb_sym_min_alloc_size = (mrb_str_new_cstr(mrb, "min_alloc_size"));
  lmc_rb_sym_force = (mrb_str_new_cstr(mrb, "force"));
  DONE;
}

void
mrb_mruby_cache_gem_final(mrb_state* mrb)
{
}
