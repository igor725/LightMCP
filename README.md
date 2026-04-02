# LightMCP
LightMCP is a lightweight MCP stdio-based server written in C++. This server supports very basic protocol functionality for tools creation.

The example executable contains two tools:
* Arbitrary Lua script execution
* Random number generation

> [!WARNING]  
> It is up to user to provide sandboxed Lua interpreter without harmful functionality.
> Using stock interpreter with untouched `os`, `io`, `ffi` libraries may lead to
> malicious code execution by an AI model! Every script AI writes should be
> approved by user first.
