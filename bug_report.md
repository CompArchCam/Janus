In the original implementation, "static/analysis/ControlFlow.cpp" has the following bug:
*  In line 544, 
    ```cpp
    bb.ipdom = &function.entry[d];
    ```
    In this case, I have verified that `d` can exceed the boundaries of the basic blocks vector/array
    defined. As a result, using `&function.entry[d]` as a `BasicBlock*` is undefined. Currently `ipdom`
    is only used in generating the visualisation of the post dominance tree. This results in undefined
    behaviour where the immediate post dominators for basic blocks with no valid post dominators are
    instead shown to be post-dominated by a random basic block with a random block id,
    ( *sometimes a valid one, which is even worse* ). In the current implementation, this bug is
    already fixed by adding an explicit condition to make sure that `d` is always within bound,
    or else a warning message would be outputed to stdout (as of now, exact handling subject to 
    more discussions).
