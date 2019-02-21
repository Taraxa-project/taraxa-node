#
# @Copyright: Taraxa.io 
# @Author: Chia-Chun Lin 
# @Date: 2019-02-20 16:06:11 
# @Last Modified by: Chia-Chun Lin 
# @Last Modified time: 2019-02-20 16:06:11 
#  

CXX := g++
CXXFLAGS := -std=c++17 -I.. -o -Wno 
OBJ_DIR := obj

SRC_DIR := google/api
SUB_DIR := google/api/experimental

SRC := $(wildcard api/*.cc) 
OBJ := $(patsubst api/%.cc, $(OBJ_DIR)/%.o, $(SRC))
SUB_SRC := $(wildcard api/experimental/*.cc)
SUB_OBJ := $(patsubst api/experimental/%.cc, $(OBJ_DIR)/%.o, $(SUB_SRC))

all: create_object_dir $(OBJ) $(SUB_OBJ)

create_object_dir: 
	@echo Making object files ...
	@if [ ! -d $(OBJ_DIR) ]; then	mkdir -p $(OBJ_DIR); fi 

$(OBJ_DIR)/%.o: api/%.cc
	@$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJ_DIR)/%.o: api/experimental/%.cc
	@$(CXX) $(CXXFLAGS) -c -o $@ $<

c: clean
clean: 
	@echo CLEAN && rm -r $(OBJ_DIR)/*

