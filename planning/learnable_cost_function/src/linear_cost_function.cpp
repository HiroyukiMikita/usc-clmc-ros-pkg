/*
 * linear_cost_function.cpp
 *
 *  Created on: Jul 27, 2011
 *      Author: kalakris
 */

#include <ros/assert.h>
#include <learnable_cost_function/linear_cost_function.h>

namespace learnable_cost_function
{

LinearCostFunction::LinearCostFunction()
{
}

LinearCostFunction::~LinearCostFunction()
{
}

void LinearCostFunction::getValueAndGradient(boost::shared_ptr<Input const> input, double& value,
                         bool compute_gradient, Eigen::VectorXd& gradient, bool& state_validity,
                         std::vector<double>& weighted_feature_values)
{
  value = 0.0;
  int num_input_dimensions = input->getNumDimensions();
  gradient = Eigen::VectorXd::Zero(num_input_dimensions);
  state_validity = true;
  weighted_feature_values.resize(num_feature_values_);
  int counter = 0;
  for (unsigned int i=0; i<features_.size(); ++i)
  {
    bool validity;
    features_[i].feature->computeValuesAndGradients(input,
                                                    features_[i].values,
                                                    compute_gradient,
                                                    features_[i].gradients,
                                                    validity
                                                    );
    state_validity = state_validity && validity;
    for (int j=0; j<features_[i].num_values; ++j)
    {
      weighted_feature_values[counter] = features_[i].weights[j] * features_[i].values[j];
      value += weighted_feature_values[counter];
      ++counter;
      if (compute_gradient)
      {
//        ROS_INFO("gradients[j]: %d, %d", features_[i].gradients[j].rows(), features_[i].gradients[j].cols());
        gradient += features_[i].weights[j] * features_[i].gradients[j];
      }
    }
  }
}

void LinearCostFunction::clear()
{
  features_.clear();
  num_feature_values_ = 0;
}

void LinearCostFunction::addFeaturesAndWeights(std::vector<boost::shared_ptr<Feature> > features,
                           std::vector<double> weights)
{
  int num_features = features.size();

  int weight_index = 0;
  for (int i=0; i<num_features; ++i)
  {
    FeatureInfo fi;
    fi.feature = features[i];
    fi.num_values = features[i]->getNumValues();
    fi.weights.resize(fi.num_values);
    fi.values.resize(fi.num_values);
    for (int j=0; j<fi.num_values; ++j)
    {
      ROS_ASSERT(weight_index < (int)weights.size());
      fi.weights[j] = weights[weight_index++];
    }
    features_.push_back(fi);
  }

  num_feature_values_ += weight_index;

}

boost::shared_ptr<CostFunction> LinearCostFunction::clone()
{
  LinearCostFunction* lcf = new LinearCostFunction();
  lcf->features_ = features_;

  // clone each feature:
  for (unsigned int i=0; i<features_.size(); ++i)
  {
    lcf->features_[i].feature = features_[i].feature->clone();
  }

  lcf->num_feature_values_ = num_feature_values_;
  //ROS_DEBUG("Cloned linear cost function with %d features, %d values", features_.size(), num_feature_values_);
  boost::shared_ptr<CostFunction> ret(lcf);
  return ret;
}

void LinearCostFunction::debugCost(double cost, const std::vector<double>& weighted_feature_values)
{
  ROS_DEBUG("Cost = %f", cost);
  int counter = 0;
  for (unsigned int i=0; i<features_.size(); ++i)
  {
    ROS_DEBUG("Feature %s:", features_[i].feature->getName().c_str());
    for (int j=0; j<features_[i].num_values; ++j)
    {
      ROS_DEBUG("%d: %f", j, weighted_feature_values[counter]);
      ++counter;
    }
  }
}

}
