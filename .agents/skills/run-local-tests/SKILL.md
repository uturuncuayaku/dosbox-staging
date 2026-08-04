---
name: run-local-tests
description: Guidelines for compiling and executing unit tests via local user scripts (andresdocs/scripts/) without blocking or waiting on execution.
---

# Run Local Tests Skill

This skill defines the workflow for compiling and executing unit tests in the DOSBox-Staging repository without consuming tokens by waiting or polling on long-running build commands.

## Core Rule: Non-Blocking Test Execution

1. **Do Not Poll or Wait on Builds**:
   - Never execute long-running `ninja` builds or test binaries synchronously while waiting for completion in a loop.
   - Do not consume turn cycles or token budget waiting for compilation.

2. **Use Local User Scripts**:
   - Refer the user to the local executable test script:
     `andresdocs/scripts/run_append_tests.sh`
   - Provide the user with the direct command to run in their terminal:
     ```bash
     ./andresdocs/scripts/run_append_tests.sh
     ```

3. **User Alert Workflow**:
   - The test script includes terminal alert signals (`\a`) and a completion banner.
   - When testing is needed, provide the script path/command to the user and prompt them to pass the test results back once the script completes.
