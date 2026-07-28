# Semantic APPEND Tracing Implementation

The `dos_append` trace logging subsystem has been entirely overhauled to provide a structured, domain-specific execution tree rather than a flat debug stream.

## Architectural Changes

1. **`AppendTraceScope` RAII Refactor**
   The class now acts as an intelligent tracing node:
   - **Unique Trace IDs**: Each trace block now generates a unique ID (e.g. `[APPEND-TRACE-5]`) to make it easy to correlate entry and exit scopes during deep, nested operations.
   - **Chronometer Integration**: High-resolution timers (`std::chrono`) measure execution duration in microseconds, logging precise performance profiles on scope exit.
   - **Execution Depth Indentation**: A static depth counter now automatically indents output blocks according to the call tree, making the caller/callee relationship visibly clear in logs.

2. **Semantic Tracing API**
   Raw strings have been abandoned in favor of strict semantic methods:
   - `trace.State(key, value)`
   - `trace.Action(description)`
   - `trace.Decision(choice, reason)`
   - `trace.Result(result)`
   - `trace.Error(error, details)`

## Verification

The 16 existing `APPEND` unit tests successfully compile and pass, proving that the tracing logic seamlessly wraps the implementation without mutating its behavior.

Here is a snippet of the new semantic logging output structure:

```text
    ======================================================================
    [APPEND-TRACE-3]
    ENTER dos_append::ResolveName()
    Reason:
        Find file in APPEND paths
    ======================================================================
    
    STATE:
        Input filename:
            FILE.TXT
    
        ======================================================================
        [APPEND-TRACE-5]
        ENTER dos_append::CheckCandidate()
        Reason:
            Evaluate candidate path against DOS_FileExists
        ======================================================================
        
        STATE:
            Candidate:
                C:\ONE\FILE.TXT
        
        ACTION:
            Checking DOS_FileExists
        
        DECISION:
            Stop search
            Reason:
                First matching file found
        
        RESULT:
            true
        
        ======================================================================
        [APPEND-TRACE-5]
        EXIT dos_append::CheckCandidate()
        Duration:
            0.320 ms
        ======================================================================
        
    STATE:
        Resolved path:
            C:\ONE\FILE.TXT
    
    RESULT:
        true
    
    ======================================================================
    [APPEND-TRACE-3]
    EXIT dos_append::ResolveName()
    Duration:
        1.202 ms
    ======================================================================
```

> [!TIP]
> The Loguru global configurations were intentionally left untouched (as discussed) to prevent test-pollution and ensure this code integrates flawlessly with DOSBox Staging's broader logging architecture. Standard preambles (e.g., timestamps) will appear on the far left.
