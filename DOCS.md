# Documentation Index

Welcome to the entservices-appgateway documentation! This index will help you find the information you need.

## Quick Start

New to the project? Start here:
1. Read the [README.md](README.md) for project overview and quick start
2. Follow [DEVELOPMENT.md](DEVELOPMENT.md) to set up your development environment
3. Check [API.md](API.md) for available APIs and usage examples

## Documentation Files

### [README.md](README.md)
**For: Users, Developers, DevOps**

Main project documentation covering:
- Project overview and purpose
- Architecture summary
- Building and installation instructions
- Configuration guide
- Testing overview
- Quick usage examples

**Start here if:** You want to understand what the project does or get it running quickly.

---

### [ARCHITECTURE.md](ARCHITECTURE.md)
**For: Developers, Architects**

Detailed technical architecture documentation covering:
- System design and component interactions
- Plugin architecture and communication patterns
- Request resolution flow
- Data flow examples
- Threading model and concurrency
- Security considerations
- Extension points

**Start here if:** You need to understand the internal design, modify core functionality, or integrate with the gateway.

---

### [DEVELOPMENT.md](DEVELOPMENT.md)
**For: Developers, Contributors**

Comprehensive development guide covering:
- Development environment setup
- Building the project (detailed)
- Running and debugging plugins
- Testing (L1 unit tests and L2 integration tests)
- Code style and standards
- Common development tasks
- Troubleshooting guide
- Best practices

**Start here if:** You're contributing code, fixing bugs, or adding new features.

---

### [API.md](API.md)
**For: Application Developers, Integrators**

Complete API reference covering:
- All available Firebolt APIs
- Request/response formats and examples
- Event notifications and subscriptions
- Error codes and handling
- Permission requirements
- Testing with curl examples

**Start here if:** You're building applications that use the App Gateway APIs or integrating with the system.

---

### [CONTRIBUTING.md](CONTRIBUTING.md)
**For: Contributors**

Guidelines for contributing to the project:
- Contribution requirements
- CLA (Contributor License Agreement) information

---

### [CHANGELOG.md](CHANGELOG.md)
**For: All Users**

Version history and release notes:
- New features by version
- Bug fixes and improvements
- Breaking changes

---

## By Use Case

### I want to...

#### ...understand what this project does
→ [README.md](README.md) - Overview section

#### ...build and run the project
→ [README.md](README.md) - Building section  
→ [DEVELOPMENT.md](DEVELOPMENT.md) - Building the Project section

#### ...use the APIs in my application
→ [API.md](API.md) - Complete API reference  
→ [README.md](README.md) - API Examples section

#### ...add a new API method
→ [DEVELOPMENT.md](DEVELOPMENT.md) - Adding a New API Method section  
→ [ARCHITECTURE.md](ARCHITECTURE.md) - Extension Points section

#### ...understand the architecture
→ [ARCHITECTURE.md](ARCHITECTURE.md) - Complete technical design  
→ [README.md](README.md) - Architecture section

#### ...debug a problem
→ [DEVELOPMENT.md](DEVELOPMENT.md) - Running and Debugging section  
→ [DEVELOPMENT.md](DEVELOPMENT.md) - Troubleshooting section

#### ...write tests
→ [DEVELOPMENT.md](DEVELOPMENT.md) - Testing section  
→ [Tests/README.md](Tests/README.md) - Test infrastructure details

#### ...contribute code
→ [CONTRIBUTING.md](CONTRIBUTING.md) - Contribution guidelines  
→ [DEVELOPMENT.md](DEVELOPMENT.md) - Code Style and Standards section

#### ...configure the gateway
→ [README.md](README.md) - Configuration section  
→ [ARCHITECTURE.md](ARCHITECTURE.md) - Configuration Management section

#### ...understand permission system
→ [API.md](API.md) - Permission System section  
→ [ARCHITECTURE.md](ARCHITECTURE.md) - Permission System section

## Additional Resources

### Test Documentation
- **[Tests/README.md](Tests/README.md)** - L1 and L2 test framework documentation

### Plugin-Specific Documentation
- **[AppGateway/tests/CurlCmds.md](AppGateway/tests/CurlCmds.md)** - AppGateway test commands
- **[AppGatewayCommon/tests/CurlCmds.md](AppGatewayCommon/tests/CurlCmds.md)** - AppGatewayCommon test commands
- **[AppNotifications/tests/CurlCmds.md](AppNotifications/tests/CurlCmds.md)** - AppNotifications test commands

### External Documentation
- **[Thunder Framework Documentation](https://rdkcentral.github.io/Thunder/)** - Core framework documentation
- **[Firebolt APIs](https://github.com/rdkcentral/firebolt-apis)** - Firebolt API specifications
- **[RDK Central](https://rdkcentral.com/)** - RDK project portal

## Documentation Maintenance

### For Maintainers

When updating documentation:
- Keep all docs synchronized (especially API changes)
- Update this index if you add/remove documentation files
- Update CHANGELOG.md for releases
- Verify all links are working
- Update code examples to match current APIs

### Documentation Standards
- Use Markdown format
- Include code examples where relevant
- Provide clear section headers
- Use consistent terminology
- Keep examples up-to-date with code

## Getting Help

Can't find what you're looking for?

1. **Search the documentation** - Use your browser's find function (Ctrl+F / Cmd+F)
2. **Check Issues** - [GitHub Issues](https://github.com/rdkcentral/entservices-appgateway/issues)
3. **Ask Questions** - Create a new issue with the "question" label
4. **Review Examples** - Check test files for usage examples

## Contributing to Documentation

Found an error or want to improve the docs?

1. Documentation follows the same contribution process as code
2. See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines
3. Submit pull requests for documentation improvements
4. Documentation improvements are always welcome!

---

**Last Updated:** January 2026  
**Documentation Version:** 1.0
