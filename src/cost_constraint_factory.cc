/**
 @file    constraint_factory.cc
 @author  Alexander W. Winkler (winklera@ethz.ch)
 @date    Jul 19, 2016
 @brief   Brief description
 */

#include <xpp/opt/cost_constraint_factory.h>

#include <xpp/opt/a_spline_cost.h>
#include <xpp/opt/linear_spline_equations.h>
#include <xpp/opt/linear_spline_equality_constraint.h>
#include <xpp/opt/range_of_motion_constraint.h>
#include <xpp/opt/convexity_constraint.h>
#include <xpp/opt/support_area_constraint.h>
#include <xpp/opt/dynamic_constraint.h>
#include <xpp/opt/polygon_center_constraint.h>
#include <xpp/opt/contact_load_constraint.h>
//#include <xpp/opt/obstacle_constraint.h>
//#include <xpp/opt/a_foothold_constraint.h>

#include <xpp/soft_constraint.h>

namespace xpp {
namespace opt {

CostConstraintFactory::CostConstraintFactory ()
{
}

CostConstraintFactory::~CostConstraintFactory ()
{
  // TODO Auto-generated destructor stub
}

void
CostConstraintFactory::Init (const ComMotionPtr& com,
                             const EEMotionPtr& _ee_motion,
                             const EELoadPtr& _ee_load,
                             const CopPtr& _cop,
                             const MotionTypePtr& _params, const StateLin2d& initial_state,
                             const StateLin2d& final_state)
{
  com_motion = com;
  ee_motion = _ee_motion;
  ee_load = _ee_load;
  cop = _cop;

  params = _params;
  initial_geom_state_ = initial_state;
  final_geom_state_ = final_state;
}

CostConstraintFactory::ConstraintPtrVec
CostConstraintFactory::GetConstraint (ConstraintName name) const
{
  switch (name) {
    case InitCom:     return MakeInitialConstraint();
    case FinalCom:    return MakeFinalConstraint();
    case JunctionCom: return MakeJunctionConstraint();
    case Convexity:   return MakeConvexityConstraint();
    case Dynamic:     return MakeDynamicConstraint();
    case RomBox:      return MakeRangeOfMotionBoxConstraint();
    case FinalStance: return MakeFinalStanceConstraint();
    case Obstacle:    return MakeObstacleConstraint();
    default: throw std::runtime_error("constraint not defined!");
  }
}

CostConstraintFactory::CostPtr
CostConstraintFactory::GetCost(CostName name) const
{
  switch (name) {
    case ComCostID:          return MakeMotionCost();
    case RangOfMotionCostID: return ToCost(MakeRangeOfMotionBoxConstraint().front());
    case PolyCenterCostID:   return ToCost(MakePolygonCenterConstraint().front());
    case FinalComCostID:     return ToCost(MakeFinalConstraint().front());
    case FinalStanceCostID:  return ToCost(MakeFinalStanceConstraint().front());
    default: throw std::runtime_error("cost not defined!");
  }
}

VariableSet
CostConstraintFactory::SplineCoeffVariables () const
{
  return VariableSet(com_motion->GetOptimizationParameters(), com_motion->GetID());
}

VariableSet
CostConstraintFactory::ContactVariables (const Vector2d initial_pos) const
{
  return VariableSet(ee_motion->GetOptimizationParameters(), ee_motion->GetID());
}

VariableSet
CostConstraintFactory::ConvexityVariables () const
{
  // initialize load values as if each leg is carrying half of total load
  Eigen::VectorXd lambdas = ee_load->GetOptimizationParameters();
  lambdas.fill(1./2);
  return VariableSet(lambdas, ee_load->GetID(), Bound(0.0, 1.0));


// this would initializate the load parameters to equal distribution depending
// on how many contacts in that phase. This also depends on how much each contact
// is allowed to be unloaded
//  double lambda_deviation_percent = 1.0; // 100 percent
//  VariableSet::VecBound bounds;
//  int k=0;
//  for (auto node : motion_structure.GetPhaseStampedVec()) {
//    int n_contacts_at_node = node.GetAllContacts().size();
//    double avg = 1./n_contacts_at_node;
//    double min = avg - avg*lambda_deviation_percent;
//    double max = 1-min;
//    for (int j=0; j<n_contacts_at_node; ++j) {
//      lambdas(k++) = avg;
//      bounds.push_back(AConstraint::Bound(min, max));
//    }
//  }
//  return VariableSet(lambdas, VariableNames::kConvexity, bounds);
}

VariableSet
CostConstraintFactory::CopVariables () const
{
  // could also be generalized
  return VariableSet(cop->GetOptimizationParameters(), cop->GetID());
}


CostConstraintFactory::ConstraintPtrVec
CostConstraintFactory::MakeInitialConstraint () const
{
  LinearSplineEquations eq(*com_motion);
  auto constraint = std::make_shared<LinearSplineEqualityConstraint>(*com_motion);

  StateLin2d initial_com_state = initial_geom_state_;
  initial_com_state.p += params->offset_geom_to_com_.topRows<kDim2d>();

  constraint->Init(eq.MakeInitial(initial_com_state), "Initial XY");
  return {constraint};
}

CostConstraintFactory::ConstraintPtrVec
CostConstraintFactory::MakeFinalConstraint () const
{
  LinearSplineEquations eq(*com_motion);
  auto constraint = std::make_shared<LinearSplineEqualityConstraint>(*com_motion);

  StateLin2d final_com_state = final_geom_state_;
  final_com_state.p += params->offset_geom_to_com_.topRows<kDim2d>();

  constraint->Init(eq.MakeFinal(final_geom_state_, {kPos, kVel, kAcc}), "Final XY");
  return {constraint};
}

CostConstraintFactory::ConstraintPtrVec
CostConstraintFactory::MakeJunctionConstraint () const
{
  LinearSplineEquations eq(*com_motion);
  auto constraint = std::make_shared<LinearSplineEqualityConstraint>(*com_motion);
  constraint->Init(eq.MakeJunction(), "Junction");
  return {constraint};
}

CostConstraintFactory::ConstraintPtrVec
CostConstraintFactory::MakeDynamicConstraint() const
{
  auto constraint = std::make_shared<DynamicConstraint>();
  constraint->Init(*com_motion, *cop, ee_motion->GetTotalTime(),
                   params->dt_nodes_);
  return {constraint};
}

CostConstraintFactory::ConstraintPtrVec
CostConstraintFactory::MakeRangeOfMotionBoxConstraint () const
{
  auto constraint = std::make_shared<RangeOfMotionBox>(
      params->GetMaximumDeviationFromNominal(),
      params->GetNominalStanceInBase()
      );

  constraint->Init(*com_motion, *ee_motion, params->dt_nodes_);
  return {constraint};
}

CostConstraintFactory::ConstraintPtrVec
CostConstraintFactory::MakeConvexityConstraint() const
{
  auto cop_constrait = std::make_shared<SupportAreaConstraint>();
  cop_constrait->Init(*ee_motion,
                   *ee_load,
                   *cop,
                   ee_motion->GetTotalTime(),
                   params->dt_nodes_);

  auto convexity = std::make_shared<ConvexityConstraint>();
  convexity->Init(*ee_load);

  auto contact_load = std::make_shared<ContactLoadConstraint>();
  contact_load->Init(*ee_motion, *ee_load);

  return {cop_constrait, convexity, contact_load};
}


CostConstraintFactory::ConstraintPtrVec
CostConstraintFactory::MakeFinalStanceConstraint () const
{
//  auto constr = std::make_shared<FootholdFinalStanceConstraint>(
//      motion_structure,
//      final_geom_state_.p,
//      params->GetNominalStanceInBase());
//  return constr;
}

CostConstraintFactory::ConstraintPtrVec
CostConstraintFactory::MakeObstacleConstraint () const
{
//  auto constraint = std::make_shared<ObstacleLineStrip>();
//  return constraint;
}

CostConstraintFactory::ConstraintPtrVec
CostConstraintFactory::MakePolygonCenterConstraint () const
{
  auto constraint = std::make_shared<PolygonCenterConstraint>();
  constraint->Init(*ee_load, *ee_motion);
  return {constraint};
}

CostConstraintFactory::CostPtr
CostConstraintFactory::MakeMotionCost() const
{
  LinearSplineEquations eq(*com_motion);
  Eigen::MatrixXd term;

  MotionDerivative dxdt = kAcc;

  switch (dxdt) {
    case kAcc:  term = eq.MakeAcceleration(params->weight_com_motion_xy_); break;
    case kJerk: term = eq.MakeJerk(params->weight_com_motion_xy_); break;
    default: assert(false); break; // this cost is not implemented
  }

  MatVec mv(term.rows(), term.cols());
  mv.M = term;
  mv.v.setZero();

  auto cost = std::make_shared<QuadraticSplineCost>();
  cost->Init(mv, *com_motion);
  return cost;
}

CostConstraintFactory::CostPtr
CostConstraintFactory::ToCost (const ConstraintPtr& constraint) const
{
  return std::make_shared<SoftConstraint>(constraint);
}

} /* namespace opt */
} /* namespace xpp */

