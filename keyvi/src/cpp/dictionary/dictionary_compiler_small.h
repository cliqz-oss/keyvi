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
 * dictionary_compiler_small.h
 *
 *  Created on: Sep 17, 2016
 *      Author: hendrik
 */

#ifndef DICTIONARY_COMPILER_SMALL_H_
#define DICTIONARY_COMPILER_SMALL_H_

#include <algorithm>
#include <functional>
#include <boost/property_tree/ptree.hpp>

#include "dictionary/dictionary_compiler_base.h"
#include "dictionary/fsa/internal/null_value_store.h"
#include "dictionary/fsa/internal/serialization_utils.h"
#include "dictionary/fsa/generator_adapter.h"
#include "dictionary/fsa/internal/constants.h"

//#define ENABLE_TRACING
#include "dictionary/util/trace.h"

namespace keyvi {
namespace dictionary {

/**
 * Simple implementation of Dictionary Compiler
 */
template<class PersistenceT, class ValueStoreT = fsa::internal::NullValueStore>
class DictionaryCompilerSmall final : public DictionaryCompilerBase <PersistenceT, ValueStoreT> {
   public:
    /**
     * Instantiate a dictionary compiler (small, simple version), to be used with small data sets.
     *
     * Note the memory limit only limits the memory used for internal buffers,
     * memory usage for small short-lived objects and the library itself is part of the limit.
     *
     * @param memory_limit memory limit for internal memory usage
     */
    DictionaryCompilerSmall(size_t memory_limit = 10485760,
                       const compiler_param_t& params = compiler_param_t())
        : DictionaryCompilerBase<PersistenceT, ValueStoreT>(memory_limit, params),
          key_values_(),
          size_of_keys_(0) {}

    ~DictionaryCompilerSmall(){}

    DictionaryCompilerSmall& operator=(DictionaryCompilerSmall const&) = delete;
    DictionaryCompilerSmall(const DictionaryCompilerSmall& that) = delete;

    void Add(const std::string& input_key, typename ValueStoreT::value_t value =
                 ValueStoreT::no_value) {

      size_of_keys_ += input_key.size();
      key_values_.push_back(key_value_t(std::move(input_key), this->RegisterValue(value)));
    }

#ifdef Py_PYTHON_H
    template<typename StringType>
    void __setitem__ (StringType input_key, typename ValueStoreT::value_t value =
        ValueStoreT::no_value) {
      return Add(input_key, value);
    }
#endif

    /**
     * Do the final compilation
     */
    void Compile(callback_t progress_callback = nullptr, void* user_data = nullptr) {
      this->CloseValueStore();
      this->CreateGenerator(size_of_keys_);
      size_t added_key_values = 0;
      size_t callback_trigger = 0;
      if (key_values_.size() > 0)
      {
        size_t number_of_items = key_values_.size();

        callback_trigger = 1+(number_of_items-1)/100;

        if (callback_trigger > 100000) {
          callback_trigger = 100000;
        }

        std::sort(key_values_.begin(), key_values_.end());

        if (!this->IsStableInsert()) {

          for (auto key_value: key_values_) {
            TRACE("adding to generator: %s", key_value.key.c_str());

            this->AddToGenerator(std::move(key_value.key), key_value.value);
            ++added_key_values;
            if (progress_callback && (added_key_values % callback_trigger == 0)){
              progress_callback(added_key_values, number_of_items, user_data);
            }
          }

        } else {

          // special mode for stable (incremental) inserts, in this case we have to respect the order and take
          // the last value if keys are equal

          auto key_values_it = key_values_.begin();
          key_value_t last_key_value = *key_values_it++;

          while (key_values_it != key_values_.end())
          {
            key_value_t key_value = *key_values_it++;

            // dedup with last one wins
            if (last_key_value.key == key_value.key) {
              TRACE("Detected duplicated keys, dedup them, last one wins.");

              // we know that last added values have a lower id (minimization is turned off)
              if (last_key_value.value.value_idx < key_value.value.value_idx) {
                last_key_value = key_value;
              }
              continue;
            }

            TRACE("adding to generator: %s", last_key_value.key.c_str());

            this->AddToGenerator(std::move(last_key_value.key), last_key_value.value);
            ++added_key_values;
            if (progress_callback && (added_key_values % callback_trigger == 0)){
              progress_callback(added_key_values, number_of_items, user_data);
            }

            last_key_value = key_value;
          }

          // add the last one
          TRACE("adding to generator: %s", last_key_value.key.c_str());

          this->AddToGenerator(std::move(last_key_value.key), last_key_value.value);
          ++added_key_values;
        }
      }

      this->CloseGenerator();
    }

   private:
    std::vector<key_value_pair> key_values_;
    size_t size_of_keys_ = 0;
};


} /* namespace dictionary */
} /* namespace keyvi */

#endif /* DICTIONARY_COMPILER_SMALL_H_ */
