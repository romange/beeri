// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "base/cuckoo_map.h"
#include "base/random.h"
#include "base/gtest.h"

#include <unordered_set>
#include <sparsehash/dense_hash_set>
#include "base/hash.h"
#include "base/random.h"

DEFINE_int32(shrink_items, 200, "");

namespace base {

struct cityhash32 {
  size_t operator()(uint64 val) const {
    return base::CityHash32(val);
  }
};

class CuckooMapTest : public testing::Test {
};


using namespace std;

TEST_F(CuckooMapTest, BasicMapSeq) {
  CuckooMap<int> m;
  EXPECT_EQ(CuckooMapTable::npos, m.find(200));

  m.SetEmptyKey(0);
  uint32 kLength = 100000;
  for (uint32 k = 1; k <= kLength; ++k) {
    int data = k + 117;
    pair<CuckooMapTable::dense_id, bool> res = m.Insert(k, data);
    EXPECT_TRUE(res.second);
    ASSERT_EQ(k, m.FromDenseId(res.first).first);
    int* val = m.FromDenseId(res.first).second;
    ASSERT_TRUE(val != nullptr);
    ASSERT_EQ(data, *val);
    ASSERT_EQ(res.first, m.find(k));
  }
  auto res = m.Insert(1, 10);
  EXPECT_FALSE(res.second);
  EXPECT_EQ(1, m.FromDenseId(res.first).first);
  for (uint32 k = 1; k <= kLength; ++k) {
    ASSERT_NE(CuckooMapTable::npos, m.find(k));
  }
  for (uint32 k = kLength + 1; k <= kLength*2; ++k) {
    ASSERT_EQ(CuckooMapTable::npos, m.find(k));
  }

  EXPECT_EQ(kLength, m.size());
  LOG(INFO) << "Utilization " << m.Utilization();

  m.Clear();
  EXPECT_TRUE(m.empty());
  EXPECT_EQ(0, m.size());
}

TEST_F(CuckooMapTest, RandomInput) {
  MTRandom rand;

  CuckooMap<uint64> m;
  m.SetEmptyKey(0);
  const uint32 kLength = 100000;
  for (int k = 0; k < kLength; ++k) {
    uint64 v = rand.Rand64();
    while (v ==0 || m.find(v) != CuckooMapTable::npos) {
      v = rand.Rand64();
    }
    uint64 data = v * 2;
    pair<CuckooMapTable::dense_id, bool> res = m.Insert(v, data);
    EXPECT_TRUE(res.second);
    ASSERT_EQ(res.first, m.find(v));
    auto key_val = m.FromDenseId(res.first);
    ASSERT_EQ(v, key_val.first);
    ASSERT_TRUE(key_val.second);
    ASSERT_EQ(data, *key_val.second);
  }
  EXPECT_EQ(kLength, m.size());
}

TEST_F(CuckooMapTest, Compact) {
  for (int iter = 17; iter <= FLAGS_shrink_items; ++iter) {
    LOG(INFO) << "Iter " << iter;
    CuckooMap<uint64> m;
    m.SetEmptyKey(0);
    for (uint64 k = 1; k < iter; ++k) {
      m.Insert(k * k, k);
    }

    m.Compact(1.05);
    uint32 count = 0;
    for (CuckooMapTable::dense_id i = 0; i < m.Capacity(); ++i) {
      auto res_pair = m.FromDenseId(i);
      uint64 key = res_pair.first;
      if (key == 0) continue;  // empty value.
      ++count;
      ASSERT_EQ(i, m.find(key)) << "Can not finde consistent dense_id for " << key;
      ASSERT_EQ(key, (*res_pair.second) * (*res_pair.second) );
    }
    EXPECT_EQ(count, m.size());
    for (uint64 k = 1; k < iter; ++k) {
      CuckooMapTable::dense_id id = m.find(k * k);
      ASSERT_NE(CuckooMapTable::npos, id);
      ASSERT_EQ(k*k, m.FromDenseId(id).first);
    }
  }
}

DECLARE_BENCHMARK_FUNC(BM_InsertDenseSet, iters) {
  ::google::dense_hash_set<uint64, cityhash32> set;
  set.set_empty_key(0);
  for (uint64 i = 0; i < iters; ++i) {
    set.insert(1 + (i + 1)*i);
  }
}

DECLARE_BENCHMARK_FUNC(BM_InsertCuckoo, iters) {
  CuckooMapTable m(0, int(iters*1.3));
  m.SetEmptyKey(0);
  m.SetGrowth(1.5);
  for (uint64 i = 0; i < iters; ++i) {
    m.Insert(1+ (i + 1)*i, nullptr);
  }
  LOG(INFO) << "BM_InsertCuckoo: " << iters << " " << m.BytesAllocated();
}

DECLARE_BENCHMARK_FUNC(BM_InsertUnorderedSet, iters) {
  std::unordered_set<uint32, cityhash32> set;
  for (int i = 0; i < iters; ++i) {
    sink_result(set.insert(i + 1));
  }
}

DECLARE_BENCHMARK_FUNC(BM_FindCuckooSetSeq, iters) {
  StopBenchmarkTiming();
  CuckooSet m(unsigned(iters*1.3));
  m.SetEmptyKey(0);
  for (int i = 0; i < iters; ++i) {
    m.Insert(i + 1);
  }
  LOG(INFO) << "BM_FindCuckooSetSeq: " << iters << " " << m.BytesAllocated();
  StartBenchmarkTiming();
  for (uint32 i = 1; i <= iters; ++i) {
    CuckooMapTable::dense_id d = m.find(i);
    DCHECK_NE(CuckooMapTable::npos, d);
    sink_result(d);
    sink_result(m.find(iters + i + 1));
    sink_result(m.find(2*iters + i + 1));
  }
}

DECLARE_BENCHMARK_FUNC(BM_FindUnorderedSet, iters) {
  StopBenchmarkTiming();
  std::unordered_set<uint64, cityhash32> set;
  for (int i = 0; i < iters; ++i) {
    set.insert(i + 1);
  }
  // LOG(INFO) << "Load factor " << set.load_factor();
  StartBenchmarkTiming();
  for (int i = 0; i < iters; ++i) {
    sink_result(set.find(i + 1));
  }
}

DECLARE_BENCHMARK_FUNC(BM_FindDenseSetSeq, iters) {
  StopBenchmarkTiming();
  ::google::dense_hash_set<uint64, cityhash32> set;
  set.set_empty_key(0);
  for (int i = 0; i < iters; ++i) {
    set.insert(i + 1);
  }
  set.resize(0);
  // LOG(INFO) << "Load factor " << set.load_factor();
  StartBenchmarkTiming();
  for (int i = 0; i < iters; ++i) {
    sink_result(set.find(i + 1));
    sink_result(set.find(iters + i + 1));
    sink_result(set.find(2*iters + i + 1));
  }
}

DECLARE_BENCHMARK_FUNC(BM_FindDenseSetRandom, iters) {
  StopBenchmarkTiming();
  ::google::dense_hash_set<uint64, cityhash32> set;
  set.set_empty_key(0);
  MTRandom rand(10);
  std::vector<uint64> vals(iters, 0);
  for (int i = 0; i < iters; ++i) {
    vals[i] = rand.Rand64();
    if (vals[i] == 0) vals[i] = 1;
    set.insert(vals[i]);
  }
  StartBenchmarkTiming();
  for (int i = 0; i < iters; ++i) {
    sink_result(set.find(vals[i]));
    sink_result(set.find(i + 1));
    sink_result(set.find(iters + i + 1));
  }
}

DECLARE_BENCHMARK_FUNC(BM_FindCuckooRandom, iters) {
  StopBenchmarkTiming();
  CuckooMapTable m(0, unsigned(iters*1.3));
  m.SetEmptyKey(0);
  MTRandom rand(20);
  std::vector<uint64> vals(iters, 0);
  for (int i = 0; i < iters; ++i) {
    vals[i] = rand.Rand64();
    if (vals[i] == 0) vals[i] = 1;
    m.Insert(vals[i], nullptr);
  }
  StartBenchmarkTiming();
  for (int i = 0; i < iters; ++i) {
    sink_result(m.find(vals[i])); // to simulate hits
    // to simulate 66% of misses
    sink_result(m.find(i + 1));
    sink_result(m.find(iters + i + 1));
  }
}

DECLARE_BENCHMARK_FUNC(BM_FindCuckooRandomAfterCompact, iters) {
  StopBenchmarkTiming();
  CuckooMapTable m(0, unsigned(iters*1.3));
  m.SetEmptyKey(0);
  MTRandom rand(20);
  std::vector<uint64> vals(iters, 0);
  for (int i = 0; i < iters; ++i) {
    vals[i] = rand.Rand64();
    if (vals[i] == 0) vals[i] = 1;
    m.Insert(vals[i], nullptr);
  }
  CHECK(m.Compact(1.05));
  StartBenchmarkTiming();
  for (int i = 0; i < iters; ++i) {
    sink_result(m.find(vals[i])); // to simulate hits
    sink_result(m.find(i + 1));  // to simulate misses
  }
}

DECLARE_BENCHMARK_FUNC(BM_CuckooCompact, iters) {
  StopBenchmarkTiming();
  CuckooMapTable m(0, unsigned(iters*1.3));
  m.SetEmptyKey(0);
  MTRandom rand(20);
  std::vector<uint64> vals(iters, 0);
  for (int i = 0; i < iters; ++i) {
    vals[i] = rand.Rand64();
    if (vals[i] == 0) vals[i] = 1;
    m.Insert(vals[i], nullptr);
  }
  LOG(INFO) << "BM_CuckooCompact before compact: " << iters << " " << m.BytesAllocated();
  StartBenchmarkTiming();
  CHECK(m.Compact(1.05));
  LOG(INFO) << "BM_CuckooCompact after compact: " << iters << " " << m.BytesAllocated();
}

}  // namespace base
