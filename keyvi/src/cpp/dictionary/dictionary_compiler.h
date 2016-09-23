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
 * dictionary_compiler.h
 *
 *  Created on: Jul 17, 2014
 *      Author: hendrik
 */

#ifndef DICTIONARY_COMPILER_H_
#define DICTIONARY_COMPILER_H_

#include <algorithm>
#include <functional>
#include "tpie/serialization_sorter.h"

#include "dictionary/dictionary_compiler_base.h"
#include "dictionary/util/tpie_initializer.h"
#include "dictionary/fsa/internal/null_value_store.h"
#include "dictionary/fsa/internal/serialization_utils.h"
#include "dictionary/fsa/internal/constants.h"

//#define ENABLE_TRACING
#include "dictionary/util/trace.h"

namespace keyvi {
namespace dictionary {

/**
 * Tpie serialization and deserialization for sorting.
 */
template<typename Dst>
void serialize(Dst & d, const keyvi::dictionary::key_value_pair & pt) {
  using tpie::serialize;
  serialize(d, pt.key);
  serialize(d, pt.value);
}
template<typename Src>
void unserialize(Src & s, keyvi::dictionary::key_value_pair & pt) {
  using tpie::unserialize;
  unserialize(s, pt.key);
  unserialize(s, pt.value);
}

/**
 * Dictionary Compiler
 */
template<class PersistenceT, class ValueStoreT = fsa::internal::NullValueStore>
class DictionaryCompiler final: public DictionaryCompilerBase <PersistenceT, ValueStoreT>
  {
   public:
    /**
     * Instantiate a dictionary compiler.
     *
     * Note the memory limit only limits the memory used for internal buffers,
     * memory usage for small short-lived objects and the library itself is part of the limit.
     *
     * @param memory_limit memory limit for internal memory usage
     */
    DictionaryCompiler(size_t memory_limit = 1073741824,
                       const compiler_param_t& params = compiler_param_t())
        : DictionaryCompilerBase<PersistenceT, ValueStoreT>(memory_limit, params),
          initializer_(util::TpieIntializer::getInstance()),
          sorter_(),
          size_of_keys_(0)
    {
      sorter_.set_available_memory(memory_limit);
      sorter_.begin();

      // set temp path for tpie
      initializer_.SetTempDirectory(this->GetParameters()[TEMPORARY_PATH_KEY]);
    }

    virtual ~DictionaryCompiler(){}

    DictionaryCompiler& operator=(DictionaryCompiler const&) = delete;
    DictionaryCompiler(const DictionaryCompiler& that) = delete;

    virtual void Add(const std::string& input_key, typename ValueStoreT::value_t value =
                 ValueStoreT::no_value) override {
      size_of_keys_ += input_key.size();
      sorter_.push(key_value_t(std::move(input_key), this->RegisterValue(value)));
    }

    virtual void Delete(const std::string& input_key) override {
      if (!this->IsStableInsert()) {
        throw compiler_exception("delete only available when using stable_inserts option");
      }

      sorter_.push(key_value_t(std::move(input_key), this->DeletionMarker()));
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
    virtual void Compile(callback_t progress_callback = nullptr, void* user_data = nullptr) override {
      this->CloseValueStore();
      sorter_.end();
      sorter_.merge_runs();
      size_t added_key_values = 0;
      size_t callback_trigger = 0;
      this->CreateGenerator(size_of_keys_);

      // check that at least 1 item is there
      if (sorter_.can_pull()) {
        size_t number_of_items = sorter_.item_count();

        callback_trigger = 1+(number_of_items-1)/100;

        if (callback_trigger > 100000) {
          callback_trigger = 100000;
        }

        if (!this->IsStableInsert()) {

          while (sorter_.can_pull()) {
            key_value_t key_value = sorter_.pull();

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

          key_value_t last_key_value = sorter_.pull();

          while (sorter_.can_pull()) {
            key_value_t key_value = sorter_.pull();

            // dedup with last one wins
            if (last_key_value.key == key_value.key) {
              TRACE("Detected duplicated keys, dedup them, last one wins.");

              // check the counter to determine which key_value has been added last
              if (last_key_value.value.count < key_value.value.count) {
                last_key_value = key_value;
              }
              continue;
            }

            if (!last_key_value.value.deleted) {
              TRACE("adding to generator: %s", last_key_value.key.c_str());
              this->AddToGenerator(std::move(last_key_value.key), last_key_value.value);
              ++added_key_values;
              if (progress_callback && (added_key_values % callback_trigger == 0)){
                progress_callback(added_key_values, number_of_items, user_data);
              }
            } else {
              TRACE("skipping deleted key: %s", last_key_value.key.c_str());
            }

            last_key_value = key_value;
          }

          // add the last one
          TRACE("adding to generator: %s", last_key_value.key.c_str());
          if (!last_key_value.value.deleted) {
            this->AddToGenerator(std::move(last_key_value.key), last_key_value.value);
          }
          ++added_key_values;

        }
      }

      this->CloseGenerator();
    }

   private:
    util::TpieIntializer& initializer_;
    tpie::serialization_sorter<key_value_t> sorter_;
    size_t size_of_keys_;
};

} /* namespace dictionary */
} /* namespace keyvi */

#endif /* DICTIONARY_COMPILER_H_ */
