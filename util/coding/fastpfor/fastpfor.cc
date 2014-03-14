/**
 * This code is released under the
 * Apache License Version 2.0 http://www.apache.org/licenses/.
 *
 * (c) Daniel Lemire, http://lemire.me/en/
 *
 * Modified by Roman Gershman romange@gmai.com
 */

#include "util/coding/fastpfor/fastpfor.h"

#include "base/bits.h"
#include "base/logging.h"
#include "strings/numbers.h"
#include "util/coding/fastpfor/bitpackinghelpers.h"
#include "util/coding/fastpfor/packingvectors.h"
#include "util/coding/fastpfor/variablebyte.h"

using std::vector;

FastPFor::FastPFor(uint32_t ps) :
      PageSize(ps), datatobepacked(32),
              bytescontainer(PageSize + 3 * PageSize / BlockSize) {
  CHECK_EQ(0, ps % BlockSize);
  static_assert(BlockSize < 256, "");
}

// sometimes, mem. usage can grow too much, this clears it up
void FastPFor::resetBuffer() {
  for (size_t i = 0; i < datatobepacked.size(); ++i) {
    vector<uint32_t> ().swap(datatobepacked[i]);
  }
}

uint32_t FastPFor::uncompressedLength(const uint32_t* in, const size_t length) {
  return *in;
}

const uint32_t * FastPFor::decodeArray(const uint32_t *in, size_t length,
                                       uint32_t *out, size_t &nvalue) {
  const uint32_t * const initin(in);
  const size_t decompressed_length = *in;
  const size_t rounded_length = (decompressed_length / PACKSIZE) * PACKSIZE;
  ++in;
  CHECK_LE(decompressed_length, nvalue);
  nvalue = decompressed_length;
  const uint32_t * const finalout(out + rounded_length);
  while (out != finalout) {
    size_t thisnvalue = 0;
    size_t thissize = finalout > PageSize + out ? PageSize : (finalout - out);
    __decodeArray(in, thisnvalue, out, thissize);
    in += thisnvalue;
    out += thissize;
  }
  CHECK_GE(length, in - initin);
  resetBuffer();// if you don't do this, the codec has a "memory".
  if (rounded_length < decompressed_length) {
    size_t nvalue2 = decompressed_length - rounded_length;
    length -= (in - initin);
    DVLOG(2) << "Varbyte decoding of " << nvalue2 << " integers";
    const uint32_t *in3 = VariableByte::decodeArray(in, length, out, nvalue2);
    CHECK_GE(length, in3 - in);
    in = in3;
  }
  return in;
}

/*
 * The input size (length) should be a multiple of
 * BlockSizeInUnitsOfPackSize * PACKSIZE. (This was done
 * to simplify slightly the implementation.)
 */
void FastPFor::encodeArray(const uint32_t *in, const size_t length, uint32_t *out,
                           size_t &nvalue) {
  VLOG(1) << "FastPFor::encodeArray: " << length;
  const size_t roundedlength = (length / PACKSIZE) * PACKSIZE;
  const uint32_t * const initout(out);
  const uint32_t * const finalin(in + roundedlength);
  const uint32_t * in_start = in;
  *out++ = static_cast<uint32_t>(length);
  const size_t init_nvalue = nvalue;
  nvalue = 1;
  while (in != finalin) {
    size_t thissize = finalin > PageSize + in ? PageSize : (finalin - in);
    size_t thisnvalue = 0;
    __encodeArray(in, thissize, out, thisnvalue);
    nvalue += thisnvalue;
    out += thisnvalue;
    in += thissize;
  }
  CHECK_EQ(out - initout, nvalue);
  if (nvalue > init_nvalue) {
    for (size_t i = 0; i < length; ++i) {
      LOG(INFO) << in_start[i];
    }
    LOG(FATAL) << "foo;";
  }
  CHECK_GE(init_nvalue, nvalue);
  resetBuffer();// if you don't do this, the buffer has a memory

  if (roundedlength < length) {
    size_t nvalue2 = init_nvalue - nvalue;
    DVLOG(2) << "varbyte encoding of " << length - roundedlength << " ints";
    VariableByte::encodeArray(in, length - roundedlength, out, nvalue2);
    nvalue += nvalue2;
  }
}

static inline uint32_t cost(uint32_t block_size, uint32_t cexcept, uint8_t pack_width,
                            uint8_t max_width) {
  return cexcept * FastPFor::overheadofeachexcept + (cexcept + (cexcept & 1)) * (max_width - pack_width) +
        pack_width * block_size + 8;// the  extra 8 is the cost of storing maxbits
}

void FastPFor::getBestParams(
  const uint32_t * in, const uint32_t block_size,
  CodeParams* params) {
  uint16_t freqs[33] = {0};
  params->min_val = *in;
  freqs[asmbits(*in)]++;
  params->shr = Bits::Bsf(*in);
  for (uint32_t k = 1; k < block_size; ++k) {
    uint32_t v = in[k];
    uint8_t l = asmbits(v);
    freqs[l]++;
    params->min_val = std::min(params->min_val, v);
    params->shr = std::min<uint8_t>(params->shr, Bits::Bsf(v));
    //FastHex32ToBuffer(in[k], buf);
    //DVLOG(2) << buf << "  length " << int(l);
  }
  params->shr &= 31;  // 32->0.
  params->maxb = 32;
  while (freqs[params->maxb] == 0)
    params->maxb--;
  params->maxb -= params->shr;
  params->bestb = params->maxb;
  uint32_t bestcost = params->maxb * block_size;
  uint32_t cexcept = 0;
  for (uint32_t b = params->maxb - 1; b < 32; --b) {
    cexcept += freqs[b + 1 + params->shr];
    uint32_t thiscost = cost(block_size, cexcept, b, params->maxb);
    if (thiscost < bestcost) {
      bestcost = thiscost;
      params->bestb = b;
      params->bestcexcept = cexcept;
    }
  }
  if (params->min_val == 0 && params->shr == 0)
    return;
  params->min_val >>= params->shr;
  memset(freqs, 0, sizeof(uint16_t)*33);
  for (uint32_t k = 0; k < block_size; ++k) {
    base_reduced_[k] = (in[k] >> params->shr) - params->min_val;
    uint8_t l = asmbits(base_reduced_[k]);
    freqs[l]++;
  }
  if (params->min_val == 0)
    return;
  cexcept = 0;
  uint32_t prev_best_cost = bestcost;
  uint8_t min_maxb = params->maxb;
  while (freqs[min_maxb] == 0) min_maxb--;
  uint8_t start = min_maxb - 1;
  if (min_maxb < params->maxb)
    ++start;
  uint8_t min_val_bit_cost = 8*(asmbits(params->min_val)/7+1) + 16;  // 16 is to make it worthwhile.
  for (uint8_t b = start; b < 32; --b) {
    cexcept += freqs[b + 1];
    uint32_t thiscost = cost(block_size, cexcept, b, min_maxb) + min_val_bit_cost;
    if (thiscost < bestcost) {
      bestcost = thiscost;
      params->bestb = b;
      params->bestcexcept = cexcept;
    }
  }
  if (prev_best_cost == bestcost) {
    if (params->shr) {
      for (auto& v : base_reduced_)
        v += params->min_val;
    }
    params->min_val = 0;
  } else {
    params->maxb = min_maxb;
  }
}

void FastPFor::__encodeArray(const uint32_t *in, const size_t length, uint32_t *out,
        size_t & nvalue) {
  uint32_t * const initout = out; // keep track of this
  CHECK_EQ(0, length % PACKSIZE);
  uint32_t * const headerout = out++; // keep track of this
  const uint32_t * const final = in + length;
  for (uint32_t k = 0; k < 32; ++k)
    datatobepacked[k].clear();
  bc_ptr_ = &bytescontainer[0];
  for (; in + BlockSize <= final; in += BlockSize) {
    out = __encodeBlock(in, BlockSize, out);
  }

  for (uint32_t block_size = BlockSize/2; block_size >= PACKSIZE; block_size /= 2) {
    if ( in + block_size <= final) {
      out = __encodeBlock(in, block_size, out);
      in += block_size;
    }
  }
  headerout[0] = static_cast<uint32_t> (out - headerout);
  const uint32_t bytescontainersize = static_cast<uint32_t>(bc_ptr_ - &bytescontainer[0]);
  *(out++) = bytescontainersize;
  memcpy(out, &bytescontainer[0], bytescontainersize);
  out += (bytescontainersize + sizeof(uint32_t) - 1) / sizeof(uint32_t);
  uint32_t bitmap = 0;
  for (uint32_t k = 0; k < 32; ++k) {
    bitmap |= (uint32_t(!datatobepacked[k].empty()) << k);
  }
  *(out++) = bitmap;

  for (uint32_t k = 0; k < 32; ++k) {
    if (datatobepacked[k].size() > 0)
      out = packingvector<32>::packmeupwithoutmask(datatobepacked[k], out, k + 1);
  }
  nvalue = out - initout;
}

uint32_t * FastPFor::__encodeBlock(const uint32_t* in, const size_t block_size, uint32_t *out) {
  DCHECK_LE(block_size, BlockSize);
  const uint8_t* bc_start = bc_ptr_;
  CodeParams params;
  getBestParams(in, block_size, &params);
  // bestb
  DVLOG(2) << block_size << " bestb " << int(params.bestb) << " cexcept: "
           << int(params.bestcexcept) << " shr: " << int(params.shr)
           << ", maxb: " << int(params.maxb) << ", base: " << params.min_val;
  uint8_t msb = (uint8_t(params.min_val != 0) << 7) | (uint8_t(params.shr != 0) << 6);
  *bc_ptr_++ = params.bestb | msb;
  *bc_ptr_++ = params.bestcexcept;
  if (msb) {
    if (params.min_val != 0)
      bc_ptr_ = VariableByte::encodeNum(params.min_val, bc_ptr_);
    if (params.shr != 0)
      *bc_ptr_++ = params.shr;
    in = base_reduced_;
  }

  if (params.bestcexcept > 0) {
    uint8_t except_count = 0;
    DCHECK_LE(params.maxb, 32);
    *bc_ptr_++ = params.maxb;
    DCHECK_LT(params.bestb, params.maxb);
    vector<uint32_t>& exceptioncontainer = datatobepacked[params.maxb - params.bestb - 1];
    const uint32_t maxval = 1U << params.bestb;
    for (uint32_t k = 0; k < block_size; ++k) {
      uint32_t v = in[k];
      if (v >= maxval) {
        // we have an exception
        exceptioncontainer.push_back(v >> params.bestb);
        *bc_ptr_++ = k;
        ++except_count;
      }
    }
    DCHECK_EQ(params.bestcexcept, except_count);
  }
  out = packblockup(in, out, params.bestb, block_size);
  DVLOG(2) << "bc advanced by " << bc_ptr_ - bc_start;
  return out;
}

void FastPFor::__decodeArray(const uint32_t *in, size_t & length, uint32_t *out,
       size_t nvalue) {
  const uint32_t * const initin = in;
  const uint32_t * const headerin = in++;
  const uint32_t wheremeta = headerin[0];
  const uint32_t *inexcept = headerin + wheremeta;
  const uint32_t bytesize = *inexcept++;
  Source src{in, reinterpret_cast<const uint8_t *> (inexcept)};
  inexcept += (bytesize + sizeof(uint32_t) - 1) / sizeof(uint32_t);
  const uint32_t bitmap = *(inexcept++);
  for (uint32_t k = 0; k < 32; ++k) {
    if ((bitmap & (1U << k)) != 0) {
      inexcept = packingvector<32>::unpackme(inexcept, datatobepacked[k], k + 1);
    }
  }
  length = inexcept - initin;
  vector<uint32_t>::const_iterator unpackpointers[32];
  for (uint32_t k = 0; k < 32; ++k) {
    unpackpointers[k] = datatobepacked[k].begin();
  }
  uint32 cnt = nvalue / BlockSize;
  for (uint32_t run = 0; run < cnt; ++run, out += BlockSize) {
    __decodeBlock(BlockSize, unpackpointers, &src, out);
  }
  nvalue -= cnt * BlockSize;
  for (uint32_t block_size = BlockSize/2; block_size >= PACKSIZE; block_size /= 2) {
    if (nvalue >= block_size) {
      __decodeBlock(block_size, unpackpointers, &src, out);
      out += block_size;
      nvalue -= block_size;
    }
  }
  CHECK_EQ(wheremeta, src.in - headerin);
}

void FastPFor::__decodeBlock(const uint32_t block_size,
  vector<uint32_t>::const_iterator unpackpointers[], Source* src, uint32_t *out) {
  const uint8_t * bytep = src->bytep;
  uint8_t b = *bytep++;
  const uint8_t cexcept = *bytep++;
  const uint8_t has_base = b >> 7;
  const uint8_t has_rsh = (b >> 6) & 1;
  uint8_t shr = 0;
  uint32_t base = 0;
  b &= 63;
  if (has_base) {
    bytep = VariableByte::decodeNum(bytep, bytep + 5, &base);
    CHECK_NOTNULL(bytep);
    CHECK_GT(base, 0);
  }
  if (has_rsh) {
    shr = *bytep++;
    CHECK(shr > 0 && shr < 32);
  }

  DVLOG(2) << "decoding " << block_size << " bestb: " << int(b) << " base " << base << " cexcept "
           << int(cexcept) << " shr: " << int(shr);
  src->in = unpackblock(src->in, out, b, block_size);

  if (cexcept > 0) {
    const uint8_t maxbits = *bytep++;
    DCHECK_LE(maxbits, 32);
    DCHECK_LT(b, maxbits);
    vector<uint32_t>::const_iterator & exceptionsptr = unpackpointers[maxbits - b - 1];
    for (uint32_t k = 0; k < cexcept; ++k) {
      const uint8_t pos = *(bytep++);
      out[pos] |= (*(exceptionsptr++)) << b;
    }
  }
  if (has_base | has_rsh) {
    for (uint32_t k = 0; k < block_size; ++k) {
      out[k] += base;
      out[k] <<= shr;
    }
  }
  DVLOG(2) << "bytep advanced by " << bytep - src->bytep;
  src->bytep = bytep;
}

 uint32_t FastPFor::maxCompressedLength(const size_t length) {
  // bytes: 4 global, 12 per page, 2 per block.
  const size_t num_blocks = length / BlockSize;
  const size_t BlocksInPage = PageSize / BlockSize;
  const size_t num_pages = (num_blocks + BlocksInPage - 1) / BlocksInPage;

  // due to imprefection of packingvector packing we could use 31 more integers for each bitwidth in
  // [1-32] range. 31*(32*33/2)/32 = 512 integers.
  constexpr size_t extraPadding = 512;

  uint32_t num_ints = 1 + num_pages * 3 +
    (num_blocks * (BlockSize*sizeof(uint32_t) + 10) + sizeof(uint32_t) - 1) / sizeof(uint32_t) +
      extraPadding;

  // Add variable byte encoding.
  num_ints += ((length % BlockSize) * (sizeof(uint32_t) + 1) +
                sizeof(uint32_t) - 1) / sizeof(uint32_t);
  return num_ints;
 }