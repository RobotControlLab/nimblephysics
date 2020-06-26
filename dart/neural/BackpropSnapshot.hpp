#ifndef DART_NEURAL_SNAPSHOT_HPP_
#define DART_NEURAL_SNAPSHOT_HPP_

#include <unordered_map>
#include <vector>

#include <Eigen/Dense>

#include "dart/neural/NeuralUtils.hpp"
#include "dart/simulation/World.hpp"

namespace dart {
namespace neural {

class BackpropSnapshot
{
public:
  /// This saves a snapshot from a forward pass, with all the info we need in
  /// order to efficiently compute a backwards pass. Crucially, the positions
  /// must all be snapshots from before the timestep, yet this constructor must
  /// be called after the timestep.
  BackpropSnapshot(
      simulation::WorldPtr world,
      Eigen::VectorXd forwardPassPosition,
      Eigen::VectorXd forwardPassVelocity,
      Eigen::VectorXd forwardPassTorques);

  /// This computes the implicit backprop without forming intermediate
  /// Jacobians. It takes a LossGradient with the position and velocity vectors
  /// filled it, though the loss with respect to torque is ignored and can be
  /// null. It returns a LossGradient with all three values filled in, position,
  /// velocity, and torque.
  void backprop(
      simulation::WorldPtr world,
      LossGradient& thisTimestepLoss,
      const LossGradient& nextTimestepLoss);

  /// This computes and returns the whole vel-vel jacobian. For backprop, you
  /// don't actually need this matrix, you can compute backprop directly. This
  /// is here if you want access to the full Jacobian for some reason.
  Eigen::MatrixXd getVelVelJacobian(simulation::WorldPtr world);

  /// This computes and returns the whole pos-vel jacobian. For backprop, you
  /// don't actually need this matrix, you can compute backprop directly. This
  /// is here if you want access to the full Jacobian for some reason.
  Eigen::MatrixXd getPosVelJacobian(simulation::WorldPtr world);

  /// This computes and returns the whole force-vel jacobian. For backprop, you
  /// don't actually need this matrix, you can compute backprop directly. This
  /// is here if you want access to the full Jacobian for some reason.
  Eigen::MatrixXd getForceVelJacobian(simulation::WorldPtr world);

  /// This computes and returns the whole pos-pos jacobian. For backprop, you
  /// don't actually need this matrix, you can compute backprop directly. This
  /// is here if you want access to the full Jacobian for some reason.
  Eigen::MatrixXd getPosPosJacobian(simulation::WorldPtr world);

  /// This computes and returns the whole vel-pos jacobian. For backprop, you
  /// don't actually need this matrix, you can compute backprop directly. This
  /// is here if you want access to the full Jacobian for some reason.
  Eigen::MatrixXd getVelPosJacobian(simulation::WorldPtr world);

  /// Returns a concatenated vector of all the Skeletons' position()'s in the
  /// World, in order in which the Skeletons appear in the World's
  /// getSkeleton(i) returns them.
  Eigen::VectorXd getForwardPassPosition();

  /// Returns a concatenated vector of all the Skeletons' velocity()'s in the
  /// World, in order in which the Skeletons appear in the World's
  /// getSkeleton(i) returns them.
  Eigen::VectorXd getForwardPassVelocity();

  /// Returns a concatenated vector of all the joint torques that were applied
  /// during the forward pass.
  Eigen::VectorXd getForwardPassTorques();

  /////////////////////////////////////////////////////////////////////////////
  /// Just public for testing
  /////////////////////////////////////////////////////////////////////////////

  /// This returns the A_c matrix. You shouldn't ever need this matrix, it's
  /// just here to enable testing.
  Eigen::MatrixXd getClampingConstraintMatrix(simulation::WorldPtr world);

  /// This returns the V_c matrix. You shouldn't ever need this matrix, it's
  /// just here to enable testing.
  Eigen::MatrixXd getMassedClampingConstraintMatrix(simulation::WorldPtr world);

  /// This returns the A_ub matrix. You shouldn't ever need this matrix, it's
  /// just here to enable testing.
  Eigen::MatrixXd getUpperBoundConstraintMatrix(simulation::WorldPtr world);

  /// This returns the V_c matrix. You shouldn't ever need this matrix, it's
  /// just here to enable testing.
  Eigen::MatrixXd getMassedUpperBoundConstraintMatrix(
      simulation::WorldPtr world);

  /// This returns the E matrix. You shouldn't ever need this matrix, it's
  /// just here to enable testing.
  Eigen::MatrixXd getUpperBoundMappingMatrix();

  /// This returns the B matrix. You shouldn't ever need this matrix, it's
  /// just here to enable testing.
  Eigen::MatrixXd getBouncingConstraintMatrix(simulation::WorldPtr world);

  /// This returns the mass matrix for the whole world, a block diagonal
  /// concatenation of the skeleton mass matrices.
  Eigen::MatrixXd getMassMatrix(simulation::WorldPtr world);

  /// This returns the inverse mass matrix for the whole world, a block diagonal
  /// concatenation of the skeleton inverse mass matrices.
  Eigen::MatrixXd getInvMassMatrix(simulation::WorldPtr world);

  /// This returns the pos-C(pos,vel) Jacobian for the whole world, a block
  /// diagonal concatenation of the skeleton pos-C(pos,vel) Jacobians.
  Eigen::MatrixXd getPosCJacobian(simulation::WorldPtr world);

  /// This returns the vel-C(pos,vel) Jacobian for the whole world, a block
  /// diagonal concatenation of the skeleton vel-C(pos,vel) Jacobians.
  Eigen::MatrixXd getVelCJacobian(simulation::WorldPtr world);

  /// This computes and returns the whole vel-vel jacobian by finite
  /// differences. This is SUPER SLOW, and is only here for testing.
  Eigen::MatrixXd finiteDifferenceVelVelJacobian(simulation::WorldPtr world);

  /// This computes and returns the whole pos-C(pos,vel) jacobian by finite
  /// differences. This is SUPER SUPER SLOW, and is only here for testing.
  Eigen::MatrixXd finiteDifferencePosVelJacobian(simulation::WorldPtr world);

  /// This computes and returns the whole force-vel jacobian by finite
  /// differences. This is SUPER SLOW, and is only here for testing.
  Eigen::MatrixXd finiteDifferenceForceVelJacobian(simulation::WorldPtr world);

  /// This computes and returns the whole vel-vel jacobian by finite
  /// differences. This is SUPER SUPER SLOW, and is only here for testing.
  Eigen::MatrixXd finiteDifferencePosPosJacobian(
      simulation::WorldPtr world, std::size_t subdivisions = 20);

  /// This computes and returns the whole vel-pos jacobian by finite
  /// differences. This is SUPER SUPER SLOW, and is only here for testing.
  Eigen::MatrixXd finiteDifferenceVelPosJacobian(
      simulation::WorldPtr world, std::size_t subdivisions = 20);

  /// This returns the P_c matrix. You shouldn't ever need this matrix, it's
  /// just here to enable testing.
  Eigen::MatrixXd getProjectionIntoClampsMatrix(simulation::WorldPtr world);

  /// These was the mX() vector used to construct this. Pretty much only here
  /// for testing.
  Eigen::VectorXd getContactConstraintImpluses();

  /// These was the fIndex() vector used to construct this. Pretty much only
  /// here for testing.
  Eigen::VectorXi getContactConstraintMappings();

  /// Returns the vector of the coefficients on the diagonal of the bounce
  /// matrix. These are 1+restitutionCoeff[i].
  Eigen::VectorXd getBounceDiagonals();

  /// Returns the vector of the restitution coeffs, sized for the number of
  /// bouncing collisions.
  Eigen::VectorXd getRestitutionDiagonals();

  /// Returns the penetration correction hack "bounce" (or 0 if the contact is
  /// not inter-penetrating or is actively bouncing) at each contact point.
  Eigen::VectorXd getPenetrationCorrectionVelocities();

protected:
  /// This is the global timestep length. This is included here because it shows
  /// up as a constant in some of the matrices.
  double mTimeStep;

  /// This is the total DOFs for this World
  std::size_t mNumDOFs;

  /// This is the number of total dimensions on all the constraints active in
  /// the world
  std::size_t mNumConstraintDim;

  /// This is the number of total constraint dimensions that are clamping
  std::size_t mNumClamping;

  /// This is the number of total constraint dimensions that are upper bounded
  std::size_t mNumUpperBound;

  /// This is the number of total constraint dimensions that are upper bounded
  std::size_t mNumBouncing;

  /// These are the offsets into the total degrees of freedom for each skeleton
  std::unordered_map<std::string, std::size_t> mSkeletonOffset;

  /// These are the gradient constraint matrices from the LCP solver
  std::vector<std::shared_ptr<ConstrainedGroupGradientMatrices>>
      mGradientMatrices;

  /// The position of all the DOFs of the world, when this snapshot was created
  Eigen::VectorXd mForwardPassPosition;

  /// The velocities of all the DOFs of the world, when this snapshot was
  /// created
  Eigen::VectorXd mForwardPassVelocity;

  /// The torques on all the DOFs of the world, when this snapshot was created
  Eigen::VectorXd mForwardPassTorques;

private:
  enum MatrixToAssemble
  {
    CLAMPING,
    MASSED_CLAMPING,
    UPPER_BOUND,
    MASSED_UPPER_BOUND,
    BOUNCING
  };

  Eigen::MatrixXd assembleMatrix(
      simulation::WorldPtr world, MatrixToAssemble whichMatrix);

  enum BlockDiagonalMatrixToAssemble
  {
    MASS,
    INV_MASS,
    POS_C,
    VEL_C
  };

  Eigen::MatrixXd assembleBlockDiagonalMatrix(
      simulation::WorldPtr world, BlockDiagonalMatrixToAssemble whichMatrix);

  enum VectorToAssemble
  {
    CONTACT_CONSTRAINT_IMPULSES,
    CONTACT_CONSTRAINT_MAPPINGS,
    BOUNCE_DIAGONALS,
    RESTITUTION_DIAGONALS,
    PENETRATION_VELOCITY_HACK
  };
  template <typename Vec>
  Vec assembleVector(VectorToAssemble whichVector);

  template <typename Vec>
  const Vec& getVectorToAssemble(
      std::shared_ptr<ConstrainedGroupGradientMatrices> matrices,
      VectorToAssemble whichVector);
};

using BackpropSnapshotPtr = std::shared_ptr<BackpropSnapshot>;

} // namespace neural
} // namespace dart

#endif