/**
 * @name User-controlled kernel write input reaches command parsing
 * @description Tracks data copied from userspace into a character-device
 *              write handler as it reaches string parsing functions.
 * @kind path-problem
 * @problem.severity warning
 * @security-severity 5.0
 * @precision high
 * @id cpp/kernel-write-user-input
 */

import cpp
import semmle.code.cpp.dataflow.new.DataFlow
import semmle.code.cpp.dataflow.new.TaintTracking

module KernelWriteConfig implements DataFlow::ConfigSig {
  predicate isSource(DataFlow::Node source) {
    exists(FunctionCall call |
      call.getEnclosingFunction().getName() = "dev_write" and
      call.getTarget().getName() = "copy_from_user" and
      source.asIndirectExpr(1) = call.getArgument(0)
    )
  }

  predicate isSink(DataFlow::Node sink) {
    exists(FunctionCall call |
      call.getEnclosingFunction().getName() = "dev_write" and
      call.getTarget().getName() in ["memchr", "strchr", "strcmp"] and
      (
        sink.asIndirectExpr(1) = call.getArgument(0) or
        (
          call.getTarget().getName() = "strcmp" and
          sink.asIndirectExpr(1) = call.getArgument(1)
        )
      )
    )
  }
}

module KernelWriteFlow = TaintTracking::Global<KernelWriteConfig>;

import KernelWriteFlow::PathGraph

from KernelWriteFlow::PathNode source, KernelWriteFlow::PathNode sink
where KernelWriteFlow::flowPath(source, sink)
select sink.getNode(), source, sink,
  "Data copied from userspace reaches command parsing."
