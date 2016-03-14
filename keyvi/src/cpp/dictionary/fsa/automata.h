/* * keyvi - A key value store.
 *
 * Copyright 2015 Hendrik Muhs<hendrik.muhs@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * automata.h
 *
 *  Created on: May 12, 2014
 *      Author: hendrik
 */

#ifndef AUTOMATA_H_
#define AUTOMATA_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <sys/mman.h>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include "dictionary/fsa/internal/constants.h"
#include "dictionary/fsa/internal/value_store_factory.h"
#include "dictionary/fsa/internal/serialization_utils.h"
#include "dictionary/fsa/traversal/traversal_base.h"
#include "dictionary/fsa/traversal/weighted_traversal.h"
#include "dictionary/util/vint.h"
#include "dictionary/util/endian.h"
#include "dictionary/fsa/internal/intrinsics.h"
#include "dictionary/dictionary_merger_fwd.h"

//#define ENABLE_TRACING
#include "dictionary/util/trace.h"

namespace keyvi {
namespace dictionary {
namespace fsa {


class Automata
final {

   public:
    Automata(const char * filename, bool load_lazy=false) {
      file_ = open(filename, O_RDONLY);

      if (file_ == -1) {
        throw std::invalid_argument("file not found");
      }

      char magic[8];
      read(file_, magic, sizeof(magic));

      // check magic
      if (std::strncmp(magic, "KEYVIFSA", 8)){
        throw std::invalid_argument("not a keyvi file");
      }

      automata_properties_ = internal::SerializationUtils::ReadJsonRecord(file_);
      sparse_array_properties_ = internal::SerializationUtils::ReadJsonRecord(file_);

      compact_size_ = boost::lexical_cast<uint32_t> (sparse_array_properties_.get<std::string>("version")) == 2;
      size_t bucket_size = compact_size_ ? sizeof(uint16_t) : sizeof(uint32_t);

      // get start state and number of keys
      start_state_ = boost::lexical_cast<uint64_t> (automata_properties_.get<std::string>("start_state"));
      number_of_keys_ = boost::lexical_cast<uint64_t> (automata_properties_.get<std::string>("number_of_keys"));

      size_t offset = lseek(file_, 0, SEEK_CUR);

      size_t array_size = boost::lexical_cast<size_t>(sparse_array_properties_.get<std::string>("size"));

      lseek(file_, offset + array_size + bucket_size * array_size - 1, SEEK_SET);

      char ch = 0;
      // forward 1 position
      if (read(file_, &ch, sizeof(ch)) != sizeof(ch)) {
        throw std::invalid_argument("file is corrupt(truncated)");
      }

      TRACE("labels start offset: %d", offset);
      labels_start_offset_ = offset;

      TRACE("transitions start offset: %d", offset + array_size);
      transitions_start_offset_ = offset + array_size;

      TRACE("full file size %zu", offset + array_size + bucket_size * array_size);

      TRACE("value store position %zu", lseek(file, 0, SEEK_CUR));

      // initialize value store
      internal::value_store_t value_store_type =
          static_cast<internal::value_store_t>(
              boost::lexical_cast<int> (automata_properties_.get<std::string>(
              "value_store_type")));
      value_store_reader_ = internal::ValueStoreFactory::MakeReader(value_store_type, file_);
    }

    ~Automata() {
      close(file_);
      delete value_store_reader_;
    }

    Automata() = delete;
    Automata& operator=(Automata const&) = delete;
    Automata(const Automata& that) = delete;

    /**
     * Get the start(root) stage of the FSA
     *
     * @return index of root state.
     */
    uint64_t GetStartState() const {
      return start_state_;
    }

    uint64_t GetNumberOfKeys() const {
      return number_of_keys_;
    }

    uint64_t TryWalkTransition(uint64_t starting_state, unsigned char c) const {
      if (GetLabel(starting_state + c) == c) {
        return ResolvePointer(starting_state, c);
      }
      return 0;
    }

    /**
     * Get the outgoing states of state quickly in 1 step.
     *
     * @param starting_state The state
     * @param outgoing_states a vector given by reference to put n the outgoing states
     * @param outgoing_symbols a vector given by reference to put in the outgoing symbols (labels)
     *
     * todo: rewrite to avoid push_back on vectors, see: http://lemire.me/blog/2012/06/20/do-not-waste-time-with-stl-vectors/
     */
    template<class TransitionT, typename
    std::enable_if< std::is_base_of< traversal::Transition, TransitionT >::value, traversal::Transition>::type* = nullptr>
    void GetOutGoingTransitions(uint64_t starting_state, traversal::TraversalState<TransitionT>& traversal_state, traversal::TraversalPayload<TransitionT>& payload) const {
      // reset the state
      traversal_state.Clear();

      std::vector<char> labels_buf(256);
      pread(file_, &*labels_buf.begin(), labels_buf.size(), labels_start_offset_ + starting_state);

#if defined(KEYVI_SSE42)
      // Optimized version using SSE4.2, see http://www.strchr.com/strcmp_and_strlen_using_sse_4.2

      __m128i* labels_as_m128 = (__m128i *) &*labels_buf.begin();
      __m128i* mask_as_m128 = (__m128i *) (OUTGOING_TRANSITIONS_MASK);
      unsigned char symbol = 0;

      // check 16 bytes at a time
      for (int offset = 0; offset < 16; ++offset) {
        __m128i mask = _mm_cmpestrm(_mm_loadu_si128(labels_as_m128), 16,
                            _mm_loadu_si128(mask_as_m128), 16,
                            _SIDD_UBYTE_OPS|_SIDD_CMP_EQUAL_EACH|_SIDD_MASKED_POSITIVE_POLARITY|_SIDD_BIT_MASK);

        uint64_t mask_int = ((uint64_t*)&mask)[0];
        TRACE ("Bitmask %d", mask_int);

        if (mask_int != 0) {
          if (offset == 0) {
            // in this case we have to ignore the first bit, so start counting from 1
            mask_int = mask_int >> 1;
            for (auto i=1; i<16; ++i) {
              if ((mask_int & 1) == 1) {
                TRACE("push symbol+%d", symbol + i);
                traversal_state.Add(ResolvePointer(starting_state, symbol + i), symbol + i, payload);
              }
              mask_int = mask_int >> 1;
            }
          } else {
            for (auto i=0; i<16; ++i) {
              if ((mask_int & 1) == 1) {
                TRACE("push symbol+%d", symbol + i);
                traversal_state.Add(ResolvePointer(starting_state, symbol + i), symbol + i, payload);
              }
              mask_int = mask_int >> 1;
            }
          }
        }

        ++labels_as_m128;
        ++mask_as_m128;
        symbol +=16;
      }
#else
      uint64_t* labels_as_ll = (unsigned long int *) &*labels_buf.begin();
      uint64_t* mask_as_ll = (unsigned long int *) (OUTGOING_TRANSITIONS_MASK);
      unsigned char symbol = 0;

      // check 8 bytes at a time
      for (int offset = 0; offset < 32; ++offset) {

        uint64_t xor_labels_with_mask = *labels_as_ll^*mask_as_ll;

        if (((xor_labels_with_mask & 0x00000000000000ffULL) == 0) && offset > 0){
          traversal_state.Add(ResolvePointer(starting_state, symbol), symbol, payload);
        }
        if ((xor_labels_with_mask & 0x000000000000ff00ULL)== 0){
          traversal_state.Add(ResolvePointer(starting_state, symbol + 1), symbol + 1, payload);
        }
        if ((xor_labels_with_mask & 0x0000000000ff0000ULL)== 0){
          traversal_state.Add(ResolvePointer(starting_state, symbol + 2), symbol + 2, payload);
        }
        if ((xor_labels_with_mask & 0x00000000ff000000ULL)== 0){
          traversal_state.Add(ResolvePointer(starting_state, symbol + 3), symbol + 3, payload);
        }
        if ((xor_labels_with_mask & 0x000000ff00000000ULL)== 0){
          traversal_state.Add(ResolvePointer(starting_state, symbol + 4), symbol + 4, payload);
        }
        if ((xor_labels_with_mask & 0x0000ff0000000000ULL)== 0){
          traversal_state.Add(ResolvePointer(starting_state, symbol + 5), symbol + 5, payload);
        }
        if ((xor_labels_with_mask & 0x00ff000000000000ULL)== 0){
          traversal_state.Add(ResolvePointer(starting_state, symbol + 6), symbol + 6, payload);
        }
        if ((xor_labels_with_mask & 0xff00000000000000ULL)== 0){
          traversal_state.Add(ResolvePointer(starting_state, symbol + 7), symbol + 7, payload);
        }

        ++labels_as_ll;
        ++mask_as_ll;
        symbol +=8;
      }
#endif

      //post, e.g. sort transitions
      TRACE("postprocess transitions");
      traversal_state.PostProcess(payload);

      return;
    }

    template<class TransitionT, typename
    std::enable_if< std::is_base_of< traversal::WeightedTransition, TransitionT >::value, traversal::WeightedTransition>::type* = nullptr>
    inline void GetOutGoingTransitions(uint64_t starting_state, traversal::TraversalState<TransitionT>& traversal_state, traversal::TraversalPayload<TransitionT>& payload) const {
      // reset the state
      traversal_state.Clear();

      std::vector<char> labels_buf(256);
      pread(file_, &*labels_buf.begin(), labels_buf.size(), labels_start_offset_ + starting_state);

      uint32_t parent_weight = GetWeightValue(starting_state);

    #if defined(KEYVI_SSE42)
      // Optimized version using SSE4.2, see http://www.strchr.com/strcmp_and_strlen_using_sse_4.2

      __m128i* labels_as_m128 = (__m128i *) &*labels_buf.begin();
      __m128i* mask_as_m128 = (__m128i *) (OUTGOING_TRANSITIONS_MASK);
      unsigned char symbol = 0;

      // check 16 bytes at a time
      for (int offset = 0; offset < 16; ++offset) {
        __m128i mask = _mm_cmpestrm(_mm_loadu_si128(labels_as_m128), 16,
                            _mm_loadu_si128(mask_as_m128), 16,
                            _SIDD_UBYTE_OPS|_SIDD_CMP_EQUAL_EACH|_SIDD_MASKED_POSITIVE_POLARITY|_SIDD_BIT_MASK);

        uint64_t mask_int = ((uint64_t*)&mask)[0];
        TRACE ("Bitmask %d", mask_int);

        if (mask_int != 0) {
          if (offset == 0) {
            // in this case we have to ignore the first bit, so start counting from 1
            mask_int = mask_int >> 1;
            for (auto i=1; i<16; ++i) {
              if ((mask_int & 1) == 1) {
                TRACE("push symbol+%d", symbol + i);
                uint64_t child_state = ResolvePointer(starting_state, symbol + i);
                uint32_t weight = GetWeightValue(child_state);
                weight = weight != 0 ? weight : parent_weight;
                traversal_state.Add(child_state, weight, symbol + i, payload);
              }
              mask_int = mask_int >> 1;
            }
          } else {
            for (auto i=0; i<16; ++i) {
              if ((mask_int & 1) == 1) {
                TRACE("push symbol+%d", symbol + i);
                uint64_t child_state = ResolvePointer(starting_state, symbol + i);
                uint32_t weight = GetWeightValue(child_state);
                weight = weight != 0 ? weight : parent_weight;
                traversal_state.Add(child_state, weight, symbol + i, payload);
              }
              mask_int = mask_int >> 1;
            }
          }
        }

        ++labels_as_m128;
        ++mask_as_m128;
        symbol +=16;
      }
    #else
      uint64_t* labels_as_ll = (unsigned long int *) &*labels_buf.begin();
      uint64_t* mask_as_ll = (unsigned long int *) (OUTGOING_TRANSITIONS_MASK);
      unsigned char symbol = 0;

      // check 8 bytes at a time
      for (int offset = 0; offset < 32; ++offset) {

        uint64_t xor_labels_with_mask = *labels_as_ll^*mask_as_ll;

        if (((xor_labels_with_mask & 0x00000000000000ffULL) == 0) && offset > 0){
          uint64_t child_state = ResolvePointer(starting_state, symbol);
          uint32_t weight = GetWeightValue(child_state);
          weight = weight != 0 ? weight : parent_weight;
          traversal_state.Add(child_state, weight, symbol, payload);
        }
        if ((xor_labels_with_mask & 0x000000000000ff00ULL)== 0){
          uint64_t child_state = ResolvePointer(starting_state, symbol + 1);
          uint32_t weight = GetWeightValue(child_state);
          weight = weight != 0 ? weight : parent_weight;
          traversal_state.Add(child_state, weight, symbol + 1, payload);
        }
        if ((xor_labels_with_mask & 0x0000000000ff0000ULL)== 0){
          uint64_t child_state = ResolvePointer(starting_state, symbol + 2);
          uint32_t weight = GetWeightValue(child_state);
          weight = weight != 0 ? weight : parent_weight;
          traversal_state.Add(child_state, weight, symbol + 2, payload);
        }
        if ((xor_labels_with_mask & 0x00000000ff000000ULL)== 0){
          uint64_t child_state = ResolvePointer(starting_state, symbol + 3);
          uint32_t weight = GetWeightValue(child_state);
          weight = weight != 0 ? weight : parent_weight;
          traversal_state.Add(child_state, weight, symbol + 3, payload);
        }
        if ((xor_labels_with_mask & 0x000000ff00000000ULL)== 0){
          uint64_t child_state = ResolvePointer(starting_state, symbol + 4);
          uint32_t weight = GetWeightValue(child_state);
          weight = weight != 0 ? weight : parent_weight;
          traversal_state.Add(child_state, weight, symbol + 4, payload);
        }
        if ((xor_labels_with_mask & 0x0000ff0000000000ULL)== 0){
          uint64_t child_state = ResolvePointer(starting_state, symbol + 5);
          uint32_t weight = GetWeightValue(child_state);
          weight = weight != 0 ? weight : parent_weight;
          traversal_state.Add(child_state, weight, symbol + 5, payload);
        }
        if ((xor_labels_with_mask & 0x00ff000000000000ULL)== 0){
          uint64_t child_state = ResolvePointer(starting_state, symbol + 6);
          uint32_t weight = GetWeightValue(child_state);
          weight = weight != 0 ? weight : parent_weight;
          traversal_state.Add(child_state, weight, symbol + 6, payload);
        }
        if ((xor_labels_with_mask & 0xff00000000000000ULL)== 0){
          uint64_t child_state = ResolvePointer(starting_state, symbol + 7);
          uint32_t weight = GetWeightValue(child_state);
          weight = weight != 0 ? weight : parent_weight;
          traversal_state.Add(child_state, weight, symbol + 7, payload);
        }

        ++labels_as_ll;
        ++mask_as_ll;
        symbol +=8;
      }
    #endif

      //post, e.g. sort transitions
      TRACE("postprocess transitions");
      traversal_state.PostProcess(payload);

      return;
    }

    bool IsFinalState(uint64_t state_to_check) const {

      if (GetLabel(state_to_check + FINAL_OFFSET_TRANSITION) == FINAL_OFFSET_CODE) {
        return true;
      }
      return false;
    }

    uint64_t GetStateValue(uint64_t state) const {
      if (!compact_size_) {
        return be32toh(GetTransition(state + FINAL_OFFSET_TRANSITION));
      }

      // compact mode:
      return util::decodeVarshort(file_, transitions_start_offset_ + sizeof(uint16_t) * (state + FINAL_OFFSET_TRANSITION));
    }

    uint32_t GetWeightValue(uint64_t state) const {
      if (!compact_size_) {
        if (GetLabel(state + INNER_WEIGHT_TRANSITION) != 0) {
          return 0;
        }

        return be32toh(GetTransition(state + INNER_WEIGHT_TRANSITION));
      }

      if (GetLabel(state + INNER_WEIGHT_TRANSITION_COMPACT) != 0) {
        return 0;
      }

      return GetTransitionCompact(state + INNER_WEIGHT_TRANSITION_COMPACT);
    }

    internal::IValueStoreReader::attributes_t GetValueAsAttributeVector(uint64_t state_value) const {
      return value_store_reader_->GetValueAsAttributeVector(state_value);
    }

    std::string GetValueAsString(uint64_t state_value) const {
      return value_store_reader_->GetValueAsString(state_value);
    }

    std::string GetRawValueAsString(uint64_t state_value) const {
      return value_store_reader_->GetRawValueAsString(state_value);
    }

    std::string GetStatistics() const {
      std::ostringstream buf;
      buf << "General" << std::endl;
      boost::property_tree::write_json (buf, automata_properties_, false);
      buf << std::endl << "Persistence" << std::endl;
      boost::property_tree::write_json (buf, sparse_array_properties_, false);
      buf << std::endl << "Value Store" << std::endl;
      buf << value_store_reader_->GetStatistics();
      return buf.str();
    }

    boost::property_tree::ptree GetManifest() const {
      return automata_properties_.get_child("manifest", boost::property_tree::ptree());
    }

    std::string GetManifestAsString() const {
      std::ostringstream buf;

      const boost::property_tree::ptree &manifest = automata_properties_.get_child("manifest", boost::property_tree::ptree());
      boost::property_tree::write_json (buf, manifest, false);

      return buf.str();
    }

   private:
    boost::property_tree::ptree automata_properties_;
    boost::property_tree::ptree sparse_array_properties_;
    internal::IValueStoreReader* value_store_reader_;
    int file_;
    uint64_t labels_start_offset_;
    uint64_t transitions_start_offset_;
    bool compact_size_;
    uint64_t start_state_;
    uint64_t number_of_keys_;

    template<typename , typename>
    friend class ::keyvi::dictionary::DictionaryMerger;

    internal::IValueStoreReader* GetValueStore() const {
      return value_store_reader_;
    }

    inline uint64_t ResolvePointer(uint64_t starting_state, unsigned char c) const {
      if (!compact_size_) {
        return be32toh(GetTransition(starting_state + c));
      }

      uint16_t pt = le16toh(GetTransitionCompact(starting_state + c));
      uint64_t resolved_ptr;

      if ((pt & 0xC000) == 0xC000) {
        TRACE("Compact Transition uint16 absolute");

        resolved_ptr = pt & 0x3FFF;
        TRACE("Compact Transition after resolve %d", resolved_ptr);
        return resolved_ptr;
      }

      if (pt & 0x8000){
        TRACE("Compact Transition overflow %d (from %d) starting from %d", pt, starting_state + c, starting_state);

        // clear the first bit
        pt &= 0x7FFF;
        size_t overflow_bucket;
        TRACE("Compact Transition overflow bucket %d", pt);

        overflow_bucket = (pt >> 4) + starting_state + c - 512;

        TRACE("Compact Transition found overflow bucket %d", overflow_bucket);

        resolved_ptr = util::decodeVarshort(file_, transitions_start_offset_ + sizeof(uint16_t) * overflow_bucket);
        resolved_ptr = (resolved_ptr << 3) + (pt & 0x7);

        if (pt & 0x8){
          // relative coding
          resolved_ptr = (starting_state + c) - resolved_ptr + 512;
        }

      } else {
        TRACE("Compact Transition uint16 transition %d", pt);
          resolved_ptr = (starting_state + c) - pt + 512;

      }

      TRACE("Compact Transition after resolve %d", resolved_ptr);
      return resolved_ptr;
    }

    unsigned char GetLabel(uint64_t off) const
    {
      char ch = 0;
      pread(file_, &ch, sizeof(ch), labels_start_offset_ + off);
      return (unsigned char)ch;
    }

    uint32_t GetTransition(uint64_t off) const
    {
      uint32_t res = 0;
      pread(file_, (char*)&res, sizeof(res), transitions_start_offset_ + off * sizeof(uint32_t));
      return res;
    }

    uint16_t GetTransitionCompact(uint64_t off) const
    {
      uint16_t res = 0;
      pread(file_, (char*)&res, sizeof(res), transitions_start_offset_ + off * sizeof(uint16_t));
      return res;
    }

  };

  // shared pointer
  typedef std::shared_ptr<Automata> automata_t;

  } /* namespace fsa */
  } /* namespace dictionary */
  } /* namespace keyvi */

#endif /* AUTOMATA_H_ */
