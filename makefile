#!/bin/make -f 
#

## base makefile
BUILD_BASE:=.
include base.mk

## PROJECTS
include telnet.mk

## DEPENDENCIES
-include *.depend

## Rebuild when makefile altered 
# currently broken 
# $(ALL_OBJS) *.depend : makefile
