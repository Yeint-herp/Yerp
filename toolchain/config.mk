LLVM_REPO := https://github.com/llvm/llvm-project.git
LLVM_VERSION := release/22.x

LLVM_TARGETS_TO_BUILD := X86
LLVM_PROJECTS := clang;lld

LLVM_JOBS ?= 4
LLVM_LINK_JOBS ?= 1

LLVM_BUILD_TYPE := Release
