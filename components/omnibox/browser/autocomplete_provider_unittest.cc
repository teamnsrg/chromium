// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_provider.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "net/url_request/url_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

static std::ostream& operator<<(std::ostream& os,
                                const AutocompleteResult::const_iterator& it) {
  return os << static_cast<const AutocompleteMatch*>(&(*it));
}

namespace {

const size_t kResultsPerProvider = 3;
const char kTestTemplateURLKeyword[] = "t";

class TestingSchemeClassifier : public AutocompleteSchemeClassifier {
 public:
  TestingSchemeClassifier() {}

  metrics::OmniboxInputType GetInputTypeForScheme(
      const std::string& scheme) const override {
    return net::URLRequest::IsHandledProtocol(scheme)
               ? metrics::OmniboxInputType::URL
               : metrics::OmniboxInputType::INVALID;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingSchemeClassifier);
};

// AutocompleteProviderClient implementation that calls the specified closure
// when the result is ready.
class AutocompleteProviderClientWithClosure
    : public MockAutocompleteProviderClient {
 public:
  AutocompleteProviderClientWithClosure() {}

  void set_closure(const base::Closure& closure) { closure_ = closure; }

 private:
  void OnAutocompleteControllerResultReady(
      AutocompleteController* controller) override {
    if (!closure_.is_null())
      closure_.Run();
    if (base::RunLoop::IsRunningOnCurrentThread())
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

  base::Closure closure_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteProviderClientWithClosure);
};

}  // namespace

// Autocomplete provider that provides known results. Note that this is
// refcounted so that it can also be a task on the message loop.
class TestProvider : public AutocompleteProvider {
 public:
  TestProvider(int relevance,
               const base::string16& prefix,
               const base::string16 match_keyword,
               AutocompleteProviderClient* client)
      : AutocompleteProvider(AutocompleteProvider::TYPE_SEARCH),
        listener_(nullptr),
        relevance_(relevance),
        prefix_(prefix),
        match_keyword_(match_keyword),
        client_(client) {}

  void Start(const AutocompleteInput& input, bool minimal_changes) override;

  void set_listener(AutocompleteProviderListener* listener) {
    listener_ = listener;
  }

 private:
  ~TestProvider() override {}

  void Run();

  void AddResults(int start_at, int num);
  void AddResultsWithSearchTermsArgs(
      int start_at,
      int num,
      AutocompleteMatch::Type type,
      const TemplateURLRef::SearchTermsArgs& search_terms_args);

  AutocompleteProviderListener* listener_;
  int relevance_;
  const base::string16 prefix_;
  const base::string16 match_keyword_;
  AutocompleteProviderClient* client_;

  DISALLOW_COPY_AND_ASSIGN(TestProvider);
};

void TestProvider::Start(const AutocompleteInput& input, bool minimal_changes) {
  if (minimal_changes)
    return;

  matches_.clear();

  if (input.from_omnibox_focus())
    return;

  // Generate 4 results synchronously, the rest later.
  AddResults(0, 1);
  AddResultsWithSearchTermsArgs(
      1, 1, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
      TemplateURLRef::SearchTermsArgs(base::ASCIIToUTF16("echo")));
  AddResultsWithSearchTermsArgs(
      2, 1, AutocompleteMatchType::NAVSUGGEST,
      TemplateURLRef::SearchTermsArgs(base::ASCIIToUTF16("nav")));
  AddResultsWithSearchTermsArgs(
      3, 1, AutocompleteMatchType::SEARCH_SUGGEST,
      TemplateURLRef::SearchTermsArgs(base::ASCIIToUTF16("query")));

  if (input.want_asynchronous_matches()) {
    done_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&TestProvider::Run, this));
  }
}

void TestProvider::Run() {
  AddResults(1, kResultsPerProvider);
  done_ = true;
  DCHECK(listener_);
  listener_->OnProviderUpdate(true);
}

void TestProvider::AddResults(int start_at, int num) {
  AddResultsWithSearchTermsArgs(
      start_at, num, AutocompleteMatchType::URL_WHAT_YOU_TYPED,
      TemplateURLRef::SearchTermsArgs(base::string16()));
}

void TestProvider::AddResultsWithSearchTermsArgs(
    int start_at,
    int num,
    AutocompleteMatch::Type type,
    const TemplateURLRef::SearchTermsArgs& search_terms_args) {
  for (int i = start_at; i < num; i++) {
    AutocompleteMatch match(this, relevance_ - i, false, type);

    match.fill_into_edit = prefix_ + base::UTF8ToUTF16(base::NumberToString(i));
    match.destination_url = GURL(base::UTF16ToUTF8(match.fill_into_edit));
    match.allowed_to_be_default_match = true;

    match.contents = match.fill_into_edit;
    match.contents_class.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));
    match.description = match.fill_into_edit;
    match.description_class.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));
    match.search_terms_args.reset(
        new TemplateURLRef::SearchTermsArgs(search_terms_args));
    if (!match_keyword_.empty()) {
      match.keyword = match_keyword_;
      ASSERT_NE(nullptr,
                match.GetTemplateURL(client_->GetTemplateURLService(), false));
    }

    matches_.push_back(match);
  }
}

// Helper class to make running tests of ClassifyAllMatchesInString() more
// convenient.
class ClassifyTest {
 public:
  ClassifyTest(const base::string16& text,
               const bool text_is_query,
               ACMatchClassifications matches);
  ~ClassifyTest();

  ACMatchClassifications RunTest(const base::string16& find_text);

 private:
  const base::string16 text_;
  const bool text_is_query_;
  const ACMatchClassifications matches_;
};

ClassifyTest::ClassifyTest(const base::string16& text,
                           const bool text_is_query,
                           ACMatchClassifications matches)
    : text_(text), text_is_query_(text_is_query), matches_(matches) {}

ClassifyTest::~ClassifyTest() {}

ACMatchClassifications ClassifyTest::RunTest(const base::string16& find_text) {
  return AutocompleteProvider::ClassifyAllMatchesInString(
      find_text, AutocompleteProvider::CreateWordMapForString(find_text), text_,
      text_is_query_, matches_);
}

class AutocompleteProviderTest : public testing::Test {
 public:
  AutocompleteProviderTest();
  ~AutocompleteProviderTest() override;

 protected:
  struct KeywordTestData {
    const base::string16 fill_into_edit;
    const base::string16 keyword;
    const base::string16 expected_associated_keyword;
  };

  struct AssistedQueryStatsTestData {
    const AutocompleteMatch::Type match_type;
    const std::string expected_aqs;
  };

  // Registers a test TemplateURL under the given keyword.
  void RegisterTemplateURL(const base::string16 keyword,
                           const std::string& template_url);

  // Resets |controller_| with two TestProviders.  |provider1_ptr| and
  // |provider2_ptr| are updated to point to the new providers if non-NULL.
  void ResetControllerWithTestProviders(bool same_destinations,
                                        TestProvider** provider1_ptr,
                                        TestProvider** provider2_ptr);

  // Runs a query on the input "a", and makes sure both providers' input is
  // properly collected.
  void RunTest();

  // Constructs an AutocompleteResult from |match_data|, sets the |controller_|
  // to pretend it was running against input |input|, calls the |controller_|'s
  // UpdateAssociatedKeywords, and checks that the matches have associated
  // keywords as expected.
  void RunKeywordTest(const base::string16& input,
                      const KeywordTestData* match_data,
                      size_t size);

  void RunAssistedQueryStatsTest(
      const AssistedQueryStatsTestData* aqs_test_data,
      size_t size);

  void RunQuery(const std::string& query, bool allow_exact_keyword_match);

  void ResetControllerWithKeywordAndSearchProviders();
  void ResetControllerWithKeywordProvider();
  void RunExactKeymatchTest(bool allow_exact_keyword_match);

  void CopyResults();

  // Returns match.destination_url as it would be set by
  // AutocompleteController::UpdateMatchDestinationURL().
  GURL GetDestinationURL(AutocompleteMatch match,
                         base::TimeDelta query_formulation_time) const;

  void set_search_provider_field_trial_triggered_in_session(bool val) {
    controller_->search_provider_->set_field_trial_triggered_in_session(val);
  }
  bool search_provider_field_trial_triggered_in_session() {
    return controller_->search_provider_->field_trial_triggered_in_session();
  }
  void set_current_page_classification(
      metrics::OmniboxEventProto::PageClassification classification) {
    controller_->input_.current_page_classification_ = classification;
  }

  AutocompleteResult result_;

 private:
  // Resets the controller with the given |type|. |type| is a bitmap containing
  // AutocompleteProvider::Type values that will (potentially, depending on
  // platform, flags, etc.) be instantiated.
  void ResetControllerWithType(int type);

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<AutocompleteController> controller_;
  // Owned by |controller_|.
  AutocompleteProviderClientWithClosure* client_;
  // Used to ensure that |client_| ownership has been passed to |controller_|
  // exactly once.
  bool client_owned_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteProviderTest);
};

AutocompleteProviderTest::AutocompleteProviderTest()
    : client_(new AutocompleteProviderClientWithClosure()),
      client_owned_(false) {
  client_->set_template_url_service(
      std::make_unique<TemplateURLService>(nullptr, 0));
}

AutocompleteProviderTest::~AutocompleteProviderTest() {
  EXPECT_TRUE(client_owned_);
}

void AutocompleteProviderTest::RegisterTemplateURL(
    const base::string16 keyword,
    const std::string& template_url) {
  TemplateURLData data;
  data.SetURL(template_url);
  data.SetShortName(keyword);
  data.SetKeyword(keyword);
  TemplateURLService* turl_model = client_->GetTemplateURLService();
  TemplateURL* default_turl =
      turl_model->Add(std::make_unique<TemplateURL>(data));
  turl_model->SetUserSelectedDefaultSearchProvider(default_turl);
  turl_model->Load();
  TemplateURLID default_provider_id = default_turl->id();
  ASSERT_NE(0, default_provider_id);
}

void AutocompleteProviderTest::ResetControllerWithTestProviders(
    bool same_destinations,
    TestProvider** provider1_ptr,
    TestProvider** provider2_ptr) {
  // TODO: Move it outside this method, after refactoring the existing
  // unit tests.  Specifically:
  //   (1) Make sure that AutocompleteMatch.keyword is set iff there is
  //       a corresponding call to RegisterTemplateURL; otherwise the
  //       controller flow will crash; this practically means that
  //       RunTests/ResetControllerXXX/RegisterTemplateURL should
  //       be coordinated with each other.
  //   (2) Inject test arguments rather than rely on the hardcoded values, e.g.
  //       don't rely on kResultsPerProvided and default relevance ordering
  //       (B > A).
  RegisterTemplateURL(base::ASCIIToUTF16(kTestTemplateURLKeyword),
                      "http://aqs/{searchTerms}/{google:assistedQueryStats}");

  AutocompleteController::Providers providers;

  // Construct two new providers, with either the same or different prefixes.
  TestProvider* provider1 =
      new TestProvider(kResultsPerProvider, base::ASCIIToUTF16("http://a"),
                       base::ASCIIToUTF16(kTestTemplateURLKeyword), client_);
  providers.push_back(provider1);

  TestProvider* provider2 = new TestProvider(
      kResultsPerProvider * 2,
      base::ASCIIToUTF16(same_destinations ? "http://a" : "http://b"),
      base::string16(), client_);
  providers.push_back(provider2);

  // Reset the controller to contain our new providers.
  ResetControllerWithType(0);

  // We're going to swap the providers vector, but the old vector should be
  // empty so no elements need to be freed at this point.
  EXPECT_TRUE(controller_->providers_.empty());
  controller_->providers_.swap(providers);
  provider1->set_listener(controller_.get());
  provider2->set_listener(controller_.get());

  client_->set_closure(base::Bind(&AutocompleteProviderTest::CopyResults,
                                  base::Unretained(this)));

  if (provider1_ptr)
    *provider1_ptr = provider1;
  if (provider2_ptr)
    *provider2_ptr = provider2;
}

void AutocompleteProviderTest::ResetControllerWithKeywordAndSearchProviders() {
  // Reset the default TemplateURL.
  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16("default"));
  data.SetKeyword(base::ASCIIToUTF16("default"));
  data.SetURL("http://defaultturl/{searchTerms}");
  TemplateURLService* turl_model = client_->GetTemplateURLService();
  TemplateURL* default_turl =
      turl_model->Add(std::make_unique<TemplateURL>(data));
  turl_model->SetUserSelectedDefaultSearchProvider(default_turl);
  TemplateURLID default_provider_id = default_turl->id();
  ASSERT_NE(0, default_provider_id);

  // Create another TemplateURL for KeywordProvider.
  TemplateURLData data2;
  data2.SetShortName(base::ASCIIToUTF16("k"));
  data2.SetKeyword(base::ASCIIToUTF16("k"));
  data2.SetURL("http://keyword/{searchTerms}");
  TemplateURL* keyword_turl =
      turl_model->Add(std::make_unique<TemplateURL>(data2));
  ASSERT_NE(0, keyword_turl->id());

  ResetControllerWithType(AutocompleteProvider::TYPE_KEYWORD |
                          AutocompleteProvider::TYPE_SEARCH);
}

void AutocompleteProviderTest::ResetControllerWithKeywordProvider() {
  TemplateURLService* turl_model = client_->GetTemplateURLService();

  // Create a TemplateURL for KeywordProvider.
  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16("foo.com"));
  data.SetKeyword(base::ASCIIToUTF16("foo.com"));
  data.SetURL("http://foo.com/{searchTerms}");
  TemplateURL* keyword_turl =
      turl_model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_NE(0, keyword_turl->id());

  // Make a TemplateURL for KeywordProvider that a shorter version of the
  // first.
  data.SetShortName(base::ASCIIToUTF16("f"));
  data.SetKeyword(base::ASCIIToUTF16("f"));
  data.SetURL("http://f.com/{searchTerms}");
  keyword_turl = turl_model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_NE(0, keyword_turl->id());

  // Create another TemplateURL for KeywordProvider.
  data.SetShortName(base::ASCIIToUTF16("bar.com"));
  data.SetKeyword(base::ASCIIToUTF16("bar.com"));
  data.SetURL("http://bar.com/{searchTerms}");
  keyword_turl = turl_model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_NE(0, keyword_turl->id());

  ResetControllerWithType(AutocompleteProvider::TYPE_KEYWORD);
}

void AutocompleteProviderTest::ResetControllerWithType(int type) {
  EXPECT_FALSE(client_owned_);
  controller_.reset(
      new AutocompleteController(base::WrapUnique(client_), nullptr, type));
  client_owned_ = true;
}

void AutocompleteProviderTest::RunTest() {
  RunQuery("a", true);
}

void AutocompleteProviderTest::RunKeywordTest(const base::string16& input,
                                              const KeywordTestData* match_data,
                                              size_t size) {
  ACMatches matches;
  for (size_t i = 0; i < size; ++i) {
    AutocompleteMatch match;
    match.relevance = 1000;  // Arbitrary non-zero value.
    match.allowed_to_be_default_match = true;
    match.fill_into_edit = match_data[i].fill_into_edit;
    match.transition = ui::PAGE_TRANSITION_KEYWORD;
    match.keyword = match_data[i].keyword;
    matches.push_back(match);
  }

  AutocompleteInput autocomplete_input(
      input,
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
      TestingSchemeClassifier());
  autocomplete_input.set_prefer_keyword(true);
  controller_->input_ = autocomplete_input;
  AutocompleteResult result;
  result.AppendMatches(controller_->input_, matches);
  controller_->UpdateAssociatedKeywords(&result);
  for (size_t j = 0; j < result.size(); ++j) {
    EXPECT_EQ(match_data[j].expected_associated_keyword,
              result.match_at(j)->associated_keyword
                  ? result.match_at(j)->associated_keyword->keyword
                  : base::string16());
  }
}

void AutocompleteProviderTest::RunAssistedQueryStatsTest(
    const AssistedQueryStatsTestData* aqs_test_data,
    size_t size) {
  // Prepare input.
  const size_t kMaxRelevance = 1000;
  ACMatches matches;
  for (size_t i = 0; i < size; ++i) {
    AutocompleteMatch match(nullptr, kMaxRelevance - i, false,
                            aqs_test_data[i].match_type);
    match.allowed_to_be_default_match = true;
    match.keyword = base::ASCIIToUTF16(kTestTemplateURLKeyword);
    match.search_terms_args.reset(
        new TemplateURLRef::SearchTermsArgs(base::string16()));
    matches.push_back(match);
  }
  result_.Reset();
  result_.AppendMatches(AutocompleteInput(), matches);

  // Update AQS.
  controller_->UpdateAssistedQueryStats(&result_);

  // Verify data.
  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ(aqs_test_data[i].expected_aqs,
              result_.match_at(i)->search_terms_args->assisted_query_stats);
  }
}

void AutocompleteProviderTest::RunQuery(const std::string& query,
                                        bool allow_exact_keyword_match) {
  result_.Reset();
  AutocompleteInput input(base::ASCIIToUTF16(query),
                          metrics::OmniboxEventProto::OTHER,
                          TestingSchemeClassifier());
  input.set_prevent_inline_autocomplete(true);
  input.set_allow_exact_keyword_match(allow_exact_keyword_match);
  controller_->Start(input);

  if (!controller_->done())
    // The message loop will terminate when all autocomplete input has been
    // collected.
    base::RunLoop().Run();
}

void AutocompleteProviderTest::RunExactKeymatchTest(
    bool allow_exact_keyword_match) {
  // Send the controller input which exactly matches the keyword provider we
  // created in ResetControllerWithKeywordAndSearchProviders().  The default
  // match should thus be a search-other-engine match iff
  // |allow_exact_keyword_match| is true.  Regardless, the match should
  // be from SearchProvider.  (It provides all verbatim search matches,
  // keyword or not.)
  RunQuery("k test", allow_exact_keyword_match);
  EXPECT_EQ(AutocompleteProvider::TYPE_SEARCH,
            controller_->result().default_match()->provider->type());
  EXPECT_EQ(allow_exact_keyword_match
                ? AutocompleteMatchType::SEARCH_OTHER_ENGINE
                : AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
            controller_->result().default_match()->type);
}

void AutocompleteProviderTest::CopyResults() {
  result_.CopyFrom(controller_->result());
}

GURL AutocompleteProviderTest::GetDestinationURL(
    AutocompleteMatch match,
    base::TimeDelta query_formulation_time) const {
  controller_->UpdateMatchDestinationURLWithQueryFormulationTime(
      query_formulation_time, &match);
  return match.destination_url;
}

// Tests that the default selection is set properly when updating results.
TEST_F(AutocompleteProviderTest, Query) {
  TestProvider* provider1 = nullptr;
  TestProvider* provider2 = nullptr;
  ResetControllerWithTestProviders(false, &provider1, &provider2);
  RunTest();

  // Make sure the default match gets set to the highest relevance match.  The
  // highest relevance matches should come from the second provider.
  EXPECT_EQ(kResultsPerProvider * 2, result_.size());
  ASSERT_NE(result_.end(), result_.default_match());
  EXPECT_EQ(provider2, result_.default_match()->provider);
}

// Tests assisted query stats.
TEST_F(AutocompleteProviderTest, AssistedQueryStats) {
  ResetControllerWithTestProviders(false, nullptr, nullptr);
  RunTest();

  ASSERT_EQ(kResultsPerProvider * 2, result_.size());

  // Now, check the results from the second provider, as they should not have
  // assisted query stats set.
  for (size_t i = 0; i < kResultsPerProvider; ++i) {
    EXPECT_TRUE(
        result_.match_at(i)->search_terms_args->assisted_query_stats.empty());
  }
  // The first provider has a test keyword, so AQS should be non-empty.
  for (size_t i = kResultsPerProvider; i < kResultsPerProvider * 2; ++i) {
    EXPECT_FALSE(
        result_.match_at(i)->search_terms_args->assisted_query_stats.empty());
  }
}

TEST_F(AutocompleteProviderTest, RemoveDuplicates) {
  TestProvider* provider1 = nullptr;
  TestProvider* provider2 = nullptr;
  ResetControllerWithTestProviders(true, &provider1, &provider2);
  RunTest();

  // Make sure all the first provider's results were eliminated by the second
  // provider's.
  EXPECT_EQ(kResultsPerProvider, result_.size());
  for (AutocompleteResult::const_iterator i(result_.begin());
       i != result_.end(); ++i)
    EXPECT_EQ(provider2, i->provider);
}

TEST_F(AutocompleteProviderTest, AllowExactKeywordMatch) {
  ResetControllerWithKeywordAndSearchProviders();
  RunExactKeymatchTest(true);
  RunExactKeymatchTest(false);
}

// Ensures matches from (only) the default search provider respect any extra
// query params set on the command line.
TEST_F(AutocompleteProviderTest, ExtraQueryParams) {
  ResetControllerWithKeywordAndSearchProviders();
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kExtraSearchQueryParams, "a=b");
  RunExactKeymatchTest(true);
  CopyResults();
  ASSERT_EQ(2U, result_.size());
  EXPECT_EQ("http://keyword/test",
            result_.match_at(0)->destination_url.possibly_invalid_spec());
  EXPECT_EQ("http://defaultturl/k%20test?a=b",
            result_.match_at(1)->destination_url.possibly_invalid_spec());
}

// Test that redundant associated keywords are removed.
TEST_F(AutocompleteProviderTest, RedundantKeywordsIgnoredInResult) {
  ResetControllerWithKeywordProvider();

  {
    KeywordTestData duplicate_url[] = {
      { base::ASCIIToUTF16("fo"), base::string16(), base::string16() },
      { base::ASCIIToUTF16("foo.com"), base::string16(),
        base::ASCIIToUTF16("foo.com") },
      { base::ASCIIToUTF16("foo.com"), base::string16(), base::string16() }
    };

    SCOPED_TRACE("Duplicate url");
    RunKeywordTest(base::ASCIIToUTF16("fo"), duplicate_url,
                   base::size(duplicate_url));
  }

  {
    KeywordTestData keyword_match[] = {
      { base::ASCIIToUTF16("foo.com"), base::ASCIIToUTF16("foo.com"),
        base::string16() },
      { base::ASCIIToUTF16("foo.com"), base::string16(), base::string16() }
    };

    SCOPED_TRACE("Duplicate url with keyword match");
    RunKeywordTest(base::ASCIIToUTF16("fo"), keyword_match,
                   base::size(keyword_match));
  }

  {
    KeywordTestData multiple_keyword[] = {
      { base::ASCIIToUTF16("fo"), base::string16(), base::string16() },
      { base::ASCIIToUTF16("foo.com"), base::string16(),
        base::ASCIIToUTF16("foo.com") },
      { base::ASCIIToUTF16("foo.com"), base::string16(), base::string16() },
      { base::ASCIIToUTF16("bar.com"), base::string16(),
        base::ASCIIToUTF16("bar.com") },
    };

    SCOPED_TRACE("Duplicate url with multiple keywords");
    RunKeywordTest(base::ASCIIToUTF16("fo"), multiple_keyword,
                   base::size(multiple_keyword));
  }
}

// Test that exact match keywords trump keywords associated with
// the match.
TEST_F(AutocompleteProviderTest, ExactMatchKeywords) {
  ResetControllerWithKeywordProvider();

  {
    KeywordTestData keyword_match[] = {
      { base::ASCIIToUTF16("foo.com"), base::string16(),
        base::ASCIIToUTF16("foo.com") }
    };

    SCOPED_TRACE("keyword match as usual");
    RunKeywordTest(base::ASCIIToUTF16("fo"), keyword_match,
                   base::size(keyword_match));
  }

  // The same result set with an input of "f" (versus "fo") should get
  // a different associated keyword because "f" is an exact match for
  // a keyword and that should trump the keyword normally associated with
  // this match.
  {
    KeywordTestData keyword_match[] = {
      { base::ASCIIToUTF16("foo.com"), base::string16(),
        base::ASCIIToUTF16("f") }
    };

    SCOPED_TRACE("keyword exact match");
    RunKeywordTest(base::ASCIIToUTF16("f"), keyword_match,
                   base::size(keyword_match));
  }
}

TEST_F(AutocompleteProviderTest, UpdateAssistedQueryStats) {
  ResetControllerWithTestProviders(false, nullptr, nullptr);

  {
    AssistedQueryStatsTestData test_data[] = {
      //  MSVC doesn't support zero-length arrays, so supply some dummy data.
      { AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, "" }
    };
    SCOPED_TRACE("No matches");
    // Note: We pass 0 here to ignore the dummy data above.
    RunAssistedQueryStatsTest(test_data, 0);
  }

  {
    AssistedQueryStatsTestData test_data[] = {
      { AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, "chrome..69i57" }
    };
    SCOPED_TRACE("One match");
    RunAssistedQueryStatsTest(test_data, base::size(test_data));
  }

  {
    AssistedQueryStatsTestData test_data[] = {
      { AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
        "chrome..69i57j69i58j5l2j0l3j69i59" },
      { AutocompleteMatchType::URL_WHAT_YOU_TYPED,
        "chrome..69i57j69i58j5l2j0l3j69i59" },
      { AutocompleteMatchType::NAVSUGGEST,
        "chrome.2.69i57j69i58j5l2j0l3j69i59" },
      { AutocompleteMatchType::NAVSUGGEST,
        "chrome.3.69i57j69i58j5l2j0l3j69i59" },
      { AutocompleteMatchType::SEARCH_SUGGEST,
        "chrome.4.69i57j69i58j5l2j0l3j69i59" },
      { AutocompleteMatchType::SEARCH_SUGGEST,
        "chrome.5.69i57j69i58j5l2j0l3j69i59" },
      { AutocompleteMatchType::SEARCH_SUGGEST,
        "chrome.6.69i57j69i58j5l2j0l3j69i59" },
      { AutocompleteMatchType::SEARCH_HISTORY,
        "chrome.7.69i57j69i58j5l2j0l3j69i59" },
    };
    SCOPED_TRACE("Multiple matches");
    RunAssistedQueryStatsTest(test_data, base::size(test_data));
  }
}

TEST_F(AutocompleteProviderTest, GetDestinationURL) {
  ResetControllerWithKeywordAndSearchProviders();

  // For the destination URL to have aqs parameters for query formulation time
  // and the field trial triggered bit, many conditions need to be satisfied.
  AutocompleteMatch match(nullptr, 1100, false,
                          AutocompleteMatchType::SEARCH_SUGGEST);
  GURL url(GetDestinationURL(match, base::TimeDelta::FromMilliseconds(2456)));
  EXPECT_TRUE(url.path().empty());

  // The protocol needs to be https.
  RegisterTemplateURL(base::ASCIIToUTF16(kTestTemplateURLKeyword),
                      "https://aqs/{searchTerms}/{google:assistedQueryStats}");
  url = GetDestinationURL(match, base::TimeDelta::FromMilliseconds(2456));
  EXPECT_TRUE(url.path().empty());

  // There needs to be a keyword provider.
  match.keyword = base::ASCIIToUTF16(kTestTemplateURLKeyword);
  url = GetDestinationURL(match, base::TimeDelta::FromMilliseconds(2456));
  EXPECT_TRUE(url.path().empty());

  // search_terms_args needs to be set.
  match.search_terms_args.reset(
      new TemplateURLRef::SearchTermsArgs(base::string16()));
  url = GetDestinationURL(match, base::TimeDelta::FromMilliseconds(2456));
  EXPECT_TRUE(url.path().empty());

  // assisted_query_stats needs to have been previously set.
  match.search_terms_args->assisted_query_stats =
      "chrome.0.69i57j69i58j5l2j0l3j69i59";
  url = GetDestinationURL(match, base::TimeDelta::FromMilliseconds(2456));
  EXPECT_EQ("//aqs=chrome.0.69i57j69i58j5l2j0l3j69i59.2456j0j0&", url.path());

  // Test field trial triggered bit set.
  set_search_provider_field_trial_triggered_in_session(true);
  EXPECT_TRUE(search_provider_field_trial_triggered_in_session());
  url = GetDestinationURL(match, base::TimeDelta::FromMilliseconds(2456));
  EXPECT_EQ("//aqs=chrome.0.69i57j69i58j5l2j0l3j69i59.2456j1j0&", url.path());

  // Test page classification set.
  set_current_page_classification(metrics::OmniboxEventProto::OTHER);
  set_search_provider_field_trial_triggered_in_session(false);
  EXPECT_FALSE(search_provider_field_trial_triggered_in_session());
  url = GetDestinationURL(match, base::TimeDelta::FromMilliseconds(2456));
  EXPECT_EQ("//aqs=chrome.0.69i57j69i58j5l2j0l3j69i59.2456j0j4&", url.path());

  // Test page classification and field trial triggered set.
  set_search_provider_field_trial_triggered_in_session(true);
  EXPECT_TRUE(search_provider_field_trial_triggered_in_session());
  url = GetDestinationURL(match, base::TimeDelta::FromMilliseconds(2456));
  EXPECT_EQ("//aqs=chrome.0.69i57j69i58j5l2j0l3j69i59.2456j1j4&", url.path());
}

TEST_F(AutocompleteProviderTest, ClassifyAllMatchesInString) {
  ResetControllerWithKeywordAndSearchProviders();

  using base::ASCIIToUTF16;
  ACMatchClassifications matches =
      AutocompleteMatch::ClassificationsFromString("0,0");
  ClassifyTest classify_test(ASCIIToUTF16("A man, a plan, a canal Panama"),
                             /*text_is_query=*/false, matches);

  ACMatchClassifications spans_a = classify_test.RunTest(ASCIIToUTF16("man"));
  // ACMatch spans should be: '--MMM------------------------'
  EXPECT_EQ("0,0,2,2,5,0", AutocompleteMatch::ClassificationsToString(spans_a));

  ACMatchClassifications spans_b = classify_test.RunTest(ASCIIToUTF16("man p"));
  // ACMatch spans should be: '--MMM----M-------------M-----'
  EXPECT_EQ("0,0,2,2,5,0,9,2,10,0,23,2,24,0",
            AutocompleteMatch::ClassificationsToString(spans_b));

  ACMatchClassifications spans_c =
      classify_test.RunTest(ASCIIToUTF16("man plan panama"));
  // ACMatch spans should be:'--MMM----MMMM----------MMMMMM'
  EXPECT_EQ("0,0,2,2,5,0,9,2,13,0,23,2",
            AutocompleteMatch::ClassificationsToString(spans_c));

  ClassifyTest classify_test2(
      ASCIIToUTF16("Yahoo! Sports - Sports News, "
                   "Scores, Rumors, Fantasy Games, and more"),
      /*text_is_query=*/false, matches);

  ACMatchClassifications spans_d = classify_test2.RunTest(ASCIIToUTF16("ne"));
  // ACMatch spans should match first two letters of the "news".
  EXPECT_EQ("0,0,23,2,25,0",
            AutocompleteMatch::ClassificationsToString(spans_d));

  ACMatchClassifications spans_e =
      classify_test2.RunTest(ASCIIToUTF16("news r"));
  EXPECT_EQ(
      "0,0,10,2,11,0,19,2,20,0,23,2,27,0,32,2,33,0,37,2,38,0,41,2,42,0,"
      "66,2,67,0",
      AutocompleteMatch::ClassificationsToString(spans_e));

  matches = AutocompleteMatch::ClassificationsFromString("0,1");
  ClassifyTest classify_test3(ASCIIToUTF16("livescore.goal.com"),
                              /*text_is_query=*/false, matches);

  ACMatchClassifications spans_f = classify_test3.RunTest(ASCIIToUTF16("go"));
  // ACMatch spans should match first two letters of the "goal".
  EXPECT_EQ("0,1,10,3,12,1",
            AutocompleteMatch::ClassificationsToString(spans_f));

  matches = AutocompleteMatch::ClassificationsFromString("0,0,13,1");
  ClassifyTest classify_test4(ASCIIToUTF16("Email login: mail.somecorp.com"),
                              /*text_is_query=*/false, matches);

  ACMatchClassifications spans_g = classify_test4.RunTest(ASCIIToUTF16("ail"));
  EXPECT_EQ("0,0,2,2,5,0,13,1,14,3,17,1",
            AutocompleteMatch::ClassificationsToString(spans_g));

  ACMatchClassifications spans_h =
      classify_test4.RunTest(ASCIIToUTF16("lo log"));
  EXPECT_EQ("0,0,6,2,9,0,13,1",
            AutocompleteMatch::ClassificationsToString(spans_h));

  ACMatchClassifications spans_i =
      classify_test4.RunTest(ASCIIToUTF16("ail em"));
  // 'Email' and 'ail' should be matched.
  EXPECT_EQ("0,2,5,0,13,1,14,3,17,1",
            AutocompleteMatch::ClassificationsToString(spans_i));

  // Some web sites do not have a description.  If the string being searched is
  // empty, the classifications must also be empty: http://crbug.com/148647
  // Extra parens in the next line hack around C++03's "most vexing parse".
  class ClassifyTest classify_test5((base::string16()), /*text_is_query=*/false,
                                    ACMatchClassifications());
  ACMatchClassifications spans_j = classify_test5.RunTest(ASCIIToUTF16("man"));
  ASSERT_EQ(0U, spans_j.size());

  // Matches which end at beginning of classification merge properly.
  matches = AutocompleteMatch::ClassificationsFromString("0,4,9,0");
  ClassifyTest classify_test6(ASCIIToUTF16("html password example"),
                              /*text_is_query=*/false, matches);

  // Extra space in the next string avoids having the string be a prefix of the
  // text above, which would allow for two different valid classification sets,
  // one of which uses two spans (the first of which would mark all of "html
  // pass" as a match) and one which uses four (which marks the individual words
  // as matches but not the space between them).  This way only the latter is
  // valid.
  ACMatchClassifications spans_k =
      classify_test6.RunTest(ASCIIToUTF16("html  pass"));
  EXPECT_EQ("0,6,4,4,5,6,9,0",
            AutocompleteMatch::ClassificationsToString(spans_k));

  // Multiple matches with both beginning and end at beginning of
  // classifications merge properly.
  matches = AutocompleteMatch::ClassificationsFromString("0,1,11,0");
  ClassifyTest classify_test7(ASCIIToUTF16("http://a.co is great"),
                              /*text_is_query=*/false, matches);

  ACMatchClassifications spans_l =
      classify_test7.RunTest(ASCIIToUTF16("ht co"));
  EXPECT_EQ("0,3,2,1,9,3,11,0",
            AutocompleteMatch::ClassificationsToString(spans_l));

  // Queries should be classify the same way as google search autocomplete
  // suggestions.
  matches = AutocompleteMatch::ClassificationsFromString("0,0");
  ClassifyTest classify_test8(ASCIIToUTF16("panama canal"),
                              /*text_is_query=*/true, matches);

  ACMatchClassifications spans_m = classify_test8.RunTest(ASCIIToUTF16("pan"));
  // ACMatch spans should be: "---MMMMMMMMM";
  EXPECT_EQ("0,0,3,2", AutocompleteMatch::ClassificationsToString(spans_m));
  ACMatchClassifications spans_n =
      classify_test8.RunTest(ASCIIToUTF16("canal"));
  // ACMatch spans should be: "MMMMMM-----";
  EXPECT_EQ("0,2,7,0", AutocompleteMatch::ClassificationsToString(spans_n));

  // Search autocomplete suggesion.
  ClassifyTest classify_test9(ASCIIToUTF16("comcast webmail login"),
                              /*text_is_query=*/true, ACMatchClassifications());

  // Matches first and first part of middle word and the last word..
  ACMatchClassifications spans_o =
      classify_test9.RunTest(ASCIIToUTF16("comcast web login"));
  // ACMatch spans should be: "-----------MMMMM-----";
  EXPECT_EQ("0,0,11,2,16,0",
            AutocompleteMatch::ClassificationsToString(spans_o));

  // Matches partial word in the middle of suggestion.
  ACMatchClassifications spans_p =
      classify_test9.RunTest(ASCIIToUTF16("web"));
  // ACMatch spans should be: "MMMMMMMM---MMMMMMMMMM";
  EXPECT_EQ("0,2,8,0,11,2",
            AutocompleteMatch::ClassificationsToString(spans_p));

  ClassifyTest classify_test10(ASCIIToUTF16("comcast.net web mail login"),
                              /*text_is_query=*/true, ACMatchClassifications());

  ACMatchClassifications spans_q =
      classify_test10.RunTest(ASCIIToUTF16("comcast web login"));
  // ACMatch spans should be: "-------MMMMM----MMMMM-----";
  EXPECT_EQ("0,0,7,2,12,0,16,2,21,0",
            AutocompleteMatch::ClassificationsToString(spans_q));

  // Same with |classify_test10| except using characters in
  // base::kWhitespaceASCIIAs16 instead of white space.
  ClassifyTest classify_test11(ASCIIToUTF16("comcast.net\x0aweb\x0dmail login"),
                              /*text_is_query=*/true, ACMatchClassifications());

  ACMatchClassifications spans_r =
      classify_test11.RunTest(ASCIIToUTF16("comcast web login"));
  // ACMatch spans should be: "-------MMMMM----MMMMM-----";
  EXPECT_EQ("0,0,7,2,12,0,16,2,21,0",
            AutocompleteMatch::ClassificationsToString(spans_r));
}
