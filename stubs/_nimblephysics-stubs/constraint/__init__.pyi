from __future__ import annotations
import _nimblephysics.constraint
import typing
import _nimblephysics.collision
import _nimblephysics.dynamics
import _nimblephysics.math
import numpy
_Shape = typing.Tuple[int, ...]

__all__ = [
    "BallJointConstraint",
    "BoxedLcpConstraintSolver",
    "BoxedLcpSolver",
    "ConstrainedGroup",
    "ConstraintBase",
    "ConstraintInfo",
    "ConstraintSolver",
    "DantzigBoxedLcpSolver",
    "JointConstraint",
    "JointCoulombFrictionConstraint",
    "JointLimitConstraint",
    "LcpInputs",
    "PgsBoxedLcpSolver",
    "PgsBoxedLcpSolverOption",
    "WeldJointConstraint"
]


class ConstraintBase():
    def applyImpulse(self, impulse: float) -> None: ...
    def applyUnitImpulse(self, index: int) -> None: ...
    @staticmethod
    def compressPath(skeleton: _nimblephysics.dynamics.Skeleton) -> _nimblephysics.dynamics.Skeleton: ...
    def excite(self) -> None: ...
    def getDimension(self) -> int: ...
    def getInformation(self, info: ConstraintInfo) -> None: ...
    def getRootSkeleton(self) -> _nimblephysics.dynamics.Skeleton: ...
    @staticmethod
    def getRootSkeletonOf(skeleton: _nimblephysics.dynamics.Skeleton) -> _nimblephysics.dynamics.Skeleton: ...
    def getVelocityChange(self, vel: float, withCfm: bool) -> None: ...
    def isActive(self) -> bool: ...
    def isContactConstraint(self) -> bool: ...
    def unexcite(self) -> None: ...
    def uniteSkeletons(self) -> None: ...
    def update(self) -> None: ...
    pass
class ConstraintSolver():
    def addConstraint(self, constraint: ConstraintBase) -> None: ...
    def addSkeleton(self, skeleton: _nimblephysics.dynamics.Skeleton) -> None: ...
    def addSkeletons(self, skeletons: typing.List[_nimblephysics.dynamics.Skeleton]) -> None: ...
    def applyConstraintImpulses(self, arg0: typing.List[ConstraintBase], arg1: typing.List[float]) -> None: ...
    def buildConstrainedGroups(self) -> None: ...
    def clearLastCollisionResult(self) -> None: ...
    def enforceContactAndJointAndCustomConstraintsWithLcp(self) -> None: ...
    def getCollisionDetector(self) -> _nimblephysics.collision.CollisionDetector: ...
    def getCollisionGroup(self) -> _nimblephysics.collision.CollisionGroup: ...
    def getConstrainedGroups(self) -> typing.List[ConstrainedGroup]: ...
    def getConstraints(self) -> typing.List[ConstraintBase]: ...
    def getGradientEnabled(self) -> bool: ...
    def getTimeStep(self) -> float: ...
    def removeAllConstraints(self) -> None: ...
    def removeAllSkeletons(self) -> None: ...
    def removeConstraint(self, constraint: ConstraintBase) -> None: ...
    def removeSkeleton(self, skeleton: _nimblephysics.dynamics.Skeleton) -> None: ...
    def removeSkeletons(self, skeletons: typing.List[_nimblephysics.dynamics.Skeleton]) -> None: ...
    def replaceEnforceContactAndJointAndCustomConstraintsFn(self, arg0: typing.Callable[[], None]) -> None: ...
    def setCollisionDetector(self, collisionDetector: _nimblephysics.collision.CollisionDetector) -> None: ...
    def setContactClippingDepth(self, arg0: float) -> None: ...
    def setGradientEnabled(self, arg0: bool) -> None: ...
    def setPenetrationCorrectionEnabled(self, arg0: bool) -> None: ...
    def setTimeStep(self, timeStep: float) -> None: ...
    def solve(self) -> None: ...
    def solveConstrainedGroups(self) -> None: ...
    def updateConstraints(self) -> None: ...
    pass
class BoxedLcpSolver():
    def getType(self) -> str: ...
    def solve(self, n: int, A: float, x: float, b: float, nub: int, lo: float, hi: float, findex: int) -> None: ...
    pass
class ConstrainedGroup():
    def __init__(self) -> None: ...
    def getConstraint(self, arg0: int) -> ConstraintBase: ...
    def getNumConstraints(self) -> int: ...
    pass
class JointConstraint(ConstraintBase):
    @staticmethod
    def getConstraintForceMixing() -> float: ...
    @staticmethod
    def getErrorAllowance() -> float: ...
    @staticmethod
    def getErrorReductionParameter() -> float: ...
    @staticmethod
    def getMaxErrorReductionVelocity() -> float: ...
    @staticmethod
    def setConstraintForceMixing(cfm: float) -> None: ...
    @staticmethod
    def setErrorAllowance(allowance: float) -> None: ...
    @staticmethod
    def setErrorReductionParameter(erp: float) -> None: ...
    @staticmethod
    def setMaxErrorReductionVelocity(erv: float) -> None: ...
    pass
class ConstraintInfo():
    pass
class BoxedLcpConstraintSolver(ConstraintSolver):
    @typing.overload
    def __init__(self, timeStep: float) -> None: ...
    @typing.overload
    def __init__(self, timeStep: float, boxedLcpSolver: BoxedLcpSolver) -> None: ...
    def buildLcpInputs(self, arg0: ConstrainedGroup) -> LcpInputs: ...
    def getBoxedLcpSolver(self) -> BoxedLcpSolver: ...
    def getSecondaryBoxedLcpSolver(self) -> BoxedLcpSolver: ...
    def makeHyperAccurateAndVerySlow(self) -> None: ...
    def setBoxedLcpSolver(self, lcpSolver: BoxedLcpSolver) -> None: ...
    def solveLcp(self, arg0: LcpInputs, arg1: ConstrainedGroup) -> typing.List[float]: ...
    pass
class DantzigBoxedLcpSolver(BoxedLcpSolver):
    @staticmethod
    def getStaticType() -> str: ...
    def getType(self) -> str: ...
    def solve(self, n: int, A: float, x: float, b: float, nub: int, lo: float, hi: float, findex: int, earlyTermination: bool) -> bool: ...
    pass
class BallJointConstraint(JointConstraint, ConstraintBase):
    @typing.overload
    def __init__(self, body1: _nimblephysics.dynamics.BodyNode, body2: _nimblephysics.dynamics.BodyNode, jointPos: numpy.ndarray[numpy.float64, _Shape[3, 1]]) -> None: ...
    @typing.overload
    def __init__(self, body: _nimblephysics.dynamics.BodyNode, jointPos: numpy.ndarray[numpy.float64, _Shape[3, 1]]) -> None: ...
    pass
class JointCoulombFrictionConstraint(ConstraintBase):
    def __init__(self, joint: _nimblephysics.dynamics.Joint) -> None: ...
    @staticmethod
    def getConstraintForceMixing() -> float: ...
    @staticmethod
    def setConstraintForceMixing(cfm: float) -> None: ...
    pass
class JointLimitConstraint(ConstraintBase):
    def __init__(self, joint: _nimblephysics.dynamics.Joint) -> None: ...
    @staticmethod
    def getConstraintForceMixing() -> float: ...
    @staticmethod
    def getErrorAllowance() -> float: ...
    @staticmethod
    def getErrorReductionParameter() -> float: ...
    @staticmethod
    def getMaxErrorReductionVelocity() -> float: ...
    @staticmethod
    def setConstraintForceMixing(cfm: float) -> None: ...
    @staticmethod
    def setErrorAllowance(allowance: float) -> None: ...
    @staticmethod
    def setErrorReductionParameter(erp: float) -> None: ...
    @staticmethod
    def setMaxErrorReductionVelocity(erv: float) -> None: ...
    pass
class LcpInputs():
    @property
    def mA(self) -> numpy.ndarray[numpy.float64, _Shape[m, n]]:
        """
        :type: numpy.ndarray[numpy.float64, _Shape[m, n]]
        """
    @mA.setter
    def mA(self, arg0: numpy.ndarray[numpy.float64, _Shape[m, n]]) -> None:
        pass
    @property
    def mB(self) -> numpy.ndarray[numpy.float64, _Shape[m, 1]]:
        """
        :type: numpy.ndarray[numpy.float64, _Shape[m, 1]]
        """
    @mB.setter
    def mB(self, arg0: numpy.ndarray[numpy.float64, _Shape[m, 1]]) -> None:
        pass
    @property
    def mFIndex(self) -> numpy.ndarray[numpy.int32, _Shape[m, 1]]:
        """
        :type: numpy.ndarray[numpy.int32, _Shape[m, 1]]
        """
    @mFIndex.setter
    def mFIndex(self, arg0: numpy.ndarray[numpy.int32, _Shape[m, 1]]) -> None:
        pass
    @property
    def mHi(self) -> numpy.ndarray[numpy.float64, _Shape[m, 1]]:
        """
        :type: numpy.ndarray[numpy.float64, _Shape[m, 1]]
        """
    @mHi.setter
    def mHi(self, arg0: numpy.ndarray[numpy.float64, _Shape[m, 1]]) -> None:
        pass
    @property
    def mLo(self) -> numpy.ndarray[numpy.float64, _Shape[m, 1]]:
        """
        :type: numpy.ndarray[numpy.float64, _Shape[m, 1]]
        """
    @mLo.setter
    def mLo(self, arg0: numpy.ndarray[numpy.float64, _Shape[m, 1]]) -> None:
        pass
    @property
    def mOffset(self) -> numpy.ndarray[numpy.int32, _Shape[m, 1]]:
        """
        :type: numpy.ndarray[numpy.int32, _Shape[m, 1]]
        """
    @mOffset.setter
    def mOffset(self, arg0: numpy.ndarray[numpy.int32, _Shape[m, 1]]) -> None:
        pass
    @property
    def mW(self) -> numpy.ndarray[numpy.float64, _Shape[m, 1]]:
        """
        :type: numpy.ndarray[numpy.float64, _Shape[m, 1]]
        """
    @mW.setter
    def mW(self, arg0: numpy.ndarray[numpy.float64, _Shape[m, 1]]) -> None:
        pass
    @property
    def mX(self) -> numpy.ndarray[numpy.float64, _Shape[m, 1]]:
        """
        :type: numpy.ndarray[numpy.float64, _Shape[m, 1]]
        """
    @mX.setter
    def mX(self, arg0: numpy.ndarray[numpy.float64, _Shape[m, 1]]) -> None:
        pass
    pass
class PgsBoxedLcpSolver(BoxedLcpSolver):
    def getOption(self) -> PgsBoxedLcpSolverOption: ...
    @staticmethod
    def getStaticType() -> str: ...
    def getType(self) -> str: ...
    def setOption(self, option: PgsBoxedLcpSolverOption) -> None: ...
    def solve(self, n: int, A: float, x: float, b: float, nub: int, lo: float, hi: float, findex: int, earlyTermination: bool) -> bool: ...
    pass
class PgsBoxedLcpSolverOption():
    @typing.overload
    def __init__(self) -> None: ...
    @typing.overload
    def __init__(self, maxIteration: int) -> None: ...
    @typing.overload
    def __init__(self, maxIteration: int, deltaXTolerance: float) -> None: ...
    @typing.overload
    def __init__(self, maxIteration: int, deltaXTolerance: float, relativeDeltaXTolerance: float) -> None: ...
    @typing.overload
    def __init__(self, maxIteration: int, deltaXTolerance: float, relativeDeltaXTolerance: float, epsilonForDivision: float) -> None: ...
    @typing.overload
    def __init__(self, maxIteration: int, deltaXTolerance: float, relativeDeltaXTolerance: float, epsilonForDivision: float, randomizeConstraintOrder: bool) -> None: ...
    @property
    def mDeltaXThreshold(self) -> float:
        """
        :type: float
        """
    @mDeltaXThreshold.setter
    def mDeltaXThreshold(self, arg0: float) -> None:
        pass
    @property
    def mEpsilonForDivision(self) -> float:
        """
        :type: float
        """
    @mEpsilonForDivision.setter
    def mEpsilonForDivision(self, arg0: float) -> None:
        pass
    @property
    def mMaxIteration(self) -> int:
        """
        :type: int
        """
    @mMaxIteration.setter
    def mMaxIteration(self, arg0: int) -> None:
        pass
    @property
    def mRandomizeConstraintOrder(self) -> bool:
        """
        :type: bool
        """
    @mRandomizeConstraintOrder.setter
    def mRandomizeConstraintOrder(self, arg0: bool) -> None:
        pass
    @property
    def mRelativeDeltaXTolerance(self) -> float:
        """
        :type: float
        """
    @mRelativeDeltaXTolerance.setter
    def mRelativeDeltaXTolerance(self, arg0: float) -> None:
        pass
    pass
class WeldJointConstraint(JointConstraint, ConstraintBase):
    @typing.overload
    def __init__(self, body1: _nimblephysics.dynamics.BodyNode, body2: _nimblephysics.dynamics.BodyNode) -> None: ...
    @typing.overload
    def __init__(self, body: _nimblephysics.dynamics.BodyNode) -> None: ...
    def setRelativeTransform(self, tf: _nimblephysics.math.Isometry3) -> None: ...
    pass
