/**
 * This code is released under the
 * Apache License Version 2.0 http://www.apache.org/licenses/.
 *
 * (c) Daniel Lemire, http://lemire.me/en/
 */

#ifndef _UTIL_CODING_EPFOR_H_
#define _UTIL_CODING_EPFOR_H_

#include <cstdint>
#include <vector>
#include <cstddef>

/**
 * FastPFor
 *
 *
 * Designed by D. Lemire. This scheme is NOT patented.
 *
 * Reference and documentation:
 *
 * Daniel Lemire and Leonid Boytsov, Decoding billions of integers per second through vectorization
 * Software: Practice & Experience
 * http://arxiv.org/abs/1209.2137
 * http://onlinelibrary.wiley.com/doi/10.1002/spe.2203/abstract
 *
 */
class FastPFor {
public:
  /**
   * ps (page size) should be a multiple of BlockSize, any "large"
   * value should do.
   */
   enum {
      BlockSizeInUnitsOfPackSize = 4,
      PACKSIZE = 32,
      overheadofeachexcept = 8,
      BlockSize = BlockSizeInUnitsOfPackSize * PACKSIZE  // 128
  };

  explicit FastPFor(uint32_t ps = 65536);

  // sometimes, mem. usage can grow too much, this clears it up
  void resetBuffer();

  const uint32_t PageSize;

  const char* name() const {
    return "FastPFor";
  }

  const uint32_t * decodeArray(const uint32_t *in, size_t length,
                               uint32_t *out, size_t &nvalue);

  /*
   * The input size (length) should be a multiple of BlockSize (This was done
   * to simplify slightly the implementation.)
   */
  void encodeArray(const uint32_t *in, const size_t length, uint32_t *out,
                   size_t &nvalue);

  static uint32_t uncompressedLength(const uint32_t *in, const size_t length);

  uint32_t maxCompressedLength(const size_t length);  // requires PageSize.

private:
  std::vector<std::vector<uint32_t>> datatobepacked;
  std::vector<uint8_t> bytescontainer;
  uint32_t base_reduced_[BlockSize];
  uint8_t* bc_ptr_;

  struct CodeParams {
    uint8_t bestb = 0;
    uint8_t bestcexcept = 0;
    uint8_t maxb = 0;
    uint8_t shr = 0;
    uint32_t min_val = 0;
  };

  void getBestParams(const uint32_t * in, const uint32_t block_size, CodeParams* params);

  void __encodeArray(const uint32_t *in, const size_t length, uint32_t *out,
          size_t & nvalue);
  uint32_t * __encodeBlock(const uint32_t *in, const size_t block_size, uint32_t *out);

  void __decodeArray(const uint32_t *in, size_t & length, uint32_t *out,
         size_t nvalue);

  struct Source {
    const uint32_t *in;
    const uint8_t * bytep;
  };

  void __decodeBlock(const uint32_t block_size, std::vector<uint32_t>::const_iterator unpackpointers[],
                     Source* src, uint32_t *out);
};


#endif /* _UTIL_CODING_EPFOR_H_ */