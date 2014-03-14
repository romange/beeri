// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _CUCKOO_MAP_H
#define _CUCKOO_MAP_H

#include <memory>
#include <vector>

#include "base/bits.h"
#include "base/integral_types.h"

#include "libdivide/libdivide.h"

/* Cuckoo works very bad with non-prime table sizes.
   In particular, for random input we quickly find many numbers pairs that map to the same
   bucket pairs, i.e. n1..n8 map to buckets x and y. Once this happens cuckoo table must regrow
   no matter how unutilized the table is. However, with table prime sizes,
   those collisions disapear.
   */
namespace base {

class CuckooMapTable {
  static constexpr unsigned char kBucketLength = 4;
  static constexpr uint64 kMask1 = 0xc949d7c7509e6557ULL;
  static constexpr uint64 kMask2 = 0x9ae16a3b2f90404fULL;

  typedef uint32 BucketId;

  struct Bucket {
    uint64 key[kBucketLength];
    uint8 data[];
  } __attribute__((aligned(4), packed));
 public:
  typedef uint32 dense_id;
  typedef uint64 key_type;

  static constexpr dense_id npos = kuint32max;

  // Allocates space for at least the given number of key-values.
  explicit CuckooMapTable(const uint32 value_size, uint32 capacity);

  ~CuckooMapTable();

  // Must be called before insertions take place.
  void SetEmptyKey(const key_type& v) {
    CHECK(!empty_value_set_);
    empty_value_ = v;
    empty_value_set_ = true;
    SetEmptyValues();
  }

  // Inserts x into the map. This function invalidates all dense_ids.
  std::pair<dense_id, bool> Insert(key_type v, const uint8* data);

  // Finds the key whose value is v.
  // returns npos if v was not found.
  dense_id find(const key_type v) const;

  std::pair<key_type, uint8*> FromDenseId(dense_id d);

  std::pair<key_type, const uint8*> FromDenseId(dense_id d) const;

  // Erases all elements.
  void Clear() {
    size_ = 0;
    SetEmptyValues();
  }

  // Returns the size of the map.
  size_t size() const {
    return size_;
  }

  // Returns the capacity of current bucket array.
  // It also serves as limit for dense_id keys,
  // i.e. all valid keys are in the range [0, capacity()).
  dense_id Capacity() const {
    return bucket_count_ * kBucketLength;
  }

  bool empty() const {
    return size_ == 0;
  }

  // Sets the new growth factor. Must be greater than 1.01.
  void SetGrowth(float growth) {
    CHECK_GT(growth, 1.01f);
    growth_ = growth;
  }

  // Compacts the current hashtable to size*ratio.
  // Ratio must be greater than 1. 1.07 is a good choice allowing
  // just 7% percent of unused items.
  bool Compact(double ratio);

  // void CopyFrom(const CuckooMapTable& other);

  double Utilization() const { return size() * 1.0 / Capacity();}

  uint64 BytesAllocated() const {
    return bucket_count_*bucket_size_ + 2 * value_size_;
  }

 private:
  typedef std::pair<BucketId, BucketId> BucketIdPair;

  static dense_id ToDenseId(BucketId id, uint8 offset) {
    return id * kBucketLength + offset;
  }

  static BucketId BucketFromId(dense_id d) {
    return d / kBucketLength;
  }

  Bucket* GetBucketById(const BucketId id) {
    return reinterpret_cast<Bucket*>(buf_.get() + id * bucket_size_);
  }

  const Bucket* GetBucketById(const BucketId id) const {
    return reinterpret_cast<const Bucket*>(buf_.get() + id * bucket_size_);
  }

  // for https://github.com/ridiculousfish/libdivide or libdivide.com to speedup the
  BucketId FromHash(const key_type hash_val) const {
    uint64 p;
    switch(divide_s_alg_) {
      case 1: p = libdivide::libdivide_u64_do_alg1(hash_val, &divide_s_);
        break;
      default: p = libdivide::libdivide_u64_do_alg2(hash_val, &divide_s_);
        break;
    }
    return hash_val - p * bucket_count_;
  }

  BucketId hash1(const key_type k) const {
    return FromHash(kMask1 ^ k);
  }

  BucketId hash2(const key_type k) const {
    return FromHash(kMask2 ^ k);
  }

  // Inserts problematic key/value pairs during the compaction process.
  // Returns true if succeeds to finish.
  bool InsertProblematicKeys(const std::vector<key_type>& keys, const uint8* values);

  // Allocates and clears the memory and sets the end iterator
  void Init(BucketId bucket_capacity);

  void DoAllocate();

  BucketIdPair HashToIdPair(const key_type v) const;

  // Computes a mask with kBucketLength bits indicating empty bucket indices.
  // Uses empty_value_ to compare.
  uint32 CheckEmpty(const Bucket& bucket) const;

  // Returns an dense_id of k in buckets described by id_pair or npos if not found.
  dense_id FindInBucket(const BucketIdPair& id_pair, const key_type k) const;

  // Tries to insert the pending key by rolling it during random walk through full buckets
  // until it reaches an empty slot.
  // Returns npos if fails to insert (in case shifts_limit was reached).
  // Note that pending_key_ and *pending_ptr_ must be set before calling this function.
  dense_id RollPending(uint32 shifts_limit, const BucketIdPair& id_pair);

  // Inserts pending key/value pair into an empty slot in the bucket. Returns
  // an index of the slot.
  int InsertIntoBucket(const uint32 empty_mask, Bucket* bucket);

  // Grows the container. A new block of memory is allocated and the previous
  // content is reinserted.  This means that the peak memory usage is higher
  // than the max container size.
  void Grow();

  void SetEmptyValues();

  // Adds values from bucket_array into the storage without growing it.
  // returns true if succeeds.
  // insert_func must insert pending_key_ into  BucketIdPair and return true if succeeds.
  bool CopyBuffer(const uint8* bucket_array, uint32 count,
                  std::function<bool(const BucketIdPair&)> insert_func);

  bool ShiftExhaustive(BucketId bid);

  void SwapPending(Bucket* bucket, uint8 index);

  BucketId NextBucketId(BucketId current, key_type key) const;

  // Explores connecting buckets of parent bucket.
  // Returns true if succeeded to find a free slot and put there a pending_key.
  bool Explore(BucketId parent);

  const uint32 value_size_;
  uint32 bucket_size_ = 0;

  // The current number of values inside the container.
  size_t size_ = 0;

  key_type empty_value_ = 0;
  bool empty_value_set_ = false;

  // The number of usable buckets.
  size_t bucket_count_ = 0;
  libdivide::libdivide_u64_t divide_s_;
  int divide_s_alg_ = 0;

  // The growth factor.
  float growth_;

  // The allocated memory
  std::unique_ptr<uint8[]> buf_;

  // has capacity of 2*value_size_ in order to allow swaps.
  std::unique_ptr<uint8[]> tmp_value_;
  uint8* pending_ptr_;

  key_type pending_key_;
  // The first key_type after the usable key_types
  // key_type* end_;

  // The number of evictions that can happen during insertion of a value before
  // we grow the container.
  uint32 shifts_limit_;

  // used only in Compact();
  //std::vector<bool> node_state_;
  uint32 random_bit_indx_ = 0;

  // Auxillary data structures for compaction algorithm.
  class BucketState;
  std::vector<BucketState> compaction_info_;
  std::vector<BucketId> compaction_stack_;

  void operator=(const CuckooMapTable&) = delete;
  CuckooMapTable(const CuckooMapTable&) = delete;
};

class CuckooMapTableWrapperBase {
public:
  typedef CuckooMapTable::dense_id DenseId;
  typedef uint64 KeyType;

  static constexpr DenseId npos = CuckooMapTable::npos;

  // Must be called before insertions take place.
  void SetEmptyKey(const KeyType& k) { table_.SetEmptyKey(k); }
  DenseId find(const KeyType& v) const { return table_.find(v);}

  void Clear() {  table_.Clear(); }
  size_t size() const { return table_.size();}
  size_t Capacity() const {return table_.Capacity();}

  bool empty() const { return table_.empty();}
  void SetGrowth(float growth) { table_.SetGrowth(growth);}
  bool Compact(double ratio) { return table_.Compact(ratio);}

  double Utilization() const { return table_.Utilization();}
  uint64 BytesAllocated() const { return table_.BytesAllocated();}

  DenseId DenseIdEnd() const { return Capacity(); }
protected:
  CuckooMapTableWrapperBase(const uint32 value_size, uint32 capacity)
      : table_(value_size, capacity) {}
  CuckooMapTable table_;
};

/*
  Cuckoo Set.
*/
class CuckooSet : public CuckooMapTableWrapperBase {
public:
  // Allocates space for the minimal number of values.
  explicit CuckooSet(uint32 capacity = 0) : CuckooMapTableWrapperBase(0, capacity) {}

  // Inserts x into the map. This function invalidates all dense_ids.
  std::pair<DenseId, bool> Insert(KeyType v) { return table_.Insert(v, nullptr); }
  KeyType FromDenseId(DenseId d) const { return table_.FromDenseId(d).first;}
};

/*
  Cuckoo Map. T must be trivially copyable type.
*/
template<typename T> class CuckooMap : public CuckooMapTableWrapperBase {
public:
  explicit CuckooMap(uint32 capacity = 0) : CuckooMapTableWrapperBase(sizeof(T), capacity) {
    //static_assert(std::is_trivially_copyable<T>::value, "T should be copied trvially");
  }

  // Inserts x into the map. This function invalidates all dense_ids.
  std::pair<DenseId, bool> Insert(KeyType v, const T& t) {
    return table_.Insert(v, reinterpret_cast<const uint8*>(&t));
  }

  std::pair<KeyType, T*> FromDenseId(DenseId d) {
    auto p = table_.FromDenseId(d);
    return std::pair<KeyType, T*>(p.first, reinterpret_cast<T*>(p.second));
  }
};



// Implementation
/******************************************************************/
inline CuckooMapTable::dense_id CuckooMapTable::find(const key_type v) const {
  BucketId bid1 = hash1(v);
  const key_type* a = GetBucketById(bid1)->key;
  for (uint8 i = 0; i < kBucketLength; ++i) {
    if (a[i] == v) return ToDenseId(bid1, i);
  }

  BucketId bid2 = hash2(v);
  if (__builtin_expect(bid2 == bid1, 0)) {
    bid2 = (bid2 + 1) % bucket_count_;
  }
  const key_type* b = GetBucketById(bid2)->key;
  for (uint8 i = 0; i < kBucketLength; ++i) {
    if (b[i] == v) return ToDenseId(bid2, i);
  }
  return npos;
}

inline std::pair<CuckooMapTable::key_type, uint8*> CuckooMapTable::FromDenseId(dense_id d) {
  DCHECK_LT(d, Capacity());
  BucketId b = BucketFromId(d);
  Bucket* bucket = GetBucketById(b);
  uint8 index = d % kBucketLength;
  return std::pair<key_type, uint8*>(bucket->key[index], bucket->data + value_size_ * index);
}

inline std::pair<CuckooMapTable::key_type, const uint8*> CuckooMapTable::FromDenseId(dense_id d) const {
  DCHECK_LT(d, Capacity());
  BucketId b = BucketFromId(d);
  const Bucket* bucket = GetBucketById(b);
  uint8 index = d % kBucketLength;
  return std::pair<key_type, const uint8*>(bucket->key[index], bucket->data + value_size_ * index);
}

}  // namespace base

#endif  // _CUCKOO_MAP_H