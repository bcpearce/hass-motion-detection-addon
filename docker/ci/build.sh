#!/bin/bash
mkdir buildx && cd buildx
cmake .. --preset=Release
cmake --build .