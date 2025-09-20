# Agens - Local LLM Agent (C++20)

## Project Overview

A Japanese-focused local LLM agent that connects to Ollama and LM Studio backends. Automatically detects system specs (GPU/VRAM/RAM/Apple Silicon) and adjusts inference parameters accordingly.

## Key Features

- **Backends**: Ollama (localhost:11434) and LM Studio (localhost:1234)
- **Auto-tuning**: System detection for optimal parameters
- **Commands**: Web search, file finding, autonomous execution, shell commands
- **Japanese-first**: Always responds in Japanese with system prompt

## Build System

- **CMake** (>=3.16) with C++20 standard
- **Dependencies**: curl command-line tool for HTTP requests
- **Build**: `mkdir build && cd build && cmake .. && cmake --build . -j`
- **Executables**: `agens` (main) and `unit_tests`

## Architecture

### Core Components
- `main.cpp`: CLI parsing, REPL loop, command routing
- `chat.cpp`: Message building, JSON parsing helpers
- `backend.cpp`: Ollama/LM Studio API integration
- `system_info.cpp`: Cross-platform system detection
- `config.cpp`: Settings persistence
- `web_search.cpp`: DuckDuckGo integration
- `file_finder.cpp`: Local file relevance scoring
- `agent_mode.cpp`: Autonomous file generation from AGENT(S).md

### Key Patterns
- **HTTP via curl subprocess**: Cross-platform HTTP through temporary JSON files
- **Simple JSON parsing**: Custom lightweight parser for API responses
- **Command pattern**: Slash commands for REPL functionality
- **Config auto-save**: Settings persist immediately on change

## Important Details

- **No external HTTP library**: Uses curl subprocess for maximum compatibility
- **System detection**: Platform-specific commands (sysctl, /proc, wmic)
- **Auto-mode**: Reads AGENT(S).md files and applies file blocks from LLM output
- **Permission system**: Allow/deny patterns for shell command execution
- **Working directory**: Supports cd command with ~ expansion

## Development Workflow

- Tests run with `ctest --output-on-failure`
- Unit tests cover JSON parsing, parameter tuning, and API request building
- No network dependencies in tests
- Configuration stored in platform-specific directories (XDG_CONFIG_HOME, APPDATA)

## Security Considerations

- Shell command execution requires confirmation or explicit allow patterns
- Deny patterns take precedence over allow patterns
- Auto-mode supports dry-run and confirmation modes
