"""This provides a native trajectory optimization framework to DART, transcribing DART trajectory problems into IPOPT for solutions."""
from __future__ import annotations
import _nimblephysics.trajectory
import typing
import _nimblephysics.neural
import _nimblephysics.performance
import _nimblephysics.simulation
import numpy
_Shape = typing.Tuple[int, ...]

__all__ = [
    "IPOptOptimizer",
    "LossFn",
    "MultiShot",
    "OptimizationStep",
    "Optimizer",
    "Problem",
    "SGDOptimizer",
    "SingleShot",
    "Solution",
    "TrajectoryRollout"
]


class Optimizer():
    def registerIntermediateCallback(self, callback: typing.Callable[[Problem, int, float, float], bool]) -> None: ...
    pass
class LossFn():
    @typing.overload
    def __init__(self) -> None: ...
    @typing.overload
    def __init__(self, loss: typing.Callable[[TrajectoryRollout], float]) -> None: ...
    @typing.overload
    def __init__(self, loss: typing.Callable[[TrajectoryRollout], float], lossFnAndGrad: typing.Callable[[TrajectoryRollout, TrajectoryRollout], float]) -> None: ...
    def getLoss(self, rollout: TrajectoryRollout, perfLog: _nimblephysics.performance.PerformanceLog = None) -> float: ...
    def getLossAndGradient(self, rollout: TrajectoryRollout, gradWrtRollout: TrajectoryRollout, perfLog: _nimblephysics.performance.PerformanceLog = None) -> float: ...
    def getLowerBound(self) -> float: ...
    def getUpperBound(self) -> float: ...
    def setLowerBound(self, lowerBound: float) -> None: ...
    def setUpperBound(self, upperBound: float) -> None: ...
    pass
class Problem():
    def addConstraint(self, constraint: LossFn) -> None: ...
    def addMapping(self, key: str, mapping: _nimblephysics.neural.Mapping) -> None: ...
    def getConstraintDim(self) -> int: ...
    def getExploreAlternateStrategies(self) -> bool: ...
    def getFinalState(self, world: _nimblephysics.simulation.World, perfLog: _nimblephysics.performance.PerformanceLog = None) -> numpy.ndarray[numpy.float64, _Shape[m, 1]]: ...
    def getFlatDimName(self, world: _nimblephysics.simulation.World, dim: int) -> str: ...
    def getFlatProblemDim(self, world: _nimblephysics.simulation.World) -> int: ...
    def getLoss(self, world: _nimblephysics.simulation.World, perfLog: _nimblephysics.performance.PerformanceLog = None) -> float: ...
    def getMapping(self, key: str) -> _nimblephysics.neural.Mapping: ...
    def getMappings(self) -> typing.Dict[str, _nimblephysics.neural.Mapping]: ...
    def getNumSteps(self) -> int: ...
    def getPinnedForce(self, time: int) -> numpy.ndarray[numpy.float64, _Shape[m, 1]]: ...
    def getRepresentationStateSize(self) -> int: ...
    def getRolloutCache(self, world: _nimblephysics.simulation.World, perfLog: _nimblephysics.performance.PerformanceLog = None, useKnots: bool = True) -> TrajectoryRollout: ...
    def getStartState(self) -> numpy.ndarray[numpy.float64, _Shape[m, 1]]: ...
    def hasMapping(self, key: str) -> bool: ...
    def pinForce(self, time: int, value: numpy.ndarray[numpy.float64, _Shape[m, 1]]) -> None: ...
    def removeMapping(self, key: str) -> None: ...
    def setControlForcesRaw(self, forces: numpy.ndarray[numpy.float64, _Shape[m, n]], perfLog: _nimblephysics.performance.PerformanceLog = None) -> None: ...
    def setExploreAlternateStrategies(self, flag: bool) -> None: ...
    def setLoss(self, loss: LossFn) -> None: ...
    def setStates(self, world: _nimblephysics.simulation.World, rollout: TrajectoryRollout, perfLog: _nimblephysics.performance.PerformanceLog = None) -> None: ...
    def updateWithForces(self, world: _nimblephysics.simulation.World, forces: numpy.ndarray[numpy.float64, _Shape[m, n]], perfLog: _nimblephysics.performance.PerformanceLog = None) -> numpy.ndarray[numpy.int32, _Shape[m, 1]]: ...
    pass
class OptimizationStep():
    @property
    def constraintViolation(self) -> float:
        """
        :type: float
        """
    @property
    def index(self) -> int:
        """
        :type: int
        """
    @property
    def loss(self) -> float:
        """
        :type: float
        """
    @property
    def rollout(self) -> TrajectoryRollout:
        """
        :type: TrajectoryRollout
        """
    pass
class IPOptOptimizer(Optimizer):
    def __init__(self) -> None: ...
    def optimize(self, shot: Problem, reuseRecord: Solution = None) -> Solution: ...
    def setCheckDerivatives(self, checkDerivatives: bool = True) -> None: ...
    def setDisableLinesearch(self, disableLinesearch: bool = True) -> None: ...
    def setIterationLimit(self, iterationLimit: int = 500) -> None: ...
    def setLBFGSHistoryLength(self, historyLen: int = 1) -> None: ...
    def setPrintFrequency(self, printFrequency: int = 1) -> None: ...
    def setRecordIterations(self, recordIterations: bool = True) -> None: ...
    def setRecordPerformanceLog(self, recordPerfLog: bool = True) -> None: ...
    def setRecoverBest(self, recoverBest: bool = True) -> None: ...
    def setSilenceOutput(self, silenceOutput: bool = True) -> None: ...
    def setSuppressOutput(self, suppressOutput: bool = True) -> None: ...
    def setTolerance(self, tol: float = 1e-07) -> None: ...
    pass
class MultiShot(Problem):
    def __init__(self, world: _nimblephysics.simulation.World, loss: LossFn, steps: int, shotLength: int, tuneStartingState: bool = False) -> None: ...
    def setParallelOperationsEnabled(self, enabled: bool) -> None: ...
    pass
class SGDOptimizer(Optimizer):
    def __init__(self) -> None: ...
    def optimize(self, shot: Problem, reuseRecord: Solution = None) -> Solution: ...
    def setIterationLimit(self, iterationLimit: int = 500) -> None: ...
    def setLearningRate(self, learningRate: float = 0.1) -> None: ...
    def setTolerance(self, tol: float = 1e-07) -> None: ...
    pass
class SingleShot(Problem):
    def __init__(self, world: _nimblephysics.simulation.World, loss: LossFn, steps: int, tuneStartingState: bool = False) -> None: ...
    pass
class Solution():
    def getNumSteps(self) -> int: ...
    def getPerfLog(self) -> _nimblephysics.performance.PerformanceLog: ...
    def getStep(self, step: int) -> OptimizationStep: ...
    def reoptimize(self) -> None: ...
    def toJson(self, world: _nimblephysics.simulation.World) -> str: ...
    pass
class TrajectoryRollout():
    def copy(self) -> TrajectoryRollout: ...
    def getControlForces(self, mapping: str = 'identity') -> numpy.ndarray[numpy.float64, _Shape[m, n]]: ...
    def getMappings(self) -> typing.List[str]: ...
    def getMasses(self) -> numpy.ndarray[numpy.float64, _Shape[m, 1]]: ...
    def getPoses(self, mapping: str = 'identity') -> numpy.ndarray[numpy.float64, _Shape[m, n]]: ...
    def getVels(self, mapping: str = 'identity') -> numpy.ndarray[numpy.float64, _Shape[m, n]]: ...
    def toJson(self, world: _nimblephysics.simulation.World) -> str: ...
    pass
