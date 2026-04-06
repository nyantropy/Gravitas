#!/bin/bash
set -e

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build_release"

EXECUTABLES=(
    "GtsScene1"
    "GtsScene2"
    "GtsScene3"
)

CLEAN=0
TARGET_NAME=""

for arg in "$@"; do
    case "$arg" in
        --clean)
            CLEAN=1
            ;;
        *)
            if [ -n "$TARGET_NAME" ]; then
                echo "ERROR: Only one executable name may be specified"
                echo "Usage: $0 [--clean] [${EXECUTABLES[*]}]"
                exit 1
            fi
            TARGET_NAME="$arg"
            ;;
    esac
done

if [ "$CLEAN" -eq 1 ]; then
    echo "=== Removing existing Release build directory ==="
    rm -rf "$BUILD_DIR"
fi

if [ ! -d "$BUILD_DIR" ] || [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "=== Configuring Release build ==="
    cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
fi

echo "=== Building Release ==="
cmake --build "$BUILD_DIR" --parallel

if [ -n "$TARGET_NAME" ]; then
    MATCHED=0
    for exe in "${EXECUTABLES[@]}"; do
        if [ "$exe" = "$TARGET_NAME" ]; then
            MATCHED=1
            break
        fi
    done

    if [ "$MATCHED" -ne 1 ]; then
        echo "ERROR: Unknown executable '$TARGET_NAME'"
        echo "Known executables: ${EXECUTABLES[*]}"
        exit 1
    fi

    EXECUTABLES_TO_COPY=("$TARGET_NAME")
else
    EXECUTABLES_TO_COPY=("${EXECUTABLES[@]}")
fi

echo "=== Copying executable(s) to project root ==="
COPIED_PATHS=()
for exe in "${EXECUTABLES_TO_COPY[@]}"; do
    BUILT_EXE=$(find "$BUILD_DIR" -path "*/$exe" -type f -executable | head -1)

    if [ -z "$BUILT_EXE" ]; then
        echo "ERROR: Could not find executable '$exe' in $BUILD_DIR"
        exit 1
    fi

    cp "$BUILT_EXE" "$PROJECT_ROOT/$exe"
    chmod +x "$PROJECT_ROOT/$exe"
    COPIED_PATHS+=("$PROJECT_ROOT/$exe")
    echo "Copied $exe from $BUILT_EXE"
done

echo ""
echo "=== Done! ==="
for path in "${COPIED_PATHS[@]}"; do
    echo "Executable: $(basename "$path")"
    echo "Path: $path"
    echo "Run with: ./$(basename "$path")"
done
echo "Shaders and resources resolve relative to the engine directory."
