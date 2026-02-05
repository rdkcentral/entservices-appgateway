---
applyTo: "**/**Implementation.cpp,**/**Implementation.h,**/**.cpp,**/**.h"
---

# Instruction Summary
  1. [Critical Logging](https://github.com/rdkcentral/entservices-appgateway/blob/develop/.github/instructions/General.instructions.md#critical-logging)
  2. [Recoverable Error reporting](https://github.com/rdkcentral/entservices-appgateway/blob/develop/.github/instructions/General.instructions.md#recoverable-error-reporting)
  3. [YODA Notation in conditionals](https://github.com/rdkcentral/entservices-appgateway/blob/develop/.github/instructions/General.instructions.md#yoda-notation-in-conditionals)

### Critical Logging
### Requirement

All the critical errors in the plugin must be logged using the Thunder Framework logging mechanism. Use the following macro to log critical errors:

```
LOGERR("Error Message with parameters: %d, %s", intParam, strParam.c_str());
```

If the function returns an error code, ensure to return the appropriate Core::ERROR_* code after logging the error. Success codes should not be logged to prevent log cluttering. If a given PR is a Hotfix, ensure that the logging is essential to avoid performance degradation. 

### Example

```cpp
if (nullptr == pointer) {
    LOGERR("Pointer is null in function %s", __FUNCTION__);
    return Core::ERROR_BAD_REQUEST;
}
```

### Incorrect Example

```cpp
if (nullptr == pointer) {
    return Core::ERROR_BAD_REQUEST;
}
```


### Recoverable Error reporting 
### Requirement

Do not log recoverable logs as Errors. Use the following macro to log warning messages.

```
LOGWARN("Warning Message with parameters: %d, %s", intParam, strParam.c_str());
``` 

### Example

```cpp
// optional data is missing
if (nullptr == optionalData) {
    LOGWARN("Optional data is missing in function %s", __FUNCTION__);
} // proceed with default behavior
```

### Incorrect Example

```cpp
// optional data is missing
if (nullptr == optionalData) {
    LOGERR("Optional data is missing in function %s", __FUNCTION__);
} // proceed with default behavior
```


### YODA Notation in conditionals
### Requirement
When writing conditionals, use YODA notation to prevent accidental assignment errors. This means placing constants on the left side of the comparison.
### Example

```cpp
if (nullptr == pointer) {
    LOGERR("Pointer is null in function %s", __FUNCTION__);
    return Core::ERROR_BAD_REQUEST;
}
```

### Incorrect Example

```cpp
if (pointer == nullptr) {
    LOGERR("Pointer is null in function %s", __FUNCTION__);
    return Core::ERROR_BAD_REQUEST;
}
```


### Avoiding Public Member Variables
### Requirement
Avoid using public member variables in classes. Instead, use private member variables with public getter and setter methods to encapsulate the data.
### Example

```cpp
class MyClass {
public:
    int GetValue() const { return value; }
    void SetValue(int val) { value = val; }

private:
    int value;
};
```

### Incorrect Example

```cpp
class MyClass {
public:
    int value; // Public member variable
};
```


### Using nullptr instead of NULL
### Requirement
Use `nullptr` instead of `NULL` for pointer initialization and comparisons to improve type safety and clarity.
### Example 

```cpp
int* ptr = nullptr;
if (nullptr == ptr) {
    // Handle null pointer
}
```

### Incorrect Example

```cpp
int* ptr = NULL;
if (NULL == ptr) {
    // Handle null pointer
}
```

