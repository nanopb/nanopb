# Nanopb Development Container

This directory contains the configuration for a VS Code Development Container for the nanopb project.

## Features

The development container includes:

- **Base Tools**: GCC, G++, Make, CMake, Ninja
- **Python**: Python 3 with pip, pylint, black, mypy, pytest
- **Protobuf**: protoc compiler and Python libraries
- **Debugging**: GDB, Valgrind
- **Shell**: Zsh with Oh My Zsh
- **VS Code Extensions**:
  - C/C++ IntelliSense and debugging
  - Python support with Pylance
  - Proto3 syntax highlighting
  - GitHub Copilot
  - GitLens
  - Spell checker

## Usage

### Prerequisites

1. Install [Docker](https://www.docker.com/get-started)
2. Install [VS Code](https://code.visualstudio.com/)
3. Install the [Remote - Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) extension

### Getting Started

1. Open this repository in VS Code
2. When prompted, click "Reopen in Container" or run the command `Remote-Containers: Reopen in Container`
3. VS Code will build the container and install all extensions
4. The workspace will be mounted at `/workspace` inside the container

### Working with the Container

- The container preserves your shell history between sessions
- Git credentials are mounted from your host system (read-only)
- All nanopb tools are pre-installed and configured
- Python path is set to include the generator directory

### Running Examples

```bash
# Navigate to an example
cd examples/validation_simple

# Generate protobuf files with validation
protoc -I. -I../../generator/proto \n  --nanopb_out=--protoc-insertion-points:. \n  --nanopb-validate_out=. \n  validation_simple.proto

# Compile the example
gcc -I../.. -o validation_simple validation_simple.c validation_simple.pb.c ../../pb_*.c
```

### Customization

You can customize the development container by modifying:

- `devcontainer.json`: VS Code settings and extensions
- `Dockerfile`: System packages and tools
- `docker-compose.yml`: Container configuration and volumes

## Troubleshooting

### Container Build Issues

If the container fails to build:

1. Ensure Docker is running
2. Try rebuilding without cache: `Remote-Containers: Rebuild Container Without Cache`

### Permission Issues

The container runs as root by default. If you need a non-root user, uncomment the user creation section in the Dockerfile.

### Performance Issues

If file operations are slow, you can adjust the volume mount consistency in `docker-compose.yml`. The current setting uses `cached` for better performance on macOS.
