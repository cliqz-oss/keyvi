/*
 * dictionary_compiler_base_test.cpp
 *
 *  Created on: Sep 23, 2016
 *      Author: hendrik
 */


#include <stdio.h>
#include <boost/test/unit_test.hpp>

#include "dictionary/dictionary_compiler_small.h"
#include "dictionary/dictionary_compiler.h"
#include "dictionary/dictionary.h"
#include "dictionary/fsa/automata.h"
#include "dictionary/fsa/internal/sparse_array_persistence.h"
#include "dictionary/fsa/internal/int_value_store.h"
#include "dictionary/fsa/internal/json_value_store.h"
#include "dictionary/fsa/entry_iterator.h"

namespace keyvi {
namespace dictionary {

BOOST_AUTO_TEST_SUITE( DictionaryCompilerBaseTests )

typedef keyvi::dictionary::DictionaryCompilerBase<
  keyvi::dictionary::fsa::internal::SparseArrayPersistence<uint16_t>,
  keyvi::dictionary::fsa::internal::JsonValueStore> JsonDictionaryCompilerBase;

typedef std::shared_ptr<JsonDictionaryCompilerBase> JsonDictionaryCompilerBaseShared;


class DictionaryCompilerUtils final{
 public:
  static JsonDictionaryCompilerBaseShared GetCompiler(const std::string& type, keyvi::dictionary::compiler_param_t params){
    if (type == "small") {
        return std::make_shared<keyvi::dictionary::DictionaryCompilerSmall<
            fsa::internal::SparseArrayPersistence<uint16_t>,
            fsa::internal::JsonValueStore>>(1048576, params);
    } if (type == "tpie") {
      return std::make_shared<keyvi::dictionary::DictionaryCompiler<
          fsa::internal::SparseArrayPersistence<uint16_t>,
          fsa::internal::JsonValueStore>>(10485760, params);
  }
    throw std::logic_error("unknown type");
  }

  static std::vector<std::string> GetTypes(){
    std::vector<std::string> types {"small", "tpie"};

    return types;
  }
};

BOOST_AUTO_TEST_CASE( addAndDeletes ) {
  keyvi::dictionary::compiler_param_t params = {{STABLE_INSERTS, "true"}};

  for (auto type : DictionaryCompilerUtils::GetTypes()){

    JsonDictionaryCompilerBaseShared compiler = DictionaryCompilerUtils::GetCompiler(type, params);

    // add, delete, add again
    compiler->Add("aa", "1");
    compiler->Delete("aa");
    compiler->Delete("aa");
    compiler->Add("aa", "2");

    // delete, add
    compiler->Delete("bb");
    compiler->Add("bb", "1");

    // add, delete, last item
    compiler->Add("zz", "1");
    compiler->Delete("zz");

    compiler->Compile();

    boost::filesystem::path temp_path =
        boost::filesystem::temp_directory_path();

    temp_path /= boost::filesystem::unique_path(
          "dictionary-unit-test-dictionarycompiler-%%%%-%%%%-%%%%-%%%%");
    std::string file_name = temp_path.native();

    compiler->WriteToFile(file_name);

    Dictionary d(file_name.c_str());

    fsa::automata_t f(d.GetFsa());

    fsa::EntryIterator it(f);
    fsa::EntryIterator end_it;

    BOOST_CHECK_EQUAL("aa", it.GetKey());
    BOOST_CHECK_EQUAL("2", it.GetValueAsString());
    ++it;

    BOOST_CHECK_EQUAL("bb", it.GetKey());
    BOOST_CHECK_EQUAL("1", it.GetValueAsString());
    ++it;

    BOOST_CHECK(it == end_it);

    std::remove(file_name.c_str());
  }
}

BOOST_AUTO_TEST_CASE( DeleteUnsupported ) {
  keyvi::dictionary::compiler_param_t params = {};

  for (auto type : DictionaryCompilerUtils::GetTypes()){

    JsonDictionaryCompilerBaseShared compiler = DictionaryCompilerUtils::GetCompiler(type, params);

    // add, delete, add again
    compiler->Add("aa", "1");
    BOOST_CHECK_THROW( compiler->Delete("aa"), compiler_exception);
  }
}

BOOST_AUTO_TEST_SUITE_END()

} /* namespace dictionary */
} /* namespace keyvi */

