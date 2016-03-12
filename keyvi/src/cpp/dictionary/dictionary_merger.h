/* * keyvi - A key value store.
 *
 * Copyright 2015, 2016 Hendrik Muhs<hendrik.muhs@gmail.com>
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
 * dictionary_merger.h
 *
 *  Created on: Feb 27, 2016
 *      Author: hendrik
 */

#ifndef DICTIONARY_MERGER_H_
#define DICTIONARY_MERGER_H_

#include <queue>

#include "dictionary/fsa/generator.h"
#include "dictionary/fsa/automata.h"
#include "dictionary/fsa/entry_iterator.h"

//#define ENABLE_TRACING
#include "dictionary/util/trace.h"

namespace keyvi {
namespace dictionary {

template<class PersistenceT, class ValueStoreT = fsa::internal::NullValueStore>
class DictionaryMerger
final {
 public:
  DictionaryMerger(): dicts_to_merge_(){
  }

  void Add (const std::string& filename){
    fsa::automata_t fsa (new fsa::Automata(filename.c_str()));

    if (fsa->GetValueStore()->GetValueStoreType() != ValueStoreT::GetValueStoreType()) {
      throw std::invalid_argument("Dictionaries must have the same type.");
    }

    dicts_to_merge_.push_back(fsa);
  }

  void Merge(const std::string& filename){
    std::priority_queue<std::pair<fsa::EntryIterator, int>,
                        std::vector<std::pair<fsa::EntryIterator, int>>,
                        std::greater<std::pair<fsa::EntryIterator, int>>> pqueue;
    fsa::EntryIterator end_it;

    int i = 0;
    for (auto fsa: dicts_to_merge_) {
      fsa::EntryIterator e_it(fsa);
      pqueue.push(std::make_pair(e_it, i++));
    }

    ValueStoreT* value_store = new ValueStoreT();

    fsa::Generator<PersistenceT, ValueStoreT> generator(1073741824, fsa::generator_param_t(), value_store);

    while(!pqueue.empty()){
      auto e = pqueue.top();
      pqueue.pop();

      // todo: check for same keys and merge only the last one

      fsa::ValueHandle handle;
      handle.no_minimization = false;

      // Todo: if inner weights are used update them
      //handle.weight = value_store_->GetWeightValue(value);

      handle.value_idx = value_store->GetValue(e.first.GetFsa()->GetValueStore()->GetValueStorePayload(),
                                               e.first.GetValueId(),
                                               handle.no_minimization);

      generator.Add(e.first.GetKey(), handle);

      if (++e.first != end_it) {
        pqueue.push(e);
      }
    }
    generator.CloseFeeding();

    generator.WriteToFile(filename);
  }

 private:
  std::vector<fsa::automata_t> dicts_to_merge_;

};

} /* namespace dictionary */
} /* namespace keyvi */


#endif /* SRC_CPP_DICTIONARY_DICTIONARY_MERGER_H_ */