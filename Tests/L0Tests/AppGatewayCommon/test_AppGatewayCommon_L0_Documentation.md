# AppGatewayCommon L0 Test Documentation

## Test Design Approach

The AppGatewayCommon L0 tests were designed by referencing the structure and rigor of the AppGateway L0 tests (see AppGateway_Init_DeinitTests.cpp). The test suite targets a minimum of 75% coverage across all .cpp and .h files in the AppGatewayCommon plugin, including lifecycle, interface, positive, negative, boundary, and edge case scenarios. Mocks are used for delegate dependencies where needed, and TODOs are left for methods requiring further coverage or plugin fixes.

### Testing Structure
- Each test follows the Arrange – Act – Assert (AAA) pattern for clarity.
- Test files are consistently formatted and scenario-based.
- Assertions verify specific expected behaviors or errors.
- Plugin initialization, dependency setup, and delegate mocks are used as needed.
- Tests expose missing or incorrect plugin functionality with TODO comments.

### Reference Implementation
- Test structure, helper functions, and lifecycle validation are based on AppGateway_Init_DeinitTests.cpp.
- Plugin functionality is mapped to real scenarios, with each test covering a unique behavior.

## Quick Reference Table

| Test ID      | Summary                                 | File                              | Method Covered                  |
|--------------|-----------------------------------------|-----------------------------------|---------------------------------|
| AGC_L0_001   | Initialize/Deinitialize lifecycle        | test_AppGatewayCommon.cpp         | Initialize, Deinitialize        |
| AGC_L0_002   | Double Initialize idempotency            | test_AppGatewayCommon.cpp         | Initialize                      |
| AGC_L0_003   | Double Deinitialize robustness           | test_AppGatewayCommon.cpp         | Deinitialize                    |
| AGC_L0_004   | Positive AppGatewayRequest               | test_AppGatewayCommon.cpp         | HandleAppGatewayRequest         |
| AGC_L0_005   | Negative AppGatewayRequest               | test_AppGatewayCommon.cpp         | HandleAppGatewayRequest         |
| AGC_L0_006   | Boundary AppGatewayRequest               | test_AppGatewayCommon.cpp         | HandleAppGatewayRequest         |
| AGC_L0_007   | Delegate mock usage                      | test_AppGatewayCommon.cpp         | Delegate methods (mocked)       |
| AGC_L0_008   | Lifecycle methods                        | test_AppGatewayCommon.cpp         | LifecycleReady, etc.            |
| AGC_L0_009   | Authenticator interface                  | test_AppGatewayCommon.cpp         | Authenticate                    |
| AGC_L0_010   | Permission group default allowed         | test_AppGatewayCommon.cpp         | CheckPermissionGroup            |

## Test Descriptions

### AGC_L0_001
- **Description:** Validate plugin Initialize/Deinitialize lifecycle.
- **Inputs:** Default ServiceMock config.
- **Expected Result:** Initialize returns empty string; Deinitialize succeeds.

### AGC_L0_002
- **Description:** Validate idempotency of double Initialize.
- **Inputs:** Two plugin instances.
- **Expected Result:** Both Initialize calls succeed; no crash.

### AGC_L0_003
- **Description:** Validate robustness of double Deinitialize.
- **Inputs:** Two plugin instances.
- **Expected Result:** Both Deinitialize calls succeed; no crash.

### AGC_L0_004
- **Description:** Positive scenario for HandleAppGatewayRequest.
- **Inputs:** Valid method (device.make), default context.
- **Expected Result:** Returns ERROR_NONE.

### AGC_L0_005
- **Description:** Negative scenario for HandleAppGatewayRequest.
- **Inputs:** Invalid method, default context.
- **Expected Result:** Returns ERROR_UNKNOWN_KEY.

### AGC_L0_006
- **Description:** Boundary scenario for HandleAppGatewayRequest.
- **Inputs:** Valid method (device.make), empty payload.
- **Expected Result:** Returns ERROR_NONE.

### AGC_L0_007
- **Description:** Delegate mock usage.
- **Inputs:** Mocked SettingsDelegate.
- **Expected Result:** Delegate call verified (TODO).

### AGC_L0_008
- **Description:** Lifecycle methods coverage.
- **Inputs:** LifecycleReady, etc., default context.
- **Expected Result:** Method returns expected result (TODO).

### AGC_L0_009
- **Description:** Authenticator interface coverage.
- **Inputs:** Authenticate with sessionId.
- **Expected Result:** Returns expected appId (TODO).

### AGC_L0_010
- **Description:** Permission group default allowed.
- **Inputs:** appId, group.
- **Expected Result:** allowed == true.

## Methods Not Covered
- If any method is not covered, document:
    - Method name
    - File name
    - Reason for exclusion

## Coverage & Build
- CMakeLists.txt created for AppGatewayCommon L0 tests.
- Coverage report enabled via lcov/genhtml.
- Test structure and AAA pattern based on AppGateway_Init_DeinitTests.cpp.

---

For full coverage, add more mocks and edge case tests as needed. Strict, scenario-based assertions are used throughout. Arrange-Act-Assert pattern is followed for readability and maintainability.
