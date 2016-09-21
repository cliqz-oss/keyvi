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
 * dictionary_compiler_common.h
 *
 *  Created on: Jul 17, 2014
 *      Author: hendrik
 */

#ifndef DICTIONARY_COMPILER_COMMON_H_
#define DICTIONARY_COMPILER_COMMON_H_

#include <algorithm>
#include <functional>
#include <boost/property_tree/ptree.hpp>
#include "tpie/serialization_sorter.h"

#include "dictionary/util/tpie_initializer.h"
#include "dictionary/fsa/internal/null_value_store.h"
#include "dictionary/fsa/internal/serialization_utils.h"
#include "dictionary/fsa/generator_adapter.h"
#include "dictionary/fsa/internal/constants.h"

//#define ENABLE_TRACING
#include "dictionary/util/trace.h"

namespace keyvi {
namespace dictionary {

/**
 * structure for internal processing
 * Note: Not using std::pair because it did not compile with Tpie
 */
struct key_value_pair {
  key_value_pair() : key(), value() {
  }

  key_value_pair(const std::string& k, const fsa::ValueHandle& v): key(k), value(v) {}

  bool operator<(const key_value_pair kv) const {
    return key < kv.key;
  }

  std::string key;
  fsa::ValueHandle value;
};
typedef key_value_pair key_value_t;
typedef const fsa::internal::IValueStoreWriter::vs_param_t compiler_param_t;
typedef std::function<void (size_t , size_t, void*)> callback_t;

template<class PersistenceT, class ValueStoreT = fsa::internal::NullValueStore>
class DictionaryCompilerBase {
 public:

  DictionaryCompilerBase(size_t memory_limit = 1073741824,
                         const compiler_param_t& params = compiler_param_t())
 : memory_limit_(memory_limit),
   params_(params) {

    if (params_.count(TEMPORARY_PATH_KEY) == 0) {
      params_[TEMPORARY_PATH_KEY] =
          boost::filesystem::temp_directory_path().string();
    }

    TRACE("tmp path set to %s", params_[TEMPORARY_PATH_KEY].c_str());

    if (params_.count(STABLE_INSERTS) > 0 && params_[STABLE_INSERTS] == "true") {
      // minimization has to be turned off in this case.
      params_[MINIMIZATION_KEY] = "off";
      stable_insert_ = true;
    }

    value_store_= new ValueStoreT(params_);
  }

  virtual ~DictionaryCompilerBase(){
    if (generator_) {
      delete generator_;
    } else {
      // if generator was not created we have to delete the value store ourselves
      delete value_store_;
    }
  }

  DictionaryCompilerBase& operator=(DictionaryCompilerBase const&) = delete;
  DictionaryCompilerBase(const DictionaryCompilerBase& that) = delete;

  virtual void Add(const std::string& input_key, typename ValueStoreT::value_t value =
      ValueStoreT::no_value) = 0;


#ifdef Py_PYTHON_H
  template<typename StringType>
  void __setitem__ (StringType input_key, typename ValueStoreT::value_t value =
      ValueStoreT::no_value) {
    return Add(input_key, value);
  }
#endif


  virtual void Compile(callback_t progress_callback = nullptr, void* user_data = nullptr) = 0;

  /**
   * Set a custom manifest to be embedded into the index file.
   *
   * @param manifest as JSON string
   */
  void SetManifestFromString(const std::string& manifest){
    SetManifest(fsa::internal::SerializationUtils::ReadJsonRecord(manifest));
  }

  /**
   * Set a custom manifest to be embedded into the index file.
   *
   * @param manifest
   */
  void SetManifest(const boost::property_tree::ptree& manifest){
    manifest_ = manifest;

    // if generator object is already there, set it otherwise cache it until it is created
    if (generator_) {
      generator_->SetManifest(manifest);
    }
  }

  void Write(std::ostream& stream) {
    generator_->Write(stream);
  }

  template<typename StringType>
  void WriteToFile(StringType filename) {
    std::ofstream out_stream(filename, std::ios::binary);
    generator_->Write(out_stream);
    out_stream.close();
  }

  fsa::internal::IValueStoreWriter::vs_param_t& GetParameters() const {
    return params_;
  }

 protected:
  void CreateGenerator(size_t size_of_keys);

  /**
   * Register a value before inserting the key(for optimization purposes).
   *
   * @param value The Value
   * @return a handle that later needs to be passed to Add()
   */
  fsa::ValueHandle RegisterValue(typename ValueStoreT::value_t value =
      ValueStoreT::no_value) const {

    fsa::ValueHandle handle;
    handle.no_minimization = false;

    handle.value_idx = value_store_->GetValue(value, handle.no_minimization);

    // if inner weights are used update them
    handle.weight = value_store_->GetWeightValue(value);

    return handle;
  }

  void CloseValueStore() const {
    value_store_->CloseFeeding();
  }

  bool IsStableInsert() const {
    return stable_insert_;
  }

  void AddToGenerator(const std::string& input_key, const fsa::ValueHandle& handle) const {
    generator_->Add(input_key, handle);
  }

  void CloseGenerator() const {
    generator_->CloseFeeding();
  }

  fsa::internal::IValueStoreWriter::vs_param_t& GetParameters(){
    return params_;
  }


 private:
  size_t memory_limit_;
  fsa::internal::IValueStoreWriter::vs_param_t params_;
  ValueStoreT* value_store_;
  fsa::GeneratorAdapterInterface<PersistenceT, ValueStoreT>* generator_ = nullptr;
  boost::property_tree::ptree manifest_ = boost::property_tree::ptree();
  bool stable_insert_ = false;
};


template<class PersistenceT, class ValueStoreT>
inline void DictionaryCompilerBase<PersistenceT, ValueStoreT>::CreateGenerator(size_t size_of_keys)
{
  // todo: find good parameters for auto-guessing this
  if (size_of_keys > UINT32_MAX){
    if (memory_limit_ > 0x280000000UL /* 10 GB */)  {
      generator_ = new fsa::GeneratorAdapter<PersistenceT, ValueStoreT, uint64_t, int64_t>(memory_limit_, params_, value_store_);
    } else {
      generator_ = new fsa::GeneratorAdapter<PersistenceT, ValueStoreT, uint64_t, int32_t>(memory_limit_, params_, value_store_);
    }
  } else {
    if (memory_limit_ > 0x140000000UL) /* 5GB */ {
      generator_ = new fsa::GeneratorAdapter<PersistenceT, ValueStoreT, uint32_t, int64_t>(memory_limit_, params_, value_store_);
    } else {
      generator_ = new fsa::GeneratorAdapter<PersistenceT, ValueStoreT, uint32_t, int32_t>(memory_limit_, params_, value_store_);
    }
  }

  // set the manifest
  generator_->SetManifest(manifest_);
}

} /* namespace dictionary */
} /* namespace keyvi */

#endif /* DICTIONARY_COMPILER_COMMIN_H_ */
