/*
 * Copyright 2021 Google LLC.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "yggdrasil_decision_forests/learner/hyperparameters_optimizer/hyperparameters_optimizer.h"

#include "gtest/gtest.h"
#include "yggdrasil_decision_forests/metric/metric.h"
#include "yggdrasil_decision_forests/model/abstract_model.pb.h"
#include "yggdrasil_decision_forests/utils/filesystem.h"
#include "yggdrasil_decision_forests/utils/logging.h"
#include "yggdrasil_decision_forests/utils/test.h"
#include "yggdrasil_decision_forests/utils/test_utils.h"

namespace yggdrasil_decision_forests {
namespace model {
namespace hyperparameters_optimizer_v2 {
namespace {

class OnAdult : public utils::TrainAndTestTester {
  void SetUp() override {
    train_config_ = PARSE_TEST_PROTO(R"pb(
      task: CLASSIFICATION
      learner: "HYPERPARAMETER_OPTIMIZER"
      label: "income"

      [yggdrasil_decision_forests.model.hyperparameters_optimizer_v2.proto
           .hyperparameters_optimizer_config] {

        optimizer {
          optimizer_key: "RANDOM"
          [yggdrasil_decision_forests.model.hyperparameters_optimizer_v2.proto
               .random] { num_trials: 25 }
        }

        base_learner {
          learner: "GRADIENT_BOOSTED_TREES"
          [yggdrasil_decision_forests.model.gradient_boosted_trees.proto
               .gradient_boosted_trees_config] { num_trees: 50 }
        }

        base_learner_deployment {
          # The multi-threading is done at the optimizer level.
          num_threads: 1
        }

        search_space {
          fields {
            name: "num_candidate_attributes_ratio"
            discrete_candidates {
              possible_values { real: 1.0 }
              possible_values { real: 0.8 }
              possible_values { real: 0.6 }
            }
          }

          fields {
            name: "use_hessian_gain"
            discrete_candidates {
              possible_values { categorical: "true" }
              possible_values { categorical: "false" }
            }
          }

          fields {
            name: "growing_strategy"
            discrete_candidates {
              possible_values { categorical: "LOCAL" }
              possible_values { categorical: "BEST_FIRST_GLOBAL" }
            }

            children {
              parent_discrete_values {
                possible_values { categorical: "LOCAL" }
              }
              name: "max_depth"
              discrete_candidates {
                possible_values { integer: 4 }
                possible_values { integer: 5 }
                possible_values { integer: 6 }
                possible_values { integer: 7 }
              }
            }

            children {
              parent_discrete_values {
                possible_values { categorical: "BEST_FIRST_GLOBAL" }
              }
              name: "max_num_nodes"
              discrete_candidates {
                possible_values { integer: 16 }
                possible_values { integer: 32 }
                possible_values { integer: 64 }
                possible_values { integer: 128 }
              }
            }
          }
        }
      }
    )pb");

    train_config_.set_learner(HyperParameterOptimizerLearner::kRegisteredName);
    train_config_.set_task(model::proto::Task::CLASSIFICATION);
    train_config_.set_label("income");
    train_config_.add_features(".*");
    dataset_filename_ = "adult.csv";
  }
};

TEST_F(OnAdult, Random) {
  TrainAndEvaluateModel();
  EXPECT_GE(metric::Accuracy(evaluation_), 0.87);
  EXPECT_LT(metric::LogLoss(evaluation_), 0.30);
}

TEST_F(OnAdult, DefaultTargetMetric) {
  metric::proto::EvaluationResults evaluation;
  EXPECT_FALSE(internal::DefaultTargetMetric(evaluation).ok());

  evaluation.mutable_uplift()->set_qini(1);
  EXPECT_TRUE(
      internal::DefaultTargetMetric(evaluation).value().uplift().has_qini());

  evaluation.mutable_ranking()->mutable_ndcg()->set_value(1);
  EXPECT_TRUE(
      internal::DefaultTargetMetric(evaluation).value().ranking().has_ndcg());

  evaluation.mutable_regression()->set_sum_square_error(1);
  EXPECT_TRUE(internal::DefaultTargetMetric(evaluation)
                  .value()
                  .regression()
                  .has_rmse());

  evaluation.mutable_classification()->set_accuracy(1);
  EXPECT_TRUE(internal::DefaultTargetMetric(evaluation)
                  .value()
                  .classification()
                  .has_accuracy());

  evaluation.mutable_classification()->add_rocs();
  evaluation.mutable_classification()->add_rocs()->set_auc(1);
  evaluation.mutable_classification()->add_rocs()->set_auc(1);
  EXPECT_TRUE(internal::DefaultTargetMetric(evaluation)
                  .value()
                  .classification()
                  .one_vs_other()
                  .has_auc());

  evaluation.set_loss_value(1);
  EXPECT_TRUE(internal::DefaultTargetMetric(evaluation).value().has_loss());
}

}  // namespace
}  // namespace hyperparameters_optimizer_v2
}  // namespace model
}  // namespace yggdrasil_decision_forests