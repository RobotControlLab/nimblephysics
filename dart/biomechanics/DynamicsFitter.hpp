#ifndef DART_BIOMECH_DYNAMICS_FITTER_HPP_
#define DART_BIOMECH_DYNAMICS_FITTER_HPP_

#include <memory>
#include <tuple>
#include <vector>

#include <coin/IpIpoptApplication.hpp>
#include <coin/IpTNLP.hpp>

#include "dart/biomechanics/ForcePlate.hpp"
#include "dart/biomechanics/MarkerFitter.hpp"
#include "dart/dynamics/BodyNode.hpp"
#include "dart/dynamics/Skeleton.hpp"
#include "dart/dynamics/SmartPointer.hpp"
#include "dart/math/Geometry.hpp"
#include "dart/math/MathTypes.hpp"
#include "dart/neural/DifferentiableExternalForce.hpp"
#include "dart/neural/WithRespectTo.hpp"

#define HUMAN_DENSITY_KG_M3 985.0

namespace dart {
namespace biomechanics {

/**
 * This class factors out the code to deal with calculating residual forces, and
 * the associated Jacobians of residual force with respect to lots of different
 * inputs.
 */
class ResidualForceHelper
{
public:
  ResidualForceHelper(
      std::shared_ptr<dynamics::Skeleton> skeleton,
      std::vector<int> forceBodies);

  ///////////////////////////////////////////
  // Computes the residual for a specific timestep
  Eigen::Vector6s calculateResidual(
      Eigen::VectorXs q,
      Eigen::VectorXs dq,
      Eigen::VectorXs ddq,
      Eigen::VectorXs forcesConcat);

  ///////////////////////////////////////////
  // Computes the residual norm for a specific timestep
  s_t calculateResidualNorm(
      Eigen::VectorXs q,
      Eigen::VectorXs dq,
      Eigen::VectorXs ddq,
      Eigen::VectorXs forcesConcat,
      bool useL1 = false);

  ///////////////////////////////////////////
  // Computes the Jacobian of the residual with respect to `wrt`
  Eigen::MatrixXs calculateResidualJacobianWrt(
      Eigen::VectorXs q,
      Eigen::VectorXs dq,
      Eigen::VectorXs ddq,
      Eigen::VectorXs forcesConcat,
      neural::WithRespectTo* wrt);

  ///////////////////////////////////////////
  // Computes the Jacobian of the residual with respect to `wrt`
  Eigen::MatrixXs finiteDifferenceResidualJacobianWrt(
      Eigen::VectorXs q,
      Eigen::VectorXs dq,
      Eigen::VectorXs ddq,
      Eigen::VectorXs forcesConcat,
      neural::WithRespectTo* wrt);

  ///////////////////////////////////////////
  // Computes the gradient of the residual norm with respect to `wrt`
  Eigen::VectorXs calculateResidualNormGradientWrt(
      Eigen::VectorXs q,
      Eigen::VectorXs dq,
      Eigen::VectorXs ddq,
      Eigen::VectorXs forcesConcat,
      neural::WithRespectTo* wrt,
      bool useL1 = false);

  ///////////////////////////////////////////
  // Computes the gradient of the residual norm with respect to `wrt`
  Eigen::VectorXs finiteDifferenceResidualNormGradientWrt(
      Eigen::VectorXs q,
      Eigen::VectorXs dq,
      Eigen::VectorXs ddq,
      Eigen::VectorXs forcesConcat,
      neural::WithRespectTo* wrt,
      bool useL1 = false);

protected:
  std::shared_ptr<dynamics::Skeleton> mSkel;
  std::vector<neural::DifferentiableExternalForce> mForces;
};

/**
 * We create a single initialization object, and pass it around to optimization
 * problems to re-use, because it's not super cheap to construct.
 */
struct DynamicsInitialization
{
  ///////////////////////////////////////////
  // Inputs from files
  std::vector<std::vector<ForcePlate>> forcePlateTrials;
  std::vector<Eigen::MatrixXs> originalPoseTrials;
  std::vector<std::vector<std::map<std::string, Eigen::Vector3s>>>
      markerObservationTrials;
  std::vector<s_t> trialTimesteps;

  ///////////////////////////////////////////
  // Assigning GRFs to specific feet
  std::vector<Eigen::MatrixXs> grfTrials;
  std::vector<int> grfBodyIndices;
  std::vector<dynamics::BodyNode*> grfBodyNodes;

  ///////////////////////////////////////////
  // Foot ground contact, and rendering
  std::vector<s_t> groundHeight;
  std::vector<bool> flatGround;
  std::vector<std::vector<dynamics::BodyNode*>> contactBodies;
  std::vector<std::vector<std::vector<s_t>>> grfBodyContactSphereRadius;
  std::vector<std::vector<std::vector<bool>>> grfBodyForceActive;
  std::vector<std::vector<std::vector<bool>>> grfBodySphereInContact;
  std::vector<std::vector<Eigen::Vector3s>> defaultForcePlateCorners;
  std::vector<std::vector<std::vector<bool>>> grfBodyOffForcePlate;
  // This is the critical value, telling us if we think we're receiving support
  // from off a force plate on this frame
  std::vector<std::vector<bool>> probablyMissingGRF;

  ///////////////////////////////////////////
  // Pure dynamics values
  Eigen::VectorXs bodyMasses;
  Eigen::VectorXs groupMasses;
  Eigen::Matrix<s_t, 3, Eigen::Dynamic> bodyCom;
  Eigen::Matrix<s_t, 6, Eigen::Dynamic> bodyInertia;
  Eigen::VectorXs groupInertias;

  ///////////////////////////////////////////
  // Values from the kinematics fitter that are relevant here as well. The key
  // difference is that the per-trial split is explicit here, because it's so
  // important for the dynamics and indexing.
  std::vector<Eigen::MatrixXs> poseTrials;
  Eigen::VectorXs groupScales;
  std::map<std::string, Eigen::Vector3s> markerOffsets;
  std::vector<std::string> trackingMarkers;

  std::vector<dynamics::Joint*> joints;
  std::vector<std::vector<std::string>> jointsAdjacentMarkers;
  Eigen::VectorXs jointWeights;
  std::vector<Eigen::MatrixXs> jointCenters;
  Eigen::VectorXs axisWeights;
  std::vector<Eigen::MatrixXs> jointAxis;

  ///////////////////////////////////////////
  // Convenience objects
  std::map<std::string, std::pair<dynamics::BodyNode*, Eigen::Vector3s>>
      updatedMarkerMap;

  ///////////////////////////////////////////
  // To support regularization
  std::vector<Eigen::MatrixXs> originalPoses;
  Eigen::VectorXs originalGroupMasses;
  Eigen::VectorXs originalGroupCOMs;
  Eigen::VectorXs originalGroupInertias;
  Eigen::VectorXs originalGroupScales;
  std::map<std::string, Eigen::Vector3s> originalMarkerOffsets;
};

/*
 * Reminder: IPOPT will want to free this object when it's done with
 * optimization. This is responsible for actually transcribing the problem into
 * a format IPOpt can work with.
 */
class DynamicsFitProblem : public Ipopt::TNLP
{
public:
  DynamicsFitProblem(
      std::shared_ptr<DynamicsInitialization> init,
      std::shared_ptr<dynamics::Skeleton> skeleton,
      std::vector<std::string> trackingMarkers,
      std::vector<dynamics::BodyNode*> footNodes);

  // This returns the dimension of the decision variables (the length of the
  // flatten() vector), which depends on which variables we choose to include in
  // the optimization problem.
  int getProblemSize();

  // This writes the problem state into a flat vector
  Eigen::VectorXs flatten();

  // This writes the upper bounds into a flat vector
  Eigen::VectorXs flattenUpperBound();

  // This writes the upper bounds into a flat vector
  Eigen::VectorXs flattenLowerBound();

  // This reads the problem state out of a flat vector, and into the init object
  void unflatten(Eigen::VectorXs x);

  // This gets the value of the loss function, as a weighted sum of the
  // discrepancy between measured and expected GRF data and other regularization
  // terms.
  s_t computeLoss(Eigen::VectorXs x, bool logExplanation = false);

  // This gets the gradient of the loss function
  Eigen::VectorXs computeGradient(Eigen::VectorXs x);

  // This gets the gradient of the loss function
  Eigen::VectorXs finiteDifferenceGradient(
      Eigen::VectorXs x, bool useRidders = true);

  // This gets the number of constraints that the problem requires
  int getConstraintSize();

  // This gets the value of the constraints vector. These constraints are only
  // active when we're including positions in the decision variables, and they
  // just enforce that finite differencing is valid to relate velocity,
  // acceleration, and position.
  Eigen::VectorXs computeConstraints(Eigen::VectorXs x);

  // This gets the sparse version of the constraints jacobian, returning objects
  // with (row,col,value).
  std::vector<std::tuple<int, int, s_t>> computeSparseConstraintsJacobian();

  // This gets the jacobian of the constraints vector with respect to x. This is
  // constraint wrt x, so doesn't take x as an input
  Eigen::MatrixXs computeConstraintsJacobian();

  // This gets the jacobian of the constraints vector with respect to x
  Eigen::MatrixXs finiteDifferenceConstraintsJacobian();

  // Print out the errors in a gradient vector in human readable form
  bool debugErrors(Eigen::VectorXs fd, Eigen::VectorXs analytical, s_t tol);

  DynamicsFitProblem& setIncludeMasses(bool value);
  DynamicsFitProblem& setIncludeCOMs(bool value);
  DynamicsFitProblem& setIncludeInertias(bool value);
  DynamicsFitProblem& setIncludePoses(bool value);
  DynamicsFitProblem& setIncludeMarkerOffsets(bool value);
  DynamicsFitProblem& setIncludeBodyScales(bool value);

  DynamicsFitProblem& setResidualWeight(s_t weight);
  DynamicsFitProblem& setMarkerWeight(s_t weight);
  DynamicsFitProblem& setJointWeight(s_t weight);

  DynamicsFitProblem& setResidualUseL1(bool l1);
  DynamicsFitProblem& setMarkerUseL1(bool l1);

  DynamicsFitProblem& setRegularizeMasses(s_t value);
  DynamicsFitProblem& setRegularizeCOMs(s_t value);
  DynamicsFitProblem& setRegularizeInertias(s_t value);
  DynamicsFitProblem& setRegularizeBodyScales(s_t value);
  DynamicsFitProblem& setRegularizePoses(s_t value);
  DynamicsFitProblem& setRegularizeTrackingMarkerOffsets(s_t value);
  DynamicsFitProblem& setRegularizeAnatomicalMarkerOffsets(s_t value);
  DynamicsFitProblem& setRegularizeImpliedDensity(s_t value);

  DynamicsFitProblem& setVelAccImplicit(bool implicit);

  //------------------------- Ipopt::TNLP --------------------------------------
  /// \brief Method to return some info about the nlp
  bool get_nlp_info(
      Ipopt::Index& n,
      Ipopt::Index& m,
      Ipopt::Index& nnz_jac_g,
      Ipopt::Index& nnz_h_lag,
      Ipopt::TNLP::IndexStyleEnum& index_style) override;

  /// \brief Method to return the bounds for my problem
  bool get_bounds_info(
      Ipopt::Index n,
      Ipopt::Number* x_l,
      Ipopt::Number* x_u,
      Ipopt::Index m,
      Ipopt::Number* g_l,
      Ipopt::Number* g_u) override;

  /// \brief Method to return the starting point for the algorithm
  bool get_starting_point(
      Ipopt::Index n,
      bool init_x,
      Ipopt::Number* x,
      bool init_z,
      Ipopt::Number* z_L,
      Ipopt::Number* z_U,
      Ipopt::Index m,
      bool init_lambda,
      Ipopt::Number* lambda) override;

  /// \brief Method to return the objective value
  bool eval_f(
      Ipopt::Index _n,
      const Ipopt::Number* _x,
      bool _new_x,
      Ipopt::Number& _obj_value) override;

  /// \brief Method to return the gradient of the objective
  bool eval_grad_f(
      Ipopt::Index _n,
      const Ipopt::Number* _x,
      bool _new_x,
      Ipopt::Number* _grad_f) override;

  /// \brief Method to return the constraint residuals
  bool eval_g(
      Ipopt::Index _n,
      const Ipopt::Number* _x,
      bool _new_x,
      Ipopt::Index _m,
      Ipopt::Number* _g) override;

  /// \brief Method to return:
  ///        1) The structure of the jacobian (if "values" is nullptr)
  ///        2) The values of the jacobian (if "values" is not nullptr)
  bool eval_jac_g(
      Ipopt::Index _n,
      const Ipopt::Number* _x,
      bool _new_x,
      Ipopt::Index _m,
      Ipopt::Index _nele_jac,
      Ipopt::Index* _iRow,
      Ipopt::Index* _jCol,
      Ipopt::Number* _values) override;

  /// \brief Method to return:
  ///        1) The structure of the hessian of the lagrangian (if "values" is
  ///           nullptr)
  ///        2) The values of the hessian of the lagrangian (if "values" is not
  ///           nullptr)
  bool eval_h(
      Ipopt::Index _n,
      const Ipopt::Number* _x,
      bool _new_x,
      Ipopt::Number _obj_factor,
      Ipopt::Index _m,
      const Ipopt::Number* _lambda,
      bool _new_lambda,
      Ipopt::Index _nele_hess,
      Ipopt::Index* _iRow,
      Ipopt::Index* _jCol,
      Ipopt::Number* _values) override;

  /// \brief This method is called when the algorithm is complete so the TNLP
  ///        can store/write the solution
  void finalize_solution(
      Ipopt::SolverReturn _status,
      Ipopt::Index _n,
      const Ipopt::Number* _x,
      const Ipopt::Number* _z_L,
      const Ipopt::Number* _z_U,
      Ipopt::Index _m,
      const Ipopt::Number* _g,
      const Ipopt::Number* _lambda,
      Ipopt::Number _obj_value,
      const Ipopt::IpoptData* _ip_data,
      Ipopt::IpoptCalculatedQuantities* _ip_cq) override;

  bool intermediate_callback(
      Ipopt::AlgorithmMode mode,
      Ipopt::Index iter,
      Ipopt::Number obj_value,
      Ipopt::Number inf_pr,
      Ipopt::Number inf_du,
      Ipopt::Number mu,
      Ipopt::Number d_norm,
      Ipopt::Number regularization_size,
      Ipopt::Number alpha_du,
      Ipopt::Number alpha_pr,
      Ipopt::Index ls_trials,
      const Ipopt::IpoptData* ip_data,
      Ipopt::IpoptCalculatedQuantities* ip_cq) override;

public:
  s_t mResidualWeight;
  s_t mMarkerWeight;
  s_t mJointWeight;

  bool mResidualUseL1;
  bool mMarkerUseL1;

  bool mIncludeMasses;
  bool mIncludeCOMs;
  bool mIncludeInertias;
  bool mIncludeBodyScales;
  bool mIncludePoses;
  bool mIncludeMarkerOffsets;
  std::shared_ptr<DynamicsInitialization> mInit;
  std::shared_ptr<dynamics::Skeleton> mSkeleton;

  s_t mRegularizeMasses;
  s_t mRegularizeCOMs;
  s_t mRegularizeInertias;
  s_t mRegularizeBodyScales;
  s_t mRegularizePoses;
  s_t mRegularizeTrackingMarkerOffsets;
  s_t mRegularizeAnatomicalMarkerOffsets;
  s_t mRegularizeImpliedDensity;

  bool mVelAccImplicit;

  std::vector<Eigen::MatrixXs> mPoses;
  std::vector<Eigen::MatrixXs> mVels;
  std::vector<Eigen::MatrixXs> mAccs;

  std::vector<std::string> mMarkerNames;
  std::vector<bool> mMarkerIsTracking;
  std::vector<std::pair<dynamics::BodyNode*, Eigen::Vector3s>> mMarkers;

  std::vector<dynamics::BodyNode*> mFootNodes;
  std::vector<int> mForceBodyIndices;
  std::shared_ptr<ResidualForceHelper> mResidualHelper;

  int mBestObjectiveValueIteration;
  s_t mBestObjectiveValue;
  Eigen::VectorXs mLastX;
  Eigen::VectorXs mBestObjectiveValueState;
};

class DynamicsFitter
{
public:
  DynamicsFitter(
      std::shared_ptr<dynamics::Skeleton> skeleton,
      std::vector<dynamics::BodyNode*> footNodes,
      std::vector<std::string> trackingMarkers);

  // This bundles together the objects we need in order to track a dynamics
  // problem around through multiple steps of optimization
  static std::shared_ptr<DynamicsInitialization> createInitialization(
      std::shared_ptr<dynamics::Skeleton> skel,
      dynamics::MarkerMap markerMap,
      std::vector<std::string> trackingMarkers,
      std::vector<dynamics::BodyNode*> grfNodes,
      std::vector<std::vector<ForcePlate>> forcePlateTrials,
      std::vector<Eigen::MatrixXs> poseTrials,
      std::vector<int> framesPerSecond,
      std::vector<std::vector<std::map<std::string, Eigen::Vector3s>>>
          markerObservationTrials);

  // This creates an optimization problem from a kinematics initialization
  static std::shared_ptr<DynamicsInitialization> createInitialization(
      std::shared_ptr<dynamics::Skeleton> skel,
      MarkerInitialization* kinematicInit,
      std::vector<std::string> trackingMarkers,
      std::vector<dynamics::BodyNode*> grfNodes,
      std::vector<std::vector<ForcePlate>> forcePlateTrials,
      std::vector<int> framesPerSecond,
      std::vector<std::vector<std::map<std::string, Eigen::Vector3s>>>
          markerObservationTrials);

  // This computes and returns the positions of the center of mass at each
  // frame
  std::vector<Eigen::Vector3s> comPositions(
      std::shared_ptr<DynamicsInitialization> init, int trial);

  // This computes and returns the acceleration of the center of mass at each
  // frame
  std::vector<Eigen::Vector3s> comAccelerations(
      std::shared_ptr<DynamicsInitialization> init, int trial);

  // This computes and returns a list of the net forces on the center of mass,
  // given the motion and link masses
  std::vector<Eigen::Vector3s> impliedCOMForces(
      std::shared_ptr<DynamicsInitialization> init,
      int trial,
      bool includeGravity = true);

  // This returns a list of the total GRF force on the body at each timestep
  std::vector<Eigen::Vector3s> measuredGRFForces(
      std::shared_ptr<DynamicsInitialization> init, int trial);

  // 0. Estimate when each foot is in contact with the ground, which we can use
  // to infer when we're missing GRF data on certain timesteps, so we don't let
  // it mess with our optimization.
  void estimateFootGroundContacts(std::shared_ptr<DynamicsInitialization> init);

  // 1. Scale the total mass of the body (keeping the ratios of body links
  // constant) to get it as close as possible to GRF gravity forces.
  void scaleLinkMassesFromGravity(std::shared_ptr<DynamicsInitialization> init);

  // 2. Estimate just link masses, while holding the positions, COMs, and
  // inertias constant
  void estimateLinkMassesFromAcceleration(
      std::shared_ptr<DynamicsInitialization> init,
      s_t regularizationWeight = 50.0);

  // 3. Run larger optimization problems to minimize a weighted combination of
  // residuals and marker RMSE, tweaking a controllable set of variables. This
  // includes the velocity and acceleration as explicit decision variables,
  // constrained by linear constraint equations. That means it needs to be
  // solved with IPOPT, using the interior point method.
  //
  // WARNING: DOES NOT PERFORM WELL WITH WARM STARTS! Becaus it uses the
  // interior point method, this doesn't warm start well. See
  // runImplicitVelAccOptimization() instead.
  void runIPOPTOptimization(
      std::shared_ptr<DynamicsInitialization> init,
      s_t residualWeight,
      s_t markerWeight,
      bool includeMasses,
      bool includeCOMs,
      bool includeInertias,
      bool includeBodyScales,
      bool includePoses,
      bool includeMarkerOffsets,
      bool implicitVelAcc);

  // 4. This runs the same optimization problem as
  // runExplicitVelAccOptimization(), but holds velocity and acc as implicit
  // functions of the position values, and removes any constraints. That means
  // we can optimize this using simple gradient descent with line search, and
  // can warm start.
  Eigen::VectorXs runSGDOptimization(
      std::shared_ptr<DynamicsInitialization> init,
      s_t residualWeight,
      s_t markerWeight,
      bool includeMasses,
      bool includeCOMs,
      bool includeInertias,
      bool includeBodyScales,
      bool includePoses,
      bool includeMarkerOffsets);

  // Get the average RMSE, in meters, of the markers
  s_t computeAverageMarkerRMSE(std::shared_ptr<DynamicsInitialization> init);

  // Get the average residual force (in newtons) and torque (in newton-meters)
  std::pair<s_t, s_t> computeAverageResidualForce(
      std::shared_ptr<DynamicsInitialization> init);

  // Get the average real measured force (in newtons) and torque (in
  // newton-meters)
  std::pair<s_t, s_t> computeAverageRealForce(
      std::shared_ptr<DynamicsInitialization> init);

  // This debugs the current state, along with visualizations of errors where
  // the dynamics do not match the force plate data
  void saveDynamicsToGUI(
      const std::string& path,
      std::shared_ptr<DynamicsInitialization> init,
      int trialIndex,
      int framesPerSecond);

  void setTolerance(double tol);
  void setIterationLimit(int limit);
  void setLBFGSHistoryLength(int len);
  void setCheckDerivatives(bool check);
  void setPrintFrequency(int freq);
  void setSilenceOutput(bool silent);
  void setDisableLinesearch(bool disable);

protected:
  std::shared_ptr<dynamics::Skeleton> mSkeleton;
  std::vector<dynamics::BodyNode*> mFootNodes;
  std::vector<std::string> mTrackingMarkers;
  // These are IPOPT settings
  double mTolerance;
  int mIterationLimit;
  int mLBFGSHistoryLength;
  bool mCheckDerivatives;
  int mPrintFrequency;
  bool mSilenceOutput;
  bool mDisableLinesearch;
};

}; // namespace biomechanics
}; // namespace dart

#endif