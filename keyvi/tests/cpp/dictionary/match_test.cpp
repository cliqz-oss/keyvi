//
// keyvi - A key value store.
//
// Copyright 2015 Hendrik Muhs<hendrik.muhs@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//


/*
 * match_test.cpp
 *
 *  Created on: Oct 20, 2016
 *      Author: hendrik
 */

#include <thread>
#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include "dictionary/fsa/generator.h"
#include "dictionary/fsa/internal/sparse_array_persistence.h"
#include "dictionary/dictionary.h"
#include "dictionary/testing/temp_dictionary.h"
namespace keyvi {
namespace dictionary {
BOOST_AUTO_TEST_SUITE( MatchTests )

BOOST_AUTO_TEST_CASE( MatchExp ) {
  std::vector<std::pair<std::string, uint32_t>> test_data = { { "test", 22 }, {
      "otherkey", 24 }, { "other", 444 }, { "bar", 200 }, };

  testing::TempDictionary dictionary(test_data);
  dictionary_t d(new Dictionary(dictionary.GetFsa()));

  bool matched = false;
  for (auto m : d->Get("test")){
      BOOST_CHECK_EQUAL("test", m.GetMatchedString());
      BOOST_CHECK_EQUAL(std::string("22"), boost::get<std::string>(m.GetAttribute("weight")));
      matched=true;
  }
  BOOST_CHECK(matched);

  matched = false;
  for (auto m : d->Get("test2")){
      matched=true;
  }

  BOOST_CHECK(!matched);

  auto m = (*d)["test"];
  BOOST_CHECK_EQUAL("22", boost::get<std::string>(m.GetAttribute("weight")));
}

BOOST_AUTO_TEST_SUITE_END()

} /* namespace dictionary */
} /* namespace keyvi */




