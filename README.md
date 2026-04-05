# LightMCP
LightMCP is a lightweight MCP stdio-based server written in C++. This server supports very basic protocol functionality for tools creation.

The example executable contains two tools:
* Arbitrary Lua script execution
* Random number generation

> [!WARNING]
> It is up to user to provide sandboxed Lua interpreter without potentially harmful functionality.
> Using stock interpreter with untouched `os`, `io`, `ffi` libraries may lead to malicious code
> execution by an AI model! Every script AI writes should be approved by user first.

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
