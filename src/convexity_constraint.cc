/**
 @file    convexity_constraint.cc
 @author  Alexander W. Winkler (winklera@ethz.ch)
 @date    Dec 4, 2016
 @brief   Brief description
 */

#include <xpp/opt/convexity_constraint.h>

namespace xpp {
namespace opt {

ConvexityConstraint::ConvexityConstraint ()
{
  name_ = "Convexity";
}

ConvexityConstraint::~ConvexityConstraint ()
{
  // TODO Auto-generated destructor stub
}

void
ConvexityConstraint::Init (const EndeffectorLoad& ee_load)
{
  ee_load_ = ee_load;

  // build constant jacobian w.r.t lambdas
  int m = ee_load_.GetNumberOfSegments();
  int n = ee_load_.GetOptVarCount();
  jac_ = Jacobian(m, n);

  for (int k=0; k<m; ++k) {
    for (auto ee : ee_load_.GetLoadValuesIdx(k).GetEEsOrdered()) {
      int idx = ee_load_.IndexDiscrete(k,ee);
      jac_.insert(k, idx) = 1.0;
    }
  }
}

void
ConvexityConstraint::UpdateVariables (const OptimizationVariables* opt_var)
{
  VectorXd lambdas = opt_var->GetVariables(ee_load_.GetID());
  ee_load_.SetOptimizationParameters(lambdas);
}

ConvexityConstraint::VectorXd
ConvexityConstraint::EvaluateConstraint () const
{
  VectorXd g(jac_.rows());

  for (int k=0; k<g.rows(); ++k) {

    double sum_k = 0.0;
    for (auto lambda : ee_load_.GetLoadValuesIdx(k).ToImpl())
      sum_k += lambda;

    g(k) = sum_k; // sum equal to 1
  }

  return g;
}

VecBound
ConvexityConstraint::GetBounds () const
{
  return VecBound(jac_.rows(), Bound(1.0, 1.0));
}

ConvexityConstraint::Jacobian
ConvexityConstraint::GetJacobianWithRespectTo (std::string var_set) const
{
  Jacobian jac; // empy matrix

  if (var_set == ee_load_.GetID()) {
    jac = jac_;
  }

  return jac;
}

} /* namespace opt */
} /* namespace xpp */
