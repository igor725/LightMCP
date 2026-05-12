# LightMCP
LightMCP is a lightweight MCP stdio-based server written in C++. This server supports very basic protocol functionality for tools creation.

The example executable contains two tools:
* Arbitrary Lua script execution
* Random number generation

This project is only tested in [LM Studio](https://lmstudio.ai/) and [llama.cpp](https://github.com/ggml-org/llama.cpp) environments. Compatibility with other clients is not guaranteed. If you encounter any error, you're welcome to report it in issues.

> [!WARNING]
> It is up to user to provide sandboxed Lua interpreter without potentially harmful functionality.
> Using stock interpreter with untouched `os`, `io`, `ffi` libraries may lead to malicious code
> execution by an AI model! The potentially harmful functionality from major Lua implementations
> like LuaJIT and all stock Lua interpreters is hidden behind CMake option called `LMCP_UNSAFE`
> and it's disabled by default. Even tho it is disabled, user should never assume that it's 
> safe to let AI execute whatever code it wants to execute, especially with custom interpreters
> I didn't take into consideration. Every script AI writes should be approved by the user first.

## Usage
Add the application to your `mcp.json`. Here's the file example:
```json
{
  "mcpServers": {
    "LightMCP": {
      "command": "D:/LightMCP/build/LightMCP.exe"
    }
  }
}
```

## License
This project is licensed under MIT license.
Used third party libraries and their corresponding licenses are placed inside the `third_party` directory.
