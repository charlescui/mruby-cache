class Cache

  class CacheError < StandardError; end
  class ShmError < CacheError; end
  class MemoryPoolFull < CacheError; end
  class LockError < CacheError; end
  class LockTimedOut < CacheError; end
  class OutOfMemoryError < CacheError; end
  class ArgError < CacheError; end
  class InitError < CacheError; end
  class RecoveryFailed < CacheError; end
  class ShmLockFailed < CacheError; end
  class ShmUnlockFailed < CacheError; end
  class MemoryPoolClosed < CacheError; end
  class DBVersionNotSupported < CacheError; end

  #  Creates a new handle for accessing a shared memory region.
  # 
  #  Cache.new :namespace=>"foo", :size_mb=> 1
  #
  #  Cache.new :namespace=>"foo", :size_mb=> 1, :min_alloc_size => 256
  #
  #
  # 
  #  Cache.new :filename=>"./foo.lmc"
  #
  #  Cache.new :filename=>"./foo.lmc", :min_alloc_size => 512
  # 
  #  You must supply at least a :namespace or :filename parameter
  #  The size_mb defaults to 1024 (1 GB).
  #
  #  The :min_alloc_size parameter was introduced to help with use cases that
  #  intend to use a hash table with growing values.  This is currently not
  #  handled well by the internal allocator as it will end up with a large list
  #  of unusable free blocks.  By setting the :min_alloc_size parameter you
  #  help the allocator to plan better ahead.
  #
  #  If you use the :namespace parameter, the .lmc file for your namespace will
  #  reside in /var/tmp/Cache.  This can be overriden by setting the
  #  LMC_NAMESPACES_ROOT_PATH variable in the environment.
  #
  #  When you first call .new for a previously not existing memory pool, a
  #  sparse file will be created and memory and disk space will be allocated to
  #  hold the empty hashtable (about 100K), so the size_mb refers
  #  only to the maximum size of the memory pool.  .new for an already existing
  #  memory pool will only map the already previously allocated RAM into the
  #  virtual address space of your process.  
  #
  # 
  def self.new(options={})
    o = { :size_mb => 0 }.merge(options || {})
    _new(stringfy_keys(o));
  end
  def has_key?(k) !get(k).nil? end
  def self.stringfy_keys(h)
    h_new = h.dup
    h_new.keys.each do |key|
      h_new[key.to_s] = h_new[key]
    end
    h_new
  end
end
