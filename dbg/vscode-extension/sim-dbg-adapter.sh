#!/bin/bash
DIR="$(cd "$(dirname "$0")/.." && pwd)"
exec "$DIR/build/sim-dbg" --dap
