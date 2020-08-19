#include "dart/neural/DifferentiableContactConstraint.hpp"

#include "dart/collision/Contact.hpp"
#include "dart/constraint/ConstraintBase.hpp"
#include "dart/constraint/ContactConstraint.hpp"
#include "dart/dynamics/DegreeOfFreedom.hpp"
#include "dart/dynamics/Joint.hpp"
#include "dart/dynamics/Skeleton.hpp"
#include "dart/neural/BackpropSnapshot.hpp"
#include "dart/neural/RestorableSnapshot.hpp"
#include "dart/simulation/World.hpp"

namespace dart {
namespace neural {

//==============================================================================
DifferentiableContactConstraint::DifferentiableContactConstraint(
    std::shared_ptr<constraint::ConstraintBase> constraint, int index)
{
  mConstraint = constraint;
  mIndex = index;
  if (mConstraint->isContactConstraint())
  {
    mContactConstraint
        = std::static_pointer_cast<constraint::ContactConstraint>(mConstraint);
    // This needs to be explicitly copied, otherwise the memory is overwritten
    mContact = std::make_shared<collision::Contact>(
        mContactConstraint->getContact());
  }
  for (auto skel : constraint->getSkeletons())
  {
    mSkeletons.push_back(skel->getName());
  }
}

//==============================================================================
Eigen::Vector3d DifferentiableContactConstraint::getContactWorldPosition()
{
  if (!mConstraint->isContactConstraint())
  {
    return Eigen::Vector3d::Zero();
  }
  return mContact->point;
}

//==============================================================================
Eigen::Vector3d DifferentiableContactConstraint::getContactWorldNormal()
{
  if (!mConstraint->isContactConstraint())
  {
    return Eigen::Vector3d::Zero();
  }
  return mContact->normal;
}

//==============================================================================
Eigen::Vector3d DifferentiableContactConstraint::getContactWorldForceDirection()
{
  if (!mConstraint->isContactConstraint())
  {
    return Eigen::Vector3d::Zero();
  }
  if (mIndex == 0)
  {
    return mContact->normal;
  }
  else
  {
    return mContactConstraint->getTangentBasisMatrixODE(mContact->normal)
        .col(mIndex - 1);
  }
}

//==============================================================================
Eigen::Vector6d DifferentiableContactConstraint::getWorldForce()
{
  Eigen::Vector6d worldForce = Eigen::Vector6d();
  worldForce.head<3>()
      = getContactWorldPosition().cross(getContactWorldForceDirection());
  worldForce.tail<3>() = getContactWorldForceDirection();
  return worldForce;
}

//==============================================================================
collision::ContactType DifferentiableContactConstraint::getContactType()
{
  if (!mConstraint->isContactConstraint())
  {
    // UNSUPPORTED is the default, and means we won't attempt to get gradients
    // for how the contact point moves as we move the skeletons.
    return collision::ContactType::UNSUPPORTED;
  }
  return mContact->type;
}

//==============================================================================
/// This figures out what type of contact this skeleton is involved in.
SkeletonContactType DifferentiableContactConstraint::getSkeletonContactType(
    std::shared_ptr<dynamics::Skeleton> skel)
{
  if (skel->getName()
      == mContactConstraint->getBodyNodeA()->getSkeleton()->getName())
  {
    switch (getContactType())
    {
      case collision::ContactType::FACE_VERTEX:
        return SkeletonContactType::FACE;
      case collision::ContactType::VERTEX_FACE:
        return SkeletonContactType::VERTEX;
      case collision::ContactType::EDGE_EDGE:
        return SkeletonContactType::EDGE;
      default:
        return SkeletonContactType::UNSUPPORTED;
    }
  }
  else if (
      skel->getName()
      == mContactConstraint->getBodyNodeB()->getSkeleton()->getName())
  {
    switch (getContactType())
    {
      case collision::ContactType::FACE_VERTEX:
        return SkeletonContactType::VERTEX;
      case collision::ContactType::VERTEX_FACE:
        return SkeletonContactType::FACE;
      case collision::ContactType::EDGE_EDGE:
        return SkeletonContactType::EDGE;
      default:
        return SkeletonContactType::UNSUPPORTED;
    }
  }
  return SkeletonContactType::NONE;
}

//==============================================================================
Eigen::VectorXd DifferentiableContactConstraint::getConstraintForces(
    std::shared_ptr<dynamics::Skeleton> skel)
{
  // If this constraint doesn't touch this skeleton, then return all 0s
  if (std::find(mSkeletons.begin(), mSkeletons.end(), skel->getName())
      == mSkeletons.end())
  {
    return Eigen::VectorXd::Zero(skel->getNumDofs());
  }

  Eigen::Vector6d worldForce = getWorldForce();

  Eigen::VectorXd taus = Eigen::VectorXd(skel->getNumDofs());
  for (int i = 0; i < skel->getNumDofs(); i++)
  {
    auto dof = skel->getDof(i);
    double multiple = getForceMultiple(dof);
    if (multiple == 0)
    {
      taus(i) = 0.0;
    }
    else
    {
      Eigen::Vector6d worldTwist = getWorldScrewAxis(dof);
      taus(i) = worldTwist.dot(worldForce) * multiple;
    }
  }

  return taus;
}

//==============================================================================
Eigen::VectorXd DifferentiableContactConstraint::getConstraintForces(
    std::shared_ptr<simulation::World> world)
{
  Eigen::VectorXd taus = Eigen::VectorXd(world->getNumDofs());
  int cursor = 0;
  for (int i = 0; i < world->getNumSkeletons(); i++)
  {
    auto skel = world->getSkeleton(i);
    int dofs = skel->getNumDofs();
    taus.segment(cursor, dofs) = getConstraintForces(skel);
    cursor += dofs;
  }
  return taus;
}

//==============================================================================
/// Returns the gradient of the contact position with respect to the
/// specified dof of this skeleton
Eigen::Vector3d DifferentiableContactConstraint::getContactPositionGradient(
    dynamics::DegreeOfFreedom* dof)
{
  Eigen::Vector3d contactPos = getContactWorldPosition();
  SkeletonContactType type = getSkeletonContactType(dof->getSkeleton());

  if (type == VERTEX)
  {
    int jointIndex = dof->getIndexInJoint();
    dynamics::BodyNode* childNode = dof->getChildBodyNode();
    // TODO:opt getRelativeJacobian() creates a whole matrix, when we only want
    // a single column.
    Eigen::Vector6d worldTwist = math::AdT(
        childNode->getWorldTransform(),
        dof->getJoint()->getRelativeJacobian().col(jointIndex));
    return math::gradientWrtTheta(worldTwist, contactPos, 0.0);
  }
  else if (type == FACE)
  {
    return Eigen::Vector3d::Zero();
  }
  else if (type == EDGE)
  {
    /// TODO: this can't possibly be right, requires more thought
    return Eigen::Vector3d::Zero();
  }
  else
  {
    // Default case
    return Eigen::Vector3d::Zero();
  }
}

//==============================================================================
/// Returns the gradient of the contact normal with respect to the
/// specified dof of this skeleton
Eigen::Vector3d DifferentiableContactConstraint::getContactNormalGradient(
    dynamics::DegreeOfFreedom* dof)
{
  Eigen::Vector3d normal = getContactWorldNormal();
  SkeletonContactType type = getSkeletonContactType(dof->getSkeleton());
  if (type == VERTEX)
  {
    return Eigen::Vector3d::Zero();
  }
  else if (type == FACE)
  {
    int jointIndex = dof->getIndexInJoint();
    math::Jacobian relativeJac = dof->getJoint()->getRelativeJacobian();
    dynamics::BodyNode* childNode = dof->getChildBodyNode();
    Eigen::Isometry3d transform = childNode->getWorldTransform();
    Eigen::Vector6d localTwist = relativeJac.col(jointIndex);
    Eigen::Vector6d worldTwist = math::AdT(transform, localTwist);
    return math::gradientWrtThetaPureRotation(worldTwist.head<3>(), normal, 0);
  }
  else if (type == EDGE)
  {
    /// TODO: this can't possibly be right, requires more thought
    return Eigen::Vector3d::Zero();
  }
  else
  {
    // Default case
    return Eigen::Vector3d::Zero();
  }
}

//==============================================================================
/// Returns the gradient of the contact force with respect to the
/// specified dof of this skeleton
Eigen::Vector3d DifferentiableContactConstraint::getContactForceGradient(
    dynamics::DegreeOfFreedom* dof)
{
  SkeletonContactType type = getSkeletonContactType(dof->getSkeleton());
  if (type == VERTEX)
  {
    return Eigen::Vector3d::Zero();
  }
  else if (type == FACE)
  {
    Eigen::Vector3d contactNormal = getContactWorldNormal();
    Eigen::Vector3d normalGradient = getContactNormalGradient(dof);
    if (mIndex == 0 || normalGradient.squaredNorm() <= 1e-12)
      return normalGradient;
    else
    {
      return mContactConstraint
          ->getTangentBasisMatrixODEGradient(contactNormal, normalGradient)
          .col(mIndex - 1);
    }
  }
  else if (type == EDGE)
  {
    /// TODO: this can't possibly be right, requires more thought
    return Eigen::Vector3d::Zero();
  }
  else
  {
    // Default case
    return Eigen::Vector3d::Zero();
  }
}

//==============================================================================
/// Returns the gradient of the full 6d twist force
Eigen::Vector6d DifferentiableContactConstraint::getContactWorldForceGradient(
    dynamics::DegreeOfFreedom* dof)
{
  Eigen::Vector3d position = getContactWorldPosition();
  Eigen::Vector3d force = getContactWorldForceDirection();
  Eigen::Vector3d forceGradient = getContactForceGradient(dof);
  Eigen::Vector3d positionGradient = getContactPositionGradient(dof);

  Eigen::Vector6d result = Eigen::Vector6d::Zero();
  result.head<3>()
      = position.cross(forceGradient) + positionGradient.cross(force);
  result.tail<3>() = forceGradient;
  return result;
}

//==============================================================================
/// Returns the gradient of the screw axis with respect to the rotate dof
Eigen::Vector6d DifferentiableContactConstraint::getScrewAxisGradient(
    dynamics::DegreeOfFreedom* screwDof, dynamics::DegreeOfFreedom* rotateDof)
{
  if (screwDof->getSkeleton()->getName() != rotateDof->getSkeleton()->getName())
    return Eigen::Vector6d::Zero();

  Eigen::Vector6d axisWorldTwist = getWorldScrewAxis(screwDof);
  Eigen::Vector6d rotateWorldTwist = getWorldScrewAxis(rotateDof);
  return math::ad(rotateWorldTwist, axisWorldTwist);
}

//==============================================================================
/// This is the analytical Jacobian for the contact position
math::LinearJacobian
DifferentiableContactConstraint::getContactPositionJacobian(
    std::shared_ptr<simulation::World> world)
{
  math::LinearJacobian jac = math::LinearJacobian::Zero(3, world->getNumDofs());
  int i = 0;
  for (auto dof : world->getDofs())
  {
    jac.col(i) = getContactPositionGradient(dof);
    i++;
  }
  return jac;
}

//==============================================================================
/// This is the analytical Jacobian for the contact normal
math::LinearJacobian
DifferentiableContactConstraint::getContactForceDirectionJacobian(
    std::shared_ptr<simulation::World> world)
{
  math::LinearJacobian jac = math::LinearJacobian::Zero(3, world->getNumDofs());
  int i = 0;
  for (auto dof : world->getDofs())
  {
    jac.col(i) = getContactForceGradient(dof);
    i++;
  }
  return jac;
}

//==============================================================================
math::Jacobian DifferentiableContactConstraint::getContactForceJacobian(
    std::shared_ptr<simulation::World> world)
{
  Eigen::Vector3d pos = getContactWorldPosition();
  Eigen::Vector3d dir = getContactWorldForceDirection();
  math::LinearJacobian posJac = getContactPositionJacobian(world);
  math::LinearJacobian dirJac = getContactForceDirectionJacobian(world);
  math::Jacobian jac = math::Jacobian::Zero(6, world->getNumDofs());

  // tau = pos cross dir
  for (int i = 0; i < world->getNumDofs(); i++)
  {
    jac.block<3, 1>(0, i) = pos.cross(dirJac.col(i)) + posJac.col(i).cross(dir);
  }
  // f = dir
  jac.block(3, 0, 3, world->getNumDofs()) = dirJac;

  return jac;
}

//==============================================================================
/// This gets the constraint force for a given DOF
double DifferentiableContactConstraint::getConstraintForce(
    dynamics::DegreeOfFreedom* dof)
{
  double multiple = getForceMultiple(dof);
  Eigen::Vector6d worldForce = getWorldForce();
  Eigen::Vector6d worldTwist = getWorldScrewAxis(dof);
  return worldTwist.dot(worldForce) * multiple;
}

//==============================================================================
/// This gets the gradient of constraint force at this joint with respect to
/// another joint
double DifferentiableContactConstraint::getConstraintForceDerivative(
    dynamics::DegreeOfFreedom* dof, dynamics::DegreeOfFreedom* wrt)
{
  double multiple = getForceMultiple(dof);
  Eigen::Vector6d worldForce = getWorldForce();
  Eigen::Vector6d gradientOfWorldForce = getContactWorldForceGradient(wrt);
  Eigen::Vector6d gradientOfWorldTwist = getScrewAxisGradient(dof, wrt);
  Eigen::Vector6d worldTwist = getWorldScrewAxis(dof);
  double dot1 = worldTwist.dot(gradientOfWorldForce);
  double dot2 = gradientOfWorldTwist.dot(worldForce);
  double sum = dot1 + dot2;
  double ret = sum * multiple;
  return (worldTwist.dot(gradientOfWorldForce)
          + gradientOfWorldTwist.dot(worldForce))
         * multiple;
}

//==============================================================================
/// This returns an analytical Jacobian relating the skeletons that this
/// contact touches.
Eigen::MatrixXd DifferentiableContactConstraint::getConstraintForcesJacobian(
    std::shared_ptr<simulation::World> world)
{
  int dim = world->getNumDofs();
  math::Jacobian forceJac = getContactForceJacobian(world);
  Eigen::Vector6d force = getWorldForce();

  Eigen::MatrixXd result = Eigen::MatrixXd::Zero(dim, dim);
  std::vector<dynamics::DegreeOfFreedom*> dofs = world->getDofs();
  for (int row = 0; row < dim; row++)
  {
    Eigen::Vector6d axis = getWorldScrewAxis(dofs[row]);
    for (int wrt = 0; wrt < dim; wrt++)
    {
      Eigen::Vector6d screwAxisGradient
          = getScrewAxisGradient(dofs[row], dofs[wrt]);
      Eigen::Vector6d forceGradient = forceJac.col(wrt);
      double multiple = getForceMultiple(dofs[row]);
      result(row, wrt)
          = multiple * (screwAxisGradient.dot(force) + axis.dot(forceGradient));
    }
  }

  return result;
}

//==============================================================================
/// The linear Jacobian for the contact position
math::LinearJacobian
DifferentiableContactConstraint::bruteForceContactPositionJacobian(
    std::shared_ptr<simulation::World> world)
{
  RestorableSnapshot snapshot(world);

  int dofs = world->getNumDofs();
  math::LinearJacobian jac = math::LinearJacobian(3, dofs);

  const double EPS = 1e-6;

  Eigen::VectorXd positions = world->getPositions();

  for (int i = 0; i < dofs; i++)
  {
    snapshot.restore();
    Eigen::VectorXd perturbedPositions = positions;
    perturbedPositions(i) += EPS;
    world->setPositions(perturbedPositions);

    std::shared_ptr<BackpropSnapshot> backpropSnapshot
        = neural::forwardPass(world, true);
    std::shared_ptr<DifferentiableContactConstraint> peerConstraint
        = getPeerConstraint(backpropSnapshot);
    jac.col(i) = (peerConstraint->getContactWorldPosition()
                  - getContactWorldPosition())
                 / EPS;
  }

  snapshot.restore();

  return jac;
}

//==============================================================================
/// The linear Jacobian for the contact normal
math::LinearJacobian
DifferentiableContactConstraint::bruteForceContactForceDirectionJacobian(
    std::shared_ptr<simulation::World> world)
{
  RestorableSnapshot snapshot(world);

  int dofs = world->getNumDofs();
  math::LinearJacobian jac = math::LinearJacobian(3, dofs);

  const double EPS = 1e-6;

  Eigen::VectorXd positions = world->getPositions();

  for (int i = 0; i < dofs; i++)
  {
    snapshot.restore();
    Eigen::VectorXd perturbedPositions = positions;
    perturbedPositions(i) += EPS;
    world->setPositions(perturbedPositions);

    std::shared_ptr<BackpropSnapshot> backpropSnapshot
        = neural::forwardPass(world, true);
    std::shared_ptr<DifferentiableContactConstraint> peerConstraint
        = getPeerConstraint(backpropSnapshot);
    jac.col(i) = (peerConstraint->getContactWorldForceDirection()
                  - getContactWorldForceDirection())
                 / EPS;
  }

  snapshot.restore();

  return jac;
}

//==============================================================================
/// This is the brute force version of getWorldForceJacobian()
math::Jacobian DifferentiableContactConstraint::bruteForceContactForceJacobian(
    std::shared_ptr<simulation::World> world)
{
  RestorableSnapshot snapshot(world);

  int dofs = world->getNumDofs();
  math::Jacobian jac = math::Jacobian(6, dofs);

  const double EPS = 1e-6;

  Eigen::VectorXd positions = world->getPositions();

  for (int i = 0; i < dofs; i++)
  {
    snapshot.restore();
    Eigen::VectorXd perturbedPositions = positions;
    perturbedPositions(i) += EPS;
    world->setPositions(perturbedPositions);

    std::shared_ptr<BackpropSnapshot> backpropSnapshot
        = neural::forwardPass(world, true);
    std::shared_ptr<DifferentiableContactConstraint> peerConstraint
        = getPeerConstraint(backpropSnapshot);
    jac.col(i) = (peerConstraint->getWorldForce() - getWorldForce()) / EPS;
  }

  snapshot.restore();

  return jac;
}

//==============================================================================
/// This is the brute force version of getConstraintForcesJacobian()
Eigen::MatrixXd
DifferentiableContactConstraint::bruteForceConstraintForcesJacobian(
    std::shared_ptr<simulation::World> world)
{
  int dims = world->getNumDofs();
  Eigen::MatrixXd result = Eigen::MatrixXd::Zero(dims, dims);

  RestorableSnapshot snapshot(world);

  Eigen::VectorXd originalPosition = world->getPositions();
  const double EPS = 1e-7;

  std::shared_ptr<BackpropSnapshot> originalBackpropSnapshot
      = neural::forwardPass(world, true);
  std::shared_ptr<DifferentiableContactConstraint> originalPeerConstraint
      = getPeerConstraint(originalBackpropSnapshot);
  Eigen::VectorXd originalOut
      = originalPeerConstraint->getConstraintForces(world);

  for (int i = 0; i < dims; i++)
  {
    Eigen::VectorXd tweakedPosition = originalPosition;
    tweakedPosition(i) += EPS;
    world->setPositions(tweakedPosition);

    std::shared_ptr<BackpropSnapshot> backpropSnapshot
        = neural::forwardPass(world, true);
    std::shared_ptr<DifferentiableContactConstraint> peerConstraint
        = getPeerConstraint(backpropSnapshot);
    Eigen::VectorXd newOut = peerConstraint->getConstraintForces(world);

    result.col(i) = (newOut - originalOut) / EPS;
  }

  snapshot.restore();

  return result;
}

//==============================================================================
Eigen::Vector3d
DifferentiableContactConstraint::estimatePerturbedContactPosition(
    std::shared_ptr<dynamics::Skeleton> skel, int dofIndex, double eps)
{
  Eigen::Vector3d contactPos = getContactWorldPosition();
  SkeletonContactType type = getSkeletonContactType(skel);

  if (type == VERTEX)
  {
    Eigen::Vector6d worldTwist = getWorldScrewAxis(skel, dofIndex);
    Eigen::Isometry3d rotation = math::expMap(worldTwist * eps);
    Eigen::Vector3d perturbedContactPos = rotation * contactPos;
    return perturbedContactPos;
  }
  else if (type == FACE)
  {
    return contactPos;
  }
  else if (type == EDGE)
  {
    /// TODO: this can't possibly be right, requires more thought
    return contactPos;
  }
  else
  {
    // Default case
    return contactPos;
  }
}

//==============================================================================
Eigen::Vector3d DifferentiableContactConstraint::estimatePerturbedContactNormal(
    std::shared_ptr<dynamics::Skeleton> skel, int dofIndex, double eps)
{
  Eigen::Vector3d normal = getContactWorldNormal();
  SkeletonContactType type = getSkeletonContactType(skel);
  if (type == VERTEX)
  {
    return normal;
  }
  else if (type == FACE)
  {
    Eigen::Vector6d worldTwist = getWorldScrewAxis(skel, dofIndex);
    Eigen::Isometry3d rotation = math::expMap(worldTwist * eps);
    rotation.translation().setZero();
    Eigen::Vector3d perturbedNormal = rotation * normal;
    return perturbedNormal;
  }
  else if (type == EDGE)
  {
    /// TODO: this can't possibly be right, requires more thought
    return normal;
  }
  else
  {
    // Default case
    return normal;
  }
}

//==============================================================================
Eigen::Vector3d
DifferentiableContactConstraint::estimatePerturbedContactForceDirection(
    std::shared_ptr<dynamics::Skeleton> skel, int dofIndex, double eps)
{
  Eigen::Vector3d forceDir = getContactWorldForceDirection();
  SkeletonContactType type = getSkeletonContactType(skel);
  if (type == VERTEX)
  {
    return forceDir;
  }
  else if (type == FACE)
  {
    Eigen::Vector3d contactNormal
        = estimatePerturbedContactNormal(skel, dofIndex, eps);
    if (mIndex == 0)
      return contactNormal;
    else
    {
      return mContactConstraint->getTangentBasisMatrixODE(contactNormal)
          .col(mIndex - 1);
    }
  }
  else if (type == EDGE)
  {
    /// TODO: this can't possibly be right, requires more thought
    return forceDir;
  }
  else
  {
    // Default case
    return forceDir;
  }
}

//==============================================================================
/// Just for testing: This analytically estimates how a screw axis will move
/// when rotated by another screw.
Eigen::Vector6d DifferentiableContactConstraint::estimatePerturbedScrewAxis(
    dynamics::DegreeOfFreedom* axis,
    dynamics::DegreeOfFreedom* rotate,
    double eps)
{
  Eigen::Vector6d axisWorldTwist = getWorldScrewAxis(axis);
  if (axis->getSkeleton()->getName() != rotate->getSkeleton()->getName())
    return axisWorldTwist;
  Eigen::Vector6d rotateWorldTwist = getWorldScrewAxis(rotate);
  Eigen::Isometry3d transform = math::expMap(rotateWorldTwist * eps);
  Eigen::Vector6d transformedAxis = math::AdT(transform, axisWorldTwist);
  return transformedAxis;
}

//==============================================================================
void DifferentiableContactConstraint::setOffsetIntoWorld(
    int offset, bool isUpperBoundConstraint)
{
  mOffsetIntoWorld = offset;
  mIsUpperBoundConstraint = isUpperBoundConstraint;
}

//==============================================================================
Eigen::Vector3d
DifferentiableContactConstraint::bruteForcePerturbedContactPosition(
    std::shared_ptr<simulation::World> world,
    std::shared_ptr<dynamics::Skeleton> skel,
    int dofIndex,
    double eps)
{
  RestorableSnapshot snapshot(world);

  auto dof = skel->getDof(dofIndex);
  dof->setPosition(dof->getPosition() + eps);

  std::shared_ptr<BackpropSnapshot> backpropSnapshot
      = neural::forwardPass(world, true);
  std::shared_ptr<DifferentiableContactConstraint> peerConstraint
      = getPeerConstraint(backpropSnapshot);

  snapshot.restore();

  return peerConstraint->getContactWorldPosition();
}

//==============================================================================
Eigen::Vector3d
DifferentiableContactConstraint::bruteForcePerturbedContactNormal(
    std::shared_ptr<simulation::World> world,
    std::shared_ptr<dynamics::Skeleton> skel,
    int dofIndex,
    double eps)
{
  RestorableSnapshot snapshot(world);

  auto dof = skel->getDof(dofIndex);
  dof->setPosition(dof->getPosition() + eps);

  std::shared_ptr<BackpropSnapshot> backpropSnapshot
      = neural::forwardPass(world, true);
  std::shared_ptr<DifferentiableContactConstraint> peerConstraint
      = getPeerConstraint(backpropSnapshot);

  snapshot.restore();

  return peerConstraint->getContactWorldNormal();
}

//==============================================================================
Eigen::Vector3d
DifferentiableContactConstraint::bruteForcePerturbedContactForceDirection(
    std::shared_ptr<simulation::World> world,
    std::shared_ptr<dynamics::Skeleton> skel,
    int dofIndex,
    double eps)
{
  RestorableSnapshot snapshot(world);

  auto dof = skel->getDof(dofIndex);
  dof->setPosition(dof->getPosition() + eps);

  std::shared_ptr<BackpropSnapshot> backpropSnapshot
      = neural::forwardPass(world, true);
  std::shared_ptr<DifferentiableContactConstraint> peerConstraint
      = getPeerConstraint(backpropSnapshot);

  snapshot.restore();

  return peerConstraint->getContactWorldForceDirection();
}

//==============================================================================
/// Just for testing: This perturbs the world position of a skeleton to read a
/// screw axis will move when rotated by another screw.
Eigen::Vector6d DifferentiableContactConstraint::bruteForceScrewAxis(
    dynamics::DegreeOfFreedom* axis,
    dynamics::DegreeOfFreedom* rotate,
    double eps)
{
  double originalPos = rotate->getPosition();
  rotate->setPosition(originalPos + eps);

  Eigen::Vector6d worldTwist = getWorldScrewAxis(axis);

  rotate->setPosition(originalPos);

  return worldTwist;
}

//==============================================================================
int DifferentiableContactConstraint::getIndexInConstraint()
{
  return mIndex;
}

//==============================================================================
// This returns 1.0 by default, 0.0 if this constraint doesn't effect the
// specified DOF, and -1.0 if the constraint effects this dof negatively.
double DifferentiableContactConstraint::getForceMultiple(
    dynamics::DegreeOfFreedom* dof)
{
  if (!mConstraint->isContactConstraint())
    return 1.0;

  // If we're in skel A
  if (dof->getSkeleton()->getName()
      == mContactConstraint->getBodyNodeA()->getSkeleton()->getName())
  {
    if (mContactConstraint->getBodyNodeA()->getTreeIndex()
            == dof->getTreeIndex()
        && mContactConstraint->getBodyNodeA()->getIndexInTree()
               >= dof->getChildBodyNode()->getIndexInTree())
    {
      return 1.0;
    }
  }
  // If we're in skel B
  else if (
      dof->getSkeleton()->getName()
      == mContactConstraint->getBodyNodeB()->getSkeleton()->getName())
  {
    if (mContactConstraint->getBodyNodeB()->getTreeIndex()
            == dof->getTreeIndex()
        && mContactConstraint->getBodyNodeB()->getIndexInTree()
               >= dof->getChildBodyNode()->getIndexInTree())
    {
      return -1.0;
    }
  }

  return 0.0;
}

//==============================================================================
Eigen::Vector6d DifferentiableContactConstraint::getWorldScrewAxis(
    std::shared_ptr<dynamics::Skeleton> skel, int dofIndex)
{
  return getWorldScrewAxis(skel->getDof(dofIndex));
}

//==============================================================================
Eigen::Vector6d DifferentiableContactConstraint::getWorldScrewAxis(
    dynamics::DegreeOfFreedom* dof)
{
  int jointIndex = dof->getIndexInJoint();
  math::Jacobian relativeJac = dof->getJoint()->getRelativeJacobian();
  dynamics::BodyNode* childNode = dof->getChildBodyNode();
  Eigen::Isometry3d transform = childNode->getWorldTransform();
  Eigen::Vector6d localTwist = relativeJac.col(jointIndex);
  Eigen::Vector6d worldTwist = math::AdT(transform, localTwist);
  return worldTwist;
}

//==============================================================================
std::shared_ptr<DifferentiableContactConstraint>
DifferentiableContactConstraint::getPeerConstraint(
    std::shared_ptr<neural::BackpropSnapshot> snapshot)
{
  if (mIsUpperBoundConstraint)
  {
    return snapshot->getUpperBoundConstraints()[mOffsetIntoWorld];
  }
  else
  {
    return snapshot->getClampingConstraints()[mOffsetIntoWorld];
  }
}

} // namespace neural
} // namespace dart
