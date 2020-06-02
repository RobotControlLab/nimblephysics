#include "dart/neural/MassedBackpropSnapshot.hpp"

#include <iostream>
#include <memory>

#include <Eigen/Dense>

#include "dart/simulation/World.hpp"

using namespace dart;
using namespace math;
using namespace dynamics;
using namespace simulation;

namespace dart {
namespace neural {

//==============================================================================
MassedBackpropSnapshot::MassedBackpropSnapshot(
    simulation::WorldPtr world,
    Eigen::VectorXd forwardPassPosition,
    Eigen::VectorXd forwardPassVelocity,
    Eigen::VectorXd forwardPassTorques)
  : BackpropSnapshot(
      world, forwardPassPosition, forwardPassVelocity, forwardPassTorques)
{
}

Eigen::MatrixXd MassedBackpropSnapshot::getProjectionIntoClampsMatrix()
{
  Eigen::MatrixXd V_c = getClampingConstraintMatrix();
  Eigen::MatrixXd V_ub = getUpperBoundConstraintMatrix();
  Eigen::MatrixXd E = getUpperBoundMappingMatrix();

  /*
  std::cout << "Computing X_c:" << std::endl;
  std::cout << "V_c size: " << V_c.size() << std::endl;
  std::cout << "V_ub size: " << V_ub.size() << std::endl;
  std::cout << "E size: " << E.size() << std::endl;
  std::cout << "M size: " << getMassMatrix().size() << std::endl;
  */

  Eigen::MatrixXd V_cInv
      = V_c.completeOrthogonalDecomposition().pseudoInverse();

  if (V_ub.size() > 0 && E.size() > 0)
  {
    // std::cout << "Doing X_c computation with V_ub and E" << std::endl;
    return (V_c + V_ub * E) * V_cInv.eval() * getInvMassMatrix()
           * V_cInv.eval().transpose() * V_c.transpose();
  }
  else
  {
    /*
    std::cout << "Doing X_c computation without V_ub and E" << std::endl;
    std::cout << "V_c: " << std::endl << V_c << std::endl;
    std::cout << "Minv: " << std::endl << getInvMassMatrix() << std::endl;
    std::cout << "V_cInv: " << std::endl << V_cInv << std::endl;
    */
    return V_c * V_cInv.eval() * getInvMassMatrix() * V_cInv.eval().transpose()
           * V_c.eval().transpose();
  }
}

Eigen::MatrixXd MassedBackpropSnapshot::getForceVelJacobian()
{
  Eigen::MatrixXd X_c = getProjectionIntoClampsMatrix();

  /*
  // TODO(keenon): Remove me
  int dofs = getNumDofs();
  Eigen::MatrixXd A_c = getClampingConstraintMatrix();
  std::cout << "A_c: " << std::endl << A_c << std::endl;
  Eigen::MatrixXd A_cInv
      = A_c.completeOrthogonalDecomposition().pseudoInverse();
  std::cout << "A_cInv: " << std::endl << A_cInv << std::endl;
  std::cout << "X_c: " << std::endl << X_c << std::endl;
  Eigen::MatrixXd P_c = A_cInv * getMassMatrix() * X_c * getMassMatrix();
  std::cout << "Recovered P_c: " << std::endl << P_c << std::endl;
  Eigen::MatrixXd classical = dt * getInvMassMatrix()
                              * (Eigen::MatrixXd::Identity(dofs, dofs)
                                 - dt * A_c * P_c * getInvMassMatrix());
  std::cout << "classical ForceVel: " << std::endl << classical << std::endl;
  // TODO(keenon): ^ Remove above

  Eigen::MatrixXd massed = dt * (getInvMassMatrix() - dt * X_c);
  std::cout << "massed ForceVel: " << std::endl << massed << std::endl;
  */

  return mTimeStep * (getInvMassMatrix() - X_c);
}

Eigen::MatrixXd MassedBackpropSnapshot::getVelVelJacobian()
{
  Eigen::MatrixXd X_c = getProjectionIntoClampsMatrix();

  Eigen::MatrixXd B = Eigen::MatrixXd::Identity(mNumDOFs, mNumDOFs);

  /*
  // TODO(keenon): Remove me
  Eigen::MatrixXd A_c = getClampingConstraintMatrix();
  std::cout << "A_c: " << std::endl << A_c << std::endl;
  Eigen::MatrixXd A_cInv
      = A_c.completeOrthogonalDecomposition().pseudoInverse();
  std::cout << "A_cInv: " << std::endl << A_cInv << std::endl;
  std::cout << "X_c: " << std::endl << X_c << std::endl;
  Eigen::MatrixXd P_c = A_cInv * getMassMatrix() * X_c * getMassMatrix();
  std::cout << "Recovered P_c: " << std::endl << P_c << std::endl;
  Eigen::MatrixXd classical = (Eigen::MatrixXd::Identity(dofs, dofs)
                               - dt * getInvMassMatrix() * A_c * P_c)
                              * B;
  std::cout << "classical VelVel: " << std::endl << classical << std::endl;
  // TODO(keenon): ^ Remove above

  Eigen::MatrixXd massed
      = (Eigen::MatrixXd::Identity(dofs, dofs) - dt * X_c * getMassMatrix())
        * B;

  std::cout << "massed VelVel: " << std::endl << massed << std::endl;
  */

  return (Eigen::MatrixXd::Identity(mNumDOFs, mNumDOFs) - X_c * getMassMatrix())
         * B;
}

} // namespace neural
} // namespace dart