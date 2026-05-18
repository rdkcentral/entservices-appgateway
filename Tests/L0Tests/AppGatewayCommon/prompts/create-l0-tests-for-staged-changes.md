Create L0 Tests for Staged Changes in entservices-appgateway

Use Tests/L0Tests/AppGatewayCommon/ as a reference for structure and helpers when creating new L0 tests for any new or modified API/method in the entservices-appgateway repo. Ensure that each test validates the expected return value and result, covers both success and failure scenarios if applicable, and follows the naming and documentation conventions. Add the new test function to AppGatewayCommon_main_test.cpp so it is executed with the current L0 test suite.

Step 1. Identify the new or modified API/method that requires L0 tests. For example, if a new method `GetPresentationFocused` has been added to `AppGatewayCommon`, you would create a new test for this method.

Thoroughly review the existing tests in `Tests/L0Tests/AppGatewayCommon/` to understand the structure and use of helpers like `DelegateGetterTest`, `PluginAndService`, and `QIGuard`. This will help you create consistent and effective tests for the new or modified API.


Step 2 — Write tests

- Add new test functions in the appropriate existing file under
  Tests/L0Tests/AppGatewayCommon/.
- Add the corresponding forward declaration and RUN_L0_TEST() entry in
  AppGatewayCommon_main_test.cpp.
- Follow the existing pattern:
  - Function signature: uint32_t Test_<descriptive_name>()
  - Use PluginAndService RAII for plugin/service lifecycle.
  - Use QIGuard<T> for QueryInterface pointers.
  - Use DefaultContext() for GatewayContext.
  - Use TestResult + ExpectTrue / ExpectEqU32 / ExpectEqStr for assertions.
  - Include a TEST_ID comment (e.g. // TEST_ID: AGC_L0_XXX) and a one-line
    description of what the test validates.
- Each test must cover exactly one of:
  - Success / happy path
  - Failure / error path (null interface, bad payload, unknown key)
  - Edge case (empty string, boundary value, case sensitivity)
- In L0, external COM-RPC interfaces are nullptr. Test the null-guard branches
  (e.g. mLifecycleManagerState == nullptr → ERROR_GENERAL with error message in
  result). Do NOT attempt to mock real COM-RPC interfaces — that is L1 scope.

Coverage target: every new/modified conditional path in the change must have at
least one test exercising it.

Step 3 — Update workflow (if needed)

Check .github/workflows/L0-tests.yml. If the new tests are added to existing
files that are already compiled, no workflow change is needed. If a new test
file was created, make the minimal change to compile and link it.

──────────────────────────────────────────────────────────────────
Rules

- Do NOT modify code outside the L0 tests folder. If the change appears to have a bug, flag it
  with clear reasoning instead of writing a test that papers over it.
- Each test must be deterministic, isolated, and validate a real behavior.
- No artificial coverage — no duplicate tests, no tests that assert nothing.
- Strictly L0 scope per the ADR: no Thunder daemon, no GoogleTest, no network.
- All tests use the custom L0 harness (L0Bootstrap.hpp, L0TestTypes.hpp,
  L0Expect.hpp), not gtest/gmock.
